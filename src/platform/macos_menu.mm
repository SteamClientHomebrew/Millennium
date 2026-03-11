/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

static const NSInteger kMillenniumMenuItemTag = 0x4D4C4E4D; // 'MLNM'
static const int kMaxMenuInstallAttempts = 90;
static const int64_t kMenuInstallRetryDelayNs = 500 * NSEC_PER_MSEC;

@interface MillenniumMenuHandler : NSObject
+ (instancetype)shared;
- (void)openMillenniumURL:(id)sender;
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

- (void)openMillenniumURL:(id)sender
{
    NSString* urlString = [(NSMenuItem*)sender representedObject];
    NSURL* url = [NSURL URLWithString:urlString];
    if (!url) {
        return;
    }

    [[NSWorkspace sharedWorkspace] openURL:url];
}
@end

static NSMenuItem* MakeMillenniumItem(NSString* title, NSString* urlString)
{
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:@selector(openMillenniumURL:) keyEquivalent:@""];
    [item setTarget:[MillenniumMenuHandler shared]];
    [item setRepresentedObject:urlString];
    return item;
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

    // Steam builds the menu incrementally. Wait until it's fully populated
    // (the standard Steam app menu has ~11 items) before inserting, otherwise
    // Steam will add items above ours and push them to the wrong position.
    if ([app_menu numberOfItems] < 9) {
        return false;
    }

    NSInteger insert_index = MIN(3, [app_menu numberOfItems]);

    NSMenuItem* millennium_item = MakeMillenniumItem(@"Millennium", @"steam://millennium/settings");
    [millennium_item setTag:kMillenniumMenuItemTag];
    [app_menu insertItem:millennium_item atIndex:insert_index];

    NSMenuItem* library_manager_item = MakeMillenniumItem(@"Millennium Library Manager", @"steam://millennium/sidebar");
    [app_menu insertItem:library_manager_item atIndex:insert_index + 1];
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
