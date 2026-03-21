#if defined(__APPLE__)

#include "platform/MacFullscreenBridge.h"

#include <raylib.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>

static NSWindow* GetMacWindow()
{
    void* handle = GetWindowHandle();
    if (handle == nullptr) {
        TraceLog(LOG_WARNING, "MacFullscreenBridge: GetWindowHandle returned null");
        return nil;
    }

    GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(handle);
    NSWindow* nsWindow = glfwGetCocoaWindow(glfwWindow);

    if (nsWindow == nil) {
        TraceLog(LOG_WARNING, "MacFullscreenBridge: glfwGetCocoaWindow returned nil");
    }

    return nsWindow;
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

    TraceLog(LOG_INFO,
             "MacFullscreenBridge: toggling native fullscreen (active=%s)",
             (window.styleMask & NSWindowStyleMaskFullScreen) ? "true" : "false");

    dispatch_async(dispatch_get_main_queue(), ^{
        [window toggleFullScreen:nil];
    });
}

#endif
