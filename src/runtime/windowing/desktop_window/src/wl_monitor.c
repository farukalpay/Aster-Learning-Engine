#include "internal.h"

#if defined(_DESKTOP_WINDOW_WAYLAND)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "wayland-client-protocol.h"


static void outputHandleGeometry(void* userData,
                                 struct wl_output* output,
                                 int32_t x,
                                 int32_t y,
                                 int32_t physicalWidth,
                                 int32_t physicalHeight,
                                 int32_t subpixel,
                                 const char* make,
                                 const char* model,
                                 int32_t transform)
{
    struct _DESKTOP_WINDOWmonitor* monitor = userData;

    monitor->wl.x = x;
    monitor->wl.y = y;
    monitor->widthMM = physicalWidth;
    monitor->heightMM = physicalHeight;

    if (strlen(monitor->name) == 0)
        snprintf(monitor->name, sizeof(monitor->name), "%s %s", make, model);
}

static void outputHandleMode(void* userData,
                             struct wl_output* output,
                             uint32_t flags,
                             int32_t width,
                             int32_t height,
                             int32_t refresh)
{
    struct _DESKTOP_WINDOWmonitor* monitor = userData;
    DESKTOP_WINDOWvidmode mode;

    mode.width = width;
    mode.height = height;
    mode.redBits = 8;
    mode.greenBits = 8;
    mode.blueBits = 8;
    mode.refreshRate = (int) round(refresh / 1000.0);

    monitor->modeCount++;
    monitor->modes =
        _desktop_window_realloc(monitor->modes, monitor->modeCount * sizeof(DESKTOP_WINDOWvidmode));
    monitor->modes[monitor->modeCount - 1] = mode;

    if (flags & WL_OUTPUT_MODE_CURRENT)
        monitor->wl.currentMode = monitor->modeCount - 1;
}

static void outputHandleDone(void* userData, struct wl_output* output)
{
    struct _DESKTOP_WINDOWmonitor* monitor = userData;

    if (monitor->widthMM <= 0 || monitor->heightMM <= 0)
    {
        // If Wayland does not provide a physical size, assume the default 96 DPI
        const DESKTOP_WINDOWvidmode* mode = &monitor->modes[monitor->wl.currentMode];
        monitor->widthMM  = (int) (mode->width * 25.4f / 96.f);
        monitor->heightMM = (int) (mode->height * 25.4f / 96.f);
    }

    for (int i = 0; i < _desktop_window.monitorCount; i++)
    {
        if (_desktop_window.monitors[i] == monitor)
            return;
    }

    _desktop_windowInputMonitor(monitor, DESKTOP_WINDOW_CONNECTED, _DESKTOP_WINDOW_INSERT_LAST);
}

static void outputHandleScale(void* userData,
                              struct wl_output* output,
                              int32_t factor)
{
    struct _DESKTOP_WINDOWmonitor* monitor = userData;

    monitor->wl.scale = factor;

    for (_DESKTOP_WINDOWwindow* window = _desktop_window.windowListHead; window; window = window->next)
    {
        for (size_t i = 0; i < window->wl.outputScaleCount; i++)
        {
            if (window->wl.outputScales[i].output == monitor->wl.output)
            {
                window->wl.outputScales[i].factor = monitor->wl.scale;
                _desktop_windowUpdateBufferScaleFromOutputsWayland(window);
                break;
            }
        }
    }
}

void outputHandleName(void* userData, struct wl_output* wl_output, const char* name)
{
    struct _DESKTOP_WINDOWmonitor* monitor = userData;

    strncpy(monitor->name, name, sizeof(monitor->name) - 1);
}

void outputHandleDescription(void* userData,
                             struct wl_output* wl_output,
                             const char* description)
{
}

static const struct wl_output_listener outputListener =
{
    outputHandleGeometry,
    outputHandleMode,
    outputHandleDone,
    outputHandleScale,
    outputHandleName,
    outputHandleDescription,
};


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowAddOutputWayland(uint32_t name, uint32_t version)
{
    if (version < 2)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Unsupported output interface version");
        return;
    }

    version = _desktop_window_min(version, WL_OUTPUT_NAME_SINCE_VERSION);

    struct wl_output* output = wl_registry_bind(_desktop_window.wl.registry,
                                                name,
                                                &wl_output_interface,
                                                version);
    if (!output)
        return;

    // The actual name of this output will be set in the geometry handler
    _DESKTOP_WINDOWmonitor* monitor = _desktop_windowAllocMonitor("", 0, 0);
    monitor->wl.scale = 1;
    monitor->wl.output = output;
    monitor->wl.name = name;

    wl_proxy_set_tag((struct wl_proxy*) output, &_desktop_window.wl.tag);
    wl_output_add_listener(output, &outputListener, monitor);
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowFreeMonitorWayland(_DESKTOP_WINDOWmonitor* monitor)
{
    if (monitor->wl.output)
        wl_output_destroy(monitor->wl.output);
}

void _desktop_windowGetMonitorPosWayland(_DESKTOP_WINDOWmonitor* monitor, int* xpos, int* ypos)
{
    if (xpos)
        *xpos = monitor->wl.x;
    if (ypos)
        *ypos = monitor->wl.y;
}

void _desktop_windowGetMonitorContentScaleWayland(_DESKTOP_WINDOWmonitor* monitor,
                                        float* xscale, float* yscale)
{
    if (xscale)
        *xscale = (float) monitor->wl.scale;
    if (yscale)
        *yscale = (float) monitor->wl.scale;
}

void _desktop_windowGetMonitorWorkareaWayland(_DESKTOP_WINDOWmonitor* monitor,
                                    int* xpos, int* ypos,
                                    int* width, int* height)
{
    if (xpos)
        *xpos = monitor->wl.x;
    if (ypos)
        *ypos = monitor->wl.y;
    if (width)
        *width = monitor->modes[monitor->wl.currentMode].width;
    if (height)
        *height = monitor->modes[monitor->wl.currentMode].height;
}

DESKTOP_WINDOWvidmode* _desktop_windowGetVideoModesWayland(_DESKTOP_WINDOWmonitor* monitor, int* found)
{
    *found = monitor->modeCount;
    return monitor->modes;
}

DESKTOP_WINDOWbool _desktop_windowGetVideoModeWayland(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWvidmode* mode)
{
    *mode = monitor->modes[monitor->wl.currentMode];
    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWbool _desktop_windowGetGammaRampWayland(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWgammaramp* ramp)
{
    _desktop_windowInputError(DESKTOP_WINDOW_FEATURE_UNAVAILABLE,
                    "Wayland: Gamma ramp access is not available");
    return DESKTOP_WINDOW_FALSE;
}

void _desktop_windowSetGammaRampWayland(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWgammaramp* ramp)
{
    _desktop_windowInputError(DESKTOP_WINDOW_FEATURE_UNAVAILABLE,
                    "Wayland: Gamma ramp access is not available");
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW native API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI struct wl_output* desktop_windowGetWaylandMonitor(DESKTOP_WINDOWmonitor* handle)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_WAYLAND)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "Wayland: Platform not initialized");
        return NULL;
    }

    return monitor->wl.output;
}

#endif // _DESKTOP_WINDOW_WAYLAND

