#import "InstallerAppDelegate.h"
#import "InstallerWindowController.h"

@implementation InstallerAppDelegate
{
	NSMutableArray* _controllers;
	BOOL _openedSomething;
}

- (instancetype)init
{
	self = [super init];
	_controllers = [NSMutableArray array];
	return self;
}

- (NSMenu*)menuNamed:(NSString*)title items:(NSArray*)items
{
	NSMenu* menu = [[NSMenu alloc] initWithTitle:title];
	for (NSArray* item in items)
	{
		if ([item count] == 0)
		{
			[menu addItem:[NSMenuItem separatorItem]];
			continue;
		}
		NSMenuItem* mi = [menu addItemWithTitle:item[0] action:NSSelectorFromString(item[1]) keyEquivalent:item[2]];
		if ([item count] > 3)
			[mi setTarget:item[3]];
	}
	return menu;
}

- (void)buildMenu
{
	NSMenu* bar = [[NSMenu alloc] initWithTitle:@"Main"];
	
	NSMenuItem* appItem = [bar addItemWithTitle:@"Installer" action:NULL keyEquivalent:@""];
	[appItem setSubmenu:[self menuNamed:@"Installer" items:@[
		@[ @"About Installer", @"orderFrontStandardAboutPanel:", @"" ],
		@[],
		@[ @"Quit Installer", @"terminate:", @"q" ] ]]];
	
	NSMenuItem* fileItem = [bar addItemWithTitle:@"File" action:NULL keyEquivalent:@""];
	[fileItem setSubmenu:[self menuNamed:@"File" items:@[
		@[ @"Open…", @"openDocument:", @"o", self ],
		@[ @"Close", @"performClose:", @"w" ] ]]];
	
	NSMenuItem* windowItem = [bar addItemWithTitle:@"Window" action:NULL keyEquivalent:@""];
	NSMenu* windowMenu = [self menuNamed:@"Window" items:@[
		@[ @"Minimize", @"performMiniaturize:", @"m" ] ]];
	[windowItem setSubmenu:windowMenu];
	
	[NSApp setMainMenu:bar];
	[NSApp setWindowsMenu:windowMenu];
}

// Returns whether the package was opened; says why not in an alert
- (BOOL)openPackage:(NSString*)path
{
	NSString* message = nil;
	
	// a package already open gets its window back
	for (InstallerWindowController* c in _controllers)
	{
		if ([[c packagePath] isEqualToString:path])
		{
			[[c window] makeKeyAndOrderFront:nil];
			return YES;
		}
	}
	
	InstallerWindowController* controller = [[InstallerWindowController alloc] initWithPackagePath:path errorMessage:&message];
	if (controller == nil)
	{
		NSAlert* alert = [[NSAlert alloc] init];
		[alert setMessageText:[NSString stringWithFormat:@"The package “%@” can’t be opened.", [path lastPathComponent]]];
		[alert setInformativeText:message ?: @"It is not a package that the installer understands."];
		[alert runModal];
		return NO;
	}
	
	_openedSomething = YES;
	[_controllers addObject:controller];
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowWillClose:)
		name:NSWindowWillCloseNotification object:[controller window]];
	[controller showWindow:nil];
	[NSApp activateIgnoringOtherApps:YES];
	return YES;
}

- (void)windowWillClose:(NSNotification*)note
{
	NSWindow* window = [note object];
	
	for (InstallerWindowController* c in [_controllers copy])
	{
		if ([c window] == window)
			[_controllers removeObject:c];
	}
	[[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:window];
}

- (void)openDocument:(id)sender
{
	NSOpenPanel* panel = [NSOpenPanel openPanel];
	[panel setAllowedFileTypes:@[ @"pkg", @"mpkg" ]];
	[panel setAllowsMultipleSelection:YES];
	if ([panel runModal] == NSModalResponseOK)
	{
		for (NSURL* url in [panel URLs])
			[self openPackage:[url path]];
	}
}

- (BOOL)application:(NSApplication*)app openFile:(NSString*)path
{
	return [self openPackage:path];
}

- (void)application:(NSApplication*)app openFiles:(NSArray*)paths
{
	BOOL ok = YES;
	for (NSString* path in paths)
		ok = [self openPackage:path] && ok;
	[app replyToOpenOrPrint:ok ? NSApplicationDelegateReplySuccess : NSApplicationDelegateReplyFailure];
}

- (void)applicationDidFinishLaunching:(NSNotification*)note
{
	[self buildMenu];
	
	// packages named on the command line
	NSArray* arguments = [[NSProcessInfo processInfo] arguments];
	for (NSUInteger i = 1; i < [arguments count]; i++)
	{
		NSString* path = arguments[i];
		NSString* ext = [[path pathExtension] lowercaseString];
		if (([ext isEqualToString:@"pkg"] || [ext isEqualToString:@"mpkg"]) && [[NSFileManager defaultManager] fileExistsAtPath:path])
			[self openPackage:path];
	}
	
	if (!_openedSomething)
		[self openDocument:nil];
	if (!_openedSomething)
		[NSApp terminate:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app
{
	return YES;
}

@end
