#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import "watchdog.h"

@interface SteamWatchDogImpl : NSObject
@property SteamWatchDogCallback onLaunch;
@property SteamWatchDogCallback onQuit;
@end

@implementation SteamWatchDogImpl

- (instancetype)initWithLaunchCallback:(SteamWatchDogCallback)onLaunch onQuit:(SteamWatchDogCallback)onQuit 
{
    self = [super init];
    if (self) 
    {
        _onLaunch = onLaunch;
        _onQuit = onQuit;

        NSNotificationCenter* workspaceCenter = [[NSWorkspace sharedWorkspace] notificationCenter];

        [workspaceCenter addObserverForName:NSWorkspaceDidLaunchApplicationNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) 
        {
                NSRunningApplication *app = (NSRunningApplication *)note.userInfo[NSWorkspaceApplicationKey];
                NSString *name = app.localizedName;
                if ([name isEqualToString:@"Steam"] && _onLaunch) 
                {
                    _onLaunch();
                }
        }];

        [workspaceCenter addObserverForName:NSWorkspaceDidTerminateApplicationNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
                NSRunningApplication *app = (NSRunningApplication *)note.userInfo[NSWorkspaceApplicationKey];
                NSString *name = app.localizedName;
                if ([name isEqualToString:@"Steam"] && _onQuit) 
                {
                    _onQuit();
                }
        }];
    }
    return self;
}
@end

void* SteamWatchDog_Start(SteamWatchDogCallback onLaunch, SteamWatchDogCallback onQuit) 
{
    @autoreleasepool 
    {
        SteamWatchDogImpl* instance = [[SteamWatchDogImpl alloc] initWithLaunchCallback:onLaunch onQuit:onQuit];
        return (__bridge_retained void*)instance;
    }
}

void SteamWatchDog_Stop(void* instance) 
{
        @autoreleasepool 
        {
            SteamWatchDogImpl* obj = (__bridge_transfer SteamWatchDogImpl*)instance;
            obj = nil;
        }
    }
