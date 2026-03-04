#ifdef __APPLE__
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <dispatch/dispatch.h>
#include <cstdlib>

namespace {
constexpr const char* kEnableEnv = "MILLENNIUM_MACOS_NATIVE_CORNERS";
constexpr const char* kRadiusEnv = "MILLENNIUM_MACOS_NATIVE_CORNER_RADIUS";
constexpr CGFloat kDefaultCornerRadius = 10.0;
constexpr CGFloat kMinCornerRadius = 6.0;
constexpr CGFloat kMaxCornerRadius = 20.0;
constexpr int kBootstrapPasses = 24;
constexpr int64_t kBootstrapDelayNs = 250 * NSEC_PER_MSEC;

enum class CornerMode {
    Disabled = 0,
    AutoFrameless = 1,
    ForceAll = 2,
};

NSString* normalized_env_value(const char* raw)
{
    if (!raw || raw[0] == '\0') {
        return nil;
    }

    NSString* value = [NSString stringWithUTF8String:raw];
    return [[value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] lowercaseString];
}

CornerMode resolve_corner_mode()
{
    NSString* value = normalized_env_value(getenv(kEnableEnv));
    if (!value) {
        return CornerMode::AutoFrameless;
    }

    if ([value isEqualToString:@"0"] || [value isEqualToString:@"false"] || [value isEqualToString:@"off"] || [value isEqualToString:@"no"] ||
        [value isEqualToString:@"disabled"]) {
        return CornerMode::Disabled;
    }

    if ([value isEqualToString:@"force"] || [value isEqualToString:@"all"]) {
        return CornerMode::ForceAll;
    }

    // Treat unknown values as "auto" to keep behavior safe and low-intrusion.
    return CornerMode::AutoFrameless;
}

CGFloat configured_corner_radius()
{
    const char* raw = getenv(kRadiusEnv);
    if (!raw || raw[0] == '\0') {
        return kDefaultCornerRadius;
    }

    char* end = nullptr;
    const double parsed = strtod(raw, &end);
    if (!end || end == raw) {
        return kDefaultCornerRadius;
    }

    const CGFloat radius = static_cast<CGFloat>(parsed);
    return MIN(kMaxCornerRadius, MAX(kMinCornerRadius, radius));
}

bool should_style_window(NSWindow* window, CornerMode mode)
{
    if (!window) {
        return false;
    }

    if ([window isKindOfClass:[NSPanel class]]) {
        return false;
    }

    if (window.level != NSNormalWindowLevel) {
        return false;
    }

    if (mode == CornerMode::ForceAll) {
        return true;
    }

    // For AppKit-native titled windows, let the system own corner treatment.
    // Auto mode styles only frameless/custom-chrome windows.
    const NSWindowStyleMask style_mask = window.styleMask;
    const bool is_borderless = (style_mask & NSWindowStyleMaskBorderless) == NSWindowStyleMaskBorderless;
    const bool has_titled_chrome = (style_mask & NSWindowStyleMaskTitled) == NSWindowStyleMaskTitled;

    return is_borderless || !has_titled_chrome;
}

bool is_corner_styling_enabled(CornerMode mode)
{
    return mode != CornerMode::Disabled;
}

void clear_corner_style_if_needed(NSWindow* window)
{
    if (!window) {
        return;
    }

    NSView* frame_view = window.contentView.superview ?: window.contentView;
    if (!frame_view || !frame_view.layer) {
        return;
    }

    frame_view.layer.cornerRadius = 0.0;
    frame_view.layer.masksToBounds = NO;
}
} // namespace

@interface MillenniumMacWindowStyleManager : NSObject
+ (instancetype)shared;
- (void)installIfEnabled;
@end

@implementation MillenniumMacWindowStyleManager {
    CornerMode _mode;
    CGFloat _cornerRadius;
}

+ (instancetype)shared
{
    static MillenniumMacWindowStyleManager* manager = nil;
    static dispatch_once_t once_token;
    dispatch_once(&once_token, ^{
        manager = [MillenniumMacWindowStyleManager new];
    });
    return manager;
}

- (instancetype)init
{
    self = [super init];
    if (!self) {
        return nil;
    }

    _mode = resolve_corner_mode();
    _cornerRadius = configured_corner_radius();
    return self;
}

- (void)installIfEnabled
{
    if (!is_corner_styling_enabled(_mode)) {
        return;
    }

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(onWindowEvent:) name:NSWindowDidBecomeMainNotification object:nil];
    [center addObserver:self selector:@selector(onWindowEvent:) name:NSWindowDidResizeNotification object:nil];
    [center addObserver:self selector:@selector(onWindowEvent:) name:NSWindowDidMiniaturizeNotification object:nil];
    [center addObserver:self selector:@selector(onWindowEvent:) name:NSWindowDidDeminiaturizeNotification object:nil];
    [center addObserver:self selector:@selector(onWindowEvent:) name:NSWindowDidEnterFullScreenNotification object:nil];
    [center addObserver:self selector:@selector(onWindowEvent:) name:NSWindowDidExitFullScreenNotification object:nil];
    [center addObserver:self selector:@selector(onWindowEvent:) name:NSWindowDidChangeOcclusionStateNotification object:nil];

    [self applyToAllWindows];
    [self scheduleBootstrapPasses:kBootstrapPasses];
}

- (void)scheduleBootstrapPasses:(int)remaining
{
    if (remaining <= 0) {
        return;
    }

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, kBootstrapDelayNs), dispatch_get_main_queue(), ^{
        [self applyToAllWindows];
        [self scheduleBootstrapPasses:remaining - 1];
    });
}

- (void)onWindowEvent:(NSNotification*)notification
{
    NSWindow* window = notification.object;
    if (!window) {
        [self applyToAllWindows];
        return;
    }

    [self applyStyleToWindow:window];
}

- (void)applyToAllWindows
{
    for (NSWindow* window in [NSApp windows]) {
        [self applyStyleToWindow:window];
    }
}

- (void)applyStyleToWindow:(NSWindow*)window
{
    if (!is_corner_styling_enabled(_mode)) {
        return;
    }

    if (!should_style_window(window, _mode)) {
        clear_corner_style_if_needed(window);
        return;
    }

    if ((window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen) {
        NSView* frameView = window.contentView.superview ?: window.contentView;
        if (frameView && frameView.layer) {
            frameView.layer.cornerRadius = 0.0;
            frameView.layer.masksToBounds = NO;
        }
        return;
    }

    NSView* frameView = window.contentView.superview ?: window.contentView;
    if (!frameView) {
        return;
    }

    [frameView setWantsLayer:YES];
    CALayer* layer = frameView.layer;
    if (!layer) {
        return;
    }

    layer.masksToBounds = YES;
    layer.cornerRadius = _cornerRadius;
    if (@available(macOS 10.15, *)) {
        layer.cornerCurve = @"continuous";
    }
}

@end

extern "C" void Plat_InstallMacOSNativeWindowStyling()
{
    static dispatch_once_t once_token;
    dispatch_once(&once_token, ^{
        dispatch_async(dispatch_get_main_queue(), ^{
            [[MillenniumMacWindowStyleManager shared] installIfEnabled];
        });
    });
}

__attribute__((constructor)) static void MillenniumMacWindowStyleConstructor()
{
    Plat_InstallMacOSNativeWindowStyling();
}
#endif
