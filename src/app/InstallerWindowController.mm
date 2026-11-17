#import "InstallerWindowController.h"
#import "HTMLText.h"
#include "pkg/Installer.h"
#include "pkg/Volumes.h"
#include <memory>
#include <string>
#include <vector>

enum Step
{
	StepIntroduction,
	StepReadMe,
	StepLicense,
	StepDestination,
	StepType,
	StepInstall,
	StepSummary,
};

static NSString* StepTitle(Step step)
{
	switch (step)
	{
		case StepIntroduction: return @"Introduction";
		case StepReadMe: return @"Read Me";
		case StepLicense: return @"License";
		case StepDestination: return @"Destination Select";
		case StepType: return @"Installation Type";
		case StepInstall: return @"Installation";
		case StepSummary: return @"Summary";
	}
	return @"";
}

static NSString* FromUTF8(const std::string& s)
{
	NSString* rv = [NSString stringWithUTF8String:s.c_str()];
	return rv != nil ? rv : @"";
}

static NSString* SizeString(unsigned long long kilobytes)
{
	if (kilobytes >= 1024ULL * 1024)
		return [NSString stringWithFormat:@"%.1f GB", kilobytes / (1024.0 * 1024)];
	if (kilobytes >= 1024)
		return [NSString stringWithFormat:@"%.1f MB", kilobytes / 1024.0];
	return [NSString stringWithFormat:@"%llu KB", kilobytes];
}

// One row of the customization outline: a choice of the package
@interface ChoiceItem : NSObject
@property (nonatomic) std::string identifier;
@property (nonatomic, copy) NSString* title;
@property (nonatomic, copy) NSString* choiceDescription;
@property (nonatomic) BOOL enabled;
@property (nonatomic) int selectionState;
@property (nonatomic) size_t installKBytes;
@property (nonatomic, strong) NSMutableArray* children;
@end
@implementation ChoiceItem
@end

// The dot in front of the current step: drawn, so that it is centered on the row whatever the font
@interface StepMarker : NSView
@end
@implementation StepMarker
- (void)drawRect:(NSRect)dirty
{
	NSRect dot = NSInsetRect([self bounds], 0.5, 0.5);
	[[NSColor blackColor] set];
	[[NSBezierPath bezierPathWithOvalInRect:dot] fill];
}
@end

@class InstallerWindowController;

// Receives what the engine reports on the installation thread
class WindowInstallerDelegate : public InstallerDelegate
{
public:
	__unsafe_unretained InstallerWindowController* owner;
	
	void message(Level level, const std::string& text) override;
	void phase(const std::string& name, double percent) override;
	bool confirmExternalProgram(const std::string& command) override;
};

@interface InstallerWindowController () <NSTableViewDataSource, NSTableViewDelegate, NSOutlineViewDataSource, NSOutlineViewDelegate>
{
	std::unique_ptr<Installer> _installer;
	WindowInstallerDelegate _delegate;
	std::vector<Volume> _volumes;
	
	NSString* _packagePath;
	NSMutableArray* _steps;      // NSNumbers of the Steps this package has
	NSUInteger _stepIndex;
	NSView* _content;
	NSMutableArray* _sidebar;
	NSMutableArray* _sidebarMarkers;
	NSButton* _continueButton;
	NSButton* _backButton;
	
	NSInteger _volumeIndex;
	BOOL _customizing;
	NSMutableArray* _choiceItems;
	NSTextField* _spaceLabel;
	NSOutlineView* _outline;
	NSTableView* _volumeTable;
	
	NSProgressIndicator* _progress;
	NSTextField* _statusLabel;
	NSTextView* _logView;
	NSMutableString* _log;
	BOOL _installing, _succeeded;
	NSString* _failure;
	
	Installer::CheckResult _check;
}
@end

void WindowInstallerDelegate::message(Level level, const std::string& text)
{
	NSString* line = [NSString stringWithFormat:@"%@%@\n", level == Warning ? @"Warning: " : @"", FromUTF8(text)];
	[owner performSelectorOnMainThread:@selector(appendLog:) withObject:line waitUntilDone:NO];
}

void WindowInstallerDelegate::phase(const std::string& name, double percent)
{
	[owner performSelectorOnMainThread:@selector(setPhase:) withObject:@{ @"name": FromUTF8(name), @"percent": @(percent) } waitUntilDone:NO];
}

// The user is asked on the main thread; the installation waits for the answer
bool WindowInstallerDelegate::confirmExternalProgram(const std::string& command)
{
	NSMutableDictionary* question = [NSMutableDictionary dictionaryWithObject:FromUTF8(command) forKey:@"command"];
	[owner performSelectorOnMainThread:@selector(askToRunProgram:) withObject:question waitUntilDone:YES];
	return [question[@"allowed"] boolValue];
}

@implementation InstallerWindowController

@synthesize packagePath = _packagePath;

- (instancetype)initWithPackagePath:(NSString*)path errorMessage:(NSString**)message
{
	NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 480)
		styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
		backing:NSBackingStoreBuffered defer:NO];
	self = [super initWithWindow:window];
	if (self == nil)
		return nil;
	
	_packagePath = [path copy];
	_delegate.owner = self;
	
	try
	{
		_installer.reset(new Installer([path fileSystemRepresentation], nullptr));
		_installer->setDelegate(&_delegate);
		
		std::vector<std::string> languages;
		for (NSString* lang in [NSLocale preferredLanguages])
		{
			languages.push_back([lang UTF8String]);
			NSRange dash = [lang rangeOfString:@"-"];
			if (dash.location != NSNotFound)
				languages.push_back([[lang substringToIndex:dash.location] UTF8String]);
		}
		languages.push_back("English");
		languages.push_back("Base");
		_installer->setPreferredLanguages(languages);
		
		_volumes = mountedVolumes();
		if (_volumes.empty() || !_installer->setTarget(_volumes[0].mountPoint))
			throw std::runtime_error("There is no volume to install on.");
		_volumeIndex = 0;
		
		_check = _installer->checkInstallation();
		
		// the steps this package has
		_steps = [NSMutableArray arrayWithObject:@(StepIntroduction)];
		Installer::Resource resource;
		if (_installer->resource("readme", resource))
			[_steps addObject:@(StepReadMe)];
		if (_installer->resource("license", resource))
			[_steps addObject:@(StepLicense)];
		[_steps addObjectsFromArray:@[ @(StepDestination), @(StepType), @(StepInstall), @(StepSummary) ]];
		
		[window setTitle:[NSString stringWithFormat:@"Install %@", FromUTF8(_installer->title())]];
	}
	catch (const std::exception& e)
	{
		if (message)
			*message = FromUTF8(e.what());
		return nil;
	}
	
	_log = [NSMutableString string];
	[self buildFrame];
	[self showStep:0];
	[window center];
	return self;
}

- (Step)step
{
	return (Step) [_steps[_stepIndex] intValue];
}

// ---- The frame: step list, content area and buttons ---------------------------------

- (void)buildFrame
{
	NSView* root = [[self window] contentView];
	
	NSBox* sidebarBox = [[NSBox alloc] initWithFrame:NSMakeRect(0, 0, 160, 480)];
	[sidebarBox setBoxType:NSBoxCustom];
	[sidebarBox setBorderType:NSNoBorder];
	[sidebarBox setFillColor:[NSColor colorWithCalibratedWhite:0.93 alpha:1]];
	[sidebarBox setTitlePosition:NSNoTitle];
	[root addSubview:sidebarBox];
	
	_sidebar = [NSMutableArray array];
	_sidebarMarkers = [NSMutableArray array];
	CGFloat y = 400;
	for (NSNumber* step in _steps)
	{
		NSTextField* label = [NSTextField labelWithString:StepTitle((Step) [step intValue])];
		[label setFrame:NSMakeRect(28, y, 128, 16)]; // as tall as the text, so the marker can be centered on it
		[root addSubview:label];
		[_sidebar addObject:label];
		
		// 7 points, centered on the text
		StepMarker* marker = [[StepMarker alloc] initWithFrame:NSMakeRect(14, y + 5, 7, 7)];
		[marker setHidden:YES];
		[root addSubview:marker];
		[_sidebarMarkers addObject:marker];
		y -= 28;
	}
	
	_content = [[NSView alloc] initWithFrame:NSMakeRect(176, 62, 450, 400)];
	[root addSubview:_content];
	
	_backButton = [[NSButton alloc] initWithFrame:NSMakeRect(424, 14, 96, 32)];
	[_backButton setTitle:@"Go Back"];
	[_backButton setBezelStyle:NSBezelStyleRounded];
	[_backButton setTarget:self];
	[_backButton setAction:@selector(goBack:)];
	[root addSubview:_backButton];
	
	_continueButton = [[NSButton alloc] initWithFrame:NSMakeRect(526, 14, 100, 32)];
	[_continueButton setBezelStyle:NSBezelStyleRounded];
	[_continueButton setKeyEquivalent:@"\r"];
	[_continueButton setTarget:self];
	[_continueButton setAction:@selector(goContinue:)];
	[root addSubview:_continueButton];
}

- (NSScrollView*)textViewFor:(NSAttributedString*)text frame:(NSRect)frame
{
	NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:frame];
	NSTextView* view = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width - 16, frame.size.height)];
	
	[view setEditable:NO];
	[view setHorizontallyResizable:NO];
	[view setVerticallyResizable:YES];
	[view setAutoresizingMask:NSViewWidthSizable];
	[[view textStorage] setAttributedString:text];
	[scroll setDocumentView:view];
	[scroll setHasVerticalScroller:YES];
	[scroll setBorderType:NSBezelBorder];
	return scroll;
}

- (NSTextField*)label:(NSString*)text frame:(NSRect)frame bold:(BOOL)bold
{
	NSTextField* label = [NSTextField wrappingLabelWithString:text];
	[label setFrame:frame];
	[label setFont:bold ? [NSFont boldSystemFontOfSize:15] : [NSFont systemFontOfSize:12]];
	return label;
}

- (void)showStep:(NSUInteger)index
{
	_stepIndex = index;
	Step step = [self step];
	
	for (NSView* v in [[_content subviews] copy])
		[v removeFromSuperview];
	for (NSUInteger i = 0; i < [_sidebar count]; i++)
	{
		NSTextField* label = _sidebar[i];
		[label setFont:i == index ? [NSFont boldSystemFontOfSize:12] : [NSFont systemFontOfSize:12]];
		[label setStringValue:StepTitle((Step) [_steps[i] intValue])];
		[_sidebarMarkers[i] setHidden:i != index];
	}
	
	[_continueButton setTitle:@"Continue"];
	[_continueButton setEnabled:YES];
	[_backButton setHidden:NO];
	[_backButton setEnabled:index > 0];
	
	switch (step)
	{
		case StepIntroduction: [self buildIntroduction]; break;
		case StepReadMe: [self buildResource:"readme" fallback:@""]; break;
		case StepLicense: [self buildResource:"license" fallback:@""]; break;
		case StepDestination: [self buildDestination]; break;
		case StepType: [self buildType]; break;
		case StepInstall: [self buildInstall]; break;
		case StepSummary: [self buildSummary]; break;
	}
}

// ---- The steps -------------------------------------------------------------------

- (void)buildIntroduction
{
	NSString* title = [NSString stringWithFormat:@"Welcome to the %@ Installer", FromUTF8(_installer->title())];
	[_content addSubview:[self label:title frame:NSMakeRect(0, 350, 450, 44) bold:YES]];
	
	NSRect textFrame = NSMakeRect(0, 0, 450, 340);
	if (_check.type != Installer::CheckResult::Pass)
	{
		NSTextField* note = [self label:FromUTF8(_check.message) frame:NSMakeRect(0, 280, 450, 56) bold:NO];
		[note setTextColor:_check.type == Installer::CheckResult::Fatal ? [NSColor redColor] : [NSColor orangeColor]];
		[_content addSubview:note];
		textFrame.size.height = 270;
		if (_check.type == Installer::CheckResult::Fatal)
			[_continueButton setEnabled:NO];
	}
	
	Installer::Resource welcome;
	NSAttributedString* text;
	if (_installer->resource("welcome", welcome))
		text = AttributedStringFromResource([NSData dataWithBytes:welcome.data.data() length:welcome.data.size()], FromUTF8(welcome.mimeType));
	else
		text = [[NSAttributedString alloc] initWithString:@"You will be guided through the steps necessary to install this software."
			attributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:12] }];
	[_content addSubview:[self textViewFor:text frame:textFrame]];
}

- (void)buildResource:(const char*)element fallback:(NSString*)fallback
{
	Installer::Resource resource;
	NSAttributedString* text = [[NSAttributedString alloc] initWithString:fallback];
	
	if (_installer->resource(element, resource))
		text = AttributedStringFromResource([NSData dataWithBytes:resource.data.data() length:resource.data.size()], FromUTF8(resource.mimeType));
	[_content addSubview:[self textViewFor:text frame:NSMakeRect(0, 0, 450, 400)]];
}

- (void)buildDestination
{
	[_content addSubview:[self label:@"Select a Destination Volume" frame:NSMakeRect(0, 360, 450, 24) bold:YES]];
	
	NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 90, 450, 260)];
	_volumeTable = [[NSTableView alloc] initWithFrame:[scroll bounds]];
	NSArray* columns = @[ @[ @"name", @"Volume", @(150) ], @[ @"free", @"Free", @(90) ], @[ @"mount", @"Mount point", @(190) ] ];
	for (NSArray* c in columns)
	{
		NSTableColumn* column = [[NSTableColumn alloc] initWithIdentifier:c[0]];
		[column setWidth:[c[2] doubleValue]];
		[[column headerCell] setStringValue:c[1]];
		[_volumeTable addTableColumn:column];
	}
	[_volumeTable setDataSource:self];
	[_volumeTable setDelegate:self];
	[scroll setDocumentView:_volumeTable];
	[scroll setHasVerticalScroller:YES];
	[scroll setBorderType:NSBezelBorder];
	[_content addSubview:scroll];
	[_volumeTable selectRowIndexes:[NSIndexSet indexSetWithIndex:_volumeIndex] byExtendingSelection:NO];
	
	_spaceLabel = [self label:@"" frame:NSMakeRect(0, 20, 450, 60) bold:NO];
	[_content addSubview:_spaceLabel];
	[self updateSpaceLabel];
}

- (void)updateSpaceLabel
{
	unsigned long long needed = _installer->requiredKilobytes();
	unsigned long long available = _volumes[_volumeIndex].available / 1024;
	BOOL fits = needed <= available;
	
	[_spaceLabel setStringValue:fits
		? [NSString stringWithFormat:@"The installation will take %@ of space on “%@”.", SizeString(needed), FromUTF8(_volumes[_volumeIndex].name)]
		: [NSString stringWithFormat:@"The installation needs %@, but only %@ are available on “%@”.", SizeString(needed), SizeString(available), FromUTF8(_volumes[_volumeIndex].name)]];
	[_spaceLabel setTextColor:fits ? [NSColor blackColor] : [NSColor redColor]];
	if ([self step] == StepDestination || [self step] == StepType)
		[_continueButton setEnabled:fits];
}

- (void)buildType
{
	[_continueButton setTitle:@"Install"];
	
	NSString* where = [NSString stringWithFormat:@"Standard Install on “%@”", FromUTF8(_volumes[_volumeIndex].name)];
	[_content addSubview:[self label:_customizing ? @"Custom Install" : where frame:NSMakeRect(0, 360, 450, 24) bold:YES]];
	
	std::vector<Installer::ChoiceNode> tree = _installer->choiceTree();
	BOOL hasChoices = !tree.empty();
	
	if (_customizing && hasChoices)
	{
		NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 90, 450, 260)];
		_outline = [[NSOutlineView alloc] initWithFrame:[scroll bounds]];
		
		NSTableColumn* column = [[NSTableColumn alloc] initWithIdentifier:@"choice"];
		NSButtonCell* cell = [[NSButtonCell alloc] init];
		[cell setButtonType:NSButtonTypeSwitch];
		[cell setAllowsMixedState:YES];
		[column setDataCell:cell];
		[column setWidth:340];
		[[column headerCell] setStringValue:@"Package Name"];
		NSTableColumn* sizeColumn = [[NSTableColumn alloc] initWithIdentifier:@"size"];
		[sizeColumn setWidth:80];
		[[sizeColumn headerCell] setStringValue:@"Size"];
		[_outline addTableColumn:column];
		[_outline addTableColumn:sizeColumn];
		[_outline setOutlineTableColumn:column];
		[_outline setDataSource:self];
		[_outline setDelegate:self];
		[scroll setDocumentView:_outline];
		[scroll setHasVerticalScroller:YES];
		[scroll setBorderType:NSBezelBorder];
		[_content addSubview:scroll];
		[self reloadChoices];
	}
	else
	{
		NSString* text = hasChoices
			? @"This will install the standard set of components. Click Customize to choose what to install."
			: @"Click Install to perform a standard installation of this software on the destination volume.";
		[_content addSubview:[self label:text frame:NSMakeRect(0, 280, 450, 60) bold:NO]];
		_outline = nil;
	}
	
	_spaceLabel = [self label:@"" frame:NSMakeRect(0, 30, 330, 50) bold:NO];
	[_content addSubview:_spaceLabel];
	[self updateSpaceLabel];
	
	if (hasChoices)
	{
		NSButton* toggle = [[NSButton alloc] initWithFrame:NSMakeRect(340, 36, 110, 32)];
		[toggle setTitle:_customizing ? @"Standard Install" : @"Customize"];
		[toggle setBezelStyle:NSBezelStyleRounded];
		[toggle setTarget:self];
		[toggle setAction:@selector(toggleCustomize:)];
		[_content addSubview:toggle];
	}
}

- (void)toggleCustomize:(id)sender
{
	_customizing = !_customizing;
	[self showStep:_stepIndex];
}

- (void)buildInstall
{
	[_continueButton setTitle:@"Close"];
	[_continueButton setEnabled:NO];
	[_backButton setEnabled:NO];
	
	_statusLabel = [self label:@"Preparing for installation…" frame:NSMakeRect(0, 360, 450, 24) bold:YES];
	[_content addSubview:_statusLabel];
	
	_progress = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(0, 328, 450, 20)];
	[_progress setStyle:NSProgressIndicatorStyleBar];
	[_progress setIndeterminate:NO];
	[_progress setMinValue:0];
	[_progress setMaxValue:100];
	[_progress setDoubleValue:0];
	[_content addSubview:_progress];
	
	NSScrollView* scroll = [self textViewFor:[[NSAttributedString alloc] initWithString:_log] frame:NSMakeRect(0, 0, 450, 310)];
	_logView = [scroll documentView];
	[_logView setFont:[NSFont userFixedPitchFontOfSize:10]];
	[_content addSubview:scroll];
	
	[self startInstallation];
}

- (void)buildSummary
{
	[_continueButton setTitle:@"Close"];
	[_backButton setHidden:YES];
	
	[_content addSubview:[self label:_succeeded ? @"The installation was completed successfully." : @"The installation failed." frame:NSMakeRect(0, 360, 450, 24) bold:YES]];
	
	NSAttributedString* text = nil;
	Installer::Resource conclusion;
	if (_succeeded)
	{
		if (_installer->resource("conclusion", conclusion))
			text = AttributedStringFromResource([NSData dataWithBytes:conclusion.data.data() length:conclusion.data.size()], FromUTF8(conclusion.mimeType));
		else
			text = [[NSAttributedString alloc] initWithString:@"The software was installed." attributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:12] }];
	}
	else
		text = [[NSAttributedString alloc] initWithString:_failure ?: @"" attributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:12] }];
	[_content addSubview:[self textViewFor:text frame:NSMakeRect(0, 0, 450, 340)]];
}

// ---- Buttons -----------------------------------------------------------------------

- (void)goBack:(id)sender
{
	if (_stepIndex > 0)
		[self showStep:_stepIndex - 1];
}

- (void)goContinue:(id)sender
{
	switch ([self step])
	{
		case StepLicense:
			[self askToAgreeToLicense];
			break;
		case StepType:
			[self showStep:_stepIndex + 1];
			break;
		case StepInstall:
		case StepSummary:
			[[self window] close];
			break;
		default:
			[self showStep:_stepIndex + 1];
	}
}

- (void)askToAgreeToLicense
{
	NSAlert* alert = [[NSAlert alloc] init];
	[alert setMessageText:@"To continue installing the software you must agree to the terms of the software license agreement."];
	[alert setInformativeText:@"Click Agree to continue or click Disagree to cancel the installation and quit the Installer."];
	[alert addButtonWithTitle:@"Agree"];
	[alert addButtonWithTitle:@"Disagree"];
	[alert addButtonWithTitle:@"Read License"];
	
	NSInteger answer = [alert runModal];
	if (answer == NSAlertFirstButtonReturn)
		[self showStep:_stepIndex + 1];
	else if (answer == NSAlertSecondButtonReturn)
		[[self window] close];
}

// ---- Installation ------------------------------------------------------------------

- (void)startInstallation
{
	_installing = YES;
	_succeeded = NO;
	_failure = nil;
	[NSThread detachNewThreadSelector:@selector(runInstallation) toTarget:self withObject:nil];
}

// on its own thread
- (void)runInstallation
{
	@autoreleasepool
	{
		NSString* failure = nil;
		try
		{
			_installer->installPackage();
		}
		catch (const std::exception& e)
		{
			failure = FromUTF8(e.what());
		}
		[self performSelectorOnMainThread:@selector(installationFinished:) withObject:failure ?: (id) [NSNull null] waitUntilDone:NO];
	}
}

- (void)installationFinished:(id)failure
{
	_installing = NO;
	_succeeded = failure == [NSNull null];
	_failure = _succeeded ? nil : failure;
	[_progress setDoubleValue:_succeeded ? 100 : [_progress doubleValue]];
	[_continueButton setEnabled:YES];
	[_continueButton setTitle:@"Continue"];
	if (_succeeded)
		[_continueButton setTitle:@"Continue"];
	
	// the Summary follows on its own, as on macOS
	[self showStep:_stepIndex + 1];
}

- (void)appendLog:(NSString*)line
{
	[_log appendString:line];
	if (_logView != nil)
	{
		[[_logView textStorage] appendAttributedString:[[NSAttributedString alloc] initWithString:line
			attributes:@{ NSFontAttributeName: [NSFont userFixedPitchFontOfSize:10] }]];
		[_logView scrollRangeToVisible:NSMakeRange([[_logView string] length], 0)];
	}
}

- (void)setPhase:(NSDictionary*)phase
{
	[_statusLabel setStringValue:phase[@"name"]];
	if ([phase[@"percent"] doubleValue] >= 0)
		[_progress setDoubleValue:[phase[@"percent"] doubleValue]];
}

- (void)askToRunProgram:(NSMutableDictionary*)question
{
	NSAlert* alert = [[NSAlert alloc] init];
	[alert setMessageText:@"This package wants to run a program."];
	[alert setInformativeText:[NSString stringWithFormat:@"“%@” is part of the package and would run with the privileges of the installer. Only allow it if you trust the package.", question[@"command"]]];
	[alert addButtonWithTitle:@"Don’t Allow"];
	[alert addButtonWithTitle:@"Allow"];
	question[@"allowed"] = @([alert runModal] == NSAlertSecondButtonReturn);
}

// ---- Volumes -----------------------------------------------------------------------

- (NSInteger)numberOfRowsInTableView:(NSTableView*)table
{
	return (NSInteger) _volumes.size();
}

- (id)tableView:(NSTableView*)table objectValueForTableColumn:(NSTableColumn*)column row:(NSInteger)row
{
	const Volume& v = _volumes[row];
	NSString* identifier = [column identifier];
	
	if ([identifier isEqualToString:@"name"])
		return FromUTF8(v.name);
	if ([identifier isEqualToString:@"free"])
		return SizeString(v.available / 1024);
	return FromUTF8(v.mountPoint);
}

- (void)tableViewSelectionDidChange:(NSNotification*)note
{
	NSInteger row = [_volumeTable selectedRow];
	if (row < 0 || row >= (NSInteger) _volumes.size())
		return;
	
	_volumeIndex = row;
	_installer->setTarget(_volumes[row].mountPoint);
	[self updateSpaceLabel];
}

// ---- Choices -----------------------------------------------------------------------

static ChoiceItem* ItemFromNode(const Installer::ChoiceNode& node)
{
	ChoiceItem* item = [[ChoiceItem alloc] init];
	item.identifier = node.id;
	item.title = FromUTF8(node.title);
	item.choiceDescription = FromUTF8(node.description);
	item.enabled = node.enabled;
	item.selectionState = node.selectionState;
	item.installKBytes = node.installKBytes;
	item.children = [NSMutableArray array];
	for (const Installer::ChoiceNode& child : node.children)
	{
		if (child.visible)
			[item.children addObject:ItemFromNode(child)];
	}
	return item;
}

- (void)reloadChoices
{
	_choiceItems = [NSMutableArray array];
	for (const Installer::ChoiceNode& node : _installer->choiceTree())
	{
		if (node.visible)
			[_choiceItems addObject:ItemFromNode(node)];
	}
	[_outline reloadData];
	for (ChoiceItem* item in _choiceItems)
		[_outline expandItem:item expandChildren:YES];
}

- (NSInteger)outlineView:(NSOutlineView*)outline numberOfChildrenOfItem:(id)item
{
	return [(item ? [(ChoiceItem*) item children] : _choiceItems) count];
}

- (id)outlineView:(NSOutlineView*)outline child:(NSInteger)index ofItem:(id)item
{
	return (item ? [(ChoiceItem*) item children] : _choiceItems)[index];
}

- (BOOL)outlineView:(NSOutlineView*)outline isItemExpandable:(id)item
{
	return [[(ChoiceItem*) item children] count] > 0;
}

- (id)outlineView:(NSOutlineView*)outline objectValueForTableColumn:(NSTableColumn*)column byItem:(id)item
{
	ChoiceItem* choice = item;
	
	if ([[column identifier] isEqualToString:@"size"])
		return choice.installKBytes ? SizeString(choice.installKBytes) : @"";
	return @(choice.selectionState == 1 ? NSOnState : (choice.selectionState == 0 ? NSOffState : NSMixedState));
}

- (void)outlineView:(NSOutlineView*)outline willDisplayCell:(id)cell forTableColumn:(NSTableColumn*)column item:(id)item
{
	ChoiceItem* choice = item;
	
	if ([[column identifier] isEqualToString:@"choice"])
	{
		[cell setTitle:choice.title];
		[cell setEnabled:choice.enabled];
	}
}

- (void)outlineView:(NSOutlineView*)outline setObjectValue:(id)value forTableColumn:(NSTableColumn*)column byItem:(id)item
{
	ChoiceItem* choice = item;
	
	if (![[column identifier] isEqualToString:@"choice"] || !choice.enabled)
		return;
	// a mixed choice becomes fully selected
	_installer->setChoiceSelected(choice.identifier, choice.selectionState != 1);
	[self reloadChoices];
	[self updateSpaceLabel];
}

@end
