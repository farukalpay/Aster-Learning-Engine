#include "internal.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

// The the sole (fake) video mode of our (sole) fake monitor
//
static DESKTOP_WINDOWvidmode getVideoMode(void)
{
    DESKTOP_WINDOWvidmode mode;
    mode.width = 1920;
    mode.height = 1080;
    mode.redBits = 8;
    mode.greenBits = 8;
    mode.blueBits = 8;
    mode.refreshRate = 60;
    return mode;
}

//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowPollMonitorsNull(void)
{
    const float dpi = 141.f;
    const DESKTOP_WINDOWvidmode mode = getVideoMode();
    _DESKTOP_WINDOWmonitor* monitor = _desktop_windowAllocMonitor("Null SuperNoop 0",
                                              (int) (mode.width * 25.4f / dpi),
                                              (int) (mode.height * 25.4f / dpi));
    _desktop_windowInputMonitor(monitor, DESKTOP_WINDOW_CONNECTED, _DESKTOP_WINDOW_INSERT_FIRST);
}

//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowFreeMonitorNull(_DESKTOP_WINDOWmonitor* monitor)
{
    _desktop_windowFreeGammaArrays(&monitor->null.ramp);
}

void _desktop_windowGetMonitorPosNull(_DESKTOP_WINDOWmonitor* monitor, int* xpos, int* ypos)
{
    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;
}

void _desktop_windowGetMonitorContentScaleNull(_DESKTOP_WINDOWmonitor* monitor,
                                     float* xscale, float* yscale)
{
    if (xscale)
        *xscale = 1.f;
    if (yscale)
        *yscale = 1.f;
}

void _desktop_windowGetMonitorWorkareaNull(_DESKTOP_WINDOWmonitor* monitor,
                                 int* xpos, int* ypos,
                                 int* width, int* height)
{
    const DESKTOP_WINDOWvidmode mode = getVideoMode();

    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 10;
    if (width)
        *width = mode.width;
    if (height)
        *height = mode.height - 10;
}

DESKTOP_WINDOWvidmode* _desktop_windowGetVideoModesNull(_DESKTOP_WINDOWmonitor* monitor, int* found)
{
    DESKTOP_WINDOWvidmode* mode = _desktop_window_calloc(1, sizeof(DESKTOP_WINDOWvidmode));
    *mode = getVideoMode();
    *found = 1;
    return mode;
}

DESKTOP_WINDOWbool _desktop_windowGetVideoModeNull(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWvidmode* mode)
{
    *mode = getVideoMode();
    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWbool _desktop_windowGetGammaRampNull(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWgammaramp* ramp)
{
    if (!monitor->null.ramp.size)
    {
        unsigned int i;

        _desktop_windowAllocGammaArrays(&monitor->null.ramp, 256);

        for (i = 0;  i < monitor->null.ramp.size;  i++)
        {
            const float gamma = 2.2f;
            float value;
            value = i / (float) (monitor->null.ramp.size - 1);
            value = powf(value, 1.f / gamma) * 65535.f + 0.5f;
            value = fminf(value, 65535.f);

            monitor->null.ramp.red[i]   = (unsigned short) value;
            monitor->null.ramp.green[i] = (unsigned short) value;
            monitor->null.ramp.blue[i]  = (unsigned short) value;
        }
    }

    _desktop_windowAllocGammaArrays(ramp, monitor->null.ramp.size);
    memcpy(ramp->red,   monitor->null.ramp.red,   sizeof(short) * ramp->size);
    memcpy(ramp->green, monitor->null.ramp.green, sizeof(short) * ramp->size);
    memcpy(ramp->blue,  monitor->null.ramp.blue,  sizeof(short) * ramp->size);
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowSetGammaRampNull(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWgammaramp* ramp)
{
    if (monitor->null.ramp.size != ramp->size)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Null: Gamma ramp size must match current ramp size");
        return;
    }

    memcpy(monitor->null.ramp.red,   ramp->red,   sizeof(short) * ramp->size);
    memcpy(monitor->null.ramp.green, ramp->green, sizeof(short) * ramp->size);
    memcpy(monitor->null.ramp.blue,  ramp->blue,  sizeof(short) * ramp->size);
}

