#include "internal.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void makeContextCurrentOSMesa(_DESKTOP_WINDOWwindow* window)
{
    if (window)
    {
        int width, height;
        _desktop_window.platform.getFramebufferSize(window, &width, &height);

        // Check to see if we need to allocate a new buffer
        if ((window->context.osmesa.buffer == NULL) ||
            (width != window->context.osmesa.width) ||
            (height != window->context.osmesa.height))
        {
            _desktop_window_free(window->context.osmesa.buffer);

            // Allocate the new buffer (width * height * 8-bit RGBA)
            window->context.osmesa.buffer = _desktop_window_calloc(4, (size_t) width * height);
            window->context.osmesa.width  = width;
            window->context.osmesa.height = height;
        }

        if (!OSMesaMakeCurrent(window->context.osmesa.handle,
                               window->context.osmesa.buffer,
                               GL_UNSIGNED_BYTE,
                               width, height))
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "OSMesa: Failed to make context current");
            return;
        }
    }

    _desktop_windowPlatformSetTls(&_desktop_window.contextSlot, window);
}

static DESKTOP_WINDOWglproc getProcAddressOSMesa(const char* procname)
{
    return (DESKTOP_WINDOWglproc) OSMesaGetProcAddress(procname);
}

static void destroyContextOSMesa(_DESKTOP_WINDOWwindow* window)
{
    if (window->context.osmesa.handle)
    {
        OSMesaDestroyContext(window->context.osmesa.handle);
        window->context.osmesa.handle = NULL;
    }

    if (window->context.osmesa.buffer)
    {
        _desktop_window_free(window->context.osmesa.buffer);
        window->context.osmesa.width = 0;
        window->context.osmesa.height = 0;
    }
}

static void swapBuffersOSMesa(_DESKTOP_WINDOWwindow* window)
{
    // No double buffering on OSMesa
}

static void swapIntervalOSMesa(int interval)
{
    // No swap interval on OSMesa
}

static int extensionSupportedOSMesa(const char* extension)
{
    // OSMesa does not have extensions
    return DESKTOP_WINDOW_FALSE;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowInitOSMesa(void)
{
    int i;
    const char* sonames[] =
    {
#if defined(_DESKTOP_WINDOW_OSMESA_LIBRARY)
        _DESKTOP_WINDOW_OSMESA_LIBRARY,
#elif defined(_WIN32)
        "libOSMesa.dll",
        "OSMesa.dll",
#elif defined(__APPLE__)
        "libOSMesa.8.dylib",
#elif defined(__CYGWIN__)
        "libOSMesa-8.so",
#elif defined(__OpenBSD__) || defined(__NetBSD__)
        "libOSMesa.so",
#else
        "libOSMesa.so.8",
        "libOSMesa.so.6",
#endif
        NULL
    };

    if (_desktop_window.osmesa.handle)
        return DESKTOP_WINDOW_TRUE;

    for (i = 0;  sonames[i];  i++)
    {
        _desktop_window.osmesa.handle = _desktop_windowPlatformLoadModule(sonames[i]);
        if (_desktop_window.osmesa.handle)
            break;
    }

    if (!_desktop_window.osmesa.handle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE, "OSMesa: Library not found");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.osmesa.CreateContextExt = (PFN_OSMesaCreateContextExt)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.osmesa.handle, "OSMesaCreateContextExt");
    _desktop_window.osmesa.CreateContextAttribs = (PFN_OSMesaCreateContextAttribs)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.osmesa.handle, "OSMesaCreateContextAttribs");
    _desktop_window.osmesa.DestroyContext = (PFN_OSMesaDestroyContext)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.osmesa.handle, "OSMesaDestroyContext");
    _desktop_window.osmesa.MakeCurrent = (PFN_OSMesaMakeCurrent)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.osmesa.handle, "OSMesaMakeCurrent");
    _desktop_window.osmesa.GetColorBuffer = (PFN_OSMesaGetColorBuffer)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.osmesa.handle, "OSMesaGetColorBuffer");
    _desktop_window.osmesa.GetDepthBuffer = (PFN_OSMesaGetDepthBuffer)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.osmesa.handle, "OSMesaGetDepthBuffer");
    _desktop_window.osmesa.GetProcAddress = (PFN_OSMesaGetProcAddress)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.osmesa.handle, "OSMesaGetProcAddress");

    if (!_desktop_window.osmesa.CreateContextExt ||
        !_desktop_window.osmesa.DestroyContext ||
        !_desktop_window.osmesa.MakeCurrent ||
        !_desktop_window.osmesa.GetColorBuffer ||
        !_desktop_window.osmesa.GetDepthBuffer ||
        !_desktop_window.osmesa.GetProcAddress)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "OSMesa: Failed to load required entry points");

        _desktop_windowTerminateOSMesa();
        return DESKTOP_WINDOW_FALSE;
    }

    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateOSMesa(void)
{
    if (_desktop_window.osmesa.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.osmesa.handle);
        _desktop_window.osmesa.handle = NULL;
    }
}

#define SET_ATTRIB(a, v) \
{ \
    assert(((size_t) index + 1) < sizeof(attribs) / sizeof(attribs[0])); \
    attribs[index++] = a; \
    attribs[index++] = v; \
}

DESKTOP_WINDOWbool _desktop_windowCreateContextOSMesa(_DESKTOP_WINDOWwindow* window,
                                  const _DESKTOP_WINDOWctxconfig* ctxconfig,
                                  const _DESKTOP_WINDOWfbconfig* fbconfig)
{
    OSMesaContext share = NULL;
    const int accumBits = fbconfig->accumRedBits +
                          fbconfig->accumGreenBits +
                          fbconfig->accumBlueBits +
                          fbconfig->accumAlphaBits;

    if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_ES_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "OSMesa: OpenGL ES is not available on OSMesa");
        return DESKTOP_WINDOW_FALSE;
    }

    if (ctxconfig->share)
        share = ctxconfig->share->context.osmesa.handle;

    if (OSMesaCreateContextAttribs)
    {
        int index = 0, attribs[40];

        SET_ATTRIB(OSMESA_FORMAT, OSMESA_RGBA);
        SET_ATTRIB(OSMESA_DEPTH_BITS, fbconfig->depthBits);
        SET_ATTRIB(OSMESA_STENCIL_BITS, fbconfig->stencilBits);
        SET_ATTRIB(OSMESA_ACCUM_BITS, accumBits);

        if (ctxconfig->profile == DESKTOP_WINDOW_OPENGL_CORE_PROFILE)
        {
            SET_ATTRIB(OSMESA_PROFILE, OSMESA_CORE_PROFILE);
        }
        else if (ctxconfig->profile == DESKTOP_WINDOW_OPENGL_COMPAT_PROFILE)
        {
            SET_ATTRIB(OSMESA_PROFILE, OSMESA_COMPAT_PROFILE);
        }

        if (ctxconfig->major != 1 || ctxconfig->minor != 0)
        {
            SET_ATTRIB(OSMESA_CONTEXT_MAJOR_VERSION, ctxconfig->major);
            SET_ATTRIB(OSMESA_CONTEXT_MINOR_VERSION, ctxconfig->minor);
        }

        if (ctxconfig->forward)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                            "OSMesa: Forward-compatible contexts not supported");
            return DESKTOP_WINDOW_FALSE;
        }

        SET_ATTRIB(0, 0);

        window->context.osmesa.handle =
            OSMesaCreateContextAttribs(attribs, share);
    }
    else
    {
        if (ctxconfig->profile)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                            "OSMesa: OpenGL profiles unavailable");
            return DESKTOP_WINDOW_FALSE;
        }

        window->context.osmesa.handle =
            OSMesaCreateContextExt(OSMESA_RGBA,
                                   fbconfig->depthBits,
                                   fbconfig->stencilBits,
                                   accumBits,
                                   share);
    }

    if (window->context.osmesa.handle == NULL)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                        "OSMesa: Failed to create context");
        return DESKTOP_WINDOW_FALSE;
    }

    window->context.makeCurrent = makeContextCurrentOSMesa;
    window->context.swapBuffers = swapBuffersOSMesa;
    window->context.swapInterval = swapIntervalOSMesa;
    window->context.extensionSupported = extensionSupportedOSMesa;
    window->context.getProcAddress = getProcAddressOSMesa;
    window->context.destroy = destroyContextOSMesa;

    return DESKTOP_WINDOW_TRUE;
}

#undef SET_ATTRIB


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW native API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI int desktop_windowGetOSMesaColorBuffer(DESKTOP_WINDOWwindow* handle, int* width,
                                     int* height, int* format, void** buffer)
{
    void* mesaBuffer;
    GLint mesaWidth, mesaHeight, mesaFormat;
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);

    if (window->context.source != DESKTOP_WINDOW_OSMESA_CONTEXT_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT, NULL);
        return DESKTOP_WINDOW_FALSE;
    }

    if (!OSMesaGetColorBuffer(window->context.osmesa.handle,
                              &mesaWidth, &mesaHeight,
                              &mesaFormat, &mesaBuffer))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "OSMesa: Failed to retrieve color buffer");
        return DESKTOP_WINDOW_FALSE;
    }

    if (width)
        *width = mesaWidth;
    if (height)
        *height = mesaHeight;
    if (format)
        *format = mesaFormat;
    if (buffer)
        *buffer = mesaBuffer;

    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWAPI int desktop_windowGetOSMesaDepthBuffer(DESKTOP_WINDOWwindow* handle,
                                     int* width, int* height,
                                     int* bytesPerValue,
                                     void** buffer)
{
    void* mesaBuffer;
    GLint mesaWidth, mesaHeight, mesaBytes;
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);

    if (window->context.source != DESKTOP_WINDOW_OSMESA_CONTEXT_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT, NULL);
        return DESKTOP_WINDOW_FALSE;
    }

    if (!OSMesaGetDepthBuffer(window->context.osmesa.handle,
                              &mesaWidth, &mesaHeight,
                              &mesaBytes, &mesaBuffer))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "OSMesa: Failed to retrieve depth buffer");
        return DESKTOP_WINDOW_FALSE;
    }

    if (width)
        *width = mesaWidth;
    if (height)
        *height = mesaHeight;
    if (bytesPerValue)
        *bytesPerValue = mesaBytes;
    if (buffer)
        *buffer = mesaBuffer;

    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWAPI OSMesaContext desktop_windowGetOSMesaContext(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (window->context.source != DESKTOP_WINDOW_OSMESA_CONTEXT_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT, NULL);
        return NULL;
    }

    return window->context.osmesa.handle;
}

