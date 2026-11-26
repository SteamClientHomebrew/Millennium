// Launch notifications from NSWorkspace: the agent sleeps until an app starts.
#import <AppKit/AppKit.h>

#include "watch.h"

void watch_app_launches(const char* bundle_id, void (*on_launch)(pid_t pid))
{
    @autoreleasepool {
        NSString* wanted = @(bundle_id);
        NSWorkspace* workspace = NSWorkspace.sharedWorkspace;
        for (NSRunningApplication* app in [NSRunningApplication runningApplicationsWithBundleIdentifier:wanted]) {
            on_launch(app.processIdentifier);
        }
        [workspace.notificationCenter addObserverForName:NSWorkspaceDidLaunchApplicationNotification
                                                  object:nil
                                                   queue:nil
                                              usingBlock:^(NSNotification* note) {
                                                  NSRunningApplication* app = note.userInfo[NSWorkspaceApplicationKey];
                                                  if ([app.bundleIdentifier isEqualToString:wanted]) on_launch(app.processIdentifier);
                                              }];
    }
    CFRunLoopRun();
}
