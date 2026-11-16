#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

// A plain rendering of the HTML that packages use for their Welcome, Read Me, License and
// Conclusion pages (AppKit's own HTML import is not available here): paragraphs, line
// breaks, headings, bold, italic, underline, lists and character references. Everything else
// is dropped and its text kept.
#ifdef __cplusplus
extern "C" {
#endif

NSAttributedString* AttributedStringFromHTML(NSString* html);

// The text of a package resource as shown to the user: RTF, HTML or plain text, by MIME type
NSAttributedString* AttributedStringFromResource(NSData* data, NSString* mimeType);

#ifdef __cplusplus
}
#endif
