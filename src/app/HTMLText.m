#import "HTMLText.h"

static NSString* DecodeEntity(NSString* name)
{
	static NSDictionary* named;
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		named = @{ @"amp": @"&", @"lt": @"<", @"gt": @">", @"quot": @"\"", @"apos": @"'", @"nbsp": @" ",
				@"copy": @"©", @"reg": @"®", @"trade": @"™", @"ndash": @"–", @"mdash": @"—",
				@"lsquo": @"‘", @"rsquo": @"’", @"ldquo": @"“", @"rdquo": @"”", @"hellip": @"…",
				@"bull": @"•" };
	});
	
	if ([name hasPrefix:@"#"])
	{
		unsigned long code = 0;
		NSString* digits = [name substringFromIndex:1];
		if ([digits hasPrefix:@"x"] || [digits hasPrefix:@"X"])
			code = strtoul([[digits substringFromIndex:1] UTF8String], NULL, 16);
		else
			code = strtoul([digits UTF8String], NULL, 10);
		if (code == 0 || code > 0xFFFF)
			return nil;
		unichar c = (unichar) code;
		return [NSString stringWithCharacters:&c length:1];
	}
	return named[name];
}

@interface HTMLRenderer : NSObject
{
	NSMutableAttributedString* _out;
	int _bold, _italic, _underline, _pre, _skip;
	CGFloat _size;
	BOOL _atLineStart, _pendingSpace;
}
- (NSAttributedString*)render:(NSString*)html;
@end

@implementation HTMLRenderer

- (void)append:(NSString*)text
{
	if (_skip > 0 || [text length] == 0)
		return;
	
	NSFont* font = _bold ? [NSFont boldSystemFontOfSize:_size] : [NSFont systemFontOfSize:_size];
	if (_italic)
	{
		NSFont* italic = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSItalicFontMask];
		if (italic != nil)
			font = italic;
	}
	NSMutableDictionary* attrs = [NSMutableDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
	if (_underline)
		[attrs setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
	[_out appendAttributedString:[[NSAttributedString alloc] initWithString:text attributes:attrs]];
	_atLineStart = [text hasSuffix:@"\n"];
}

// A line break, unless the output is at the start of a line already; blank adds an empty line
- (void)breakLine:(BOOL)blank
{
	if (_skip > 0 || [_out length] == 0)
		return;
	if (!_atLineStart)
		[self append:@"\n"];
	if (blank && ![[_out string] hasSuffix:@"\n\n"])
		[self append:@"\n"];
	_pendingSpace = NO;
}

- (void)text:(NSString*)raw
{
	if (_pre > 0)
	{
		[self append:raw];
		return;
	}
	
	// whitespace runs collapse to one space, none at the start of a line
	NSMutableString* collapsed = [NSMutableString string];
	BOOL space = _pendingSpace;
	for (NSUInteger i = 0; i < [raw length]; i++)
	{
		unichar c = [raw characterAtIndex:i];
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			space = YES;
		else
		{
			if (space && !_atLineStart && [collapsed length] + [_out length] > 0)
				[collapsed appendString:@" "];
			space = NO;
			_atLineStart = NO;
			[collapsed appendFormat:@"%C", c];
		}
	}
	_pendingSpace = space;
	[self append:collapsed];
	if ([collapsed length] > 0)
		_atLineStart = NO;
}

- (void)tag:(NSString*)body
{
	BOOL closing = [body hasPrefix:@"/"];
	NSString* name = closing ? [body substringFromIndex:1] : body;
	NSRange end = [name rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@" \t\r\n/"]];
	if (end.location != NSNotFound)
		name = [name substringToIndex:end.location];
	name = [name lowercaseString];
	int delta = closing ? -1 : 1;
	
	if ([name isEqualToString:@"b"] || [name isEqualToString:@"strong"])
		_bold = MAX(0, _bold + delta);
	else if ([name isEqualToString:@"i"] || [name isEqualToString:@"em"])
		_italic = MAX(0, _italic + delta);
	else if ([name isEqualToString:@"u"])
		_underline = MAX(0, _underline + delta);
	else if ([name isEqualToString:@"pre"])
	{
		_pre = MAX(0, _pre + delta);
		[self breakLine:NO];
	}
	else if ([name isEqualToString:@"head"] || [name isEqualToString:@"style"] || [name isEqualToString:@"script"] || [name isEqualToString:@"title"])
		_skip = MAX(0, _skip + delta);
	else if ([name isEqualToString:@"br"])
		[self append:@"\n"];
	else if ([name isEqualToString:@"p"] || [name isEqualToString:@"div"] || [name isEqualToString:@"ul"] || [name isEqualToString:@"ol"] || [name isEqualToString:@"table"])
		[self breakLine:!closing || [name isEqualToString:@"p"]];
	else if ([name isEqualToString:@"tr"])
		[self breakLine:NO];
	else if ([name isEqualToString:@"li"])
	{
		[self breakLine:NO];
		if (!closing)
		{
			[self append:@"• "];
			_atLineStart = NO;
		}
	}
	else if ([name length] == 2 && [name characterAtIndex:0] == 'h' && [name characterAtIndex:1] >= '1' && [name characterAtIndex:1] <= '6')
	{
		[self breakLine:YES];
		int level = [name characterAtIndex:1] - '0';
		_size = closing ? 13 : (level <= 2 ? 20 : (level <= 4 ? 16 : 13));
		_bold = MAX(0, _bold + delta);
	}
}

- (NSAttributedString*)render:(NSString*)html
{
	_out = [[NSMutableAttributedString alloc] init];
	_bold = _italic = _underline = _pre = _skip = 0;
	_size = 13;
	_atLineStart = YES;
	_pendingSpace = NO;
	
	NSUInteger i = 0, length = [html length];
	NSMutableString* run = [NSMutableString string];
	
	while (i < length)
	{
		unichar c = [html characterAtIndex:i];
		
		if (c == '<')
		{
			if ([html compare:@"<!--" options:0 range:NSMakeRange(i, MIN((NSUInteger) 4, length - i))] == NSOrderedSame)
			{
				NSRange close = [html rangeOfString:@"-->" options:0 range:NSMakeRange(i, length - i)];
				i = close.location == NSNotFound ? length : NSMaxRange(close);
				continue;
			}
			NSRange close = [html rangeOfString:@">" options:0 range:NSMakeRange(i, length - i)];
			if (close.location == NSNotFound)
				break;
			[self text:run];
			[run setString:@""];
			[self tag:[html substringWithRange:NSMakeRange(i + 1, close.location - i - 1)]];
			i = NSMaxRange(close);
		}
		else if (c == '&')
		{
			NSRange semi = [html rangeOfString:@";" options:0 range:NSMakeRange(i, MIN((NSUInteger) 10, length - i))];
			NSString* decoded = semi.location == NSNotFound ? nil : DecodeEntity([html substringWithRange:NSMakeRange(i + 1, semi.location - i - 1)]);
			if (decoded != nil)
			{
				[run appendString:decoded];
				i = NSMaxRange(semi);
			}
			else
			{
				[run appendString:@"&"];
				i++;
			}
		}
		else
		{
			[run appendFormat:@"%C", c];
			i++;
		}
	}
	[self text:run];
	
	// no trailing blank lines
	while ([_out length] > 0 && [[_out string] hasSuffix:@"\n"])
		[_out deleteCharactersInRange:NSMakeRange([_out length] - 1, 1)];
	return _out;
}

@end

NSAttributedString* AttributedStringFromHTML(NSString* html)
{
	return [[[HTMLRenderer alloc] init] render:html];
}

NSAttributedString* AttributedStringFromResource(NSData* data, NSString* mimeType)
{
	if ([mimeType isEqualToString:@"text/rtf"])
	{
		NSAttributedString* rtf = [[NSAttributedString alloc] initWithRTF:data documentAttributes:NULL];
		if (rtf != nil)
			return rtf;
	}
	
	NSString* text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
	if (text == nil)
		text = [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];
	if (text == nil)
		text = @"";
	
	if ([mimeType isEqualToString:@"text/html"])
		return AttributedStringFromHTML(text);
	
	return [[NSAttributedString alloc] initWithString:text attributes:@{ NSFontAttributeName: [NSFont systemFontOfSize:12] }];
}
