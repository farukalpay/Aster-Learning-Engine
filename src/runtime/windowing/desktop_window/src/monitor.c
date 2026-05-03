#include "internal.h"

#include <assert.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>


// Lexically compare video modes, used by qsort
//
static int compareVideoModes(const void* fp, const void* sp)
{
    const DESKTOP_WINDOWvidmode* fm = fp;
    const DESKTOP_WINDOWvidmode* sm = sp;
    const int fbpp = fm->redBits + fm->greenBits + fm->blueBits;
    const int sbpp = sm->redBits + sm->greenBits + sm->blueBits;
    const int farea = fm->width * fm->height;
    const int sarea = sm->width * sm->height;

    // First sort on color bits per pixel
    if (fbpp != sbpp)
        return fbpp - sbpp;

    // Then sort on screen area
    if (farea != sarea)
        return farea - sarea;

    // Then sort on width
    if (fm->width != sm->width)
        return fm->width - sm->width;

    // Lastly sort on refresh rate
    return fm->refreshRate - sm->refreshRate;
}

// Retrieves the available modes for the specified monitor
//
static DESKTOP_WINDOWbool refreshVideoModes(_DESKTOP_WINDOWmonitor* monitor)
{
    int modeCount;
    DESKTOP_WINDOWvidmode* modes;

    if (monitor->modes)
        return DESKTOP_WINDOW_TRUE;

    modes = _desktop_window.platform.getVideoModes(monitor, &modeCount);
    if (!modes)
        return DESKTOP_WINDOW_FALSE;

    qsort(modes, modeCount, sizeof(DESKTOP_WINDOWvidmode), compareVideoModes);

    _desktop_window_free(monitor->modes);
    monitor->modes = modes;
    monitor->modeCount = modeCount;

    return DESKTOP_WINDOW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                         DESKTOP_WINDOW event API                       //////
//////////////////////////////////////////////////////////////////////////

// Notifies shared code of a monitor connection or disconnection
//
void _desktop_windowInputMonitor(_DESKTOP_WINDOWmonitor* monitor, int action, int placement)
{
    assert(monitor != NULL);
    assert(action == DESKTOP_WINDOW_CONNECTED || action == DESKTOP_WINDOW_DISCONNECTED);
    assert(placement == _DESKTOP_WINDOW_INSERT_FIRST || placement == _DESKTOP_WINDOW_INSERT_LAST);

    if (action == DESKTOP_WINDOW_CONNECTED)
    {
        _desktop_window.monitorCount++;
        _desktop_window.monitors =
            _desktop_window_realloc(_desktop_window.monitors,
                          sizeof(_DESKTOP_WINDOWmonitor*) * _desktop_window.monitorCount);

        if (placement == _DESKTOP_WINDOW_INSERT_FIRST)
        {
            memmove(_desktop_window.monitors + 1,
                    _desktop_window.monitors,
                    ((size_t) _desktop_window.monitorCount - 1) * sizeof(_DESKTOP_WINDOWmonitor*));
            _desktop_window.monitors[0] = monitor;
        }
        else
            _desktop_window.monitors[_desktop_window.monitorCount - 1] = monitor;
    }
    else if (action == DESKTOP_WINDOW_DISCONNECTED)
    {
        int i;
        _DESKTOP_WINDOWwindow* window;

        for (window = _desktop_window.windowListHead;  window;  window = window->next)
        {
            if (window->monitor == monitor)
            {
                int width, height, xoff, yoff;
                _desktop_window.platform.getWindowSize(window, &width, &height);
                _desktop_window.platform.setWindowMonitor(window, NULL, 0, 0, width, height, 0);
                _desktop_window.platform.getWindowFrameSize(window, &xoff, &yoff, NULL, NULL);
                _desktop_window.platform.setWindowPos(window, xoff, yoff);
            }
        }

        for (i = 0;  i < _desktop_window.monitorCount;  i++)
        {
            if (_desktop_window.monitors[i] == monitor)
            {
                _desktop_window.monitorCount--;
                memmove(_desktop_window.monitors + i,
                        _desktop_window.monitors + i + 1,
                        ((size_t) _desktop_window.monitorCount - i) * sizeof(_DESKTOP_WINDOWmonitor*));
                break;
            }
        }
    }

    if (_desktop_window.callbacks.monitor)
        _desktop_window.callbacks.monitor((DESKTOP_WINDOWmonitor*) monitor, action);

    if (action == DESKTOP_WINDOW_DISCONNECTED)
        _desktop_windowFreeMonitor(monitor);
}

// Notifies shared code that a full screen window has acquired or released
// a monitor
//
void _desktop_windowInputMonitorWindow(_DESKTOP_WINDOWmonitor* monitor, _DESKTOP_WINDOWwindow* window)
{
    assert(monitor != NULL);
    monitor->window = window;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Allocates and returns a monitor object with the specified name and dimensions
//
_DESKTOP_WINDOWmonitor* _desktop_windowAllocMonitor(const char* name, int widthMM, int heightMM)
{
    _DESKTOP_WINDOWmonitor* monitor = _desktop_window_calloc(1, sizeof(_DESKTOP_WINDOWmonitor));
    monitor->widthMM = widthMM;
    monitor->heightMM = heightMM;

    strncpy(monitor->name, name, sizeof(monitor->name) - 1);

    return monitor;
}

// Frees a monitor object and any data associated with it
//
void _desktop_windowFreeMonitor(_DESKTOP_WINDOWmonitor* monitor)
{
    if (monitor == NULL)
        return;

    _desktop_window.platform.freeMonitor(monitor);

    _desktop_windowFreeGammaArrays(&monitor->originalRamp);
    _desktop_windowFreeGammaArrays(&monitor->currentRamp);

    _desktop_window_free(monitor->modes);
    _desktop_window_free(monitor);
}

// Allocates red, green and blue value arrays of the specified size
//
void _desktop_windowAllocGammaArrays(DESKTOP_WINDOWgammaramp* ramp, unsigned int size)
{
    ramp->red = _desktop_window_calloc(size, sizeof(unsigned short));
    ramp->green = _desktop_window_calloc(size, sizeof(unsigned short));
    ramp->blue = _desktop_window_calloc(size, sizeof(unsigned short));
    ramp->size = size;
}

// Frees the red, green and blue value arrays and clears the struct
//
void _desktop_windowFreeGammaArrays(DESKTOP_WINDOWgammaramp* ramp)
{
    _desktop_window_free(ramp->red);
    _desktop_window_free(ramp->green);
    _desktop_window_free(ramp->blue);

    memset(ramp, 0, sizeof(DESKTOP_WINDOWgammaramp));
}

// Chooses the video mode most closely matching the desired one
//
const DESKTOP_WINDOWvidmode* _desktop_windowChooseVideoMode(_DESKTOP_WINDOWmonitor* monitor,
                                        const DESKTOP_WINDOWvidmode* desired)
{
    int i;
    unsigned int sizeDiff, leastSizeDiff = UINT_MAX;
    unsigned int rateDiff, leastRateDiff = UINT_MAX;
    unsigned int colorDiff, leastColorDiff = UINT_MAX;
    const DESKTOP_WINDOWvidmode* current;
    const DESKTOP_WINDOWvidmode* closest = NULL;

    if (!refreshVideoModes(monitor))
        return NULL;

    for (i = 0;  i < monitor->modeCount;  i++)
    {
        current = monitor->modes + i;

        colorDiff = 0;

        if (desired->redBits != DESKTOP_WINDOW_DONT_CARE)
            colorDiff += abs(current->redBits - desired->redBits);
        if (desired->greenBits != DESKTOP_WINDOW_DONT_CARE)
            colorDiff += abs(current->greenBits - desired->greenBits);
        if (desired->blueBits != DESKTOP_WINDOW_DONT_CARE)
            colorDiff += abs(current->blueBits - desired->blueBits);

        sizeDiff = abs((current->width - desired->width) *
                       (current->width - desired->width) +
                       (current->height - desired->height) *
                       (current->height - desired->height));

        if (desired->refreshRate != DESKTOP_WINDOW_DONT_CARE)
            rateDiff = abs(current->refreshRate - desired->refreshRate);
        else
            rateDiff = UINT_MAX - current->refreshRate;

        if ((colorDiff < leastColorDiff) ||
            (colorDiff == leastColorDiff && sizeDiff < leastSizeDiff) ||
            (colorDiff == leastColorDiff && sizeDiff == leastSizeDiff && rateDiff < leastRateDiff))
        {
            closest = current;
            leastSizeDiff = sizeDiff;
            leastRateDiff = rateDiff;
            leastColorDiff = colorDiff;
        }
    }

    return closest;
}

// Performs lexical comparison between two @ref DESKTOP_WINDOWvidmode structures
//
int _desktop_windowCompareVideoModes(const DESKTOP_WINDOWvidmode* fm, const DESKTOP_WINDOWvidmode* sm)
{
    return compareVideoModes(fm, sm);
}

// Splits a color depth into red, green and blue bit depths
//
void _desktop_windowSplitBPP(int bpp, int* red, int* green, int* blue)
{
    int delta;

    // We assume that by 32 the user really meant 24
    if (bpp == 32)
        bpp = 24;

    // Convert "bits per pixel" to red, green & blue sizes

    *red = *green = *blue = bpp / 3;
    delta = bpp - (*red * 3);
    if (delta >= 1)
        *green = *green + 1;

    if (delta == 2)
        *red = *red + 1;
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW public API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI DESKTOP_WINDOWmonitor** desktop_windowGetMonitors(int* count)
{
    assert(count != NULL);

    *count = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    *count = _desktop_window.monitorCount;
    return (DESKTOP_WINDOWmonitor**) _desktop_window.monitors;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWmonitor* desktop_windowGetPrimaryMonitor(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (!_desktop_window.monitorCount)
        return NULL;

    return (DESKTOP_WINDOWmonitor*) _desktop_window.monitors[0];
}

DESKTOP_WINDOWAPI void desktop_windowGetMonitorPos(DESKTOP_WINDOWmonitor* handle, int* xpos, int* ypos)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);

    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT();

    _desktop_window.platform.getMonitorPos(monitor, xpos, ypos);
}

DESKTOP_WINDOWAPI void desktop_windowGetMonitorWorkarea(DESKTOP_WINDOWmonitor* handle,
                                    int* xpos, int* ypos,
                                    int* width, int* height)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);

    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;
    if (width)
        *width = 0;
    if (height)
        *height = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT();

    _desktop_window.platform.getMonitorWorkarea(monitor, xpos, ypos, width, height);
}

DESKTOP_WINDOWAPI void desktop_windowGetMonitorPhysicalSize(DESKTOP_WINDOWmonitor* handle, int* widthMM, int* heightMM)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);

    if (widthMM)
        *widthMM = 0;
    if (heightMM)
        *heightMM = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (widthMM)
        *widthMM = monitor->widthMM;
    if (heightMM)
        *heightMM = monitor->heightMM;
}

DESKTOP_WINDOWAPI void desktop_windowGetMonitorContentScale(DESKTOP_WINDOWmonitor* handle,
                                        float* xscale, float* yscale)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);

    if (xscale)
        *xscale = 0.f;
    if (yscale)
        *yscale = 0.f;

    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.getMonitorContentScale(monitor, xscale, yscale);
}

DESKTOP_WINDOWAPI const char* desktop_windowGetMonitorName(DESKTOP_WINDOWmonitor* handle)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    return monitor->name;
}

DESKTOP_WINDOWAPI void desktop_windowSetMonitorUserPointer(DESKTOP_WINDOWmonitor* handle, void* pointer)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();
    monitor->userPointer = pointer;
}

DESKTOP_WINDOWAPI void* desktop_windowGetMonitorUserPointer(DESKTOP_WINDOWmonitor* handle)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    return monitor->userPointer;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWmonitorfun desktop_windowSetMonitorCallback(DESKTOP_WINDOWmonitorfun cbfun)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWmonitorfun, _desktop_window.callbacks.monitor, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI const DESKTOP_WINDOWvidmode* desktop_windowGetVideoModes(DESKTOP_WINDOWmonitor* handle, int* count)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);
    assert(count != NULL);

    *count = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (!refreshVideoModes(monitor))
        return NULL;

    *count = monitor->modeCount;
    return monitor->modes;
}

DESKTOP_WINDOWAPI const DESKTOP_WINDOWvidmode* desktop_windowGetVideoMode(DESKTOP_WINDOWmonitor* handle)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (!_desktop_window.platform.getVideoMode(monitor, &monitor->currentMode))
        return NULL;

    return &monitor->currentMode;
}

DESKTOP_WINDOWAPI void desktop_windowSetGamma(DESKTOP_WINDOWmonitor* handle, float gamma)
{
    unsigned int i;
    unsigned short* values;
    DESKTOP_WINDOWgammaramp ramp;
    const DESKTOP_WINDOWgammaramp* original;
    assert(handle != NULL);
    assert(gamma > 0.f);
    assert(gamma <= FLT_MAX);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (gamma != gamma || gamma <= 0.f || gamma > FLT_MAX)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Invalid gamma value %f", gamma);
        return;
    }

    original = desktop_windowGetGammaRamp(handle);
    if (!original)
        return;

    values = _desktop_window_calloc(original->size, sizeof(unsigned short));

    for (i = 0;  i < original->size;  i++)
    {
        float value;

        // Calculate intensity
        value = i / (float) (original->size - 1);
        // Apply gamma curve
        value = powf(value, 1.f / gamma) * 65535.f + 0.5f;
        // Clamp to value range
        value = fminf(value, 65535.f);

        values[i] = (unsigned short) value;
    }

    ramp.red = values;
    ramp.green = values;
    ramp.blue = values;
    ramp.size = original->size;

    desktop_windowSetGammaRamp(handle, &ramp);
    _desktop_window_free(values);
}

DESKTOP_WINDOWAPI const DESKTOP_WINDOWgammaramp* desktop_windowGetGammaRamp(DESKTOP_WINDOWmonitor* handle)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    _desktop_windowFreeGammaArrays(&monitor->currentRamp);
    if (!_desktop_window.platform.getGammaRamp(monitor, &monitor->currentRamp))
        return NULL;

    return &monitor->currentRamp;
}

DESKTOP_WINDOWAPI void desktop_windowSetGammaRamp(DESKTOP_WINDOWmonitor* handle, const DESKTOP_WINDOWgammaramp* ramp)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    assert(monitor != NULL);
    assert(ramp != NULL);
    assert(ramp->size > 0);
    assert(ramp->red != NULL);
    assert(ramp->green != NULL);
    assert(ramp->blue != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (ramp->size <= 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                        "Invalid gamma ramp size %i",
                        ramp->size);
        return;
    }

    if (!monitor->originalRamp.size)
    {
        if (!_desktop_window.platform.getGammaRamp(monitor, &monitor->originalRamp))
            return;
    }

    _desktop_window.platform.setGammaRamp(monitor, ramp);
}

