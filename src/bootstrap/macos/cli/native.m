#import <AppKit/AppKit.h>

#include "native.h"

void watch_app_launches(const char* bundle_id, void (*on_launch)(pid_t pid))
{
    @autoreleasepool {
        NSString* wanted = @(bundle_id);
        for (NSRunningApplication* app in [NSRunningApplication runningApplicationsWithBundleIdentifier:wanted]) {
            on_launch(app.processIdentifier);
        }
        [NSWorkspace.sharedWorkspace.notificationCenter addObserverForName:NSWorkspaceDidLaunchApplicationNotification
                                                                    object:nil
                                                                     queue:nil
                                                                usingBlock:^(NSNotification* note) {
                                                                    NSRunningApplication* app = note.userInfo[NSWorkspaceApplicationKey];
                                                                    if ([app.bundleIdentifier isEqualToString:wanted]) on_launch(app.processIdentifier);
                                                                }];
    }
    CFRunLoopRun();
}

static int write_plist(const char* path, NSDictionary* plist)
{
    @autoreleasepool {
        NSError* error = nil;
        NSData* data = [NSPropertyListSerialization dataWithPropertyList:plist format:NSPropertyListXMLFormat_v1_0 options:0 error:&error];
        return data && [data writeToFile:@(path) options:NSDataWritingAtomic error:&error] ? 0 : -1;
    }
}

int write_agent_plist(const char* path, const char* label, const char* program)
{
    return write_plist(path, @{
        @"Label" : @(label),
        @"ProgramArguments" : @[ @(program), @"watch" ],
        @"RunAtLoad" : @YES,
        @"KeepAlive" : @YES,
        @"LimitLoadToSessionType" : @"Aqua",
    });
}

int write_debugger_entitlements(const char* path)
{
    return write_plist(path, @{ @"com.apple.security.cs.debugger" : @YES });
}
