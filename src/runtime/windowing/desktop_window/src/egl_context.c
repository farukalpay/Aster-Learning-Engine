#include "internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


// Return a description of the specified EGL error
//
static const char* getEGLErrorString(EGLint error)
{
    switch (error)
    {
        case EGL_SUCCESS:
            return "Success";
        case EGL_NOT_INITIALIZED:
            return "EGL is not or could not be initialized";
        case EGL_BAD_ACCESS:
            return "EGL cannot access a requested resource";
        case EGL_BAD_ALLOC:
            return "EGL failed to allocate resources for the requested operation";
        case EGL_BAD_ATTRIBUTE:
            return "An unrecognized attribute or attribute value was passed in the attribute list";
        case EGL_BAD_CONTEXT:
            return "An EGLContext argument does not name a valid EGL rendering context";
        case EGL_BAD_CONFIG:
            return "An EGLConfig argument does not name a valid EGL frame buffer configuration";
        case EGL_BAD_CURRENT_SURFACE:
            return "The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid";
        case EGL_BAD_DISPLAY:
            return "An EGLDisplay argument does not name a valid EGL display connection";
        case EGL_BAD_SURFACE:
            return "An EGLSurface argument does not name a valid surface configured for GL rendering";
        case EGL_BAD_MATCH:
            return "Arguments are inconsistent";
        case EGL_BAD_PARAMETER:
            return "One or more argument values are invalid";
        case EGL_BAD_NATIVE_PIXMAP:
            return "A NativePixmapType argument does not refer to a valid native pixmap";
        case EGL_BAD_NATIVE_WINDOW:
            return "A NativeWindowType argument does not refer to a valid native window";
        case EGL_CONTEXT_LOST:
            return "The application must destroy all contexts and reinitialise";
        default:
            return "ERROR: UNKNOWN EGL ERROR";
    }
}

// Returns the specified attribute of the specified EGLConfig
//
static int getEGLConfigAttrib(EGLConfig config, int attrib)
{
    int value;
    eglGetConfigAttrib(_desktop_window.egl.display, config, attrib, &value);
    return value;
}

// Return the EGLConfig most closely matching the specified hints
//
static DESKTOP_WINDOWbool chooseEGLConfig(const _DESKTOP_WINDOWctxconfig* ctxconfig,
                                const _DESKTOP_WINDOWfbconfig* fbconfig,
                                EGLConfig* result)
{
    EGLConfig* nativeConfigs;
    _DESKTOP_WINDOWfbconfig* usableConfigs;
    const _DESKTOP_WINDOWfbconfig* closest;
    int i, nativeCount, usableCount, apiBit;
    DESKTOP_WINDOWbool wrongApiAvailable = DESKTOP_WINDOW_FALSE;

    if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_ES_API)
    {
        if (ctxconfig->major == 1)
            apiBit = EGL_OPENGL_ES_BIT;
        else
            apiBit = EGL_OPENGL_ES2_BIT;
    }
    else
        apiBit = EGL_OPENGL_BIT;

    if (fbconfig->stereo)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_FORMAT_UNAVAILABLE, "EGL: Stereo rendering not supported");
        return DESKTOP_WINDOW_FALSE;
    }

    eglGetConfigs(_desktop_window.egl.display, NULL, 0, &nativeCount);
    if (!nativeCount)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE, "EGL: No EGLConfigs returned");
        return DESKTOP_WINDOW_FALSE;
    }

    nativeConfigs = _desktop_window_calloc(nativeCount, sizeof(EGLConfig));
    eglGetConfigs(_desktop_window.egl.display, nativeConfigs, nativeCount, &nativeCount);

    usableConfigs = _desktop_window_calloc(nativeCount, sizeof(_DESKTOP_WINDOWfbconfig));
    usableCount = 0;

    for (i = 0;  i < nativeCount;  i++)
    {
        const EGLConfig n = nativeConfigs[i];
        _DESKTOP_WINDOWfbconfig* u = usableConfigs + usableCount;

        // Only consider RGB(A) EGLConfigs
        if (getEGLConfigAttrib(n, EGL_COLOR_BUFFER_TYPE) != EGL_RGB_BUFFER)
            continue;

        // Only consider window EGLConfigs
        if (!(getEGLConfigAttrib(n, EGL_SURFACE_TYPE) & EGL_WINDOW_BIT))
            continue;

#if defined(_DESKTOP_WINDOW_X11)
        if (_desktop_window.platform.platformID == DESKTOP_WINDOW_PLATFORM_X11)
        {
            XVisualInfo vi = {0};

            // Only consider EGLConfigs with associated Visuals
            vi.visualid = getEGLConfigAttrib(n, EGL_NATIVE_VISUAL_ID);
            if (!vi.visualid)
                continue;

            if (fbconfig->transparent)
            {
                int count;
                XVisualInfo* vis =
                    XGetVisualInfo(_desktop_window.x11.display, VisualIDMask, &vi, &count);
                if (vis)
                {
                    u->transparent = _desktop_windowIsVisualTransparentX11(vis[0].visual);
                    XFree(vis);
                }
            }
        }
#endif // _DESKTOP_WINDOW_X11

        if (!(getEGLConfigAttrib(n, EGL_RENDERABLE_TYPE) & apiBit))
        {
            wrongApiAvailable = DESKTOP_WINDOW_TRUE;
            continue;
        }

        u->redBits = getEGLConfigAttrib(n, EGL_RED_SIZE);
        u->greenBits = getEGLConfigAttrib(n, EGL_GREEN_SIZE);
        u->blueBits = getEGLConfigAttrib(n, EGL_BLUE_SIZE);

        u->alphaBits = getEGLConfigAttrib(n, EGL_ALPHA_SIZE);
        u->depthBits = getEGLConfigAttrib(n, EGL_DEPTH_SIZE);
        u->stencilBits = getEGLConfigAttrib(n, EGL_STENCIL_SIZE);

#if defined(_DESKTOP_WINDOW_WAYLAND)
        if (_desktop_window.platform.platformID == DESKTOP_WINDOW_PLATFORM_WAYLAND)
        {
            // NOTE: The wl_surface opaque region is no guarantee that its buffer
            //       is presented as opaque, if it also has an alpha channel
            // HACK: If EGL_EXT_present_opaque is unavailable, ignore any config
            //       with an alpha channel to ensure the buffer is opaque
            if (!_desktop_window.egl.EXT_present_opaque)
            {
                if (!fbconfig->transparent && u->alphaBits > 0)
                    continue;
            }
        }
#endif // _DESKTOP_WINDOW_WAYLAND

        u->samples = getEGLConfigAttrib(n, EGL_SAMPLES);
        u->doublebuffer = fbconfig->doublebuffer;

        u->handle = (uintptr_t) n;
        usableCount++;
    }

    closest = _desktop_windowChooseFBConfig(fbconfig, usableConfigs, usableCount);
    if (closest)
        *result = (EGLConfig) closest->handle;
    else
    {
        if (wrongApiAvailable)
        {
            if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_ES_API)
            {
                if (ctxconfig->major == 1)
                {
                    _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                                    "EGL: Failed to find support for OpenGL ES 1.x");
                }
                else
                {
                    _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                                    "EGL: Failed to find support for OpenGL ES 2 or later");
                }
            }
            else
            {
                _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                                "EGL: Failed to find support for OpenGL");
            }
        }
        else
        {
            _desktop_windowInputError(DESKTOP_WINDOW_FORMAT_UNAVAILABLE,
                            "EGL: Failed to find a suitable EGLConfig");
        }
    }

    _desktop_window_free(nativeConfigs);
    _desktop_window_free(usableConfigs);

    return closest != NULL;
}

static void makeContextCurrentEGL(_DESKTOP_WINDOWwindow* window)
{
    if (window)
    {
        if (!eglMakeCurrent(_desktop_window.egl.display,
                            window->context.egl.surface,
                            window->context.egl.surface,
                            window->context.egl.handle))
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "EGL: Failed to make context current: %s",
                            getEGLErrorString(eglGetError()));
            return;
        }
    }
    else
    {
        if (!eglMakeCurrent(_desktop_window.egl.display,
                            EGL_NO_SURFACE,
                            EGL_NO_SURFACE,
                            EGL_NO_CONTEXT))
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "EGL: Failed to clear current context: %s",
                            getEGLErrorString(eglGetError()));
            return;
        }
    }

    _desktop_windowPlatformSetTls(&_desktop_window.contextSlot, window);
}

static void swapBuffersEGL(_DESKTOP_WINDOWwindow* window)
{
    if (window != _desktop_windowPlatformGetTls(&_desktop_window.contextSlot))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "EGL: The context must be current on the calling thread when swapping buffers");
        return;
    }

#if defined(_DESKTOP_WINDOW_WAYLAND)
    if (_desktop_window.platform.platformID == DESKTOP_WINDOW_PLATFORM_WAYLAND)
    {
        // NOTE: Swapping buffers on a hidden window on Wayland makes it visible
        if (!window->wl.visible)
            return;
    }
#endif

    eglSwapBuffers(_desktop_window.egl.display, window->context.egl.surface);
}

static void swapIntervalEGL(int interval)
{
    eglSwapInterval(_desktop_window.egl.display, interval);
}

static int extensionSupportedEGL(const char* extension)
{
    const char* extensions = eglQueryString(_desktop_window.egl.display, EGL_EXTENSIONS);
    if (extensions)
    {
        if (_desktop_windowStringInExtensionString(extension, extensions))
            return DESKTOP_WINDOW_TRUE;
    }

    return DESKTOP_WINDOW_FALSE;
}

static DESKTOP_WINDOWglproc getProcAddressEGL(const char* procname)
{
    _DESKTOP_WINDOWwindow* window = _desktop_windowPlatformGetTls(&_desktop_window.contextSlot);
    assert(window != NULL);

    if (window->context.egl.client)
    {
        DESKTOP_WINDOWglproc proc = (DESKTOP_WINDOWglproc)
            _desktop_windowPlatformGetModuleSymbol(window->context.egl.client, procname);
        if (proc)
            return proc;
    }

    return eglGetProcAddress(procname);
}

static void destroyContextEGL(_DESKTOP_WINDOWwindow* window)
{
    // NOTE: Do not unload libGL.so.1 while the X11 display is still open,
    //       as it will make XCloseDisplay segfault
    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_X11 ||
        window->context.client != DESKTOP_WINDOW_OPENGL_API)
    {
        if (window->context.egl.client)
        {
            _desktop_windowPlatformFreeModule(window->context.egl.client);
            window->context.egl.client = NULL;
        }
    }

    if (window->context.egl.surface)
    {
        eglDestroySurface(_desktop_window.egl.display, window->context.egl.surface);
        window->context.egl.surface = EGL_NO_SURFACE;
    }

    if (window->context.egl.handle)
    {
        eglDestroyContext(_desktop_window.egl.display, window->context.egl.handle);
        window->context.egl.handle = EGL_NO_CONTEXT;
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Initialize EGL
//
DESKTOP_WINDOWbool _desktop_windowInitEGL(void)
{
    int i;
    EGLint* attribs = NULL;
    const char* extensions;
    const char* sonames[] =
    {
#if defined(_DESKTOP_WINDOW_EGL_LIBRARY)
        _DESKTOP_WINDOW_EGL_LIBRARY,
#elif defined(_DESKTOP_WINDOW_WIN32)
        "libEGL.dll",
        "EGL.dll",
#elif defined(_DESKTOP_WINDOW_COCOA)
        "libEGL.dylib",
#elif defined(__CYGWIN__)
        "libEGL-1.so",
#elif defined(__OpenBSD__) || defined(__NetBSD__)
        "libEGL.so",
#else
        "libEGL.so.1",
#endif
        NULL
    };

    if (_desktop_window.egl.handle)
        return DESKTOP_WINDOW_TRUE;

    for (i = 0;  sonames[i];  i++)
    {
        _desktop_window.egl.handle = _desktop_windowPlatformLoadModule(sonames[i]);
        if (_desktop_window.egl.handle)
            break;
    }

    if (!_desktop_window.egl.handle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE, "EGL: Library not found");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.egl.prefix = (strncmp(sonames[i], "lib", 3) == 0);

    _desktop_window.egl.GetConfigAttrib = (PFN_eglGetConfigAttrib)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglGetConfigAttrib");
    _desktop_window.egl.GetConfigs = (PFN_eglGetConfigs)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglGetConfigs");
    _desktop_window.egl.GetDisplay = (PFN_eglGetDisplay)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglGetDisplay");
    _desktop_window.egl.GetError = (PFN_eglGetError)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglGetError");
    _desktop_window.egl.Initialize = (PFN_eglInitialize)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglInitialize");
    _desktop_window.egl.Terminate = (PFN_eglTerminate)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglTerminate");
    _desktop_window.egl.BindAPI = (PFN_eglBindAPI)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglBindAPI");
    _desktop_window.egl.CreateContext = (PFN_eglCreateContext)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglCreateContext");
    _desktop_window.egl.DestroySurface = (PFN_eglDestroySurface)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglDestroySurface");
    _desktop_window.egl.DestroyContext = (PFN_eglDestroyContext)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglDestroyContext");
    _desktop_window.egl.CreateWindowSurface = (PFN_eglCreateWindowSurface)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglCreateWindowSurface");
    _desktop_window.egl.MakeCurrent = (PFN_eglMakeCurrent)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglMakeCurrent");
    _desktop_window.egl.SwapBuffers = (PFN_eglSwapBuffers)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglSwapBuffers");
    _desktop_window.egl.SwapInterval = (PFN_eglSwapInterval)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglSwapInterval");
    _desktop_window.egl.QueryString = (PFN_eglQueryString)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglQueryString");
    _desktop_window.egl.GetProcAddress = (PFN_eglGetProcAddress)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.egl.handle, "eglGetProcAddress");

    if (!_desktop_window.egl.GetConfigAttrib ||
        !_desktop_window.egl.GetConfigs ||
        !_desktop_window.egl.GetDisplay ||
        !_desktop_window.egl.GetError ||
        !_desktop_window.egl.Initialize ||
        !_desktop_window.egl.Terminate ||
        !_desktop_window.egl.BindAPI ||
        !_desktop_window.egl.CreateContext ||
        !_desktop_window.egl.DestroySurface ||
        !_desktop_window.egl.DestroyContext ||
        !_desktop_window.egl.CreateWindowSurface ||
        !_desktop_window.egl.MakeCurrent ||
        !_desktop_window.egl.SwapBuffers ||
        !_desktop_window.egl.SwapInterval ||
        !_desktop_window.egl.QueryString ||
        !_desktop_window.egl.GetProcAddress)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "EGL: Failed to load required entry points");

        _desktop_windowTerminateEGL();
        return DESKTOP_WINDOW_FALSE;
    }

    extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (extensions && eglGetError() == EGL_SUCCESS)
        _desktop_window.egl.EXT_client_extensions = DESKTOP_WINDOW_TRUE;

    if (_desktop_window.egl.EXT_client_extensions)
    {
        _desktop_window.egl.EXT_platform_base =
            _desktop_windowStringInExtensionString("EGL_EXT_platform_base", extensions);
        _desktop_window.egl.EXT_platform_x11 =
            _desktop_windowStringInExtensionString("EGL_EXT_platform_x11", extensions);
        _desktop_window.egl.EXT_platform_wayland =
            _desktop_windowStringInExtensionString("EGL_EXT_platform_wayland", extensions);
        _desktop_window.egl.ANGLE_platform_angle =
            _desktop_windowStringInExtensionString("EGL_ANGLE_platform_angle", extensions);
        _desktop_window.egl.ANGLE_platform_angle_opengl =
            _desktop_windowStringInExtensionString("EGL_ANGLE_platform_angle_opengl", extensions);
        _desktop_window.egl.ANGLE_platform_angle_d3d =
            _desktop_windowStringInExtensionString("EGL_ANGLE_platform_angle_d3d", extensions);
        _desktop_window.egl.ANGLE_platform_angle_vulkan =
            _desktop_windowStringInExtensionString("EGL_ANGLE_platform_angle_vulkan", extensions);
        _desktop_window.egl.ANGLE_platform_angle_metal =
            _desktop_windowStringInExtensionString("EGL_ANGLE_platform_angle_metal", extensions);
    }

    if (_desktop_window.egl.EXT_platform_base)
    {
        _desktop_window.egl.GetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
            eglGetProcAddress("eglGetPlatformDisplayEXT");
        _desktop_window.egl.CreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)
            eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
    }

    _desktop_window.egl.platform = _desktop_window.platform.getEGLPlatform(&attribs);
    if (_desktop_window.egl.platform)
    {
        _desktop_window.egl.display =
            eglGetPlatformDisplayEXT(_desktop_window.egl.platform,
                                     _desktop_window.platform.getEGLNativeDisplay(),
                                     attribs);
    }
    else
        _desktop_window.egl.display = eglGetDisplay(_desktop_window.platform.getEGLNativeDisplay());

    _desktop_window_free(attribs);

    if (_desktop_window.egl.display == EGL_NO_DISPLAY)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "EGL: Failed to get EGL display: %s",
                        getEGLErrorString(eglGetError()));

        _desktop_windowTerminateEGL();
        return DESKTOP_WINDOW_FALSE;
    }

    if (!eglInitialize(_desktop_window.egl.display, &_desktop_window.egl.major, &_desktop_window.egl.minor))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "EGL: Failed to initialize EGL: %s",
                        getEGLErrorString(eglGetError()));

        _desktop_windowTerminateEGL();
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.egl.KHR_create_context =
        extensionSupportedEGL("EGL_KHR_create_context");
    _desktop_window.egl.KHR_create_context_no_error =
        extensionSupportedEGL("EGL_KHR_create_context_no_error");
    _desktop_window.egl.KHR_gl_colorspace =
        extensionSupportedEGL("EGL_KHR_gl_colorspace");
    _desktop_window.egl.KHR_get_all_proc_addresses =
        extensionSupportedEGL("EGL_KHR_get_all_proc_addresses");
    _desktop_window.egl.KHR_context_flush_control =
        extensionSupportedEGL("EGL_KHR_context_flush_control");
    _desktop_window.egl.EXT_present_opaque =
        extensionSupportedEGL("EGL_EXT_present_opaque");

    return DESKTOP_WINDOW_TRUE;
}

// Terminate EGL
//
void _desktop_windowTerminateEGL(void)
{
    if (_desktop_window.egl.display)
    {
        eglTerminate(_desktop_window.egl.display);
        _desktop_window.egl.display = EGL_NO_DISPLAY;
    }

    if (_desktop_window.egl.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.egl.handle);
        _desktop_window.egl.handle = NULL;
    }
}

#define SET_ATTRIB(a, v) \
{ \
    assert(((size_t) index + 1) < sizeof(attribs) / sizeof(attribs[0])); \
    attribs[index++] = a; \
    attribs[index++] = v; \
}

// Create the OpenGL or OpenGL ES context
//
DESKTOP_WINDOWbool _desktop_windowCreateContextEGL(_DESKTOP_WINDOWwindow* window,
                               const _DESKTOP_WINDOWctxconfig* ctxconfig,
                               const _DESKTOP_WINDOWfbconfig* fbconfig)
{
    EGLint attribs[40];
    EGLConfig config;
    EGLContext share = NULL;
    EGLNativeWindowType native;
    int index = 0;

    if (!_desktop_window.egl.display)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE, "EGL: API not available");
        return DESKTOP_WINDOW_FALSE;
    }

    if (ctxconfig->share)
        share = ctxconfig->share->context.egl.handle;

    if (!chooseEGLConfig(ctxconfig, fbconfig, &config))
        return DESKTOP_WINDOW_FALSE;

    if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_ES_API)
    {
        if (!eglBindAPI(EGL_OPENGL_ES_API))
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "EGL: Failed to bind OpenGL ES: %s",
                            getEGLErrorString(eglGetError()));
            return DESKTOP_WINDOW_FALSE;
        }
    }
    else
    {
        if (!eglBindAPI(EGL_OPENGL_API))
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "EGL: Failed to bind OpenGL: %s",
                            getEGLErrorString(eglGetError()));
            return DESKTOP_WINDOW_FALSE;
        }
    }

    if (_desktop_window.egl.KHR_create_context)
    {
        int mask = 0, flags = 0;

        if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_API)
        {
            if (ctxconfig->forward)
                flags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;

            if (ctxconfig->profile == DESKTOP_WINDOW_OPENGL_CORE_PROFILE)
                mask |= EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
            else if (ctxconfig->profile == DESKTOP_WINDOW_OPENGL_COMPAT_PROFILE)
                mask |= EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR;
        }

        if (ctxconfig->debug)
            flags |= EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;

        if (ctxconfig->robustness)
        {
            if (ctxconfig->robustness == DESKTOP_WINDOW_NO_RESET_NOTIFICATION)
            {
                SET_ATTRIB(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR,
                           EGL_NO_RESET_NOTIFICATION_KHR);
            }
            else if (ctxconfig->robustness == DESKTOP_WINDOW_LOSE_CONTEXT_ON_RESET)
            {
                SET_ATTRIB(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR,
                           EGL_LOSE_CONTEXT_ON_RESET_KHR);
            }

            flags |= EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR;
        }

        if (ctxconfig->major != 1 || ctxconfig->minor != 0)
        {
            SET_ATTRIB(EGL_CONTEXT_MAJOR_VERSION_KHR, ctxconfig->major);
            SET_ATTRIB(EGL_CONTEXT_MINOR_VERSION_KHR, ctxconfig->minor);
        }

        if (ctxconfig->noerror)
        {
            if (_desktop_window.egl.KHR_create_context_no_error)
                SET_ATTRIB(EGL_CONTEXT_OPENGL_NO_ERROR_KHR, DESKTOP_WINDOW_TRUE);
        }

        if (mask)
            SET_ATTRIB(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, mask);

        if (flags)
            SET_ATTRIB(EGL_CONTEXT_FLAGS_KHR, flags);
    }
    else
    {
        if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_ES_API)
            SET_ATTRIB(EGL_CONTEXT_CLIENT_VERSION, ctxconfig->major);
    }

    if (_desktop_window.egl.KHR_context_flush_control)
    {
        if (ctxconfig->release == DESKTOP_WINDOW_RELEASE_BEHAVIOR_NONE)
        {
            SET_ATTRIB(EGL_CONTEXT_RELEASE_BEHAVIOR_KHR,
                       EGL_CONTEXT_RELEASE_BEHAVIOR_NONE_KHR);
        }
        else if (ctxconfig->release == DESKTOP_WINDOW_RELEASE_BEHAVIOR_FLUSH)
        {
            SET_ATTRIB(EGL_CONTEXT_RELEASE_BEHAVIOR_KHR,
                       EGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_KHR);
        }
    }

    SET_ATTRIB(EGL_NONE, EGL_NONE);

    window->context.egl.handle = eglCreateContext(_desktop_window.egl.display,
                                                  config, share, attribs);

    if (window->context.egl.handle == EGL_NO_CONTEXT)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                        "EGL: Failed to create context: %s",
                        getEGLErrorString(eglGetError()));
        return DESKTOP_WINDOW_FALSE;
    }

    // Set up attributes for surface creation
    index = 0;

    if (fbconfig->sRGB)
    {
        if (_desktop_window.egl.KHR_gl_colorspace)
            SET_ATTRIB(EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR);
    }

    if (!fbconfig->doublebuffer)
        SET_ATTRIB(EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER);

    if (_desktop_window.platform.platformID == DESKTOP_WINDOW_PLATFORM_WAYLAND)
    {
        if (_desktop_window.egl.EXT_present_opaque)
            SET_ATTRIB(EGL_PRESENT_OPAQUE_EXT, !fbconfig->transparent);
    }

    SET_ATTRIB(EGL_NONE, EGL_NONE);

    native = _desktop_window.platform.getEGLNativeWindow(window);
    // HACK: ANGLE does not implement eglCreatePlatformWindowSurfaceEXT
    //       despite reporting EGL_EXT_platform_base
    if (_desktop_window.egl.platform && _desktop_window.egl.platform != EGL_PLATFORM_ANGLE_ANGLE)
    {
        window->context.egl.surface =
            eglCreatePlatformWindowSurfaceEXT(_desktop_window.egl.display, config, native, attribs);
    }
    else
    {
        window->context.egl.surface =
            eglCreateWindowSurface(_desktop_window.egl.display, config, native, attribs);
    }

    if (window->context.egl.surface == EGL_NO_SURFACE)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "EGL: Failed to create window surface: %s",
                        getEGLErrorString(eglGetError()));
        return DESKTOP_WINDOW_FALSE;
    }

    window->context.egl.config = config;

    // Load the appropriate client library
    if (!_desktop_window.egl.KHR_get_all_proc_addresses)
    {
        int i;
        const char** sonames;
        const char* es1sonames[] =
        {
#if defined(_DESKTOP_WINDOW_GLESV1_LIBRARY)
            _DESKTOP_WINDOW_GLESV1_LIBRARY,
#elif defined(_DESKTOP_WINDOW_WIN32)
            "GLESv1_CM.dll",
            "libGLES_CM.dll",
#elif defined(_DESKTOP_WINDOW_COCOA)
            "libGLESv1_CM.dylib",
#elif defined(__OpenBSD__) || defined(__NetBSD__)
            "libGLESv1_CM.so",
#else
            "libGLESv1_CM.so.1",
            "libGLES_CM.so.1",
#endif
            NULL
        };
        const char* es2sonames[] =
        {
#if defined(_DESKTOP_WINDOW_GLESV2_LIBRARY)
            _DESKTOP_WINDOW_GLESV2_LIBRARY,
#elif defined(_DESKTOP_WINDOW_WIN32)
            "GLESv2.dll",
            "libGLESv2.dll",
#elif defined(_DESKTOP_WINDOW_COCOA)
            "libGLESv2.dylib",
#elif defined(__CYGWIN__)
            "libGLESv2-2.so",
#elif defined(__OpenBSD__) || defined(__NetBSD__)
            "libGLESv2.so",
#else
            "libGLESv2.so.2",
#endif
            NULL
        };
        const char* glsonames[] =
        {
#if defined(_DESKTOP_WINDOW_OPENGL_LIBRARY)
            _DESKTOP_WINDOW_OPENGL_LIBRARY,
#elif defined(_DESKTOP_WINDOW_WIN32)
#elif defined(_DESKTOP_WINDOW_COCOA)
#elif defined(__OpenBSD__) || defined(__NetBSD__)
            "libGL.so",
#else
            "libOpenGL.so.0",
            "libGL.so.1",
#endif
            NULL
        };

        if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_ES_API)
        {
            if (ctxconfig->major == 1)
                sonames = es1sonames;
            else
                sonames = es2sonames;
        }
        else
            sonames = glsonames;

        for (i = 0;  sonames[i];  i++)
        {
            // HACK: Match presence of lib prefix to increase chance of finding
            //       a matching pair in the jungle that is Win32 EGL/GLES
            if (_desktop_window.egl.prefix != (strncmp(sonames[i], "lib", 3) == 0))
                continue;

            window->context.egl.client = _desktop_windowPlatformLoadModule(sonames[i]);
            if (window->context.egl.client)
                break;
        }

        if (!window->context.egl.client)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "EGL: Failed to load client library");
            return DESKTOP_WINDOW_FALSE;
        }
    }

    window->context.makeCurrent = makeContextCurrentEGL;
    window->context.swapBuffers = swapBuffersEGL;
    window->context.swapInterval = swapIntervalEGL;
    window->context.extensionSupported = extensionSupportedEGL;
    window->context.getProcAddress = getProcAddressEGL;
    window->context.destroy = destroyContextEGL;

    return DESKTOP_WINDOW_TRUE;
}

#undef SET_ATTRIB

// Returns the Visual and depth of the chosen EGLConfig
//
#if defined(_DESKTOP_WINDOW_X11)
DESKTOP_WINDOWbool _desktop_windowChooseVisualEGL(const _DESKTOP_WINDOWwndconfig* wndconfig,
                              const _DESKTOP_WINDOWctxconfig* ctxconfig,
                              const _DESKTOP_WINDOWfbconfig* fbconfig,
                              Visual** visual, int* depth)
{
    XVisualInfo* result;
    XVisualInfo desired;
    EGLConfig native;
    EGLint visualID = 0, count = 0;
    const long vimask = VisualScreenMask | VisualIDMask;

    if (!chooseEGLConfig(ctxconfig, fbconfig, &native))
        return DESKTOP_WINDOW_FALSE;

    eglGetConfigAttrib(_desktop_window.egl.display, native,
                       EGL_NATIVE_VISUAL_ID, &visualID);

    desired.screen = _desktop_window.x11.screen;
    desired.visualid = visualID;

    result = XGetVisualInfo(_desktop_window.x11.display, vimask, &desired, &count);
    if (!result)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "EGL: Failed to retrieve Visual for EGLConfig");
        return DESKTOP_WINDOW_FALSE;
    }

    *visual = result->visual;
    *depth = result->depth;

    XFree(result);
    return DESKTOP_WINDOW_TRUE;
}
#endif // _DESKTOP_WINDOW_X11


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW native API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI EGLDisplay desktop_windowGetEGLDisplay(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(EGL_NO_DISPLAY);
    return _desktop_window.egl.display;
}

DESKTOP_WINDOWAPI EGLContext desktop_windowGetEGLContext(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(EGL_NO_CONTEXT);

    if (window->context.source != DESKTOP_WINDOW_EGL_CONTEXT_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT, NULL);
        return EGL_NO_CONTEXT;
    }

    return window->context.egl.handle;
}

DESKTOP_WINDOWAPI EGLSurface desktop_windowGetEGLSurface(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(EGL_NO_SURFACE);

    if (window->context.source != DESKTOP_WINDOW_EGL_CONTEXT_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT, NULL);
        return EGL_NO_SURFACE;
    }

    return window->context.egl.surface;
}

