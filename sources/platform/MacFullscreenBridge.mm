#if defined(__APPLE__)

#include "platform/MacFullscreenBridge.h"

#include <raylib.h>
#import <Cocoa/Cocoa.h>

bool IsMacNativeFullscreenSupported()
{
    return GetWindowHandle() != nullptr;
}

bool IsMacNativeFullscreenActive()
{
    void* handle = GetWindowHandle();
    if (handle == nullptr) {
        return false;
    }

    NSWindow* window = (__bridge NSWindow*)handle;
    if (window == nil) {
        return false;
    }

    return (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
}

void ToggleMacNativeFullscreen()
{
    void* handle = GetWindowHandle();
    if (handle == nullptr) {
        return;
    }

    NSWindow* window = (__bridge NSWindow*)handle;
    if (window == nil) {
        return;
    }

    [window toggleFullScreen:nil];
}

#endif