#ifndef NOMINMAX
 #define NOMINMAX
#endif

#ifndef VC_EXTRALEAN
 #define VC_EXTRALEAN
#endif

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif

// This is a workaround for the fact that desktop_window3.h needs to export APIENTRY (for
// example to allow applications to correctly declare a GL_KHR_debug callback)
// but windows.h assumes no one will define APIENTRY before it does
#undef APIENTRY

// DESKTOP_WINDOW on Windows is Unicode only and does not work in MBCS mode
#ifndef UNICODE
 #define UNICODE
#endif

// DESKTOP_WINDOW requires Windows XP or later
#if WINVER < 0x0501
 #undef WINVER
 #define WINVER 0x0501
#endif
#if _WIN32_WINNT < 0x0501
 #undef _WIN32_WINNT
 #define _WIN32_WINNT 0x0501
#endif

// DESKTOP_WINDOW uses DirectInput8 interfaces
#define DIRECTINPUT_VERSION 0x0800

// DESKTOP_WINDOW uses OEM cursor resources
#define OEMRESOURCE

#include <wctype.h>
#include <windows.h>
#include <dinput.h>
#include <xinput.h>
#include <dbt.h>

// HACK: Define macros that some windows.h variants don't
#ifndef WM_MOUSEHWHEEL
 #define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef WM_DWMCOMPOSITIONCHANGED
 #define WM_DWMCOMPOSITIONCHANGED 0x031E
#endif
#ifndef WM_DWMCOLORIZATIONCOLORCHANGED
 #define WM_DWMCOLORIZATIONCOLORCHANGED 0x0320
#endif
#ifndef WM_COPYGLOBALDATA
 #define WM_COPYGLOBALDATA 0x0049
#endif
#ifndef WM_UNICHAR
 #define WM_UNICHAR 0x0109
#endif
#ifndef UNICODE_NOCHAR
 #define UNICODE_NOCHAR 0xFFFF
#endif
#ifndef WM_DPICHANGED
 #define WM_DPICHANGED 0x02E0
#endif
#ifndef GET_XBUTTON_WPARAM
 #define GET_XBUTTON_WPARAM(w) (HIWORD(w))
#endif
#ifndef EDS_ROTATEDMODE
 #define EDS_ROTATEDMODE 0x00000004
#endif
#ifndef DISPLAY_DEVICE_ACTIVE
 #define DISPLAY_DEVICE_ACTIVE 0x00000001
#endif
#ifndef _WIN32_WINNT_WINBLUE
 #define _WIN32_WINNT_WINBLUE 0x0603
#endif
#ifndef _WIN32_WINNT_WIN8
 #define _WIN32_WINNT_WIN8 0x0602
#endif
#ifndef WM_GETDPISCALEDSIZE
 #define WM_GETDPISCALEDSIZE 0x02e4
#endif
#ifndef USER_DEFAULT_SCREEN_DPI
 #define USER_DEFAULT_SCREEN_DPI 96
#endif
#ifndef OCR_HAND
 #define OCR_HAND 32649
#endif

#if WINVER < 0x0601
typedef struct
{
    DWORD cbSize;
    DWORD ExtStatus;
} CHANGEFILTERSTRUCT;
#ifndef MSGFLT_ALLOW
 #define MSGFLT_ALLOW 1
#endif
#endif /*Windows 7*/

#if WINVER < 0x0600
#define DWM_BB_ENABLE 0x00000001
#define DWM_BB_BLURREGION 0x00000002
typedef struct
{
    DWORD dwFlags;
    BOOL fEnable;
    HRGN hRgnBlur;
    BOOL fTransitionOnMaximized;
} DWM_BLURBEHIND;
#else
 #include <dwmapi.h>
#endif /*Windows Vista*/

#ifndef DPI_ENUMS_DECLARED
typedef enum
{
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
typedef enum
{
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;
#endif /*DPI_ENUMS_DECLARED*/

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE) -4)
#endif /*DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2*/

// Replacement for versionhelpers.h macros, as we cannot rely on the
// application having a correct embedded manifest
//
#define IsWindowsVistaOrGreater()                                     \
    _desktop_windowIsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_VISTA),   \
                                        LOBYTE(_WIN32_WINNT_VISTA), 0)
#define IsWindows7OrGreater()                                         \
    _desktop_windowIsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_WIN7),    \
                                        LOBYTE(_WIN32_WINNT_WIN7), 0)
#define IsWindows8OrGreater()                                         \
    _desktop_windowIsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_WIN8),    \
                                        LOBYTE(_WIN32_WINNT_WIN8), 0)
#define IsWindows8Point1OrGreater()                                   \
    _desktop_windowIsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_WINBLUE), \
                                        LOBYTE(_WIN32_WINNT_WINBLUE), 0)

// Windows 10 Anniversary Update
#define _desktop_windowIsWindows10Version1607OrGreaterWin32() \
    _desktop_windowIsWindows10BuildOrGreaterWin32(14393)
// Windows 10 Creators Update
#define _desktop_windowIsWindows10Version1703OrGreaterWin32() \
    _desktop_windowIsWindows10BuildOrGreaterWin32(15063)

// HACK: Define macros that some xinput.h variants don't
#ifndef XINPUT_CAPS_WIRELESS
 #define XINPUT_CAPS_WIRELESS 0x0002
#endif
#ifndef XINPUT_DEVSUBTYPE_WHEEL
 #define XINPUT_DEVSUBTYPE_WHEEL 0x02
#endif
#ifndef XINPUT_DEVSUBTYPE_ARCADE_STICK
 #define XINPUT_DEVSUBTYPE_ARCADE_STICK 0x03
#endif
#ifndef XINPUT_DEVSUBTYPE_FLIGHT_STICK
 #define XINPUT_DEVSUBTYPE_FLIGHT_STICK 0x04
#endif
#ifndef XINPUT_DEVSUBTYPE_DANCE_PAD
 #define XINPUT_DEVSUBTYPE_DANCE_PAD 0x05
#endif
#ifndef XINPUT_DEVSUBTYPE_GUITAR
 #define XINPUT_DEVSUBTYPE_GUITAR 0x06
#endif
#ifndef XINPUT_DEVSUBTYPE_DRUM_KIT
 #define XINPUT_DEVSUBTYPE_DRUM_KIT 0x08
#endif
#ifndef XINPUT_DEVSUBTYPE_ARCADE_PAD
 #define XINPUT_DEVSUBTYPE_ARCADE_PAD 0x13
#endif
#ifndef XUSER_MAX_COUNT
 #define XUSER_MAX_COUNT 4
#endif

// HACK: Define macros that some dinput.h variants don't
#ifndef DIDFT_OPTIONAL
 #define DIDFT_OPTIONAL 0x80000000
#endif

#define WGL_NUMBER_PIXEL_FORMATS_ARB 0x2000
#define WGL_SUPPORT_DESKTOP_GRAPHICS_ARB 0x2010
#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_PIXEL_TYPE_ARB 0x2013
#define WGL_TYPE_RGBA_ARB 0x202b
#define WGL_ACCELERATION_ARB 0x2003
#define WGL_NO_ACCELERATION_ARB 0x2025
#define WGL_RED_BITS_ARB 0x2015
#define WGL_RED_SHIFT_ARB 0x2016
#define WGL_GREEN_BITS_ARB 0x2017
#define WGL_GREEN_SHIFT_ARB 0x2018
#define WGL_BLUE_BITS_ARB 0x2019
#define WGL_BLUE_SHIFT_ARB 0x201a
#define WGL_ALPHA_BITS_ARB 0x201b
#define WGL_ALPHA_SHIFT_ARB 0x201c
#define WGL_ACCUM_BITS_ARB 0x201d
#define WGL_ACCUM_RED_BITS_ARB 0x201e
#define WGL_ACCUM_GREEN_BITS_ARB 0x201f
#define WGL_ACCUM_BLUE_BITS_ARB 0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB 0x2021
#define WGL_DEPTH_BITS_ARB 0x2022
#define WGL_STENCIL_BITS_ARB 0x2023
#define WGL_AUX_BUFFERS_ARB 0x2024
#define WGL_STEREO_ARB 0x2012
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_SAMPLES_ARB 0x2042
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB 0x20a9
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x00000001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_ES2_PROFILE_BIT_EXT 0x00000004
#define WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB 0x00000004
#define WGL_LOSE_CONTEXT_ON_RESET_ARB 0x8252
#define WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB 0x8256
#define WGL_NO_RESET_NOTIFICATION_ARB 0x8261
#define WGL_CONTEXT_RELEASE_BEHAVIOR_ARB 0x2097
#define WGL_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB 0
#define WGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB 0x2098
#define WGL_CONTEXT_DESKTOP_GRAPHICS_NO_ERROR_ARB 0x31b3
#define WGL_COLORSPACE_EXT 0x309d
#define WGL_COLORSPACE_SRGB_EXT 0x3089

#define ERROR_INVALID_VERSION_ARB 0x2095
#define ERROR_INVALID_PROFILE_ARB 0x2096
#define ERROR_INCOMPATIBLE_DEVICE_CONTEXTS_ARB 0x2054

// xinput.dll function pointer typedefs
typedef DWORD (WINAPI * PFN_XInputGetCapabilities)(DWORD,DWORD,XINPUT_CAPABILITIES*);
typedef DWORD (WINAPI * PFN_XInputGetState)(DWORD,XINPUT_STATE*);
#define XInputGetCapabilities _desktop_window.win32.xinput.GetCapabilities
#define XInputGetState _desktop_window.win32.xinput.GetState

// dinput8.dll function pointer typedefs
typedef HRESULT (WINAPI * PFN_DirectInput8Create)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
#define DirectInput8Create _desktop_window.win32.dinput8.Create

// user32.dll function pointer typedefs
typedef BOOL (WINAPI * PFN_SetProcessDPIAware)(void);
typedef BOOL (WINAPI * PFN_ChangeWindowMessageFilterEx)(HWND,UINT,DWORD,CHANGEFILTERSTRUCT*);
typedef BOOL (WINAPI * PFN_EnableNonClientDpiScaling)(HWND);
typedef BOOL (WINAPI * PFN_SetProcessDpiAwarenessContext)(HANDLE);
typedef UINT (WINAPI * PFN_GetDpiForWindow)(HWND);
typedef BOOL (WINAPI * PFN_AdjustWindowRectExForDpi)(LPRECT,DWORD,BOOL,DWORD,UINT);
typedef int (WINAPI * PFN_GetSystemMetricsForDpi)(int,UINT);
#define SetProcessDPIAware _desktop_window.win32.user32.SetProcessDPIAware_
#define ChangeWindowMessageFilterEx _desktop_window.win32.user32.ChangeWindowMessageFilterEx_
#define EnableNonClientDpiScaling _desktop_window.win32.user32.EnableNonClientDpiScaling_
#define SetProcessDpiAwarenessContext _desktop_window.win32.user32.SetProcessDpiAwarenessContext_
#define GetDpiForWindow _desktop_window.win32.user32.GetDpiForWindow_
#define AdjustWindowRectExForDpi _desktop_window.win32.user32.AdjustWindowRectExForDpi_
#define GetSystemMetricsForDpi _desktop_window.win32.user32.GetSystemMetricsForDpi_

// dwmapi.dll function pointer typedefs
typedef HRESULT (WINAPI * PFN_DwmIsCompositionEnabled)(BOOL*);
typedef HRESULT (WINAPI * PFN_DwmFlush)(VOID);
typedef HRESULT(WINAPI * PFN_DwmEnableBlurBehindWindow)(HWND,const DWM_BLURBEHIND*);
typedef HRESULT (WINAPI * PFN_DwmGetColorizationColor)(DWORD*,BOOL*);
#define DwmIsCompositionEnabled _desktop_window.win32.dwmapi.IsCompositionEnabled
#define DwmFlush _desktop_window.win32.dwmapi.Flush
#define DwmEnableBlurBehindWindow _desktop_window.win32.dwmapi.EnableBlurBehindWindow
#define DwmGetColorizationColor _desktop_window.win32.dwmapi.GetColorizationColor

// shcore.dll function pointer typedefs
typedef HRESULT (WINAPI * PFN_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS);
typedef HRESULT (WINAPI * PFN_GetDpiForMonitor)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*);
#define SetProcessDpiAwareness _desktop_window.win32.shcore.SetProcessDpiAwareness_
#define GetDpiForMonitor _desktop_window.win32.shcore.GetDpiForMonitor_

// ntdll.dll function pointer typedefs
typedef LONG (WINAPI * PFN_RtlVerifyVersionInfo)(OSVERSIONINFOEXW*,ULONG,ULONGLONG);
#define RtlVerifyVersionInfo _desktop_window.win32.ntdll.RtlVerifyVersionInfo_

// WGL extension pointer typedefs
typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC)(int);
typedef BOOL (WINAPI * PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC,int,int,UINT,const int*,int*);
typedef const char* (WINAPI * PFNWGLGETEXTENSIONSSTRINGEXTPROC)(void);
typedef const char* (WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC);
typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC,HGLRC,const int*);
#define wglSwapIntervalEXT _desktop_window.wgl.SwapIntervalEXT
#define wglGetPixelFormatAttribivARB _desktop_window.wgl.GetPixelFormatAttribivARB
#define wglGetExtensionsStringEXT _desktop_window.wgl.GetExtensionsStringEXT
#define wglGetExtensionsStringARB _desktop_window.wgl.GetExtensionsStringARB
#define wglCreateContextAttribsARB _desktop_window.wgl.CreateContextAttribsARB

// desktop_graphics32.dll function pointer typedefs
typedef HGLRC (WINAPI * PFN_wglCreateContext)(HDC);
typedef BOOL (WINAPI * PFN_wglDeleteContext)(HGLRC);
typedef PROC (WINAPI * PFN_wglGetProcAddress)(LPCSTR);
typedef HDC (WINAPI * PFN_wglGetCurrentDC)(void);
typedef HGLRC (WINAPI * PFN_wglGetCurrentContext)(void);
typedef BOOL (WINAPI * PFN_wglMakeCurrent)(HDC,HGLRC);
typedef BOOL (WINAPI * PFN_wglShareLists)(HGLRC,HGLRC);
#define wglCreateContext _desktop_window.wgl.CreateContext
#define wglDeleteContext _desktop_window.wgl.DeleteContext
#define wglGetProcAddress _desktop_window.wgl.GetProcAddress
#define wglGetCurrentDC _desktop_window.wgl.GetCurrentDC
#define wglGetCurrentContext _desktop_window.wgl.GetCurrentContext
#define wglMakeCurrent _desktop_window.wgl.MakeCurrent
#define wglShareLists _desktop_window.wgl.ShareLists

typedef VkFlags VkWin32SurfaceCreateFlagsKHR;

typedef struct VkWin32SurfaceCreateInfoKHR
{
    VkStructureType                 sType;
    const void*                     pNext;
    VkWin32SurfaceCreateFlagsKHR    flags;
    HINSTANCE                       hinstance;
    HWND                            hwnd;
} VkWin32SurfaceCreateInfoKHR;

typedef VkResult (APIENTRY *PFN_vkCreateWin32SurfaceKHR)(VkInstance,const VkWin32SurfaceCreateInfoKHR*,const VkAllocationCallbacks*,VkSurfaceKHR*);
typedef VkBool32 (APIENTRY *PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)(VkPhysicalDevice,uint32_t);

#define DESKTOP_WINDOW_WIN32_WINDOW_STATE         _DESKTOP_WINDOWwindowWin32  win32;
#define DESKTOP_WINDOW_WIN32_LIBRARY_WINDOW_STATE _DESKTOP_WINDOWlibraryWin32 win32;
#define DESKTOP_WINDOW_WIN32_MONITOR_STATE        _DESKTOP_WINDOWmonitorWin32 win32;
#define DESKTOP_WINDOW_WIN32_CURSOR_STATE         _DESKTOP_WINDOWcursorWin32  win32;

#define DESKTOP_WINDOW_WGL_CONTEXT_STATE          _DESKTOP_WINDOWcontextWGL wgl;
#define DESKTOP_WINDOW_WGL_LIBRARY_CONTEXT_STATE  _DESKTOP_WINDOWlibraryWGL wgl;


// WGL-specific per-context data
//
typedef struct _DESKTOP_WINDOWcontextWGL
{
    HDC       dc;
    HGLRC     handle;
    int       interval;
} _DESKTOP_WINDOWcontextWGL;

// WGL-specific global data
//
typedef struct _DESKTOP_WINDOWlibraryWGL
{
    HINSTANCE                           instance;
    PFN_wglCreateContext                CreateContext;
    PFN_wglDeleteContext                DeleteContext;
    PFN_wglGetProcAddress               GetProcAddress;
    PFN_wglGetCurrentDC                 GetCurrentDC;
    PFN_wglGetCurrentContext            GetCurrentContext;
    PFN_wglMakeCurrent                  MakeCurrent;
    PFN_wglShareLists                   ShareLists;

    PFNWGLSWAPINTERVALEXTPROC           SwapIntervalEXT;
    PFNWGLGETPIXELFORMATATTRIBIVARBPROC GetPixelFormatAttribivARB;
    PFNWGLGETEXTENSIONSSTRINGEXTPROC    GetExtensionsStringEXT;
    PFNWGLGETEXTENSIONSSTRINGARBPROC    GetExtensionsStringARB;
    PFNWGLCREATECONTEXTATTRIBSARBPROC   CreateContextAttribsARB;
    DESKTOP_WINDOWbool                            EXT_swap_control;
    DESKTOP_WINDOWbool                            EXT_colorspace;
    DESKTOP_WINDOWbool                            ARB_multisample;
    DESKTOP_WINDOWbool                            ARB_framebuffer_sRGB;
    DESKTOP_WINDOWbool                            EXT_framebuffer_sRGB;
    DESKTOP_WINDOWbool                            ARB_pixel_format;
    DESKTOP_WINDOWbool                            ARB_create_context;
    DESKTOP_WINDOWbool                            ARB_create_context_profile;
    DESKTOP_WINDOWbool                            EXT_create_context_es2_profile;
    DESKTOP_WINDOWbool                            ARB_create_context_robustness;
    DESKTOP_WINDOWbool                            ARB_create_context_no_error;
    DESKTOP_WINDOWbool                            ARB_context_flush_control;
} _DESKTOP_WINDOWlibraryWGL;

// Win32-specific per-window data
//
typedef struct _DESKTOP_WINDOWwindowWin32
{
    HWND                handle;
    HICON               bigIcon;
    HICON               smallIcon;

    DESKTOP_WINDOWbool            cursorTracked;
    DESKTOP_WINDOWbool            frameAction;
    DESKTOP_WINDOWbool            iconified;
    DESKTOP_WINDOWbool            maximized;
    // Whether to enable framebuffer transparency on DWM
    DESKTOP_WINDOWbool            transparent;
    DESKTOP_WINDOWbool            scaleToMonitor;
    DESKTOP_WINDOWbool            keymenu;
    DESKTOP_WINDOWbool            showDefault;

    // Cached size used to filter out duplicate events
    int                 width, height;

    // The last received cursor position, regardless of source
    int                 lastCursorPosX, lastCursorPosY;
    // The last received high surrogate when decoding pairs of UTF-16 messages
    WCHAR               highSurrogate;
} _DESKTOP_WINDOWwindowWin32;

// Win32-specific global data
//
typedef struct _DESKTOP_WINDOWlibraryWin32
{
    HINSTANCE           instance;
    HWND                helperWindowHandle;
    ATOM                helperWindowClass;
    ATOM                mainWindowClass;
    HDEVNOTIFY          deviceNotificationHandle;
    int                 acquiredMonitorCount;
    char*               clipboardString;
    short int           keycodes[512];
    short int           scancodes[DESKTOP_WINDOW_KEY_LAST + 1];
    char                keynames[DESKTOP_WINDOW_KEY_LAST + 1][5];
    // Where to place the cursor when re-enabled
    double              restoreCursorPosX, restoreCursorPosY;
    // The window whose disabled cursor mode is active
    _DESKTOP_WINDOWwindow*        disabledCursorWindow;
    // The window the cursor is captured in
    _DESKTOP_WINDOWwindow*        capturedCursorWindow;
    RAWINPUT*           rawInput;
    int                 rawInputSize;
    UINT                mouseTrailSize;
    // The cursor handle to use to hide the cursor (NULL or a transparent cursor)
    HCURSOR             blankCursor;

    struct {
        HINSTANCE                       instance;
        PFN_DirectInput8Create          Create;
        IDirectInput8W*                 api;
    } dinput8;

    struct {
        HINSTANCE                       instance;
        PFN_XInputGetCapabilities       GetCapabilities;
        PFN_XInputGetState              GetState;
    } xinput;

    struct {
        HINSTANCE                       instance;
        PFN_SetProcessDPIAware          SetProcessDPIAware_;
        PFN_ChangeWindowMessageFilterEx ChangeWindowMessageFilterEx_;
        PFN_EnableNonClientDpiScaling   EnableNonClientDpiScaling_;
        PFN_SetProcessDpiAwarenessContext SetProcessDpiAwarenessContext_;
        PFN_GetDpiForWindow             GetDpiForWindow_;
        PFN_AdjustWindowRectExForDpi    AdjustWindowRectExForDpi_;
        PFN_GetSystemMetricsForDpi      GetSystemMetricsForDpi_;
    } user32;

    struct {
        HINSTANCE                       instance;
        PFN_DwmIsCompositionEnabled     IsCompositionEnabled;
        PFN_DwmFlush                    Flush;
        PFN_DwmEnableBlurBehindWindow   EnableBlurBehindWindow;
        PFN_DwmGetColorizationColor     GetColorizationColor;
    } dwmapi;

    struct {
        HINSTANCE                       instance;
        PFN_SetProcessDpiAwareness      SetProcessDpiAwareness_;
        PFN_GetDpiForMonitor            GetDpiForMonitor_;
    } shcore;

    struct {
        HINSTANCE                       instance;
        PFN_RtlVerifyVersionInfo        RtlVerifyVersionInfo_;
    } ntdll;
} _DESKTOP_WINDOWlibraryWin32;

// Win32-specific per-monitor data
//
typedef struct _DESKTOP_WINDOWmonitorWin32
{
    HMONITOR            handle;
    // This size matches the static size of DISPLAY_DEVICE.DeviceName
    WCHAR               adapterName[32];
    WCHAR               displayName[32];
    char                publicAdapterName[32];
    char                publicDisplayName[32];
    DESKTOP_WINDOWbool            modesPruned;
    DESKTOP_WINDOWbool            modeChanged;
} _DESKTOP_WINDOWmonitorWin32;

// Win32-specific per-cursor data
//
typedef struct _DESKTOP_WINDOWcursorWin32
{
    HCURSOR             handle;
} _DESKTOP_WINDOWcursorWin32;


DESKTOP_WINDOWbool _desktop_windowConnectWin32(int platformID, _DESKTOP_WINDOWplatform* platform);
int _desktop_windowInitWin32(void);
void _desktop_windowTerminateWin32(void);

WCHAR* _desktop_windowCreateWideStringFromUTF8Win32(const char* source);
char* _desktop_windowCreateUTF8FromWideStringWin32(const WCHAR* source);
BOOL _desktop_windowIsWindowsVersionOrGreaterWin32(WORD major, WORD minor, WORD sp);
BOOL _desktop_windowIsWindows10BuildOrGreaterWin32(WORD build);
void _desktop_windowInputErrorWin32(int error, const char* description);
void _desktop_windowUpdateKeyNamesWin32(void);

void _desktop_windowPollMonitorsWin32(void);
void _desktop_windowSetVideoModeWin32(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWvidmode* desired);
void _desktop_windowRestoreVideoModeWin32(_DESKTOP_WINDOWmonitor* monitor);
void _desktop_windowGetHMONITORContentScaleWin32(HMONITOR handle, float* xscale, float* yscale);

DESKTOP_WINDOWbool _desktop_windowCreateWindowWin32(_DESKTOP_WINDOWwindow* window, const _DESKTOP_WINDOWwndconfig* wndconfig, const _DESKTOP_WINDOWctxconfig* ctxconfig, const _DESKTOP_WINDOWfbconfig* fbconfig);
void _desktop_windowDestroyWindowWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowSetWindowTitleWin32(_DESKTOP_WINDOWwindow* window, const char* title);
void _desktop_windowSetWindowIconWin32(_DESKTOP_WINDOWwindow* window, int count, const DESKTOP_WINDOWimage* images);
void _desktop_windowGetWindowPosWin32(_DESKTOP_WINDOWwindow* window, int* xpos, int* ypos);
void _desktop_windowSetWindowPosWin32(_DESKTOP_WINDOWwindow* window, int xpos, int ypos);
void _desktop_windowGetWindowSizeWin32(_DESKTOP_WINDOWwindow* window, int* width, int* height);
void _desktop_windowSetWindowSizeWin32(_DESKTOP_WINDOWwindow* window, int width, int height);
void _desktop_windowSetWindowSizeLimitsWin32(_DESKTOP_WINDOWwindow* window, int minwidth, int minheight, int maxwidth, int maxheight);
void _desktop_windowSetWindowAspectRatioWin32(_DESKTOP_WINDOWwindow* window, int numer, int denom);
void _desktop_windowGetFramebufferSizeWin32(_DESKTOP_WINDOWwindow* window, int* width, int* height);
void _desktop_windowGetWindowFrameSizeWin32(_DESKTOP_WINDOWwindow* window, int* left, int* top, int* right, int* bottom);
void _desktop_windowGetWindowContentScaleWin32(_DESKTOP_WINDOWwindow* window, float* xscale, float* yscale);
void _desktop_windowIconifyWindowWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowRestoreWindowWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowMaximizeWindowWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowShowWindowWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowHideWindowWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowRequestWindowAttentionWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowFocusWindowWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowSetWindowMonitorWin32(_DESKTOP_WINDOWwindow* window, _DESKTOP_WINDOWmonitor* monitor, int xpos, int ypos, int width, int height, int refreshRate);
DESKTOP_WINDOWbool _desktop_windowWindowFocusedWin32(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowWindowIconifiedWin32(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowWindowVisibleWin32(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowWindowMaximizedWin32(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowWindowHoveredWin32(_DESKTOP_WINDOWwindow* window);
DESKTOP_WINDOWbool _desktop_windowFramebufferTransparentWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowSetWindowResizableWin32(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled);
void _desktop_windowSetWindowDecoratedWin32(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled);
void _desktop_windowSetWindowFloatingWin32(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled);
void _desktop_windowSetWindowMousePassthroughWin32(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled);
float _desktop_windowGetWindowOpacityWin32(_DESKTOP_WINDOWwindow* window);
void _desktop_windowSetWindowOpacityWin32(_DESKTOP_WINDOWwindow* window, float opacity);

void _desktop_windowSetRawMouseMotionWin32(_DESKTOP_WINDOWwindow *window, DESKTOP_WINDOWbool enabled);
DESKTOP_WINDOWbool _desktop_windowRawMouseMotionSupportedWin32(void);

void _desktop_windowPollEventsWin32(void);
void _desktop_windowWaitEventsWin32(void);
void _desktop_windowWaitEventsTimeoutWin32(double timeout);
void _desktop_windowPostEmptyEventWin32(void);

void _desktop_windowGetCursorPosWin32(_DESKTOP_WINDOWwindow* window, double* xpos, double* ypos);
void _desktop_windowSetCursorPosWin32(_DESKTOP_WINDOWwindow* window, double xpos, double ypos);
void _desktop_windowSetCursorModeWin32(_DESKTOP_WINDOWwindow* window, int mode);
const char* _desktop_windowGetScancodeNameWin32(int scancode);
int _desktop_windowGetKeyScancodeWin32(int key);
DESKTOP_WINDOWbool _desktop_windowCreateCursorWin32(_DESKTOP_WINDOWcursor* cursor, const DESKTOP_WINDOWimage* image, int xhot, int yhot);
DESKTOP_WINDOWbool _desktop_windowCreateStandardCursorWin32(_DESKTOP_WINDOWcursor* cursor, int shape);
void _desktop_windowDestroyCursorWin32(_DESKTOP_WINDOWcursor* cursor);
void _desktop_windowSetCursorWin32(_DESKTOP_WINDOWwindow* window, _DESKTOP_WINDOWcursor* cursor);
void _desktop_windowSetClipboardStringWin32(const char* string);
const char* _desktop_windowGetClipboardStringWin32(void);

EGLenum _desktop_windowGetEGLPlatformWin32(EGLint** attribs);
EGLNativeDisplayType _desktop_windowGetEGLNativeDisplayWin32(void);
EGLNativeWindowType _desktop_windowGetEGLNativeWindowWin32(_DESKTOP_WINDOWwindow* window);

void _desktop_windowGetRequiredInstanceExtensionsWin32(char** extensions);
DESKTOP_WINDOWbool _desktop_windowGetPhysicalDevicePresentationSupportWin32(VkInstance instance, VkPhysicalDevice device, uint32_t queuefamily);
VkResult _desktop_windowCreateWindowSurfaceWin32(VkInstance instance, _DESKTOP_WINDOWwindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);

void _desktop_windowFreeMonitorWin32(_DESKTOP_WINDOWmonitor* monitor);
void _desktop_windowGetMonitorPosWin32(_DESKTOP_WINDOWmonitor* monitor, int* xpos, int* ypos);
void _desktop_windowGetMonitorContentScaleWin32(_DESKTOP_WINDOWmonitor* monitor, float* xscale, float* yscale);
void _desktop_windowGetMonitorWorkareaWin32(_DESKTOP_WINDOWmonitor* monitor, int* xpos, int* ypos, int* width, int* height);
DESKTOP_WINDOWvidmode* _desktop_windowGetVideoModesWin32(_DESKTOP_WINDOWmonitor* monitor, int* count);
DESKTOP_WINDOWbool _desktop_windowGetVideoModeWin32(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWvidmode* mode);
DESKTOP_WINDOWbool _desktop_windowGetGammaRampWin32(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWgammaramp* ramp);
void _desktop_windowSetGammaRampWin32(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWgammaramp* ramp);

DESKTOP_WINDOWbool _desktop_windowInitJoysticksWin32(void);
void _desktop_windowTerminateJoysticksWin32(void);
DESKTOP_WINDOWbool _desktop_windowPollJoystickWin32(_DESKTOP_WINDOWjoystick* js, int mode);
const char* _desktop_windowGetMappingNameWin32(void);
void _desktop_windowUpdateGamepadGUIDWin32(char* guid);

DESKTOP_WINDOWbool _desktop_windowInitWGL(void);
void _desktop_windowTerminateWGL(void);
DESKTOP_WINDOWbool _desktop_windowCreateContextWGL(_DESKTOP_WINDOWwindow* window,
                               const _DESKTOP_WINDOWctxconfig* ctxconfig,
                               const _DESKTOP_WINDOWfbconfig* fbconfig);

