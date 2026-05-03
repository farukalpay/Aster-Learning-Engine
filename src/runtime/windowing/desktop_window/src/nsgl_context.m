#include "internal.h"

#if defined(_DESKTOP_WINDOW_COCOA)

#include <unistd.h>
#include <math.h>

static void makeContextCurrentNSGL(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {

    if (window)
        [window->context.nsgl.object makeCurrentContext];
    else
        [NSOpenGLContext clearCurrentContext];

    _desktop_windowPlatformSetTls(&_desktop_window.contextSlot, window);

    } // autoreleasepool
}

static void swapBuffersNSGL(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {

    // HACK: Simulate vsync with usleep as NSGL swap interval does not apply to
    //       windows with a non-visible occlusion state
    if (window->ns.occluded)
    {
        int interval = 0;
        [window->context.nsgl.object getValues:&interval
                                  forParameter:NSOpenGLContextParameterSwapInterval];

        if (interval > 0)
        {
            const double framerate = 60.0;
            const uint64_t frequency = _desktop_windowPlatformGetTimerFrequency();
            const uint64_t value = _desktop_windowPlatformGetTimerValue();

            const double elapsed = value / (double) frequency;
            const double period = 1.0 / framerate;
            const double delay = period - fmod(elapsed, period);

            usleep(floorl(delay * 1e6));
        }
    }

    [window->context.nsgl.object flushBuffer];

    } // autoreleasepool
}

static void swapIntervalNSGL(int interval)
{
    @autoreleasepool {

    _DESKTOP_WINDOWwindow* window = _desktop_windowPlatformGetTls(&_desktop_window.contextSlot);
    assert(window != NULL);

    [window->context.nsgl.object setValues:&interval
                              forParameter:NSOpenGLContextParameterSwapInterval];

    } // autoreleasepool
}

static int extensionSupportedNSGL(const char* extension)
{
    // There are no NSGL extensions
    return DESKTOP_WINDOW_FALSE;
}

static DESKTOP_WINDOWglproc getProcAddressNSGL(const char* procname)
{
    CFStringRef symbolName = CFStringCreateWithCString(kCFAllocatorDefault,
                                                       procname,
                                                       kCFStringEncodingASCII);

    DESKTOP_WINDOWglproc symbol = CFBundleGetFunctionPointerForName(_desktop_window.nsgl.framework,
                                                          symbolName);

    CFRelease(symbolName);

    return symbol;
}

static void destroyContextNSGL(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {

    [window->context.nsgl.pixelFormat release];
    window->context.nsgl.pixelFormat = nil;

    [window->context.nsgl.object release];
    window->context.nsgl.object = nil;

    } // autoreleasepool
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Initialize OpenGL support
//
DESKTOP_WINDOWbool _desktop_windowInitNSGL(void)
{
    if (_desktop_window.nsgl.framework)
        return DESKTOP_WINDOW_TRUE;

    _desktop_window.nsgl.framework =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
    if (_desktop_window.nsgl.framework == NULL)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "NSGL: Failed to locate OpenGL framework");
        return DESKTOP_WINDOW_FALSE;
    }

    return DESKTOP_WINDOW_TRUE;
}

// Terminate OpenGL support
//
void _desktop_windowTerminateNSGL(void)
{
}

// Create the OpenGL context
//
DESKTOP_WINDOWbool _desktop_windowCreateContextNSGL(_DESKTOP_WINDOWwindow* window,
                                const _DESKTOP_WINDOWctxconfig* ctxconfig,
                                const _DESKTOP_WINDOWfbconfig* fbconfig)
{
    if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_ES_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "NSGL: OpenGL ES is not available via NSGL");
        return DESKTOP_WINDOW_FALSE;
    }

    if (ctxconfig->major > 2)
    {
        if (ctxconfig->major == 3 && ctxconfig->minor < 2)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                            "NSGL: The targeted version of macOS does not support OpenGL 3.0 or 3.1 but may support 3.2 and above");
            return DESKTOP_WINDOW_FALSE;
        }
    }

    if (ctxconfig->major >= 3 && ctxconfig->profile == DESKTOP_WINDOW_OPENGL_COMPAT_PROFILE)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                        "NSGL: The compatibility profile is not available on macOS");
        return DESKTOP_WINDOW_FALSE;
    }

    // Context robustness modes (GL_KHR_robustness) are not yet supported by
    // macOS but are not a hard constraint, so ignore and continue

    // Context release behaviors (GL_KHR_context_flush_control) are not yet
    // supported by macOS but are not a hard constraint, so ignore and continue

    // Debug contexts (GL_KHR_debug) are not yet supported by macOS but are not
    // a hard constraint, so ignore and continue

    // No-error contexts (GL_KHR_no_error) are not yet supported by macOS but
    // are not a hard constraint, so ignore and continue

#define ADD_ATTRIB(a) \
{ \
    assert((size_t) index < sizeof(attribs) / sizeof(attribs[0])); \
    attribs[index++] = a; \
}
#define SET_ATTRIB(a, v) { ADD_ATTRIB(a); ADD_ATTRIB(v); }

    NSOpenGLPixelFormatAttribute attribs[40];
    int index = 0;

    ADD_ATTRIB(NSOpenGLPFAAccelerated);
    ADD_ATTRIB(NSOpenGLPFAClosestPolicy);

    if (ctxconfig->nsgl.offline)
    {
        ADD_ATTRIB(NSOpenGLPFAAllowOfflineRenderers);
        // NOTE: This replaces the NSSupportsAutomaticGraphicsSwitching key in
        //       Info.plist for unbundled applications
        // HACK: This assumes that NSOpenGLPixelFormat will remain
        //       a straightforward wrapper of its CGL counterpart
        ADD_ATTRIB(kCGLPFASupportsAutomaticGraphicsSwitching);
    }

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
    if (ctxconfig->major >= 4)
    {
        SET_ATTRIB(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core);
    }
    else
#endif /*MAC_OS_X_VERSION_MAX_ALLOWED*/
    if (ctxconfig->major >= 3)
    {
        SET_ATTRIB(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core);
    }

    if (ctxconfig->major <= 2)
    {
        if (fbconfig->auxBuffers != DESKTOP_WINDOW_DONT_CARE)
            SET_ATTRIB(NSOpenGLPFAAuxBuffers, fbconfig->auxBuffers);

        if (fbconfig->accumRedBits != DESKTOP_WINDOW_DONT_CARE &&
            fbconfig->accumGreenBits != DESKTOP_WINDOW_DONT_CARE &&
            fbconfig->accumBlueBits != DESKTOP_WINDOW_DONT_CARE &&
            fbconfig->accumAlphaBits != DESKTOP_WINDOW_DONT_CARE)
        {
            const int accumBits = fbconfig->accumRedBits +
                                  fbconfig->accumGreenBits +
                                  fbconfig->accumBlueBits +
                                  fbconfig->accumAlphaBits;

            SET_ATTRIB(NSOpenGLPFAAccumSize, accumBits);
        }
    }

    if (fbconfig->redBits != DESKTOP_WINDOW_DONT_CARE &&
        fbconfig->greenBits != DESKTOP_WINDOW_DONT_CARE &&
        fbconfig->blueBits != DESKTOP_WINDOW_DONT_CARE)
    {
        int colorBits = fbconfig->redBits +
                        fbconfig->greenBits +
                        fbconfig->blueBits;

        // macOS needs non-zero color size, so set reasonable values
        if (colorBits == 0)
            colorBits = 24;
        else if (colorBits < 15)
            colorBits = 15;

        SET_ATTRIB(NSOpenGLPFAColorSize, colorBits);
    }

    if (fbconfig->alphaBits != DESKTOP_WINDOW_DONT_CARE)
        SET_ATTRIB(NSOpenGLPFAAlphaSize, fbconfig->alphaBits);

    if (fbconfig->depthBits != DESKTOP_WINDOW_DONT_CARE)
        SET_ATTRIB(NSOpenGLPFADepthSize, fbconfig->depthBits);

    if (fbconfig->stencilBits != DESKTOP_WINDOW_DONT_CARE)
        SET_ATTRIB(NSOpenGLPFAStencilSize, fbconfig->stencilBits);

    if (fbconfig->stereo)
    {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101200
        _desktop_windowInputError(DESKTOP_WINDOW_FORMAT_UNAVAILABLE,
                        "NSGL: Stereo rendering is deprecated");
        return DESKTOP_WINDOW_FALSE;
#else
        ADD_ATTRIB(NSOpenGLPFAStereo);
#endif
    }

    if (fbconfig->doublebuffer)
        ADD_ATTRIB(NSOpenGLPFADoubleBuffer);

    if (fbconfig->samples != DESKTOP_WINDOW_DONT_CARE)
    {
        if (fbconfig->samples == 0)
        {
            SET_ATTRIB(NSOpenGLPFASampleBuffers, 0);
        }
        else
        {
            SET_ATTRIB(NSOpenGLPFASampleBuffers, 1);
            SET_ATTRIB(NSOpenGLPFASamples, fbconfig->samples);
        }
    }

    // NOTE: All NSOpenGLPixelFormats on the relevant cards support sRGB
    //       framebuffer, so there's no need (and no way) to request it

    ADD_ATTRIB(0);

#undef ADD_ATTRIB
#undef SET_ATTRIB

    window->context.nsgl.pixelFormat =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    if (window->context.nsgl.pixelFormat == nil)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_FORMAT_UNAVAILABLE,
                        "NSGL: Failed to find a suitable pixel format");
        return DESKTOP_WINDOW_FALSE;
    }

    NSOpenGLContext* share = nil;

    if (ctxconfig->share)
        share = ctxconfig->share->context.nsgl.object;

    window->context.nsgl.object =
        [[NSOpenGLContext alloc] initWithFormat:window->context.nsgl.pixelFormat
                                   shareContext:share];
    if (window->context.nsgl.object == nil)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                        "NSGL: Failed to create OpenGL context");
        return DESKTOP_WINDOW_FALSE;
    }

    if (fbconfig->transparent)
    {
        GLint opaque = 0;
        [window->context.nsgl.object setValues:&opaque
                                  forParameter:NSOpenGLContextParameterSurfaceOpacity];
    }

    [window->ns.view setWantsBestResolutionOpenGLSurface:window->ns.scaleFramebuffer];

    [window->context.nsgl.object setView:window->ns.view];

    window->context.makeCurrent = makeContextCurrentNSGL;
    window->context.swapBuffers = swapBuffersNSGL;
    window->context.swapInterval = swapIntervalNSGL;
    window->context.extensionSupported = extensionSupportedNSGL;
    window->context.getProcAddress = getProcAddressNSGL;
    window->context.destroy = destroyContextNSGL;

    return DESKTOP_WINDOW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW native API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI id desktop_windowGetNSGLContext(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(nil);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_COCOA)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE,
                        "NSGL: Platform not initialized");
        return nil;
    }

    if (window->context.source != DESKTOP_WINDOW_NATIVE_CONTEXT_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT, NULL);
        return nil;
    }

    return window->context.nsgl.object;
}

#endif // _DESKTOP_WINDOW_COCOA

