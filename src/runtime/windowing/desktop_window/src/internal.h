#pragma once

#if defined(_DESKTOP_WINDOW_USE_CONFIG_H)
 #include "desktop_window_config.h"
#endif

#if defined(DESKTOP_WINDOW_INCLUDE_GLCOREARB) || \
    defined(DESKTOP_WINDOW_INCLUDE_ES1)       || \
    defined(DESKTOP_WINDOW_INCLUDE_ES2)       || \
    defined(DESKTOP_WINDOW_INCLUDE_ES3)       || \
    defined(DESKTOP_WINDOW_INCLUDE_ES31)      || \
    defined(DESKTOP_WINDOW_INCLUDE_ES32)      || \
    defined(DESKTOP_WINDOW_INCLUDE_NONE)      || \
    defined(DESKTOP_WINDOW_INCLUDE_GLEXT)     || \
    defined(DESKTOP_WINDOW_INCLUDE_GLU)       || \
    defined(DESKTOP_WINDOW_INCLUDE_VULKAN)    || \
    defined(DESKTOP_WINDOW_DLL)
 #error "You must not define any header option macros when compiling DESKTOP_WINDOW"
#endif

#define DESKTOP_WINDOW_INCLUDE_NONE
#include "../include/DESKTOP_WINDOW/desktop_window3.h"

#define _DESKTOP_WINDOW_INSERT_FIRST      0
#define _DESKTOP_WINDOW_INSERT_LAST       1

#define _DESKTOP_WINDOW_POLL_PRESENCE     0
#define _DESKTOP_WINDOW_POLL_AXES         1
#define _DESKTOP_WINDOW_POLL_BUTTONS      2
#define _DESKTOP_WINDOW_POLL_ALL          (_DESKTOP_WINDOW_POLL_AXES | _DESKTOP_WINDOW_POLL_BUTTONS)

#define _DESKTOP_WINDOW_MESSAGE_SIZE      1024

typedef int DESKTOP_WINDOWbool;
typedef void (*DESKTOP_WINDOWproc)(void);

typedef struct _DESKTOP_WINDOWerror       _DESKTOP_WINDOWerror;
typedef struct _DESKTOP_WINDOWinitconfig  _DESKTOP_WINDOWinitconfig;
typedef struct _DESKTOP_WINDOWwndconfig   _DESKTOP_WINDOWwndconfig;
typedef struct _DESKTOP_WINDOWctxconfig   _DESKTOP_WINDOWctxconfig;
typedef struct _DESKTOP_WINDOWfbconfig    _DESKTOP_WINDOWfbconfig;
typedef struct _DESKTOP_WINDOWcontext     _DESKTOP_WINDOWcontext;
typedef struct _DESKTOP_WINDOWwindow      _DESKTOP_WINDOWwindow;
typedef struct _DESKTOP_WINDOWplatform    _DESKTOP_WINDOWplatform;
typedef struct _DESKTOP_WINDOWlibrary     _DESKTOP_WINDOWlibrary;
typedef struct _DESKTOP_WINDOWmonitor     _DESKTOP_WINDOWmonitor;
typedef struct _DESKTOP_WINDOWcursor      _DESKTOP_WINDOWcursor;
typedef struct _DESKTOP_WINDOWmapelement  _DESKTOP_WINDOWmapelement;
typedef struct _DESKTOP_WINDOWmapping     _DESKTOP_WINDOWmapping;
typedef struct _DESKTOP_WINDOWjoystick    _DESKTOP_WINDOWjoystick;
typedef struct _DESKTOP_WINDOWtls         _DESKTOP_WINDOWtls;
typedef struct _DESKTOP_WINDOWmutex       _DESKTOP_WINDOWmutex;

#define GL_VERSION 0x1f02
#define GL_NONE 0
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_UNSIGNED_BYTE 0x1401
#define GL_EXTENSIONS 0x1f03
#define GL_NUM_EXTENSIONS 0x821d
#define GL_CONTEXT_FLAGS 0x821e
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x00000001
#define GL_CONTEXT_FLAG_DEBUG_BIT 0x00000002
#define GL_CONTEXT_PROFILE_MASK 0x9126
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#define GL_RESET_NOTIFICATION_STRATEGY_ARB 0x8256
#define GL_LOSE_CONTEXT_ON_RESET_ARB 0x8252
#define GL_NO_RESET_NOTIFICATION_ARB 0x8261
#define GL_CONTEXT_RELEASE_BEHAVIOR 0x82fb
#define GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH 0x82fc
#define GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR 0x00000008

typedef int GLint;
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned char GLubyte;

typedef void (APIENTRY * PFNGLCLEARPROC)(GLbitfield);
typedef const GLubyte* (APIENTRY * PFNGLGETSTRINGPROC)(GLenum);
typedef void (APIENTRY * PFNGLGETINTEGERVPROC)(GLenum,GLint*);
typedef const GLubyte* (APIENTRY * PFNGLGETSTRINGIPROC)(GLenum,GLuint);

#define EGL_SUCCESS 0x3000
#define EGL_NOT_INITIALIZED 0x3001
#define EGL_BAD_ACCESS 0x3002
#define EGL_BAD_ALLOC 0x3003
#define EGL_BAD_ATTRIBUTE 0x3004
#define EGL_BAD_CONFIG 0x3005
#define EGL_BAD_CONTEXT 0x3006
#define EGL_BAD_CURRENT_SURFACE 0x3007
#define EGL_BAD_DISPLAY 0x3008
#define EGL_BAD_MATCH 0x3009
#define EGL_BAD_NATIVE_PIXMAP 0x300a
#define EGL_BAD_NATIVE_WINDOW 0x300b
#define EGL_BAD_PARAMETER 0x300c
#define EGL_BAD_SURFACE 0x300d
#define EGL_CONTEXT_LOST 0x300e
#define EGL_COLOR_BUFFER_TYPE 0x303f
#define EGL_RGB_BUFFER 0x308e
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES_BIT 0x0001
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_OPENGL_BIT 0x0008
#define EGL_ALPHA_SIZE 0x3021
#define EGL_BLUE_SIZE 0x3022
#define EGL_GREEN_SIZE 0x3023
#define EGL_RED_SIZE 0x3024
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3026
#define EGL_SAMPLES 0x3031
#define EGL_OPENGL_ES_API 0x30a0
#define EGL_OPENGL_API 0x30a2
#define EGL_NONE 0x3038
#define EGL_RENDER_BUFFER 0x3086
#define EGL_SINGLE_BUFFER 0x3085
#define EGL_EXTENSIONS 0x3055
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_NATIVE_VISUAL_ID 0x302e
#define EGL_NO_SURFACE ((EGLSurface) 0)
#define EGL_NO_DISPLAY ((EGLDisplay) 0)
#define EGL_NO_CONTEXT ((EGLContext) 0)
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType) 0)

#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR 0x31bd
#define EGL_NO_RESET_NOTIFICATION_KHR 0x31be
#define EGL_LOSE_CONTEXT_ON_RESET_KHR 0x31bf
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR 0x00000004
#define EGL_CONTEXT_MAJOR_VERSION_KHR 0x3098
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30fb
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR 0x30fd
#define EGL_CONTEXT_FLAGS_KHR 0x30fc
#define EGL_CONTEXT_OPENGL_NO_ERROR_KHR 0x31b3
#define EGL_GL_COLORSPACE_KHR 0x309d
#define EGL_GL_COLORSPACE_SRGB_KHR 0x3089
#define EGL_CONTEXT_RELEASE_BEHAVIOR_KHR 0x2097
#define EGL_CONTEXT_RELEASE_BEHAVIOR_NONE_KHR 0
#define EGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_KHR 0x2098
#define EGL_PLATFORM_X11_EXT 0x31d5
#define EGL_PLATFORM_WAYLAND_EXT 0x31d8
#define EGL_PRESENT_OPAQUE_EXT 0x31df
#define EGL_PLATFORM_ANGLE_ANGLE 0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE 0x3203
#define EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE 0x320d
#define EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE 0x320e
#define EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE 0x3207
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#define EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE 0x3450
#define EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE 0x3489
#define EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE 0x348f

typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLDisplay;
typedef void* EGLSurface;

typedef void* EGLNativeDisplayType;
typedef void* EGLNativeWindowType;

// EGL function pointer typedefs
typedef EGLBoolean (APIENTRY * PFN_eglGetConfigAttrib)(EGLDisplay,EGLConfig,EGLint,EGLint*);
typedef EGLBoolean (APIENTRY * PFN_eglGetConfigs)(EGLDisplay,EGLConfig*,EGLint,EGLint*);
typedef EGLDisplay (APIENTRY * PFN_eglGetDisplay)(EGLNativeDisplayType);
typedef EGLint (APIENTRY * PFN_eglGetError)(void);
typedef EGLBoolean (APIENTRY * PFN_eglInitialize)(EGLDisplay,EGLint*,EGLint*);
typedef EGLBoolean (APIENTRY * PFN_eglTerminate)(EGLDisplay);
typedef EGLBoolean (APIENTRY * PFN_eglBindAPI)(EGLenum);
typedef EGLContext (APIENTRY * PFN_eglCreateContext)(EGLDisplay,EGLConfig,EGLContext,const EGLint*);
typedef EGLBoolean (APIENTRY * PFN_eglDestroySurface)(EGLDisplay,EGLSurface);
typedef EGLBoolean (APIENTRY * PFN_eglDestroyContext)(EGLDisplay,EGLContext);
typedef EGLSurface (APIENTRY * PFN_eglCreateWindowSurface)(EGLDisplay,EGLConfig,EGLNativeWindowType,const EGLint*);
typedef EGLBoolean (APIENTRY * PFN_eglMakeCurrent)(EGLDisplay,EGLSurface,EGLSurface,EGLContext);
typedef EGLBoolean (APIENTRY * PFN_eglSwapBuffers)(EGLDisplay,EGLSurface);
typedef EGLBoolean (APIENTRY * PFN_eglSwapInterval)(EGLDisplay,EGLint);
typedef const char* (APIENTRY * PFN_eglQueryString)(EGLDisplay,EGLint);
typedef DESKTOP_WINDOWglproc (APIENTRY * PFN_eglGetProcAddress)(const char*);
#define eglGetConfigAttrib _desktop_window.egl.GetConfigAttrib
#define eglGetConfigs _desktop_window.egl.GetConfigs
#define eglGetDisplay _desktop_window.egl.GetDisplay
#define eglGetError _desktop_window.egl.GetError
#define eglInitialize _desktop_window.egl.Initialize
#define eglTerminate _desktop_window.egl.Terminate
#define eglBindAPI _desktop_window.egl.BindAPI
#define eglCreateContext _desktop_window.egl.CreateContext
#define eglDestroySurface _desktop_window.egl.DestroySurface
#define eglDestroyContext _desktop_window.egl.DestroyContext
#define eglCreateWindowSurface _desktop_window.egl.CreateWindowSurface
#define eglMakeCurrent _desktop_window.egl.MakeCurrent
#define eglSwapBuffers _desktop_window.egl.SwapBuffers
#define eglSwapInterval _desktop_window.egl.SwapInterval
#define eglQueryString _desktop_window.egl.QueryString
#define eglGetProcAddress _desktop_window.egl.GetProcAddress

typedef EGLDisplay (APIENTRY * PFNEGLGETPLATFORMDISPLAYEXTPROC)(EGLenum,void*,const EGLint*);
typedef EGLSurface (APIENTRY * PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)(EGLDisplay,EGLConfig,void*,const EGLint*);
#define eglGetPlatformDisplayEXT _desktop_window.egl.GetPlatformDisplayEXT
#define eglCreatePlatformWindowSurfaceEXT _desktop_window.egl.CreatePlatformWindowSurfaceEXT

#define OSMESA_RGBA 0x1908
#define OSMESA_FORMAT 0x22
#define OSMESA_DEPTH_BITS 0x30
#define OSMESA_STENCIL_BITS 0x31
#define OSMESA_ACCUM_BITS 0x32
#define OSMESA_PROFILE 0x33
#define OSMESA_CORE_PROFILE 0x34
#define OSMESA_COMPAT_PROFILE 0x35
#define OSMESA_CONTEXT_MAJOR_VERSION 0x36
#define OSMESA_CONTEXT_MINOR_VERSION 0x37

typedef void* OSMesaContext;
typedef void (*OSMESAproc)(void);

typedef OSMesaContext (GLAPIENTRY * PFN_OSMesaCreateContextExt)(GLenum,GLint,GLint,GLint,OSMesaContext);
typedef OSMesaContext (GLAPIENTRY * PFN_OSMesaCreateContextAttribs)(const int*,OSMesaContext);
typedef void (GLAPIENTRY * PFN_OSMesaDestroyContext)(OSMesaContext);
typedef int (GLAPIENTRY * PFN_OSMesaMakeCurrent)(OSMesaContext,void*,int,int,int);
typedef int (GLAPIENTRY * PFN_OSMesaGetColorBuffer)(OSMesaContext,int*,int*,int*,void**);
typedef int (GLAPIENTRY * PFN_OSMesaGetDepthBuffer)(OSMesaContext,int*,int*,int*,void**);
typedef DESKTOP_WINDOWglproc (GLAPIENTRY * PFN_OSMesaGetProcAddress)(const char*);
#define OSMesaCreateContextExt _desktop_window.osmesa.CreateContextExt
#define OSMesaCreateContextAttribs _desktop_window.osmesa.CreateContextAttribs
#define OSMesaDestroyContext _desktop_window.osmesa.DestroyContext
#define OSMesaMakeCurrent _desktop_window.osmesa.MakeCurrent
#define OSMesaGetColorBuffer _desktop_window.osmesa.GetColorBuffer
#define OSMesaGetDepthBuffer _desktop_window.osmesa.GetDepthBuffer
#define OSMesaGetProcAddress _desktop_window.osmesa.GetProcAddress

#define VK_NULL_HANDLE 0

typedef void* VkInstance;
typedef void* VkPhysicalDevice;
typedef uint64_t VkSurfaceKHR;
typedef uint32_t VkFlags;
typedef uint32_t VkBool32;

typedef enum VkStructureType
{
    VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR = 1000004000,
    VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR = 1000005000,
    VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR = 1000006000,
    VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR = 1000009000,
    VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK = 1000123000,
    VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT = 1000217000,
    VK_STRUCTURE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkStructureType;

typedef enum VkResult
{
    VK_SUCCESS = 0,
    VK_NOT_READY = 1,
    VK_TIMEOUT = 2,
    VK_EVENT_SET = 3,
    VK_EVENT_RESET = 4,
    VK_INCOMPLETE = 5,
    VK_ERROR_OUT_OF_HOST_MEMORY = -1,
    VK_ERROR_OUT_OF_DEVICE_MEMORY = -2,
    VK_ERROR_INITIALIZATION_FAILED = -3,
    VK_ERROR_DEVICE_LOST = -4,
    VK_ERROR_MEMORY_MAP_FAILED = -5,
    VK_ERROR_LAYER_NOT_PRESENT = -6,
    VK_ERROR_EXTENSION_NOT_PRESENT = -7,
    VK_ERROR_FEATURE_NOT_PRESENT = -8,
    VK_ERROR_INCOMPATIBLE_DRIVER = -9,
    VK_ERROR_TOO_MANY_OBJECTS = -10,
    VK_ERROR_FORMAT_NOT_SUPPORTED = -11,
    VK_ERROR_SURFACE_LOST_KHR = -1000000000,
    VK_SUBOPTIMAL_KHR = 1000001003,
    VK_ERROR_OUT_OF_DATE_KHR = -1000001004,
    VK_ERROR_INCOMPATIBLE_DISPLAY_KHR = -1000003001,
    VK_ERROR_NATIVE_WINDOW_IN_USE_KHR = -1000000001,
    VK_ERROR_VALIDATION_FAILED_EXT = -1000011001,
    VK_RESULT_MAX_ENUM = 0x7FFFFFFF
} VkResult;

typedef struct VkAllocationCallbacks VkAllocationCallbacks;

typedef struct VkExtensionProperties
{
    char            extensionName[256];
    uint32_t        specVersion;
} VkExtensionProperties;

typedef void (APIENTRY * PFN_vkVoidFunction)(void);

typedef PFN_vkVoidFunction (APIENTRY * PFN_vkGetInstanceProcAddr)(VkInstance,const char*);
typedef VkResult (APIENTRY * PFN_vkEnumerateInstanceExtensionProperties)(const char*,uint32_t*,VkExtensionProperties*);
#define vkGetInstanceProcAddr _desktop_window.vk.GetInstanceProcAddr

#include "platform.h"

#define DESKTOP_WINDOW_NATIVE_INCLUDE_NONE
#include "../include/DESKTOP_WINDOW/desktop_window3native.h"

// Checks for whether the library has been initialized
#define _DESKTOP_WINDOW_REQUIRE_INIT()                         \
    if (!_desktop_window.initialized)                          \
    {                                                \
        _desktop_windowInputError(DESKTOP_WINDOW_NOT_INITIALIZED, NULL); \
        return;                                      \
    }
#define _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(x)              \
    if (!_desktop_window.initialized)                          \
    {                                                \
        _desktop_windowInputError(DESKTOP_WINDOW_NOT_INITIALIZED, NULL); \
        return x;                                    \
    }

// Swaps the provided pointers
#define _DESKTOP_WINDOW_SWAP(type, x, y) \
    {                          \
        type t;                \
        t = x;                 \
        x = y;                 \
        y = t;                 \
    }

// Per-thread error structure
//
struct _DESKTOP_WINDOWerror
{
    _DESKTOP_WINDOWerror*     next;
    int             code;
    char            description[_DESKTOP_WINDOW_MESSAGE_SIZE];
};

// Initialization configuration
//
// Parameters relating to the initialization of the library
//
struct _DESKTOP_WINDOWinitconfig
{
    DESKTOP_WINDOWbool      hatButtons;
    int           angleType;
    int           platformID;
    PFN_vkGetInstanceProcAddr vulkanLoader;
    struct {
        DESKTOP_WINDOWbool  menubar;
        DESKTOP_WINDOWbool  chdir;
    } ns;
    struct {
        DESKTOP_WINDOWbool  xcbVulkanSurface;
    } x11;
    struct {
        int       libdecorMode;
    } wl;
};

// Window configuration
//
// Parameters relating to the creation of the window but not directly related
// to the framebuffer.  This is used to pass window creation parameters from
// shared code to the platform API.
//
struct _DESKTOP_WINDOWwndconfig
{
    int           xpos;
    int           ypos;
    int           width;
    int           height;
    const char*   title;
    DESKTOP_WINDOWbool      resizable;
    DESKTOP_WINDOWbool      visible;
    DESKTOP_WINDOWbool      decorated;
    DESKTOP_WINDOWbool      focused;
    DESKTOP_WINDOWbool      autoIconify;
    DESKTOP_WINDOWbool      floating;
    DESKTOP_WINDOWbool      maximized;
    DESKTOP_WINDOWbool      centerCursor;
    DESKTOP_WINDOWbool      focusOnShow;
    DESKTOP_WINDOWbool      mousePassthrough;
    DESKTOP_WINDOWbool      scaleToMonitor;
    DESKTOP_WINDOWbool      scaleFramebuffer;
    struct {
        char      frameName[256];
    } ns;
    struct {
        char      className[256];
        char      instanceName[256];
    } x11;
    struct {
        DESKTOP_WINDOWbool  keymenu;
        DESKTOP_WINDOWbool  showDefault;
    } win32;
    struct {
        char      appId[256];
    } wl;
};

// Context configuration
//
// Parameters relating to the creation of the context but not directly related
// to the framebuffer.  This is used to pass context creation parameters from
// shared code to the platform API.
//
struct _DESKTOP_WINDOWctxconfig
{
    int           client;
    int           source;
    int           major;
    int           minor;
    DESKTOP_WINDOWbool      forward;
    DESKTOP_WINDOWbool      debug;
    DESKTOP_WINDOWbool      noerror;
    int           profile;
    int           robustness;
    int           release;
    _DESKTOP_WINDOWwindow*  share;
    struct {
        DESKTOP_WINDOWbool  offline;
    } nsgl;
};

// Framebuffer configuration
//
// This describes buffers and their sizes.  It also contains
// a platform-specific ID used to map back to the backend API object.
//
// It is used to pass framebuffer parameters from shared code to the platform
// API and also to enumerate and select available framebuffer configs.
//
struct _DESKTOP_WINDOWfbconfig
{
    int         redBits;
    int         greenBits;
    int         blueBits;
    int         alphaBits;
    int         depthBits;
    int         stencilBits;
    int         accumRedBits;
    int         accumGreenBits;
    int         accumBlueBits;
    int         accumAlphaBits;
    int         auxBuffers;
    DESKTOP_WINDOWbool    stereo;
    int         samples;
    DESKTOP_WINDOWbool    sRGB;
    DESKTOP_WINDOWbool    doublebuffer;
    DESKTOP_WINDOWbool    transparent;
    uintptr_t   handle;
};

// Context structure
//
struct _DESKTOP_WINDOWcontext
{
    int                 client;
    int                 source;
    int                 major, minor, revision;
    DESKTOP_WINDOWbool            forward, debug, noerror;
    int                 profile;
    int                 robustness;
    int                 release;

    PFNGLGETSTRINGIPROC  GetStringi;
    PFNGLGETINTEGERVPROC GetIntegerv;
    PFNGLGETSTRINGPROC   GetString;

    void (*makeCurrent)(_DESKTOP_WINDOWwindow*);
    void (*swapBuffers)(_DESKTOP_WINDOWwindow*);
    void (*swapInterval)(int);
    int (*extensionSupported)(const char*);
    DESKTOP_WINDOWglproc (*getProcAddress)(const char*);
    void (*destroy)(_DESKTOP_WINDOWwindow*);

    struct {
        EGLConfig       config;
        EGLContext      handle;
        EGLSurface      surface;
        void*           client;
    } egl;

    struct {
        OSMesaContext   handle;
        int             width;
        int             height;
        void*           buffer;
    } osmesa;

    // This is defined in platform.h
    DESKTOP_WINDOW_PLATFORM_CONTEXT_STATE
};

// Window and context structure
//
struct _DESKTOP_WINDOWwindow
{
    struct _DESKTOP_WINDOWwindow* next;

    // Window settings and state
    DESKTOP_WINDOWbool            resizable;
    DESKTOP_WINDOWbool            decorated;
    DESKTOP_WINDOWbool            autoIconify;
    DESKTOP_WINDOWbool            floating;
    DESKTOP_WINDOWbool            focusOnShow;
    DESKTOP_WINDOWbool            mousePassthrough;
    DESKTOP_WINDOWbool            shouldClose;
    void*               userPointer;
    DESKTOP_WINDOWbool            doublebuffer;
    DESKTOP_WINDOWvidmode         videoMode;
    _DESKTOP_WINDOWmonitor*       monitor;
    _DESKTOP_WINDOWcursor*        cursor;
    char*               title;

    int                 minwidth, minheight;
    int                 maxwidth, maxheight;
    int                 numer, denom;

    DESKTOP_WINDOWbool            stickyKeys;
    DESKTOP_WINDOWbool            stickyMouseButtons;
    DESKTOP_WINDOWbool            lockKeyMods;
    int                 cursorMode;
    char                mouseButtons[DESKTOP_WINDOW_MOUSE_BUTTON_LAST + 1];
    char                keys[DESKTOP_WINDOW_KEY_LAST + 1];
    // Virtual cursor position when cursor is disabled
    double              virtualCursorPosX, virtualCursorPosY;
    DESKTOP_WINDOWbool            rawMouseMotion;

    _DESKTOP_WINDOWcontext        context;

    struct {
        DESKTOP_WINDOWwindowposfun          pos;
        DESKTOP_WINDOWwindowsizefun         size;
        DESKTOP_WINDOWwindowclosefun        close;
        DESKTOP_WINDOWwindowrefreshfun      refresh;
        DESKTOP_WINDOWwindowfocusfun        focus;
        DESKTOP_WINDOWwindowiconifyfun      iconify;
        DESKTOP_WINDOWwindowmaximizefun     maximize;
        DESKTOP_WINDOWframebuffersizefun    fbsize;
        DESKTOP_WINDOWwindowcontentscalefun scale;
        DESKTOP_WINDOWmousebuttonfun        mouseButton;
        DESKTOP_WINDOWcursorposfun          cursorPos;
        DESKTOP_WINDOWcursorenterfun        cursorEnter;
        DESKTOP_WINDOWscrollfun             scroll;
        DESKTOP_WINDOWkeyfun                key;
        DESKTOP_WINDOWcharfun               character;
        DESKTOP_WINDOWcharmodsfun           charmods;
        DESKTOP_WINDOWdropfun               drop;
    } callbacks;

    // This is defined in platform.h
    DESKTOP_WINDOW_PLATFORM_WINDOW_STATE
};

// Monitor structure
//
struct _DESKTOP_WINDOWmonitor
{
    char            name[128];
    void*           userPointer;

    // Physical dimensions in millimeters.
    int             widthMM, heightMM;

    // The window whose video mode is current on this monitor
    _DESKTOP_WINDOWwindow*    window;

    DESKTOP_WINDOWvidmode*    modes;
    int             modeCount;
    DESKTOP_WINDOWvidmode     currentMode;

    DESKTOP_WINDOWgammaramp   originalRamp;
    DESKTOP_WINDOWgammaramp   currentRamp;

    // This is defined in platform.h
    DESKTOP_WINDOW_PLATFORM_MONITOR_STATE
};

// Cursor structure
//
struct _DESKTOP_WINDOWcursor
{
    _DESKTOP_WINDOWcursor*    next;
    // This is defined in platform.h
    DESKTOP_WINDOW_PLATFORM_CURSOR_STATE
};

// Gamepad mapping element structure
//
struct _DESKTOP_WINDOWmapelement
{
    uint8_t         type;
    uint8_t         index;
    int8_t          axisScale;
    int8_t          axisOffset;
};

// Gamepad mapping structure
//
struct _DESKTOP_WINDOWmapping
{
    char            name[128];
    char            guid[33];
    _DESKTOP_WINDOWmapelement buttons[15];
    _DESKTOP_WINDOWmapelement axes[6];
};

// Joystick structure
//
struct _DESKTOP_WINDOWjoystick
{
    DESKTOP_WINDOWbool        allocated;
    DESKTOP_WINDOWbool        connected;
    float*          axes;
    int             axisCount;
    unsigned char*  buttons;
    int             buttonCount;
    unsigned char*  hats;
    int             hatCount;
    char            name[128];
    void*           userPointer;
    char            guid[33];
    _DESKTOP_WINDOWmapping*   mapping;

    // This is defined in platform.h
    DESKTOP_WINDOW_PLATFORM_JOYSTICK_STATE
};

// Thread local storage structure
//
struct _DESKTOP_WINDOWtls
{
    // This is defined in platform.h
    DESKTOP_WINDOW_PLATFORM_TLS_STATE
};

// Mutex structure
//
struct _DESKTOP_WINDOWmutex
{
    // This is defined in platform.h
    DESKTOP_WINDOW_PLATFORM_MUTEX_STATE
};

// Platform API structure
//
struct _DESKTOP_WINDOWplatform
{
    int platformID;
    // init
    DESKTOP_WINDOWbool (*init)(void);
    void (*terminate)(void);
    // input
    void (*getCursorPos)(_DESKTOP_WINDOWwindow*,double*,double*);
    void (*setCursorPos)(_DESKTOP_WINDOWwindow*,double,double);
    void (*setCursorMode)(_DESKTOP_WINDOWwindow*,int);
    void (*setRawMouseMotion)(_DESKTOP_WINDOWwindow*,DESKTOP_WINDOWbool);
    DESKTOP_WINDOWbool (*rawMouseMotionSupported)(void);
    DESKTOP_WINDOWbool (*createCursor)(_DESKTOP_WINDOWcursor*,const DESKTOP_WINDOWimage*,int,int);
    DESKTOP_WINDOWbool (*createStandardCursor)(_DESKTOP_WINDOWcursor*,int);
    void (*destroyCursor)(_DESKTOP_WINDOWcursor*);
    void (*setCursor)(_DESKTOP_WINDOWwindow*,_DESKTOP_WINDOWcursor*);
    const char* (*getScancodeName)(int);
    int (*getKeyScancode)(int);
    void (*setClipboardString)(const char*);
    const char* (*getClipboardString)(void);
    DESKTOP_WINDOWbool (*initJoysticks)(void);
    void (*terminateJoysticks)(void);
    DESKTOP_WINDOWbool (*pollJoystick)(_DESKTOP_WINDOWjoystick*,int);
    const char* (*getMappingName)(void);
    void (*updateGamepadGUID)(char*);
    // monitor
    void (*freeMonitor)(_DESKTOP_WINDOWmonitor*);
    void (*getMonitorPos)(_DESKTOP_WINDOWmonitor*,int*,int*);
    void (*getMonitorContentScale)(_DESKTOP_WINDOWmonitor*,float*,float*);
    void (*getMonitorWorkarea)(_DESKTOP_WINDOWmonitor*,int*,int*,int*,int*);
    DESKTOP_WINDOWvidmode* (*getVideoModes)(_DESKTOP_WINDOWmonitor*,int*);
    DESKTOP_WINDOWbool (*getVideoMode)(_DESKTOP_WINDOWmonitor*,DESKTOP_WINDOWvidmode*);
    DESKTOP_WINDOWbool (*getGammaRamp)(_DESKTOP_WINDOWmonitor*,DESKTOP_WINDOWgammaramp*);
    void (*setGammaRamp)(_DESKTOP_WINDOWmonitor*,const DESKTOP_WINDOWgammaramp*);
    // window
    DESKTOP_WINDOWbool (*createWindow)(_DESKTOP_WINDOWwindow*,const _DESKTOP_WINDOWwndconfig*,const _DESKTOP_WINDOWctxconfig*,const _DESKTOP_WINDOWfbconfig*);
    void (*destroyWindow)(_DESKTOP_WINDOWwindow*);
    void (*setWindowTitle)(_DESKTOP_WINDOWwindow*,const char*);
    void (*setWindowIcon)(_DESKTOP_WINDOWwindow*,int,const DESKTOP_WINDOWimage*);
    void (*getWindowPos)(_DESKTOP_WINDOWwindow*,int*,int*);
    void (*setWindowPos)(_DESKTOP_WINDOWwindow*,int,int);
    void (*getWindowSize)(_DESKTOP_WINDOWwindow*,int*,int*);
    void (*setWindowSize)(_DESKTOP_WINDOWwindow*,int,int);
    void (*setWindowSizeLimits)(_DESKTOP_WINDOWwindow*,int,int,int,int);
    void (*setWindowAspectRatio)(_DESKTOP_WINDOWwindow*,int,int);
    void (*getFramebufferSize)(_DESKTOP_WINDOWwindow*,int*,int*);
    void (*getWindowFrameSize)(_DESKTOP_WINDOWwindow*,int*,int*,int*,int*);
    void (*getWindowContentScale)(_DESKTOP_WINDOWwindow*,float*,float*);
    void (*iconifyWindow)(_DESKTOP_WINDOWwindow*);
    void (*restoreWindow)(_DESKTOP_WINDOWwindow*);
    void (*maximizeWindow)(_DESKTOP_WINDOWwindow*);
    void (*showWindow)(_DESKTOP_WINDOWwindow*);
    void (*hideWindow)(_DESKTOP_WINDOWwindow*);
    void (*requestWindowAttention)(_DESKTOP_WINDOWwindow*);
    void (*focusWindow)(_DESKTOP_WINDOWwindow*);
    void (*setWindowMonitor)(_DESKTOP_WINDOWwindow*,_DESKTOP_WINDOWmonitor*,int,int,int,int,int);
    DESKTOP_WINDOWbool (*windowFocused)(_DESKTOP_WINDOWwindow*);
    DESKTOP_WINDOWbool (*windowIconified)(_DESKTOP_WINDOWwindow*);
    DESKTOP_WINDOWbool (*windowVisible)(_DESKTOP_WINDOWwindow*);
    DESKTOP_WINDOWbool (*windowMaximized)(_DESKTOP_WINDOWwindow*);
    DESKTOP_WINDOWbool (*windowHovered)(_DESKTOP_WINDOWwindow*);
    DESKTOP_WINDOWbool (*framebufferTransparent)(_DESKTOP_WINDOWwindow*);
    float (*getWindowOpacity)(_DESKTOP_WINDOWwindow*);
    void (*setWindowResizable)(_DESKTOP_WINDOWwindow*,DESKTOP_WINDOWbool);
    void (*setWindowDecorated)(_DESKTOP_WINDOWwindow*,DESKTOP_WINDOWbool);
    void (*setWindowFloating)(_DESKTOP_WINDOWwindow*,DESKTOP_WINDOWbool);
    void (*setWindowOpacity)(_DESKTOP_WINDOWwindow*,float);
    void (*setWindowMousePassthrough)(_DESKTOP_WINDOWwindow*,DESKTOP_WINDOWbool);
    void (*pollEvents)(void);
    void (*waitEvents)(void);
    void (*waitEventsTimeout)(double);
    void (*postEmptyEvent)(void);
    // EGL
    EGLenum (*getEGLPlatform)(EGLint**);
    EGLNativeDisplayType (*getEGLNativeDisplay)(void);
    EGLNativeWindowType (*getEGLNativeWindow)(_DESKTOP_WINDOWwindow*);
    // vulkan
    void (*getRequiredInstanceExtensions)(char**);
    DESKTOP_WINDOWbool (*getPhysicalDevicePresentationSupport)(VkInstance,VkPhysicalDevice,uint32_t);
    VkResult (*createWindowSurface)(VkInstance,_DESKTOP_WINDOWwindow*,const VkAllocationCallbacks*,VkSurfaceKHR*);
};

// Library global data
//
struct _DESKTOP_WINDOWlibrary
{
    DESKTOP_WINDOWbool            initialized;
    DESKTOP_WINDOWallocator       allocator;

    _DESKTOP_WINDOWplatform       platform;

    struct {
        _DESKTOP_WINDOWinitconfig init;
        _DESKTOP_WINDOWfbconfig   framebuffer;
        _DESKTOP_WINDOWwndconfig  window;
        _DESKTOP_WINDOWctxconfig  context;
        int             refreshRate;
    } hints;

    _DESKTOP_WINDOWerror*         errorListHead;
    _DESKTOP_WINDOWcursor*        cursorListHead;
    _DESKTOP_WINDOWwindow*        windowListHead;

    _DESKTOP_WINDOWmonitor**      monitors;
    int                 monitorCount;

    DESKTOP_WINDOWbool            joysticksInitialized;
    _DESKTOP_WINDOWjoystick       joysticks[DESKTOP_WINDOW_JOYSTICK_LAST + 1];
    _DESKTOP_WINDOWmapping*       mappings;
    int                 mappingCount;

    _DESKTOP_WINDOWtls            errorSlot;
    _DESKTOP_WINDOWtls            contextSlot;
    _DESKTOP_WINDOWmutex          errorLock;

    struct {
        uint64_t        offset;
        // This is defined in platform.h
        DESKTOP_WINDOW_PLATFORM_LIBRARY_TIMER_STATE
    } timer;

    struct {
        EGLenum         platform;
        EGLDisplay      display;
        EGLint          major, minor;
        DESKTOP_WINDOWbool        prefix;

        DESKTOP_WINDOWbool        KHR_create_context;
        DESKTOP_WINDOWbool        KHR_create_context_no_error;
        DESKTOP_WINDOWbool        KHR_gl_colorspace;
        DESKTOP_WINDOWbool        KHR_get_all_proc_addresses;
        DESKTOP_WINDOWbool        KHR_context_flush_control;
        DESKTOP_WINDOWbool        EXT_client_extensions;
        DESKTOP_WINDOWbool        EXT_platform_base;
        DESKTOP_WINDOWbool        EXT_platform_x11;
        DESKTOP_WINDOWbool        EXT_platform_wayland;
        DESKTOP_WINDOWbool        EXT_present_opaque;
        DESKTOP_WINDOWbool        ANGLE_platform_angle;
        DESKTOP_WINDOWbool        ANGLE_platform_angle_opengl;
        DESKTOP_WINDOWbool        ANGLE_platform_angle_d3d;
        DESKTOP_WINDOWbool        ANGLE_platform_angle_vulkan;
        DESKTOP_WINDOWbool        ANGLE_platform_angle_metal;

        void*           handle;

        PFN_eglGetConfigAttrib      GetConfigAttrib;
        PFN_eglGetConfigs           GetConfigs;
        PFN_eglGetDisplay           GetDisplay;
        PFN_eglGetError             GetError;
        PFN_eglInitialize           Initialize;
        PFN_eglTerminate            Terminate;
        PFN_eglBindAPI              BindAPI;
        PFN_eglCreateContext        CreateContext;
        PFN_eglDestroySurface       DestroySurface;
        PFN_eglDestroyContext       DestroyContext;
        PFN_eglCreateWindowSurface  CreateWindowSurface;
        PFN_eglMakeCurrent          MakeCurrent;
        PFN_eglSwapBuffers          SwapBuffers;
        PFN_eglSwapInterval         SwapInterval;
        PFN_eglQueryString          QueryString;
        PFN_eglGetProcAddress       GetProcAddress;

        PFNEGLGETPLATFORMDISPLAYEXTPROC GetPlatformDisplayEXT;
        PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC CreatePlatformWindowSurfaceEXT;
    } egl;

    struct {
        void*           handle;

        PFN_OSMesaCreateContextExt      CreateContextExt;
        PFN_OSMesaCreateContextAttribs  CreateContextAttribs;
        PFN_OSMesaDestroyContext        DestroyContext;
        PFN_OSMesaMakeCurrent           MakeCurrent;
        PFN_OSMesaGetColorBuffer        GetColorBuffer;
        PFN_OSMesaGetDepthBuffer        GetDepthBuffer;
        PFN_OSMesaGetProcAddress        GetProcAddress;

    } osmesa;

    struct {
        DESKTOP_WINDOWbool        available;
        void*           handle;
        char*           extensions[2];
        PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
        DESKTOP_WINDOWbool        KHR_surface;
        DESKTOP_WINDOWbool        KHR_win32_surface;
        DESKTOP_WINDOWbool        MVK_macos_surface;
        DESKTOP_WINDOWbool        EXT_metal_surface;
        DESKTOP_WINDOWbool        KHR_xlib_surface;
        DESKTOP_WINDOWbool        KHR_xcb_surface;
        DESKTOP_WINDOWbool        KHR_wayland_surface;
    } vk;

    struct {
        DESKTOP_WINDOWmonitorfun  monitor;
        DESKTOP_WINDOWjoystickfun joystick;
    } callbacks;

    // These are defined in platform.h
    DESKTOP_WINDOW_PLATFORM_LIBRARY_WINDOW_STATE
    DESKTOP_WINDOW_PLATFORM_LIBRARY_CONTEXT_STATE
    DESKTOP_WINDOW_PLATFORM_LIBRARY_JOYSTICK_STATE
};

// Global state shared between compilation units of DESKTOP_WINDOW
//
extern _DESKTOP_WINDOWlibrary _desktop_window;


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowPlatformInitTimer(void);
uint64_t _desktop_windowPlatformGetTimerValue(void);
uint64_t _desktop_windowPlatformGetTimerFrequency(void);

DESKTOP_WINDOWbool _desktop_windowPlatformCreateTls(_DESKTOP_WINDOWtls* tls);
void _desktop_windowPlatformDestroyTls(_DESKTOP_WINDOWtls* tls);
void* _desktop_windowPlatformGetTls(_DESKTOP_WINDOWtls* tls);
void _desktop_windowPlatformSetTls(_DESKTOP_WINDOWtls* tls, void* value);

DESKTOP_WINDOWbool _desktop_windowPlatformCreateMutex(_DESKTOP_WINDOWmutex* mutex);
void _desktop_windowPlatformDestroyMutex(_DESKTOP_WINDOWmutex* mutex);
void _desktop_windowPlatformLockMutex(_DESKTOP_WINDOWmutex* mutex);
void _desktop_windowPlatformUnlockMutex(_DESKTOP_WINDOWmutex* mutex);

void* _desktop_windowPlatformLoadModule(const char* path);
void _desktop_windowPlatformFreeModule(void* module);
DESKTOP_WINDOWproc _desktop_windowPlatformGetModuleSymbol(void* module, const char* name);


//////////////////////////////////////////////////////////////////////////
//////                         DESKTOP_WINDOW event API                       //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowInputWindowFocus(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool focused);
void _desktop_windowInputWindowPos(_DESKTOP_WINDOWwindow* window, int xpos, int ypos);
void _desktop_windowInputWindowSize(_DESKTOP_WINDOWwindow* window, int width, int height);
void _desktop_windowInputFramebufferSize(_DESKTOP_WINDOWwindow* window, int width, int height);
void _desktop_windowInputWindowContentScale(_DESKTOP_WINDOWwindow* window,
                                  float xscale, float yscale);
void _desktop_windowInputWindowIconify(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool iconified);
void _desktop_windowInputWindowMaximize(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool maximized);
void _desktop_windowInputWindowDamage(_DESKTOP_WINDOWwindow* window);
void _desktop_windowInputWindowCloseRequest(_DESKTOP_WINDOWwindow* window);
void _desktop_windowInputWindowMonitor(_DESKTOP_WINDOWwindow* window, _DESKTOP_WINDOWmonitor* monitor);

void _desktop_windowInputKey(_DESKTOP_WINDOWwindow* window,
                   int key, int scancode, int action, int mods);
void _desktop_windowInputChar(_DESKTOP_WINDOWwindow* window,
                    uint32_t codepoint, int mods, DESKTOP_WINDOWbool plain);
void _desktop_windowInputScroll(_DESKTOP_WINDOWwindow* window, double xoffset, double yoffset);
void _desktop_windowInputMouseClick(_DESKTOP_WINDOWwindow* window, int button, int action, int mods);
void _desktop_windowInputCursorPos(_DESKTOP_WINDOWwindow* window, double xpos, double ypos);
void _desktop_windowInputCursorEnter(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool entered);
void _desktop_windowInputDrop(_DESKTOP_WINDOWwindow* window, int count, const char** names);
void _desktop_windowInputJoystick(_DESKTOP_WINDOWjoystick* js, int event);
void _desktop_windowInputJoystickAxis(_DESKTOP_WINDOWjoystick* js, int axis, float value);
void _desktop_windowInputJoystickButton(_DESKTOP_WINDOWjoystick* js, int button, char value);
void _desktop_windowInputJoystickHat(_DESKTOP_WINDOWjoystick* js, int hat, char value);

void _desktop_windowInputMonitor(_DESKTOP_WINDOWmonitor* monitor, int action, int placement);
void _desktop_windowInputMonitorWindow(_DESKTOP_WINDOWmonitor* monitor, _DESKTOP_WINDOWwindow* window);

#if defined(__GNUC__)
void _desktop_windowInputError(int code, const char* format, ...)
    __attribute__((format(printf, 2, 3)));
#else
void _desktop_windowInputError(int code, const char* format, ...);
#endif


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowSelectPlatform(int platformID, _DESKTOP_WINDOWplatform* platform);

DESKTOP_WINDOWbool _desktop_windowStringInExtensionString(const char* string, const char* extensions);
const _DESKTOP_WINDOWfbconfig* _desktop_windowChooseFBConfig(const _DESKTOP_WINDOWfbconfig* desired,
                                         const _DESKTOP_WINDOWfbconfig* alternatives,
                                         unsigned int count);
DESKTOP_WINDOWbool _desktop_windowRefreshContextAttribs(_DESKTOP_WINDOWwindow* window,
                                    const _DESKTOP_WINDOWctxconfig* ctxconfig);
DESKTOP_WINDOWbool _desktop_windowIsValidContextConfig(const _DESKTOP_WINDOWctxconfig* ctxconfig);

const DESKTOP_WINDOWvidmode* _desktop_windowChooseVideoMode(_DESKTOP_WINDOWmonitor* monitor,
                                        const DESKTOP_WINDOWvidmode* desired);
int _desktop_windowCompareVideoModes(const DESKTOP_WINDOWvidmode* first, const DESKTOP_WINDOWvidmode* second);
_DESKTOP_WINDOWmonitor* _desktop_windowAllocMonitor(const char* name, int widthMM, int heightMM);
void _desktop_windowFreeMonitor(_DESKTOP_WINDOWmonitor* monitor);
void _desktop_windowAllocGammaArrays(DESKTOP_WINDOWgammaramp* ramp, unsigned int size);
void _desktop_windowFreeGammaArrays(DESKTOP_WINDOWgammaramp* ramp);
void _desktop_windowSplitBPP(int bpp, int* red, int* green, int* blue);

void _desktop_windowInitGamepadMappings(void);
_DESKTOP_WINDOWjoystick* _desktop_windowAllocJoystick(const char* name,
                                  const char* guid,
                                  int axisCount,
                                  int buttonCount,
                                  int hatCount);
void _desktop_windowFreeJoystick(_DESKTOP_WINDOWjoystick* js);
void _desktop_windowCenterCursorInContentArea(_DESKTOP_WINDOWwindow* window);

DESKTOP_WINDOWbool _desktop_windowInitEGL(void);
void _desktop_windowTerminateEGL(void);
DESKTOP_WINDOWbool _desktop_windowCreateContextEGL(_DESKTOP_WINDOWwindow* window,
                               const _DESKTOP_WINDOWctxconfig* ctxconfig,
                               const _DESKTOP_WINDOWfbconfig* fbconfig);
#if defined(_DESKTOP_WINDOW_X11)
DESKTOP_WINDOWbool _desktop_windowChooseVisualEGL(const _DESKTOP_WINDOWwndconfig* wndconfig,
                              const _DESKTOP_WINDOWctxconfig* ctxconfig,
                              const _DESKTOP_WINDOWfbconfig* fbconfig,
                              Visual** visual, int* depth);
#endif /*_DESKTOP_WINDOW_X11*/

DESKTOP_WINDOWbool _desktop_windowInitOSMesa(void);
void _desktop_windowTerminateOSMesa(void);
DESKTOP_WINDOWbool _desktop_windowCreateContextOSMesa(_DESKTOP_WINDOWwindow* window,
                                  const _DESKTOP_WINDOWctxconfig* ctxconfig,
                                  const _DESKTOP_WINDOWfbconfig* fbconfig);

DESKTOP_WINDOWbool _desktop_windowInitVulkan(int mode);
void _desktop_windowTerminateVulkan(void);
const char* _desktop_windowGetVulkanResultString(VkResult result);

size_t _desktop_windowEncodeUTF8(char* s, uint32_t codepoint);
char** _desktop_windowParseUriList(char* text, int* count);

char* _desktop_window_strdup(const char* source);
int _desktop_window_min(int a, int b);
int _desktop_window_max(int a, int b);

void* _desktop_window_calloc(size_t count, size_t size);
void* _desktop_window_realloc(void* pointer, size_t size);
void _desktop_window_free(void* pointer);

