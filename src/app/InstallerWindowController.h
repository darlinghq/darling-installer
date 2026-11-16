#import <AppKit/AppKit.h>

// One installer window: the steps of the Installer application (Introduction, Read Me,
// License, Destination Select, Installation Type, Installation, Summary) for one package.
@interface InstallerWindowController : NSWindowController

// Opens the package. Returns nil, with the reason in *message, if it cannot be installed.
- (instancetype)initWithPackagePath:(NSString*)path errorMessage:(NSString**)message;

@property (readonly) NSString* packagePath;

@end
