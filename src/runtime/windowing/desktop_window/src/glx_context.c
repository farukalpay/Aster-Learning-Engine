#include "internal.h"

#if defined(_DESKTOP_WINDOW_X11)

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifndef GLXBadProfileARB
 #define GLXBadProfileARB 13
#endif


// Returns the specified attribute of the specified GLXFBConfig
//
static int getGLXFBConfigAttrib(GLXFBConfig fbconfig, int attrib)
{
    int value;
    glXGetFBConfigAttrib(_desktop_window.x11.display, fbconfig, attrib, &value);
    return value;
}

// Return the GLXFBConfig most closely matching the specified hints
//
static DESKTOP_WINDOWbool chooseGLXFBConfig(const _DESKTOP_WINDOWfbconfig* desired,
                                  GLXFBConfig* result)
{
    GLXFBConfig* nativeConfigs;
    _DESKTOP_WINDOWfbconfig* usableConfigs;
    const _DESKTOP_WINDOWfbconfig* closest;
    int nativeCount, usableCount;
    const char* vendor;
    DESKTOP_WINDOWbool trustWindowBit = DESKTOP_WINDOW_TRUE;

    // HACK: This is a (hopefully temporary) workaround for Chromium
    //       (VirtualBox GL) not setting the window bit on any GLXFBConfigs
    vendor = glXGetClientString(_desktop_window.x11.display, GLX_VENDOR);
    if (vendor && strcmp(vendor, "Chromium") == 0)
        trustWindowBit = DESKTOP_WINDOW_FALSE;

    nativeConfigs =
        glXGetFBConfigs(_desktop_window.x11.display, _desktop_window.x11.screen, &nativeCount);
    if (!nativeConfigs || !nativeCount)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE, "GLX: No GLXFBConfigs returned");
        return DESKTOP_WINDOW_FALSE;
    }

    usableConfigs = _desktop_window_calloc(nativeCount, sizeof(_DESKTOP_WINDOWfbconfig));
    usableCount = 0;

    for (int i = 0;  i < nativeCount;  i++)
    {
        const GLXFBConfig n = nativeConfigs[i];
        _DESKTOP_WINDOWfbconfig* u = usableConfigs + usableCount;

        // Only consider RGBA GLXFBConfigs
        if (!(getGLXFBConfigAttrib(n, GLX_RENDER_TYPE) & GLX_RGBA_BIT))
            continue;

        // Only consider window GLXFBConfigs
        if (!(getGLXFBConfigAttrib(n, GLX_DRAWABLE_TYPE) & GLX_WINDOW_BIT))
        {
            if (trustWindowBit)
                continue;
        }

        if (getGLXFBConfigAttrib(n, GLX_DOUBLEBUFFER) != desired->doublebuffer)
            continue;

        if (desired->transparent)
        {
            XVisualInfo* vi = glXGetVisualFromFBConfig(_desktop_window.x11.display, n);
            if (vi)
            {
                u->transparent = _desktop_windowIsVisualTransparentX11(vi->visual);
                XFree(vi);
            }
        }

        u->redBits = getGLXFBConfigAttrib(n, GLX_RED_SIZE);
        u->greenBits = getGLXFBConfigAttrib(n, GLX_GREEN_SIZE);
        u->blueBits = getGLXFBConfigAttrib(n, GLX_BLUE_SIZE);

        u->alphaBits = getGLXFBConfigAttrib(n, GLX_ALPHA_SIZE);
        u->depthBits = getGLXFBConfigAttrib(n, GLX_DEPTH_SIZE);
        u->stencilBits = getGLXFBConfigAttrib(n, GLX_STENCIL_SIZE);

        u->accumRedBits = getGLXFBConfigAttrib(n, GLX_ACCUM_RED_SIZE);
        u->accumGreenBits = getGLXFBConfigAttrib(n, GLX_ACCUM_GREEN_SIZE);
        u->accumBlueBits = getGLXFBConfigAttrib(n, GLX_ACCUM_BLUE_SIZE);
        u->accumAlphaBits = getGLXFBConfigAttrib(n, GLX_ACCUM_ALPHA_SIZE);

        u->auxBuffers = getGLXFBConfigAttrib(n, GLX_AUX_BUFFERS);

        if (getGLXFBConfigAttrib(n, GLX_STEREO))
            u->stereo = DESKTOP_WINDOW_TRUE;

        if (_desktop_window.glx.ARB_multisample)
            u->samples = getGLXFBConfigAttrib(n, GLX_SAMPLES);

        if (_desktop_window.glx.ARB_framebuffer_sRGB || _desktop_window.glx.EXT_framebuffer_sRGB)
            u->sRGB = getGLXFBConfigAttrib(n, GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB);

        u->handle = (uintptr_t) n;
        usableCount++;
    }

    closest = _desktop_windowChooseFBConfig(desired, usableConfigs, usableCount);
    if (closest)
        *result = (GLXFBConfig) closest->handle;

    XFree(nativeConfigs);
    _desktop_window_free(usableConfigs);

    return closest != NULL;
}

// Create the DesktopGraphics context using legacy API
//
static GLXContext createLegacyContextGLX(_DESKTOP_WINDOWwindow* window,
                                         GLXFBConfig fbconfig,
                                         GLXContext share)
{
    return glXCreateNewContext(_desktop_window.x11.display,
                               fbconfig,
                               GLX_RGBA_TYPE,
                               share,
                               True);
}

static void makeContextCurrentGLX(_DESKTOP_WINDOWwindow* window)
{
    if (window)
    {
        if (!glXMakeCurrent(_desktop_window.x11.display,
                            window->context.glx.window,
                            window->context.glx.handle))
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "GLX: Failed to make context current");
            return;
        }
    }
    else
    {
        if (!glXMakeCurrent(_desktop_window.x11.display, None, NULL))
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "GLX: Failed to clear current context");
            return;
        }
    }

    _desktop_windowPlatformSetTls(&_desktop_window.contextSlot, window);
}

static void swapBuffersGLX(_DESKTOP_WINDOWwindow* window)
{
    glXSwapBuffers(_desktop_window.x11.display, window->context.glx.window);
}

static void swapIntervalGLX(int interval)
{
    _DESKTOP_WINDOWwindow* window = _desktop_windowPlatformGetTls(&_desktop_window.contextSlot);
    assert(window != NULL);

    if (_desktop_window.glx.EXT_swap_control)
    {
        _desktop_window.glx.SwapIntervalEXT(_desktop_window.x11.display,
                                  window->context.glx.window,
                                  interval);
    }
    else if (_desktop_window.glx.MESA_swap_control)
        _desktop_window.glx.SwapIntervalMESA(interval);
    else if (_desktop_window.glx.SGI_swap_control)
    {
        if (interval > 0)
            _desktop_window.glx.SwapIntervalSGI(interval);
    }
}

static int extensionSupportedGLX(const char* extension)
{
    const char* extensions =
        glXQueryExtensionsString(_desktop_window.x11.display, _desktop_window.x11.screen);
    if (extensions)
    {
        if (_desktop_windowStringInExtensionString(extension, extensions))
            return DESKTOP_WINDOW_TRUE;
    }

    return DESKTOP_WINDOW_FALSE;
}

static DESKTOP_WINDOWglproc getProcAddressGLX(const char* procname)
{
    if (_desktop_window.glx.GetProcAddress)
        return _desktop_window.glx.GetProcAddress((const GLubyte*) procname);
    else if (_desktop_window.glx.GetProcAddressARB)
        return _desktop_window.glx.GetProcAddressARB((const GLubyte*) procname);
    else
    {
        // NOTE: glvnd provides GLX 1.4, so this can only happen with libGL
        return _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, procname);
    }
}

static void destroyContextGLX(_DESKTOP_WINDOWwindow* window)
{
    if (window->context.glx.window)
    {
        glXDestroyWindow(_desktop_window.x11.display, window->context.glx.window);
        window->context.glx.window = None;
    }

    if (window->context.glx.handle)
    {
        glXDestroyContext(_desktop_window.x11.display, window->context.glx.handle);
        window->context.glx.handle = NULL;
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Initialize GLX
//
DESKTOP_WINDOWbool _desktop_windowInitGLX(void)
{
    const char* sonames[] =
    {
#if defined(_DESKTOP_WINDOW_GLX_LIBRARY)
        _DESKTOP_WINDOW_GLX_LIBRARY,
#elif defined(__CYGWIN__)
        "libGL-1.so",
#elif defined(__OpenBSD__) || defined(__NetBSD__)
        "libGL.so",
#else
        "libGLX.so.0",
        "libGL.so.1",
        "libGL.so",
#endif
        NULL
    };

    if (_desktop_window.glx.handle)
        return DESKTOP_WINDOW_TRUE;

    for (int i = 0;  sonames[i];  i++)
    {
        _desktop_window.glx.handle = _desktop_windowPlatformLoadModule(sonames[i]);
        if (_desktop_window.glx.handle)
            break;
    }

    if (!_desktop_window.glx.handle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE, "GLX: Failed to load GLX");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.glx.GetFBConfigs = (PFNGLXGETFBCONFIGSPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXGetFBConfigs");
    _desktop_window.glx.GetFBConfigAttrib = (PFNGLXGETFBCONFIGATTRIBPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXGetFBConfigAttrib");
    _desktop_window.glx.GetClientString = (PFNGLXGETCLIENTSTRINGPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXGetClientString");
    _desktop_window.glx.QueryExtension = (PFNGLXQUERYEXTENSIONPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXQueryExtension");
    _desktop_window.glx.QueryVersion = (PFNGLXQUERYVERSIONPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXQueryVersion");
    _desktop_window.glx.DestroyContext = (PFNGLXDESTROYCONTEXTPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXDestroyContext");
    _desktop_window.glx.MakeCurrent = (PFNGLXMAKECURRENTPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXMakeCurrent");
    _desktop_window.glx.SwapBuffers = (PFNGLXSWAPBUFFERSPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXSwapBuffers");
    _desktop_window.glx.QueryExtensionsString = (PFNGLXQUERYEXTENSIONSSTRINGPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXQueryExtensionsString");
    _desktop_window.glx.CreateNewContext = (PFNGLXCREATENEWCONTEXTPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXCreateNewContext");
    _desktop_window.glx.CreateWindow = (PFNGLXCREATEWINDOWPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXCreateWindow");
    _desktop_window.glx.DestroyWindow = (PFNGLXDESTROYWINDOWPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXDestroyWindow");
    _desktop_window.glx.GetVisualFromFBConfig = (PFNGLXGETVISUALFROMFBCONFIGPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXGetVisualFromFBConfig");

    if (!_desktop_window.glx.GetFBConfigs ||
        !_desktop_window.glx.GetFBConfigAttrib ||
        !_desktop_window.glx.GetClientString ||
        !_desktop_window.glx.QueryExtension ||
        !_desktop_window.glx.QueryVersion ||
        !_desktop_window.glx.DestroyContext ||
        !_desktop_window.glx.MakeCurrent ||
        !_desktop_window.glx.SwapBuffers ||
        !_desktop_window.glx.QueryExtensionsString ||
        !_desktop_window.glx.CreateNewContext ||
        !_desktop_window.glx.CreateWindow ||
        !_desktop_window.glx.DestroyWindow ||
        !_desktop_window.glx.GetVisualFromFBConfig)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "GLX: Failed to load required entry points");
        return DESKTOP_WINDOW_FALSE;
    }

    // NOTE: Unlike GLX 1.3 entry points these are not required to be present
    _desktop_window.glx.GetProcAddress = (PFNGLXGETPROCADDRESSPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXGetProcAddress");
    _desktop_window.glx.GetProcAddressARB = (PFNGLXGETPROCADDRESSPROC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.glx.handle, "glXGetProcAddressARB");

    if (!glXQueryExtension(_desktop_window.x11.display,
                           &_desktop_window.glx.errorBase,
                           &_desktop_window.glx.eventBase))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE, "GLX: GLX extension not found");
        return DESKTOP_WINDOW_FALSE;
    }

    if (!glXQueryVersion(_desktop_window.x11.display, &_desktop_window.glx.major, &_desktop_window.glx.minor))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "GLX: Failed to query GLX version");
        return DESKTOP_WINDOW_FALSE;
    }

    if (_desktop_window.glx.major == 1 && _desktop_window.glx.minor < 3)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "GLX: GLX version 1.3 is required");
        return DESKTOP_WINDOW_FALSE;
    }

    if (extensionSupportedGLX("GLX_EXT_swap_control"))
    {
        _desktop_window.glx.SwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)
            getProcAddressGLX("glXSwapIntervalEXT");

        if (_desktop_window.glx.SwapIntervalEXT)
            _desktop_window.glx.EXT_swap_control = DESKTOP_WINDOW_TRUE;
    }

    if (extensionSupportedGLX("GLX_SGI_swap_control"))
    {
        _desktop_window.glx.SwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)
            getProcAddressGLX("glXSwapIntervalSGI");

        if (_desktop_window.glx.SwapIntervalSGI)
            _desktop_window.glx.SGI_swap_control = DESKTOP_WINDOW_TRUE;
    }

    if (extensionSupportedGLX("GLX_MESA_swap_control"))
    {
        _desktop_window.glx.SwapIntervalMESA = (PFNGLXSWAPINTERVALMESAPROC)
            getProcAddressGLX("glXSwapIntervalMESA");

        if (_desktop_window.glx.SwapIntervalMESA)
            _desktop_window.glx.MESA_swap_control = DESKTOP_WINDOW_TRUE;
    }

    if (extensionSupportedGLX("GLX_ARB_multisample"))
        _desktop_window.glx.ARB_multisample = DESKTOP_WINDOW_TRUE;

    if (extensionSupportedGLX("GLX_ARB_framebuffer_sRGB"))
        _desktop_window.glx.ARB_framebuffer_sRGB = DESKTOP_WINDOW_TRUE;

    if (extensionSupportedGLX("GLX_EXT_framebuffer_sRGB"))
        _desktop_window.glx.EXT_framebuffer_sRGB = DESKTOP_WINDOW_TRUE;

    if (extensionSupportedGLX("GLX_ARB_create_context"))
    {
        _desktop_window.glx.CreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)
            getProcAddressGLX("glXCreateContextAttribsARB");

        if (_desktop_window.glx.CreateContextAttribsARB)
            _desktop_window.glx.ARB_create_context = DESKTOP_WINDOW_TRUE;
    }

    if (extensionSupportedGLX("GLX_ARB_create_context_robustness"))
        _desktop_window.glx.ARB_create_context_robustness = DESKTOP_WINDOW_TRUE;

    if (extensionSupportedGLX("GLX_ARB_create_context_profile"))
        _desktop_window.glx.ARB_create_context_profile = DESKTOP_WINDOW_TRUE;

    if (extensionSupportedGLX("GLX_EXT_create_context_es2_profile"))
        _desktop_window.glx.EXT_create_context_es2_profile = DESKTOP_WINDOW_TRUE;

    if (extensionSupportedGLX("GLX_ARB_create_context_no_error"))
        _desktop_window.glx.ARB_create_context_no_error = DESKTOP_WINDOW_TRUE;

    if (extensionSupportedGLX("GLX_ARB_context_flush_control"))
        _desktop_window.glx.ARB_context_flush_control = DESKTOP_WINDOW_TRUE;

    return DESKTOP_WINDOW_TRUE;
}

// Terminate GLX
//
void _desktop_windowTerminateGLX(void)
{
    // NOTE: This function must not call any X11 functions, as it is called
    //       after XCloseDisplay (see _desktop_windowTerminateX11 for details)

    if (_desktop_window.glx.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.glx.handle);
        _desktop_window.glx.handle = NULL;
    }
}

#define SET_ATTRIB(a, v) \
{ \
    assert(((size_t) index + 1) < sizeof(attribs) / sizeof(attribs[0])); \
    attribs[index++] = a; \
    attribs[index++] = v; \
}

// Create the DesktopGraphics or DesktopGraphics ES context
//
DESKTOP_WINDOWbool _desktop_windowCreateContextGLX(_DESKTOP_WINDOWwindow* window,
                               const _DESKTOP_WINDOWctxconfig* ctxconfig,
                               const _DESKTOP_WINDOWfbconfig* fbconfig)
{
    int attribs[40];
    GLXFBConfig native = NULL;
    GLXContext share = NULL;

    if (ctxconfig->share)
        share = ctxconfig->share->context.glx.handle;

    if (!chooseGLXFBConfig(fbconfig, &native))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_FORMAT_UNAVAILABLE,
                        "GLX: Failed to find a suitable GLXFBConfig");
        return DESKTOP_WINDOW_FALSE;
    }

    if (ctxconfig->client == DESKTOP_WINDOW_DESKTOP_GRAPHICS_ES_API)
    {
        if (!_desktop_window.glx.ARB_create_context ||
            !_desktop_window.glx.ARB_create_context_profile ||
            !_desktop_window.glx.EXT_create_context_es2_profile)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "GLX: DesktopGraphics ES requested but GLX_EXT_create_context_es2_profile is unavailable");
            return DESKTOP_WINDOW_FALSE;
        }
    }

    if (ctxconfig->forward)
    {
        if (!_desktop_window.glx.ARB_create_context)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                            "GLX: Forward compatibility requested but GLX_ARB_create_context_profile is unavailable");
            return DESKTOP_WINDOW_FALSE;
        }
    }

    if (ctxconfig->profile)
    {
        if (!_desktop_window.glx.ARB_create_context ||
            !_desktop_window.glx.ARB_create_context_profile)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                            "GLX: An DesktopGraphics profile requested but GLX_ARB_create_context_profile is unavailable");
            return DESKTOP_WINDOW_FALSE;
        }
    }

    _desktop_windowGrabErrorHandlerX11();

    if (_desktop_window.glx.ARB_create_context)
    {
        int index = 0, mask = 0, flags = 0;

        if (ctxconfig->client == DESKTOP_WINDOW_DESKTOP_GRAPHICS_API)
        {
            if (ctxconfig->forward)
                flags |= GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

            if (ctxconfig->profile == DESKTOP_WINDOW_DESKTOP_GRAPHICS_CORE_PROFILE)
                mask |= GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
            else if (ctxconfig->profile == DESKTOP_WINDOW_DESKTOP_GRAPHICS_COMPAT_PROFILE)
                mask |= GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
        }
        else
            mask |= GLX_CONTEXT_ES2_PROFILE_BIT_EXT;

        if (ctxconfig->debug)
            flags |= GLX_CONTEXT_DEBUG_BIT_ARB;

        if (ctxconfig->robustness)
        {
            if (_desktop_window.glx.ARB_create_context_robustness)
            {
                if (ctxconfig->robustness == DESKTOP_WINDOW_NO_RESET_NOTIFICATION)
                {
                    SET_ATTRIB(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB,
                               GLX_NO_RESET_NOTIFICATION_ARB);
                }
                else if (ctxconfig->robustness == DESKTOP_WINDOW_LOSE_CONTEXT_ON_RESET)
                {
                    SET_ATTRIB(GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB,
                               GLX_LOSE_CONTEXT_ON_RESET_ARB);
                }

                flags |= GLX_CONTEXT_ROBUST_ACCESS_BIT_ARB;
            }
        }

        if (ctxconfig->release)
        {
            if (_desktop_window.glx.ARB_context_flush_control)
            {
                if (ctxconfig->release == DESKTOP_WINDOW_RELEASE_BEHAVIOR_NONE)
                {
                    SET_ATTRIB(GLX_CONTEXT_RELEASE_BEHAVIOR_ARB,
                               GLX_CONTEXT_RELEASE_BEHAVIOR_NONE_ARB);
                }
                else if (ctxconfig->release == DESKTOP_WINDOW_RELEASE_BEHAVIOR_FLUSH)
                {
                    SET_ATTRIB(GLX_CONTEXT_RELEASE_BEHAVIOR_ARB,
                               GLX_CONTEXT_RELEASE_BEHAVIOR_FLUSH_ARB);
                }
            }
        }

        if (ctxconfig->noerror)
        {
            if (_desktop_window.glx.ARB_create_context_no_error)
                SET_ATTRIB(GLX_CONTEXT_DESKTOP_GRAPHICS_NO_ERROR_ARB, DESKTOP_WINDOW_TRUE);
        }

        // NOTE: Only request an explicitly versioned context when necessary, as
        //       explicitly requesting version 1.0 does not always return the
        //       highest version supported by the driver
        if (ctxconfig->major != 1 || ctxconfig->minor != 0)
        {
            SET_ATTRIB(GLX_CONTEXT_MAJOR_VERSION_ARB, ctxconfig->major);
            SET_ATTRIB(GLX_CONTEXT_MINOR_VERSION_ARB, ctxconfig->minor);
        }

        if (mask)
            SET_ATTRIB(GLX_CONTEXT_PROFILE_MASK_ARB, mask);

        if (flags)
            SET_ATTRIB(GLX_CONTEXT_FLAGS_ARB, flags);

        SET_ATTRIB(None, None);

        window->context.glx.handle =
            _desktop_window.glx.CreateContextAttribsARB(_desktop_window.x11.display,
                                              native,
                                              share,
                                              True,
                                              attribs);

        // HACK: This is a fallback for broken versions of the Mesa
        //       implementation of GLX_ARB_create_context_profile that fail
        //       default 1.0 context creation with a GLXBadProfileARB error in
        //       violation of the extension spec
        if (!window->context.glx.handle)
        {
            if (_desktop_window.x11.errorCode == _desktop_window.glx.errorBase + GLXBadProfileARB &&
                ctxconfig->client == DESKTOP_WINDOW_DESKTOP_GRAPHICS_API &&
                ctxconfig->profile == DESKTOP_WINDOW_DESKTOP_GRAPHICS_ANY_PROFILE &&
                ctxconfig->forward == DESKTOP_WINDOW_FALSE)
            {
                window->context.glx.handle =
                    createLegacyContextGLX(window, native, share);
            }
        }
    }
    else
    {
        window->context.glx.handle =
            createLegacyContextGLX(window, native, share);
    }

    _desktop_windowReleaseErrorHandlerX11();

    if (!window->context.glx.handle)
    {
        _desktop_windowInputErrorX11(DESKTOP_WINDOW_VERSION_UNAVAILABLE, "GLX: Failed to create context");
        return DESKTOP_WINDOW_FALSE;
    }

    window->context.glx.window =
        glXCreateWindow(_desktop_window.x11.display, native, window->x11.handle, NULL);
    if (!window->context.glx.window)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "GLX: Failed to create window");
        return DESKTOP_WINDOW_FALSE;
    }

    window->context.makeCurrent = makeContextCurrentGLX;
    window->context.swapBuffers = swapBuffersGLX;
    window->context.swapInterval = swapIntervalGLX;
    window->context.extensionSupported = extensionSupportedGLX;
    window->context.getProcAddress = getProcAddressGLX;
    window->context.destroy = destroyContextGLX;

    return DESKTOP_WINDOW_TRUE;
}

#undef SET_ATTRIB

// Returns the Visual and depth of the chosen GLXFBConfig
//
DESKTOP_WINDOWbool _desktop_windowChooseVisualGLX(const _DESKTOP_WINDOWwndconfig* wndconfig,
                              const _DESKTOP_WINDOWctxconfig* ctxconfig,
                              const _DESKTOP_WINDOWfbconfig* fbconfig,
                              Visual** visual, int* depth)
{
    GLXFBConfig native;
    XVisualInfo* result;

    if (!chooseGLXFBConfig(fbconfig, &native))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_FORMAT_UNAVAILABLE,
                        "GLX: Failed to find a suitable GLXFBConfig");
        return DESKTOP_WINDOW_FALSE;
    }

    result = glXGetVisualFromFBConfig(_desktop_window.x11.display, native);
    if (!result)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "GLX: Failed to retrieve Visual for GLXFBConfig");
        return DESKTOP_WINDOW_FALSE;
    }

    *visual = result->visual;
    *depth  = result->depth;

    XFree(result);
    return DESKTOP_WINDOW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW native API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI GLXContext desktop_windowGetGLXContext(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_X11)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "GLX: Platform not initialized");
        return NULL;
    }

    if (window->context.source != DESKTOP_WINDOW_NATIVE_CONTEXT_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT, NULL);
        return NULL;
    }

    return window->context.glx.handle;
}

DESKTOP_WINDOWAPI GLXWindow desktop_windowGetGLXWindow(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(None);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_X11)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "GLX: Platform not initialized");
        return None;
    }

    if (window->context.source != DESKTOP_WINDOW_NATIVE_CONTEXT_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT, NULL);
        return None;
    }

    return window->context.glx.window;
}

#endif // _DESKTOP_WINDOW_X11

