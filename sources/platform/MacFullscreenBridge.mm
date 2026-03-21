#if defined(__APPLE__)

#include "platform/MacFullscreenBridge.h"

#include <raylib.h>
#import <Cocoa/Cocoa.h>
#import <dispatch/dispatch.h>

static NSWindow* GetMacWindow()
{
    void* handle = GetWindowHandle();
    if (handle == nullptr) {
        TraceLog(LOG_WARNING, "MacFullscreenBridge: GetWindowHandle returned null");
        return nil;
    }

    id cocoaObject = (__bridge id)handle;
    if (cocoaObject == nil) {
        TraceLog(LOG_WARNING, "MacFullscreenBridge: bridged Cocoa object was nil");
        return nil;
    }

    if ([cocoaObject isKindOfClass:[NSWindow class]]) {
        return (NSWindow*)cocoaObject;
    }

    TraceLog(LOG_WARNING,
             "MacFullscreenBridge: window handle is not NSWindow, class=%s",
             class_getName([cocoaObject class]));

    return nil;
}

bool IsMacNativeFullscreenSupported()
{
    return GetMacWindow() != nil;
}

bool IsMacNativeFullscreenActive()
{
    NSWindow* window = GetMacWindow();
    if (window == nil) {
        return false;
    }

    return (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
}

void ToggleMacNativeFullscreen()
{
    NSWindow* window = GetMacWindow();
    if (window == nil) {
        TraceLog(LOG_WARNING, "MacFullscreenBridge: no NSWindow available for fullscreen toggle");
        return;
    }

    const bool active = (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
    TraceLog(LOG_INFO,
             "MacFullscreenBridge: toggling native fullscreen (active=%s)",
             active ? "true" : "false");

    dispatch_async(dispatch_get_main_queue(), ^{
        [window toggleFullScreen:nil];
    });
}

#endif