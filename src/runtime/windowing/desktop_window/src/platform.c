#include "internal.h"

#include <string.h>
#include <stdlib.h>

// These construct a string literal from individual numeric constants
#define _DESKTOP_WINDOW_CONCAT_VERSION(m, n, r) #m "." #n "." #r
#define _DESKTOP_WINDOW_MAKE_VERSION(m, n, r) _DESKTOP_WINDOW_CONCAT_VERSION(m, n, r)

//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

static const struct
{
    int ID;
    DESKTOP_WINDOWbool (*connect)(int,_DESKTOP_WINDOWplatform*);
} supportedPlatforms[] =
{
#if defined(_DESKTOP_WINDOW_WIN32)
    { DESKTOP_WINDOW_PLATFORM_WIN32, _desktop_windowConnectWin32 },
#endif
#if defined(_DESKTOP_WINDOW_COCOA)
    { DESKTOP_WINDOW_PLATFORM_COCOA, _desktop_windowConnectCocoa },
#endif
#if defined(_DESKTOP_WINDOW_WAYLAND)
    { DESKTOP_WINDOW_PLATFORM_WAYLAND, _desktop_windowConnectWayland },
#endif
#if defined(_DESKTOP_WINDOW_X11)
    { DESKTOP_WINDOW_PLATFORM_X11, _desktop_windowConnectX11 },
#endif
};

DESKTOP_WINDOWbool _desktop_windowSelectPlatform(int desiredID, _DESKTOP_WINDOWplatform* platform)
{
    const size_t count = sizeof(supportedPlatforms) / sizeof(supportedPlatforms[0]);
    size_t i;

    if (desiredID != DESKTOP_WINDOW_ANY_PLATFORM &&
        desiredID != DESKTOP_WINDOW_PLATFORM_WIN32 &&
        desiredID != DESKTOP_WINDOW_PLATFORM_COCOA &&
        desiredID != DESKTOP_WINDOW_PLATFORM_WAYLAND &&
        desiredID != DESKTOP_WINDOW_PLATFORM_X11 &&
        desiredID != DESKTOP_WINDOW_PLATFORM_NULL)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid platform ID 0x%08X", desiredID);
        return DESKTOP_WINDOW_FALSE;
    }

    // Only allow the Null platform if specifically requested
    if (desiredID == DESKTOP_WINDOW_PLATFORM_NULL)
        return _desktop_windowConnectNull(desiredID, platform);
    else if (count == 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "This binary only supports the Null platform");
        return DESKTOP_WINDOW_FALSE;
    }

#if defined(_DESKTOP_WINDOW_WAYLAND) && defined(_DESKTOP_WINDOW_X11)
    if (desiredID == DESKTOP_WINDOW_ANY_PLATFORM)
    {
        const char* const session = getenv("XDG_SESSION_TYPE");
        if (session)
        {
            // Only follow XDG_SESSION_TYPE if it is set correctly and the
            // environment looks plausble; otherwise fall back to detection
            if (strcmp(session, "wayland") == 0 && getenv("WAYLAND_DISPLAY"))
                desiredID = DESKTOP_WINDOW_PLATFORM_WAYLAND;
            else if (strcmp(session, "x11") == 0 && getenv("DISPLAY"))
                desiredID = DESKTOP_WINDOW_PLATFORM_X11;
        }
    }
#endif

    if (desiredID == DESKTOP_WINDOW_ANY_PLATFORM)
    {
        // If there is exactly one platform available for auto-selection, let it emit the
        // error on failure as the platform-specific error description may be more helpful
        if (count == 1)
            return supportedPlatforms[0].connect(supportedPlatforms[0].ID, platform);

        for (i = 0;  i < count;  i++)
        {
            if (supportedPlatforms[i].connect(desiredID, platform))
                return DESKTOP_WINDOW_TRUE;
        }

        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "Failed to detect any supported platform");
    }
    else
    {
        for (i = 0;  i < count;  i++)
        {
            if (supportedPlatforms[i].ID == desiredID)
                return supportedPlatforms[i].connect(desiredID, platform);
        }

        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "The requested platform is not supported");
    }

    return DESKTOP_WINDOW_FALSE;
}

//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW public API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI int desktop_windowGetPlatform(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(0);
    return _desktop_window.platform.platformID;
}

DESKTOP_WINDOWAPI int desktop_windowPlatformSupported(int platformID)
{
    const size_t count = sizeof(supportedPlatforms) / sizeof(supportedPlatforms[0]);
    size_t i;

    if (platformID != DESKTOP_WINDOW_PLATFORM_WIN32 &&
        platformID != DESKTOP_WINDOW_PLATFORM_COCOA &&
        platformID != DESKTOP_WINDOW_PLATFORM_WAYLAND &&
        platformID != DESKTOP_WINDOW_PLATFORM_X11 &&
        platformID != DESKTOP_WINDOW_PLATFORM_NULL)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid platform ID 0x%08X", platformID);
        return DESKTOP_WINDOW_FALSE;
    }

    if (platformID == DESKTOP_WINDOW_PLATFORM_NULL)
        return DESKTOP_WINDOW_TRUE;

    for (i = 0;  i < count;  i++)
    {
        if (platformID == supportedPlatforms[i].ID)
            return DESKTOP_WINDOW_TRUE;
    }

    return DESKTOP_WINDOW_FALSE;
}

DESKTOP_WINDOWAPI const char* desktop_windowGetVersionString(void)
{
    return _DESKTOP_WINDOW_MAKE_VERSION(DESKTOP_WINDOW_VERSION_MAJOR,
                              DESKTOP_WINDOW_VERSION_MINOR,
                              DESKTOP_WINDOW_VERSION_REVISION)
#if defined(_DESKTOP_WINDOW_WIN32)
        " Win32 WGL"
#endif
#if defined(_DESKTOP_WINDOW_COCOA)
        " Cocoa native-window"
#endif
#if defined(_DESKTOP_WINDOW_WAYLAND)
        " Wayland"
#endif
#if defined(_DESKTOP_WINDOW_X11)
        " X11 GLX"
#endif
        " Null"
        " no-client-api"
#if defined(__MINGW64_VERSION_MAJOR)
        " MinGW-w64"
#elif defined(__MINGW32__)
        " MinGW"
#elif defined(_MSC_VER)
        " VisualC"
#endif
#if defined(_DESKTOP_WINDOW_USE_HYBRID_HPG) || defined(_DESKTOP_WINDOW_USE_OPTIMUS_HPG)
        " hybrid-GPU"
#endif
#if defined(_POSIX_MONOTONIC_CLOCK)
        " monotonic"
#endif
#if defined(_DESKTOP_WINDOW_BUILD_DLL)
#if defined(_WIN32)
        " DLL"
#elif defined(__APPLE__)
        " dynamic"
#else
        " shared"
#endif
#endif
        ;
}
