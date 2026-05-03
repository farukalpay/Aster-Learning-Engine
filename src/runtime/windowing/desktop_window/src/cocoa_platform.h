#include <stdint.h>

#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDLib.h>

// NOTE: All of NSGL was deprecated in the 10.14 SDK
//       This disables the pointless warnings for every symbol we use
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif

#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#else
typedef void* id;
#endif

// NOTE: Many Cocoa enum values have been renamed and we need to build across
//       SDK versions where one is unavailable or deprecated.
//       We use the newer names in code and replace them with the older names if
//       the base SDK does not provide the newer names.

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101400
 #define NSOpenGLContextParameterSwapInterval NSOpenGLCPSwapInterval
 #define NSOpenGLContextParameterSurfaceOpacity NSOpenGLCPSurfaceOpacity
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED < 101200
 #define NSBitmapFormatAlphaNonpremultiplied NSAlphaNonpremultipliedBitmapFormat
 #define NSEventMaskAny NSAnyEventMask
 #define NSEventMaskKeyUp NSKeyUpMask
 #define NSEventModifierFlagCapsLock NSAlphaShiftKeyMask
 #define NSEventModifierFlagCommand NSCommandKeyMask
 #define NSEventModifierFlagControl NSControlKeyMask
 #define NSEventModifierFlagDeviceIndependentFlagsMask NSDeviceIndependentModifierFlagsMask
 #define NSEventModifierFlagOption NSAlternateKeyMask
 #define NSEventModifierFlagShift NSShiftKeyMask
 #define NSEventTypeApplicationDefined NSApplicationDefined
 #define NSWindowStyleMaskBorderless NSBorderlessWindowMask
 #define NSWindowStyleMaskClosable NSClosableWindowMask
 #define NSWindowStyleMaskMiniaturizable NSMiniaturizableWindowMask
 #define NSWindowStyleMaskResizable NSResizableWindowMask
 #define NSWindowStyleMaskTitled NSTitledWindowMask
#endif

// NOTE: Many Cocoa dynamically linked constants have been renamed and we need
//       to build across SDK versions where one is unavailable or deprecated.
//       We use the newer names in code and replace them with the older names if
//       the deployment target is older than the newer names.

#if MAC_OS_X_VERSION_MIN_REQUIRED < 101300
 #define NSPasteboardTypeURL NSURLPboardType
#endif

typedef VkFlags VkMacOSSurfaceCreateFlagsMVK;
typedef VkFlags VkMetalSurfaceCreateFlagsEXT;

typedef struct VkMacOSSurfaceCreateInfoMVK
{
    VkStructureType                 sType;
    const void*                     pNext;
    VkMacOSSurfaceCreateFlagsMVK    flags;
    const void*                     pView;
} VkMacOSSurfaceCreateInfoMVK;

typedef struct VkMetalSurfaceCreateInfoEXT
{
    VkStructureType                 sType;
    const void*                     pNext;
    VkMetalSurfaceCreateFlagsEXT    flags;
    const void*                     pLayer;
} VkMetalSurfaceCreateInfoEXT;

typedef VkResult (APIENTRY *PFN_vkCreateMacOSSurfaceMVK)(VkInstance,const VkMacOSSurfaceCreateInfoMVK*,const VkAllocationCallbacks*,VkSurfaceKHR*);
typedef VkResult (APIENTRY *PFN_vkCreateMetalSurfaceEXT)(VkInstance,const VkMetalSurfaceCreateInfoEXT*,const VkAllocationCallbacks*,VkSurfaceKHR*);

#define DESKTOP_WINDOW_COCOA_WINDOW_STATE         _DESKTOP_WINDOWwindowNS  ns;
#define DESKTOP_WINDOW_COCOA_LIBRARY_WINDOW_STATE _DESKTOP_WINDOWlibraryNS ns;
#define DESKTOP_WINDOW_COCOA_MONITOR_STATE        _DESKTOP_WINDOWmonitorNS ns;
#define DESKTOP_WINDOW_COCOA_CURSOR_STATE         _DESKTOP_WINDOWcursorNS  ns;

#define DESKTOP_WINDOW_NSGL_CONTEXT_STATE         _DESKTOP_WINDOWcontextNSGL nsgl;
#define DESKTOP_WINDOW_NSGL_LIBRARY_CONTEXT_STATE _DESKTOP_WINDOWlibraryNSGL nsgl;

// HIToolbox.framework pointer typedefs
#define kTISPropertyUnicodeKeyLayoutData _desktop_window.ns.tis.kPropertyUnicodeKeyLayoutData
typedef TISInputSourceRef (*PFN_TISCopyCurrentKeyboardLayoutInputSource)(void);
#define TISCopyCurrentKeyboardLayoutInputSource _desktop_window.ns.tis.CopyCurrentKeyboardLayoutInputSource
typedef void* (*PFN_TISGetInputSourceProperty)(TISInputSourceRef,CFStringRef);
#define TISGetInputSourceProperty _desktop_window.ns.tis.GetInputSourceProperty
typedef UInt8 (*PFN_LMGetKbdType)(void);
#define LMGetKbdType _desktop_window.ns.tis.GetKbdType


// NSGL-specific per-context data
//
typedef struct _DESKTOP_WINDOWcontextNSGL
{
    id                pixelFormat;
    id                object;
} _DESKTOP_WINDOWcontextNSGL;

// NSGL-specific global data
//
typedef struct _DESKTOP_WINDOWlibraryNSGL
{
    // dlopen handle for OpenGL.framework (for desktop_windowGetProcAddress)
    CFBundleRef     framework;
} _DESKTOP_WINDOWlibraryNSGL;

// Cocoa-specific per-window data
//
typedef struct _DESKTOP_WINDOWwindowNS
{
    id              object;
    id              delegate;
    id              view;
    id              layer;

    DESKTOP_WINDOWbool        maximized;
    DESKTOP_WINDOWbool        occluded;
    DESKTOP_WINDOWbool        scaleFramebuffer;

    // Cached window properties to filter out duplicate events
    int             width, height;
    int             fbWidth, fbHeight;
    float           xscale, yscale;

    // The total sum of the distances the cursor has been warped
    // since the last cursor motion event was processed
    // This is kept to counteract Cocoa doing the same internally
    double          cursorWarpDeltaX, cursorWarpDeltaY;
} _DESKTOP_WINDOWwindowNS;

// Cocoa-specific global data
//
typedef struct _DESKTOP_WINDOWlibraryNS
{
    CGEventSourceRef    eventSource;
    id                  delegate;
    DESKTOP_WINDOWbool            cursorHidden;
    TISInputSourceRef   inputSource;
    IOHIDManagerRef     hidManager;
    id                  unicodeData;
    id                  helper;
    id                  keyUpMonitor;
    id                  nibObjects;

    char                keynames[DESKTOP_WINDOW_KEY_LAST + 1][17];
    short int           keycodes[256];
    short int           scancodes[DESKTOP_WINDOW_KEY_LAST + 1];
    char*               clipboardString;
    CGPoint             cascadePoint;
    // Where to place the cursor when re-enabled
    double              restoreCursorPosX, restoreCursorPosY;
    // The window whose disabled cursor mode is active
    _DESKTOP_WINDOWwindow*        disabledCursorWindow;

    struct {
        CFBundleRef     bundle;
        PFN_TISCopyCurrentKeyboardLayoutInputSource CopyCurrentKeyboardLayoutInputSource;
        PFN_TISGetInputSourceProperty GetInputSourceProperty;
        PFN_LMGetKbdType GetKbdType;
        CFStringRef     kPropertyUnicodeKeyLayoutData;
    } tis;
} _DESKTOP_WINDOWlibraryNS;

// Cocoa-specific per-monitor data
//
typedef struct _DESKTOP_WINDOWmonitorNS
{
    CGDirectDisplayID   displayID;
    CGDisplayModeRef    previousMode;
    uint32_t            unitNumber;
    id                  screen;
    double              fallbackRefreshRate;
} _DESKTOP_WINDOWmonitorNS;

// Cocoa-specific per-cursor data
//
typedef struct _DESKTOP_WINDOWcursorNS
{
    id              object;
} _DESKTOP_WINDOWcursorNS;


DESKTOP_WINDOWbool _desktop_windowConnectCocoa(int platformID, _DESKTOP_WINDOWplatform* platform);
int _desktop_windowInitCocoa(void);
void _desktop_windowTerminateCocoa(void);

DESKTOP_WINDOWbool _desktop_windowCreateWindowCocoa(_DESKTOP_WINDOWwindow* window, const _DESKTOP_WINDOWwndconfig* wndconfig, const _DESKTOP_WINDOWctxconfig* ctxconfig, const _DESKTOP_WINDOWfbconfig* fbconfig);
void _desktop_windowDestroyWindowCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowSetWindowTitleCocoa(_DESKTOP_WINDOWwindow* window, const char* title);
void _desktop_windowSetWindowIconCocoa(_DESKTOP_WINDOWwindow* window, int count, const DESKTOP_WINDOWimage* images);
void _desktop_windowGetWindowPosCocoa(_DESKTOP_WINDOWwindow* window, int* xpos, int* ypos);
void _desktop_windowSetWindowPosCocoa(_DESKTOP_WINDOWwindow* window, int xpos, int ypos);
void _desktop_windowGetWindowSizeCocoa(_DESKTOP_WINDOWwindow* window, int* width, int* height);
void _desktop_windowSetWindowSizeCocoa(_DESKTOP_WINDOWwindow* window, int width, int height);
void _desktop_windowSetWindowSizeLimitsCocoa(_DESKTOP_WINDOWwindow* window, int minwidth, int minheight, int maxwidth, int maxheight);
void _desktop_windowSetWindowAspectRatioCocoa(_DESKTOP_WINDOWwindow* window, int numer, int denom);
void _desktop_windowGetFramebufferSizeCocoa(_DESKTOP_WINDOWwindow* window, int* width, int* height);
void _desktop_windowGetWindowFrameSizeCocoa(_DESKTOP_WINDOWwindow* window, int* left, int* top, int* right, int* bottom);
void _desktop_windowGetWindowContentScaleCocoa(_DESKTOP_WINDOWwindow* window, float* xscale, float* yscale);
void _desktop_windowIconifyWindowCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowRestoreWindowCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowMaximizeWindowCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowShowWindowCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowHideWindowCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowRequestWindowAttentionCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowFocusWindowCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowSetWindowMonitorCocoa(_DESKTOP_WINDOWwindow* window, _DESKTOP_WINDOWmonitor* monitor, int xpos, int ypos, int width, int height, int refreshRate);
DESKTOP_WINDOWbool _desktop_windowWindowFocusedCocoa(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowWindowIconifiedCocoa(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowWindowVisibleCocoa(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowWindowMaximizedCocoa(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowWindowHoveredCocoa(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowFramebufferTransparentCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowSetWindowResizableCocoa(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled);
void _desktop_windowSetWindowDecoratedCocoa(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled);
void _desktop_windowSetWindowFloatingCocoa(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled);
float _desktop_windowGetWindowOpacityCocoa(_DESKTOP_WINDOWwindow* window);
void _desktop_windowSetWindowOpacityCocoa(_DESKTOP_WINDOWwindow* window, float opacity);
void _desktop_windowSetWindowMousePassthroughCocoa(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled);

void _desktop_windowSetRawMouseMotionCocoa(_DESKTOP_WINDOWwindow *window, DESKTOP_WINDOWbool enabled);
DESKTOP_WINDOWbool _desktop_windowRawMouseMotionSupportedCocoa(void);

void _desktop_windowPollEventsCocoa(void);
void _desktop_windowWaitEventsCocoa(void);
void _desktop_windowWaitEventsTimeoutCocoa(double timeout);
void _desktop_windowPostEmptyEventCocoa(void);

void _desktop_windowGetCursorPosCocoa(_DESKTOP_WINDOWwindow* window, double* xpos, double* ypos);
void _desktop_windowSetCursorPosCocoa(_DESKTOP_WINDOWwindow* window, double xpos, double ypos);
void _desktop_windowSetCursorModeCocoa(_DESKTOP_WINDOWwindow* window, int mode);
const char* _desktop_windowGetScancodeNameCocoa(int scancode);
int _desktop_windowGetKeyScancodeCocoa(int key);
DESKTOP_WINDOWbool _desktop_windowCreateCursorCocoa(_DESKTOP_WINDOWcursor* cursor, const DESKTOP_WINDOWimage* image, int xhot, int yhot);
DESKTOP_WINDOWbool _desktop_windowCreateStandardCursorCocoa(_DESKTOP_WINDOWcursor* cursor, int shape);
void _desktop_windowDestroyCursorCocoa(_DESKTOP_WINDOWcursor* cursor);
void _desktop_windowSetCursorCocoa(_DESKTOP_WINDOWwindow* window, _DESKTOP_WINDOWcursor* cursor);
void _desktop_windowSetClipboardStringCocoa(const char* string);
const char* _desktop_windowGetClipboardStringCocoa(void);

EGLenum _desktop_windowGetEGLPlatformCocoa(EGLint** attribs);
EGLNativeDisplayType _desktop_windowGetEGLNativeDisplayCocoa(void);
EGLNativeWindowType _desktop_windowGetEGLNativeWindowCocoa(_DESKTOP_WINDOWwindow* window);

void _desktop_windowGetRequiredInstanceExtensionsCocoa(char** extensions);
DESKTOP_WINDOWbool _desktop_windowGetPhysicalDevicePresentationSupportCocoa(VkInstance instance, VkPhysicalDevice device, uint32_t queuefamily);
VkResult _desktop_windowCreateWindowSurfaceCocoa(VkInstance instance, _DESKTOP_WINDOWwindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);

void _desktop_windowFreeMonitorCocoa(_DESKTOP_WINDOWmonitor* monitor);
void _desktop_windowGetMonitorPosCocoa(_DESKTOP_WINDOWmonitor* monitor, int* xpos, int* ypos);
void _desktop_windowGetMonitorContentScaleCocoa(_DESKTOP_WINDOWmonitor* monitor, float* xscale, float* yscale);
void _desktop_windowGetMonitorWorkareaCocoa(_DESKTOP_WINDOWmonitor* monitor, int* xpos, int* ypos, int* width, int* height);
DESKTOP_WINDOWvidmode* _desktop_windowGetVideoModesCocoa(_DESKTOP_WINDOWmonitor* monitor, int* count);
DESKTOP_WINDOWbool _desktop_windowGetVideoModeCocoa(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWvidmode* mode);
DESKTOP_WINDOWbool _desktop_windowGetGammaRampCocoa(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWgammaramp* ramp);
void _desktop_windowSetGammaRampCocoa(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWgammaramp* ramp);

void _desktop_windowPollMonitorsCocoa(void);
void _desktop_windowSetVideoModeCocoa(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWvidmode* desired);
void _desktop_windowRestoreVideoModeCocoa(_DESKTOP_WINDOWmonitor* monitor);

float _desktop_windowTransformYCocoa(float y);

void* _desktop_windowLoadLocalVulkanLoaderCocoa(void);

DESKTOP_WINDOWbool _desktop_windowInitNSGL(void);
void _desktop_windowTerminateNSGL(void);
DESKTOP_WINDOWbool _desktop_windowCreateContextNSGL(_DESKTOP_WINDOWwindow* window,
                                const _DESKTOP_WINDOWctxconfig* ctxconfig,
                                const _DESKTOP_WINDOWfbconfig* fbconfig);
void _desktop_windowDestroyContextNSGL(_DESKTOP_WINDOWwindow* window);

