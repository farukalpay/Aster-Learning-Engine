#include "internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Checks whether the desired context attributes are valid
//
// This function checks things like whether the specified client API version
// exists and whether all relevant options have supported and non-conflicting
// values
//
DESKTOP_WINDOWbool _desktop_windowIsValidContextConfig(const _DESKTOP_WINDOWctxconfig* ctxconfig)
{
    if (ctxconfig->source != DESKTOP_WINDOW_NATIVE_CONTEXT_API &&
        ctxconfig->source != DESKTOP_WINDOW_EGL_CONTEXT_API &&
        ctxconfig->source != DESKTOP_WINDOW_OSMESA_CONTEXT_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM,
                        "Invalid context creation API 0x%08X",
                        ctxconfig->source);
        return DESKTOP_WINDOW_FALSE;
    }

    if (ctxconfig->client != DESKTOP_WINDOW_NO_API &&
        ctxconfig->client != DESKTOP_WINDOW_OPENGL_API &&
        ctxconfig->client != DESKTOP_WINDOW_OPENGL_ES_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM,
                        "Invalid client API 0x%08X",
                        ctxconfig->client);
        return DESKTOP_WINDOW_FALSE;
    }

    if (ctxconfig->share)
    {
        if (ctxconfig->client == DESKTOP_WINDOW_NO_API ||
            ctxconfig->share->context.client == DESKTOP_WINDOW_NO_API)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT, NULL);
            return DESKTOP_WINDOW_FALSE;
        }

        if (ctxconfig->source != ctxconfig->share->context.source)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM,
                            "Context creation APIs do not match between contexts");
            return DESKTOP_WINDOW_FALSE;
        }
    }

    if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_API)
    {
        if ((ctxconfig->major < 1 || ctxconfig->minor < 0) ||
            (ctxconfig->major == 1 && ctxconfig->minor > 5) ||
            (ctxconfig->major == 2 && ctxconfig->minor > 1) ||
            (ctxconfig->major == 3 && ctxconfig->minor > 3))
        {
            // legacy graphics API 1.0 is the smallest valid version
            // legacy graphics API 1.x series ended with version 1.5
            // legacy graphics API 2.x series ended with version 2.1
            // legacy graphics API 3.x series ended with version 3.3
            // For now, let everything else through

            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                            "Invalid legacy graphics API version %i.%i",
                            ctxconfig->major, ctxconfig->minor);
            return DESKTOP_WINDOW_FALSE;
        }

        if (ctxconfig->profile)
        {
            if (ctxconfig->profile != DESKTOP_WINDOW_OPENGL_CORE_PROFILE &&
                ctxconfig->profile != DESKTOP_WINDOW_OPENGL_COMPAT_PROFILE)
            {
                _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM,
                                "Invalid legacy graphics API profile 0x%08X",
                                ctxconfig->profile);
                return DESKTOP_WINDOW_FALSE;
            }

            if (ctxconfig->major <= 2 ||
                (ctxconfig->major == 3 && ctxconfig->minor < 2))
            {
                // Desktop legacy graphics API context profiles are only defined for version 3.2
                // and above

                _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                                "Context profiles are only defined for legacy graphics API version 3.2 and above");
                return DESKTOP_WINDOW_FALSE;
            }
        }

        if (ctxconfig->forward && ctxconfig->major <= 2)
        {
            // Forward-compatible contexts are only defined for legacy graphics API version 3.0 and above
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                            "Forward-compatibility is only defined for legacy graphics API version 3.0 and above");
            return DESKTOP_WINDOW_FALSE;
        }
    }
    else if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_ES_API)
    {
        if (ctxconfig->major < 1 || ctxconfig->minor < 0 ||
            (ctxconfig->major == 1 && ctxconfig->minor > 1) ||
            (ctxconfig->major == 2 && ctxconfig->minor > 0))
        {
            // legacy graphics API ES 1.0 is the smallest valid version
            // legacy graphics API ES 1.x series ended with version 1.1
            // legacy graphics API ES 2.x series ended with version 2.0
            // For now, let everything else through

            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                            "Invalid legacy graphics API ES version %i.%i",
                            ctxconfig->major, ctxconfig->minor);
            return DESKTOP_WINDOW_FALSE;
        }
    }

    if (ctxconfig->robustness)
    {
        if (ctxconfig->robustness != DESKTOP_WINDOW_NO_RESET_NOTIFICATION &&
            ctxconfig->robustness != DESKTOP_WINDOW_LOSE_CONTEXT_ON_RESET)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM,
                            "Invalid context robustness mode 0x%08X",
                            ctxconfig->robustness);
            return DESKTOP_WINDOW_FALSE;
        }
    }

    if (ctxconfig->release)
    {
        if (ctxconfig->release != DESKTOP_WINDOW_RELEASE_BEHAVIOR_NONE &&
            ctxconfig->release != DESKTOP_WINDOW_RELEASE_BEHAVIOR_FLUSH)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM,
                            "Invalid context release behavior 0x%08X",
                            ctxconfig->release);
            return DESKTOP_WINDOW_FALSE;
        }
    }

    return DESKTOP_WINDOW_TRUE;
}

// Chooses the framebuffer config that best matches the desired one
//
const _DESKTOP_WINDOWfbconfig* _desktop_windowChooseFBConfig(const _DESKTOP_WINDOWfbconfig* desired,
                                         const _DESKTOP_WINDOWfbconfig* alternatives,
                                         unsigned int count)
{
    unsigned int i;
    unsigned int missing, leastMissing = UINT_MAX;
    unsigned int colorDiff, leastColorDiff = UINT_MAX;
    unsigned int extraDiff, leastExtraDiff = UINT_MAX;
    const _DESKTOP_WINDOWfbconfig* current;
    const _DESKTOP_WINDOWfbconfig* closest = NULL;

    for (i = 0;  i < count;  i++)
    {
        current = alternatives + i;

        if (desired->stereo > 0 && current->stereo == 0)
        {
            // Stereo is a hard constraint
            continue;
        }

        // Count number of missing buffers
        {
            missing = 0;

            if (desired->alphaBits > 0 && current->alphaBits == 0)
                missing++;

            if (desired->depthBits > 0 && current->depthBits == 0)
                missing++;

            if (desired->stencilBits > 0 && current->stencilBits == 0)
                missing++;

            if (desired->auxBuffers > 0 &&
                current->auxBuffers < desired->auxBuffers)
            {
                missing += desired->auxBuffers - current->auxBuffers;
            }

            if (desired->samples > 0 && current->samples == 0)
            {
                // Technically, several multisampling buffers could be
                // involved, but that's a lower level implementation detail and
                // not important to us here, so we count them as one
                missing++;
            }

            if (desired->transparent != current->transparent)
                missing++;
        }

        // These polynomials make many small channel size differences matter
        // less than one large channel size difference

        // Calculate color channel size difference value
        {
            colorDiff = 0;

            if (desired->redBits != DESKTOP_WINDOW_DONT_CARE)
            {
                colorDiff += (desired->redBits - current->redBits) *
                             (desired->redBits - current->redBits);
            }

            if (desired->greenBits != DESKTOP_WINDOW_DONT_CARE)
            {
                colorDiff += (desired->greenBits - current->greenBits) *
                             (desired->greenBits - current->greenBits);
            }

            if (desired->blueBits != DESKTOP_WINDOW_DONT_CARE)
            {
                colorDiff += (desired->blueBits - current->blueBits) *
                             (desired->blueBits - current->blueBits);
            }
        }

        // Calculate non-color channel size difference value
        {
            extraDiff = 0;

            if (desired->alphaBits != DESKTOP_WINDOW_DONT_CARE)
            {
                extraDiff += (desired->alphaBits - current->alphaBits) *
                             (desired->alphaBits - current->alphaBits);
            }

            if (desired->depthBits != DESKTOP_WINDOW_DONT_CARE)
            {
                extraDiff += (desired->depthBits - current->depthBits) *
                             (desired->depthBits - current->depthBits);
            }

            if (desired->stencilBits != DESKTOP_WINDOW_DONT_CARE)
            {
                extraDiff += (desired->stencilBits - current->stencilBits) *
                             (desired->stencilBits - current->stencilBits);
            }

            if (desired->accumRedBits != DESKTOP_WINDOW_DONT_CARE)
            {
                extraDiff += (desired->accumRedBits - current->accumRedBits) *
                             (desired->accumRedBits - current->accumRedBits);
            }

            if (desired->accumGreenBits != DESKTOP_WINDOW_DONT_CARE)
            {
                extraDiff += (desired->accumGreenBits - current->accumGreenBits) *
                             (desired->accumGreenBits - current->accumGreenBits);
            }

            if (desired->accumBlueBits != DESKTOP_WINDOW_DONT_CARE)
            {
                extraDiff += (desired->accumBlueBits - current->accumBlueBits) *
                             (desired->accumBlueBits - current->accumBlueBits);
            }

            if (desired->accumAlphaBits != DESKTOP_WINDOW_DONT_CARE)
            {
                extraDiff += (desired->accumAlphaBits - current->accumAlphaBits) *
                             (desired->accumAlphaBits - current->accumAlphaBits);
            }

            if (desired->samples != DESKTOP_WINDOW_DONT_CARE)
            {
                extraDiff += (desired->samples - current->samples) *
                             (desired->samples - current->samples);
            }

            if (desired->sRGB && !current->sRGB)
                extraDiff++;
        }

        // Figure out if the current one is better than the best one found so far
        // Least number of missing buffers is the most important heuristic,
        // then color buffer size match and lastly size match for other buffers

        if (missing < leastMissing)
            closest = current;
        else if (missing == leastMissing)
        {
            if ((colorDiff < leastColorDiff) ||
                (colorDiff == leastColorDiff && extraDiff < leastExtraDiff))
            {
                closest = current;
            }
        }

        if (current == closest)
        {
            leastMissing = missing;
            leastColorDiff = colorDiff;
            leastExtraDiff = extraDiff;
        }
    }

    return closest;
}

// Retrieves the attributes of the current context
//
DESKTOP_WINDOWbool _desktop_windowRefreshContextAttribs(_DESKTOP_WINDOWwindow* window,
                                    const _DESKTOP_WINDOWctxconfig* ctxconfig)
{
    int i;
    _DESKTOP_WINDOWwindow* previous;
    const char* version;
    const char* prefixes[] =
    {
        "legacy graphics API ES-CM ",
        "legacy graphics API ES-CL ",
        "legacy graphics API ES ",
        NULL
    };

    window->context.source = ctxconfig->source;
    window->context.client = DESKTOP_WINDOW_OPENGL_API;

    previous = _desktop_windowPlatformGetTls(&_desktop_window.contextSlot);
    desktop_windowMakeContextCurrent((DESKTOP_WINDOWwindow*) window);
    if (_desktop_windowPlatformGetTls(&_desktop_window.contextSlot) != window)
        return DESKTOP_WINDOW_FALSE;

    window->context.GetIntegerv = (PFNGLGETINTEGERVPROC)
        window->context.getProcAddress("glGetIntegerv");
    window->context.GetString = (PFNGLGETSTRINGPROC)
        window->context.getProcAddress("glGetString");
    if (!window->context.GetIntegerv || !window->context.GetString)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "Entry point retrieval is broken");
        desktop_windowMakeContextCurrent((DESKTOP_WINDOWwindow*) previous);
        return DESKTOP_WINDOW_FALSE;
    }

    version = (const char*) window->context.GetString(GL_VERSION);
    if (!version)
    {
        if (ctxconfig->client == DESKTOP_WINDOW_OPENGL_API)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "legacy graphics API version string retrieval is broken");
        }
        else
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "legacy graphics API ES version string retrieval is broken");
        }

        desktop_windowMakeContextCurrent((DESKTOP_WINDOWwindow*) previous);
        return DESKTOP_WINDOW_FALSE;
    }

    for (i = 0;  prefixes[i];  i++)
    {
        const size_t length = strlen(prefixes[i]);

        if (strncmp(version, prefixes[i], length) == 0)
        {
            version += length;
            window->context.client = DESKTOP_WINDOW_OPENGL_ES_API;
            break;
        }
    }

    if (!sscanf(version, "%d.%d.%d",
                &window->context.major,
                &window->context.minor,
                &window->context.revision))
    {
        if (window->context.client == DESKTOP_WINDOW_OPENGL_API)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "No version found in legacy graphics API version string");
        }
        else
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "No version found in legacy graphics API ES version string");
        }

        desktop_windowMakeContextCurrent((DESKTOP_WINDOWwindow*) previous);
        return DESKTOP_WINDOW_FALSE;
    }

    if (window->context.major < ctxconfig->major ||
        (window->context.major == ctxconfig->major &&
         window->context.minor < ctxconfig->minor))
    {
        // The desired legacy graphics API version is greater than the actual version
        // This only happens if the machine lacks {GLX|WGL}_ARB_create_context
        // /and/ the user has requested an legacy graphics API version greater than 1.0

        // For API consistency, we emulate the behavior of the
        // {GLX|WGL}_ARB_create_context extension and fail here

        if (window->context.client == DESKTOP_WINDOW_OPENGL_API)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                            "Requested legacy graphics API version %i.%i, got version %i.%i",
                            ctxconfig->major, ctxconfig->minor,
                            window->context.major, window->context.minor);
        }
        else
        {
            _desktop_windowInputError(DESKTOP_WINDOW_VERSION_UNAVAILABLE,
                            "Requested legacy graphics API ES version %i.%i, got version %i.%i",
                            ctxconfig->major, ctxconfig->minor,
                            window->context.major, window->context.minor);
        }

        desktop_windowMakeContextCurrent((DESKTOP_WINDOWwindow*) previous);
        return DESKTOP_WINDOW_FALSE;
    }

    if (window->context.major >= 3)
    {
        // legacy graphics API 3.0+ uses a different function for extension string retrieval
        // We cache it here instead of in desktop_windowExtensionSupported mostly to alert
        // users as early as possible that their build may be broken

        window->context.GetStringi = (PFNGLGETSTRINGIPROC)
            window->context.getProcAddress("glGetStringi");
        if (!window->context.GetStringi)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "Entry point retrieval is broken");
            desktop_windowMakeContextCurrent((DESKTOP_WINDOWwindow*) previous);
            return DESKTOP_WINDOW_FALSE;
        }
    }

    if (window->context.client == DESKTOP_WINDOW_OPENGL_API)
    {
        // Read back context flags (legacy graphics API 3.0 and above)
        if (window->context.major >= 3)
        {
            GLint flags;
            window->context.GetIntegerv(GL_CONTEXT_FLAGS, &flags);

            if (flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT)
                window->context.forward = DESKTOP_WINDOW_TRUE;

            if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
                window->context.debug = DESKTOP_WINDOW_TRUE;
            else if (desktop_windowExtensionSupported("GL_ARB_debug_output") &&
                     ctxconfig->debug)
            {
                // HACK: This is a workaround for older drivers (pre KHR_debug)
                //       not setting the debug bit in the context flags for
                //       debug contexts
                window->context.debug = DESKTOP_WINDOW_TRUE;
            }

            if (flags & GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR)
                window->context.noerror = DESKTOP_WINDOW_TRUE;
        }

        // Read back legacy graphics API context profile (legacy graphics API 3.2 and above)
        if (window->context.major >= 4 ||
            (window->context.major == 3 && window->context.minor >= 2))
        {
            GLint mask;
            window->context.GetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);

            if (mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
                window->context.profile = DESKTOP_WINDOW_OPENGL_COMPAT_PROFILE;
            else if (mask & GL_CONTEXT_CORE_PROFILE_BIT)
                window->context.profile = DESKTOP_WINDOW_OPENGL_CORE_PROFILE;
            else if (desktop_windowExtensionSupported("GL_ARB_compatibility"))
            {
                // HACK: This is a workaround for the compatibility profile bit
                //       not being set in the context flags if an legacy graphics API 3.2+
                //       context was created without having requested a specific
                //       version
                window->context.profile = DESKTOP_WINDOW_OPENGL_COMPAT_PROFILE;
            }
        }

        // Read back robustness strategy
        if (desktop_windowExtensionSupported("GL_ARB_robustness"))
        {
            // NOTE: We avoid using the context flags for detection, as they are
            //       only present from 3.0 while the extension applies from 1.1

            GLint strategy;
            window->context.GetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB,
                                        &strategy);

            if (strategy == GL_LOSE_CONTEXT_ON_RESET_ARB)
                window->context.robustness = DESKTOP_WINDOW_LOSE_CONTEXT_ON_RESET;
            else if (strategy == GL_NO_RESET_NOTIFICATION_ARB)
                window->context.robustness = DESKTOP_WINDOW_NO_RESET_NOTIFICATION;
        }
    }
    else
    {
        // Read back robustness strategy
        if (desktop_windowExtensionSupported("GL_EXT_robustness"))
        {
            // NOTE: The values of these constants match those of the legacy graphics API ARB
            //       one, so we can reuse them here

            GLint strategy;
            window->context.GetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB,
                                        &strategy);

            if (strategy == GL_LOSE_CONTEXT_ON_RESET_ARB)
                window->context.robustness = DESKTOP_WINDOW_LOSE_CONTEXT_ON_RESET;
            else if (strategy == GL_NO_RESET_NOTIFICATION_ARB)
                window->context.robustness = DESKTOP_WINDOW_NO_RESET_NOTIFICATION;
        }
    }

    if (desktop_windowExtensionSupported("GL_KHR_context_flush_control"))
    {
        GLint behavior;
        window->context.GetIntegerv(GL_CONTEXT_RELEASE_BEHAVIOR, &behavior);

        if (behavior == GL_NONE)
            window->context.release = DESKTOP_WINDOW_RELEASE_BEHAVIOR_NONE;
        else if (behavior == GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH)
            window->context.release = DESKTOP_WINDOW_RELEASE_BEHAVIOR_FLUSH;
    }

    // Clearing the front buffer to black to avoid garbage pixels left over from
    // previous uses of our bit of VRAM
    {
        PFNGLCLEARPROC glClear = (PFNGLCLEARPROC)
            window->context.getProcAddress("glClear");
        glClear(GL_COLOR_BUFFER_BIT);

        if (window->doublebuffer)
            window->context.swapBuffers(window);
    }

    desktop_windowMakeContextCurrent((DESKTOP_WINDOWwindow*) previous);
    return DESKTOP_WINDOW_TRUE;
}

// Searches an extension string for the specified extension
//
DESKTOP_WINDOWbool _desktop_windowStringInExtensionString(const char* string, const char* extensions)
{
    const char* start = extensions;

    for (;;)
    {
        const char* where;
        const char* terminator;

        where = strstr(start, string);
        if (!where)
            return DESKTOP_WINDOW_FALSE;

        terminator = where + strlen(string);
        if (where == start || *(where - 1) == ' ')
        {
            if (*terminator == ' ' || *terminator == '\0')
                break;
        }

        start = terminator;
    }

    return DESKTOP_WINDOW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW public API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI void desktop_windowMakeContextCurrent(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOWwindow* previous;

    _DESKTOP_WINDOW_REQUIRE_INIT();

    previous = _desktop_windowPlatformGetTls(&_desktop_window.contextSlot);

    if (window && window->context.client == DESKTOP_WINDOW_NO_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT,
                        "Cannot make current with a window that has no legacy graphics API or legacy graphics API ES context");
        return;
    }

    if (previous)
    {
        if (!window || window->context.source != previous->context.source)
            previous->context.makeCurrent(NULL);
    }

    if (window)
        window->context.makeCurrent(window);
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindow* desktop_windowGetCurrentContext(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    return _desktop_windowPlatformGetTls(&_desktop_window.contextSlot);
}

DESKTOP_WINDOWAPI void desktop_windowSwapBuffers(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (window->context.client == DESKTOP_WINDOW_NO_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_WINDOW_CONTEXT,
                        "Cannot swap buffers of a window that has no legacy graphics API or legacy graphics API ES context");
        return;
    }

    window->context.swapBuffers(window);
}

DESKTOP_WINDOWAPI void desktop_windowSwapInterval(int interval)
{
    _DESKTOP_WINDOWwindow* window;

    _DESKTOP_WINDOW_REQUIRE_INIT();

    window = _desktop_windowPlatformGetTls(&_desktop_window.contextSlot);
    if (!window)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_CURRENT_CONTEXT,
                        "Cannot set swap interval without a current legacy graphics API or legacy graphics API ES context");
        return;
    }

    window->context.swapInterval(interval);
}

DESKTOP_WINDOWAPI int desktop_windowExtensionSupported(const char* extension)
{
    _DESKTOP_WINDOWwindow* window;
    assert(extension != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);

    window = _desktop_windowPlatformGetTls(&_desktop_window.contextSlot);
    if (!window)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_CURRENT_CONTEXT,
                        "Cannot query extension without a current legacy graphics API or legacy graphics API ES context");
        return DESKTOP_WINDOW_FALSE;
    }

    if (*extension == '\0')
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Extension name cannot be an empty string");
        return DESKTOP_WINDOW_FALSE;
    }

    if (window->context.major >= 3)
    {
        int i;
        GLint count;

        // Check if extension is in the modern legacy graphics API extensions string list

        window->context.GetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (i = 0;  i < count;  i++)
        {
            const char* en = (const char*)
                window->context.GetStringi(GL_EXTENSIONS, i);
            if (!en)
            {
                _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                                "Extension string retrieval is broken");
                return DESKTOP_WINDOW_FALSE;
            }

            if (strcmp(en, extension) == 0)
                return DESKTOP_WINDOW_TRUE;
        }
    }
    else
    {
        // Check if extension is in the old style legacy graphics API extensions string

        const char* extensions = (const char*)
            window->context.GetString(GL_EXTENSIONS);
        if (!extensions)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "Extension string retrieval is broken");
            return DESKTOP_WINDOW_FALSE;
        }

        if (_desktop_windowStringInExtensionString(extension, extensions))
            return DESKTOP_WINDOW_TRUE;
    }

    // Check if extension is in the platform-specific string
    return window->context.extensionSupported(extension);
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWglproc desktop_windowGetProcAddress(const char* procname)
{
    _DESKTOP_WINDOWwindow* window;
    assert(procname != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    window = _desktop_windowPlatformGetTls(&_desktop_window.contextSlot);
    if (!window)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_NO_CURRENT_CONTEXT,
                        "Cannot query entry point without a current legacy graphics API or legacy graphics API ES context");
        return NULL;
    }

    return window->context.getProcAddress(procname);
}

