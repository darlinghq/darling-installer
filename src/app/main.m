#import <AppKit/AppKit.h>
#import "InstallerAppDelegate.h"

int main(int argc, const char* argv[])
{
	@autoreleasepool
	{
		NSApplication* app = [NSApplication sharedApplication];
		InstallerAppDelegate* delegate = [[InstallerAppDelegate alloc] init];
		
		[app setDelegate:delegate];
		[app setActivationPolicy:NSApplicationActivationPolicyRegular];
		[app run];
	}
	return 0;
}
