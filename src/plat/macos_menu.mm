#ifdef __APPLE__
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

static const NSInteger kMillenniumMenuItemTag = 0x4D4C4E4D; // 'MLNM'
static const int kMaxMenuInstallAttempts = 90;
static const int64_t kMenuInstallRetryDelayNs = 500 * NSEC_PER_MSEC;

#ifndef MILLENNIUM_MENU_ICON_PATH
#define MILLENNIUM_MENU_ICON_PATH ""
#endif

@interface MillenniumMenuHandler : NSObject
+ (instancetype)shared;
- (void)openMillenniumSettings:(id)sender;
@end

@implementation MillenniumMenuHandler
+ (instancetype)shared
{
    static MillenniumMenuHandler* handler = nil;
    static dispatch_once_t once_token;
    dispatch_once(&once_token, ^{
        handler = [MillenniumMenuHandler new];
    });
    return handler;
}

- (void)openMillenniumSettings:(id)sender
{
    (void)sender;
    NSURL* url = [NSURL URLWithString:@"steam://millennium/settings"];
    if (!url) {
        return;
    }

    [[NSWorkspace sharedWorkspace] openURL:url];
}
@end

static NSImage* LoadMillenniumMenuIcon()
{
    static NSImage* cached_icon = nil;
    static dispatch_once_t once_token;
    dispatch_once(&once_token, ^{
        const char* icon_path = MILLENNIUM_MENU_ICON_PATH;
        if (!icon_path || icon_path[0] == '\0') {
            return;
        }

        NSString* ns_icon_path = [NSString stringWithUTF8String:icon_path];
        if (!ns_icon_path || ![[NSFileManager defaultManager] fileExistsAtPath:ns_icon_path]) {
            return;
        }

        NSImage* icon = [[NSImage alloc] initWithContentsOfFile:ns_icon_path];
        if (!icon) {
            return;
        }

        [icon setSize:NSMakeSize(16, 16)];
        cached_icon = icon;
    });

    return cached_icon;
}

static bool TryInstallMillenniumMenuItem()
{
    NSMenu* main_menu = [NSApp mainMenu];
    if (!main_menu || [main_menu numberOfItems] <= 0) {
        return false;
    }

    NSMenuItem* app_menu_item = [main_menu itemAtIndex:0];
    NSMenu* app_menu = [app_menu_item submenu];
    if (!app_menu) {
        return false;
    }

    if ([app_menu itemWithTag:kMillenniumMenuItemTag]) {
        return true;
    }

    NSInteger insert_index = [app_menu numberOfItems];
    for (NSInteger index = 0; index < [app_menu numberOfItems]; ++index) {
        NSMenuItem* candidate = [app_menu itemAtIndex:index];
        if ([candidate isSeparatorItem]) {
            insert_index = index;
            break;
        }
    }

    NSMenuItem* millennium_item = [[NSMenuItem alloc] initWithTitle:@"Millennium"
                                                             action:@selector(openMillenniumSettings:)
                                                      keyEquivalent:@""];
    NSImage* millennium_icon = LoadMillenniumMenuIcon();
    if (millennium_icon) {
        [millennium_item setImage:millennium_icon];
    }
    [millennium_item setTarget:[MillenniumMenuHandler shared]];
    [millennium_item setTag:kMillenniumMenuItemTag];
    [app_menu insertItem:millennium_item atIndex:insert_index];
    return true;
}

static void ScheduleMenuInstallAttempt(int attempts_remaining)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (TryInstallMillenniumMenuItem() || attempts_remaining <= 0) {
            return;
        }

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kMenuInstallRetryDelayNs), dispatch_get_main_queue(), ^{
            ScheduleMenuInstallAttempt(attempts_remaining - 1);
        });
    });
}

extern "C" void Plat_InstallMacOSMenuItems()
{
    static dispatch_once_t once_token;
    dispatch_once(&once_token, ^{
        ScheduleMenuInstallAttempt(kMaxMenuInstallAttempts);
    });
}
#endif
