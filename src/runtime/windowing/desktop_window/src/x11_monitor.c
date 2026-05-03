#include "internal.h"

#if defined(_DESKTOP_WINDOW_X11)

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


// Check whether the display mode should be included in enumeration
//
static DESKTOP_WINDOWbool modeIsGood(const XRRModeInfo* mi)
{
    return (mi->modeFlags & RR_Interlace) == 0;
}

// Calculates the refresh rate, in Hz, from the specified RandR mode info
//
static int calculateRefreshRate(const XRRModeInfo* mi)
{
    if (mi->hTotal && mi->vTotal)
        return (int) round((double) mi->dotClock / ((double) mi->hTotal * (double) mi->vTotal));
    else
        return 0;
}

// Returns the mode info for a RandR mode XID
//
static const XRRModeInfo* getModeInfo(const XRRScreenResources* sr, RRMode id)
{
    for (int i = 0;  i < sr->nmode;  i++)
    {
        if (sr->modes[i].id == id)
            return sr->modes + i;
    }

    return NULL;
}

// Convert RandR mode info to DESKTOP_WINDOW video mode
//
static DESKTOP_WINDOWvidmode vidmodeFromModeInfo(const XRRModeInfo* mi,
                                       const XRRCrtcInfo* ci)
{
    DESKTOP_WINDOWvidmode mode;

    if (ci->rotation == RR_Rotate_90 || ci->rotation == RR_Rotate_270)
    {
        mode.width  = mi->height;
        mode.height = mi->width;
    }
    else
    {
        mode.width  = mi->width;
        mode.height = mi->height;
    }

    mode.refreshRate = calculateRefreshRate(mi);

    _desktop_windowSplitBPP(DefaultDepth(_desktop_window.x11.display, _desktop_window.x11.screen),
                  &mode.redBits, &mode.greenBits, &mode.blueBits);

    return mode;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Poll for changes in the set of connected monitors
//
void _desktop_windowPollMonitorsX11(void)
{
    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.monitorBroken)
    {
        int disconnectedCount, screenCount = 0;
        _DESKTOP_WINDOWmonitor** disconnected = NULL;
        XineramaScreenInfo* screens = NULL;
        XRRScreenResources* sr = XRRGetScreenResourcesCurrent(_desktop_window.x11.display,
                                                              _desktop_window.x11.root);
        RROutput primary = XRRGetOutputPrimary(_desktop_window.x11.display,
                                               _desktop_window.x11.root);

        if (_desktop_window.x11.xinerama.available)
            screens = XineramaQueryScreens(_desktop_window.x11.display, &screenCount);

        disconnectedCount = _desktop_window.monitorCount;
        if (disconnectedCount)
        {
            disconnected = _desktop_window_calloc(_desktop_window.monitorCount, sizeof(_DESKTOP_WINDOWmonitor*));
            memcpy(disconnected,
                   _desktop_window.monitors,
                   _desktop_window.monitorCount * sizeof(_DESKTOP_WINDOWmonitor*));
        }

        for (int i = 0;  i < sr->noutput;  i++)
        {
            int j, type, widthMM, heightMM;

            XRROutputInfo* oi = XRRGetOutputInfo(_desktop_window.x11.display, sr, sr->outputs[i]);
            if (oi->connection != RR_Connected || oi->crtc == None)
            {
                XRRFreeOutputInfo(oi);
                continue;
            }

            for (j = 0;  j < disconnectedCount;  j++)
            {
                if (disconnected[j] &&
                    disconnected[j]->x11.output == sr->outputs[i])
                {
                    disconnected[j] = NULL;
                    break;
                }
            }

            if (j < disconnectedCount)
            {
                XRRFreeOutputInfo(oi);
                continue;
            }

            XRRCrtcInfo* ci = XRRGetCrtcInfo(_desktop_window.x11.display, sr, oi->crtc);
            if (ci->rotation == RR_Rotate_90 || ci->rotation == RR_Rotate_270)
            {
                widthMM  = oi->mm_height;
                heightMM = oi->mm_width;
            }
            else
            {
                widthMM  = oi->mm_width;
                heightMM = oi->mm_height;
            }

            if (widthMM <= 0 || heightMM <= 0)
            {
                // HACK: If RandR does not provide a physical size, assume the
                //       X11 default 96 DPI and calculate from the CRTC viewport
                // NOTE: These members are affected by rotation, unlike the mode
                //       info and output info members
                widthMM  = (int) (ci->width * 25.4f / 96.f);
                heightMM = (int) (ci->height * 25.4f / 96.f);
            }

            _DESKTOP_WINDOWmonitor* monitor = _desktop_windowAllocMonitor(oi->name, widthMM, heightMM);
            monitor->x11.output = sr->outputs[i];
            monitor->x11.crtc   = oi->crtc;

            for (j = 0;  j < screenCount;  j++)
            {
                if (screens[j].x_org == ci->x &&
                    screens[j].y_org == ci->y &&
                    screens[j].width == ci->width &&
                    screens[j].height == ci->height)
                {
                    monitor->x11.index = j;
                    break;
                }
            }

            if (monitor->x11.output == primary)
                type = _DESKTOP_WINDOW_INSERT_FIRST;
            else
                type = _DESKTOP_WINDOW_INSERT_LAST;

            _desktop_windowInputMonitor(monitor, DESKTOP_WINDOW_CONNECTED, type);

            XRRFreeOutputInfo(oi);
            XRRFreeCrtcInfo(ci);
        }

        XRRFreeScreenResources(sr);

        if (screens)
            XFree(screens);

        for (int i = 0;  i < disconnectedCount;  i++)
        {
            if (disconnected[i])
                _desktop_windowInputMonitor(disconnected[i], DESKTOP_WINDOW_DISCONNECTED, 0);
        }

        _desktop_window_free(disconnected);
    }
    else
    {
        const int widthMM = DisplayWidthMM(_desktop_window.x11.display, _desktop_window.x11.screen);
        const int heightMM = DisplayHeightMM(_desktop_window.x11.display, _desktop_window.x11.screen);

        _desktop_windowInputMonitor(_desktop_windowAllocMonitor("Display", widthMM, heightMM),
                          DESKTOP_WINDOW_CONNECTED,
                          _DESKTOP_WINDOW_INSERT_FIRST);
    }
}

// Set the current video mode for the specified monitor
//
void _desktop_windowSetVideoModeX11(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWvidmode* desired)
{
    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.monitorBroken)
    {
        DESKTOP_WINDOWvidmode current;
        RRMode native = None;

        const DESKTOP_WINDOWvidmode* best = _desktop_windowChooseVideoMode(monitor, desired);
        _desktop_windowGetVideoModeX11(monitor, &current);
        if (_desktop_windowCompareVideoModes(&current, best) == 0)
            return;

        XRRScreenResources* sr =
            XRRGetScreenResourcesCurrent(_desktop_window.x11.display, _desktop_window.x11.root);
        XRRCrtcInfo* ci = XRRGetCrtcInfo(_desktop_window.x11.display, sr, monitor->x11.crtc);
        XRROutputInfo* oi = XRRGetOutputInfo(_desktop_window.x11.display, sr, monitor->x11.output);

        for (int i = 0;  i < oi->nmode;  i++)
        {
            const XRRModeInfo* mi = getModeInfo(sr, oi->modes[i]);
            if (!modeIsGood(mi))
                continue;

            const DESKTOP_WINDOWvidmode mode = vidmodeFromModeInfo(mi, ci);
            if (_desktop_windowCompareVideoModes(best, &mode) == 0)
            {
                native = mi->id;
                break;
            }
        }

        if (native)
        {
            if (monitor->x11.oldMode == None)
                monitor->x11.oldMode = ci->mode;

            XRRSetCrtcConfig(_desktop_window.x11.display,
                             sr, monitor->x11.crtc,
                             CurrentTime,
                             ci->x, ci->y,
                             native,
                             ci->rotation,
                             ci->outputs,
                             ci->noutput);
        }

        XRRFreeOutputInfo(oi);
        XRRFreeCrtcInfo(ci);
        XRRFreeScreenResources(sr);
    }
}

// Restore the saved (original) video mode for the specified monitor
//
void _desktop_windowRestoreVideoModeX11(_DESKTOP_WINDOWmonitor* monitor)
{
    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.monitorBroken)
    {
        if (monitor->x11.oldMode == None)
            return;

        XRRScreenResources* sr =
            XRRGetScreenResourcesCurrent(_desktop_window.x11.display, _desktop_window.x11.root);
        XRRCrtcInfo* ci = XRRGetCrtcInfo(_desktop_window.x11.display, sr, monitor->x11.crtc);

        XRRSetCrtcConfig(_desktop_window.x11.display,
                         sr, monitor->x11.crtc,
                         CurrentTime,
                         ci->x, ci->y,
                         monitor->x11.oldMode,
                         ci->rotation,
                         ci->outputs,
                         ci->noutput);

        XRRFreeCrtcInfo(ci);
        XRRFreeScreenResources(sr);

        monitor->x11.oldMode = None;
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowFreeMonitorX11(_DESKTOP_WINDOWmonitor* monitor)
{
}

void _desktop_windowGetMonitorPosX11(_DESKTOP_WINDOWmonitor* monitor, int* xpos, int* ypos)
{
    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.monitorBroken)
    {
        XRRScreenResources* sr =
            XRRGetScreenResourcesCurrent(_desktop_window.x11.display, _desktop_window.x11.root);
        XRRCrtcInfo* ci = XRRGetCrtcInfo(_desktop_window.x11.display, sr, monitor->x11.crtc);

        if (ci)
        {
            if (xpos)
                *xpos = ci->x;
            if (ypos)
                *ypos = ci->y;

            XRRFreeCrtcInfo(ci);
        }

        XRRFreeScreenResources(sr);
    }
}

void _desktop_windowGetMonitorContentScaleX11(_DESKTOP_WINDOWmonitor* monitor,
                                    float* xscale, float* yscale)
{
    if (xscale)
        *xscale = _desktop_window.x11.contentScaleX;
    if (yscale)
        *yscale = _desktop_window.x11.contentScaleY;
}

void _desktop_windowGetMonitorWorkareaX11(_DESKTOP_WINDOWmonitor* monitor,
                                int* xpos, int* ypos,
                                int* width, int* height)
{
    int areaX = 0, areaY = 0, areaWidth = 0, areaHeight = 0;

    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.monitorBroken)
    {
        XRRScreenResources* sr =
            XRRGetScreenResourcesCurrent(_desktop_window.x11.display, _desktop_window.x11.root);
        XRRCrtcInfo* ci = XRRGetCrtcInfo(_desktop_window.x11.display, sr, monitor->x11.crtc);

        areaX = ci->x;
        areaY = ci->y;

        const XRRModeInfo* mi = getModeInfo(sr, ci->mode);

        if (ci->rotation == RR_Rotate_90 || ci->rotation == RR_Rotate_270)
        {
            areaWidth  = mi->height;
            areaHeight = mi->width;
        }
        else
        {
            areaWidth  = mi->width;
            areaHeight = mi->height;
        }

        XRRFreeCrtcInfo(ci);
        XRRFreeScreenResources(sr);
    }
    else
    {
        areaWidth  = DisplayWidth(_desktop_window.x11.display, _desktop_window.x11.screen);
        areaHeight = DisplayHeight(_desktop_window.x11.display, _desktop_window.x11.screen);
    }

    if (_desktop_window.x11.NET_WORKAREA && _desktop_window.x11.NET_CURRENT_DESKTOP)
    {
        Atom* extents = NULL;
        Atom* desktop = NULL;
        const unsigned long extentCount =
            _desktop_windowGetWindowPropertyX11(_desktop_window.x11.root,
                                      _desktop_window.x11.NET_WORKAREA,
                                      XA_CARDINAL,
                                      (unsigned char**) &extents);

        if (_desktop_windowGetWindowPropertyX11(_desktop_window.x11.root,
                                      _desktop_window.x11.NET_CURRENT_DESKTOP,
                                      XA_CARDINAL,
                                      (unsigned char**) &desktop) > 0)
        {
            if (extentCount >= 4 && *desktop < extentCount / 4)
            {
                const int globalX = extents[*desktop * 4 + 0];
                const int globalY = extents[*desktop * 4 + 1];
                const int globalWidth  = extents[*desktop * 4 + 2];
                const int globalHeight = extents[*desktop * 4 + 3];

                if (areaX < globalX)
                {
                    areaWidth -= globalX - areaX;
                    areaX = globalX;
                }

                if (areaY < globalY)
                {
                    areaHeight -= globalY - areaY;
                    areaY = globalY;
                }

                if (areaX + areaWidth > globalX + globalWidth)
                    areaWidth = globalX - areaX + globalWidth;
                if (areaY + areaHeight > globalY + globalHeight)
                    areaHeight = globalY - areaY + globalHeight;
            }
        }

        if (extents)
            XFree(extents);
        if (desktop)
            XFree(desktop);
    }

    if (xpos)
        *xpos = areaX;
    if (ypos)
        *ypos = areaY;
    if (width)
        *width = areaWidth;
    if (height)
        *height = areaHeight;
}

DESKTOP_WINDOWvidmode* _desktop_windowGetVideoModesX11(_DESKTOP_WINDOWmonitor* monitor, int* count)
{
    DESKTOP_WINDOWvidmode* result;

    *count = 0;

    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.monitorBroken)
    {
        XRRScreenResources* sr =
            XRRGetScreenResourcesCurrent(_desktop_window.x11.display, _desktop_window.x11.root);
        XRRCrtcInfo* ci = XRRGetCrtcInfo(_desktop_window.x11.display, sr, monitor->x11.crtc);
        XRROutputInfo* oi = XRRGetOutputInfo(_desktop_window.x11.display, sr, monitor->x11.output);

        result = _desktop_window_calloc(oi->nmode, sizeof(DESKTOP_WINDOWvidmode));

        for (int i = 0;  i < oi->nmode;  i++)
        {
            const XRRModeInfo* mi = getModeInfo(sr, oi->modes[i]);
            if (!modeIsGood(mi))
                continue;

            const DESKTOP_WINDOWvidmode mode = vidmodeFromModeInfo(mi, ci);
            int j;

            for (j = 0;  j < *count;  j++)
            {
                if (_desktop_windowCompareVideoModes(result + j, &mode) == 0)
                    break;
            }

            // Skip duplicate modes
            if (j < *count)
                continue;

            (*count)++;
            result[*count - 1] = mode;
        }

        XRRFreeOutputInfo(oi);
        XRRFreeCrtcInfo(ci);
        XRRFreeScreenResources(sr);
    }
    else
    {
        *count = 1;
        result = _desktop_window_calloc(1, sizeof(DESKTOP_WINDOWvidmode));
        _desktop_windowGetVideoModeX11(monitor, result);
    }

    return result;
}

DESKTOP_WINDOWbool _desktop_windowGetVideoModeX11(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWvidmode* mode)
{
    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.monitorBroken)
    {
        XRRScreenResources* sr =
            XRRGetScreenResourcesCurrent(_desktop_window.x11.display, _desktop_window.x11.root);
        const XRRModeInfo* mi = NULL;

        XRRCrtcInfo* ci = XRRGetCrtcInfo(_desktop_window.x11.display, sr, monitor->x11.crtc);
        if (ci)
        {
            mi = getModeInfo(sr, ci->mode);
            if (mi)
                *mode = vidmodeFromModeInfo(mi, ci);

            XRRFreeCrtcInfo(ci);
        }

        XRRFreeScreenResources(sr);

        if (!mi)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "X11: Failed to query video mode");
            return DESKTOP_WINDOW_FALSE;
        }
    }
    else
    {
        mode->width = DisplayWidth(_desktop_window.x11.display, _desktop_window.x11.screen);
        mode->height = DisplayHeight(_desktop_window.x11.display, _desktop_window.x11.screen);
        mode->refreshRate = 0;

        _desktop_windowSplitBPP(DefaultDepth(_desktop_window.x11.display, _desktop_window.x11.screen),
                      &mode->redBits, &mode->greenBits, &mode->blueBits);
    }

    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWbool _desktop_windowGetGammaRampX11(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWgammaramp* ramp)
{
    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.gammaBroken)
    {
        const size_t size = XRRGetCrtcGammaSize(_desktop_window.x11.display,
                                                monitor->x11.crtc);
        XRRCrtcGamma* gamma = XRRGetCrtcGamma(_desktop_window.x11.display,
                                              monitor->x11.crtc);

        _desktop_windowAllocGammaArrays(ramp, size);

        memcpy(ramp->red,   gamma->red,   size * sizeof(unsigned short));
        memcpy(ramp->green, gamma->green, size * sizeof(unsigned short));
        memcpy(ramp->blue,  gamma->blue,  size * sizeof(unsigned short));

        XRRFreeGamma(gamma);
        return DESKTOP_WINDOW_TRUE;
    }
    else if (_desktop_window.x11.vidmode.available)
    {
        int size;
        XF86VidModeGetGammaRampSize(_desktop_window.x11.display, _desktop_window.x11.screen, &size);

        _desktop_windowAllocGammaArrays(ramp, size);

        XF86VidModeGetGammaRamp(_desktop_window.x11.display,
                                _desktop_window.x11.screen,
                                ramp->size, ramp->red, ramp->green, ramp->blue);
        return DESKTOP_WINDOW_TRUE;
    }
    else
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "X11: Gamma ramp access not supported by server");
        return DESKTOP_WINDOW_FALSE;
    }
}

void _desktop_windowSetGammaRampX11(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWgammaramp* ramp)
{
    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.gammaBroken)
    {
        if (XRRGetCrtcGammaSize(_desktop_window.x11.display, monitor->x11.crtc) != ramp->size)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "X11: Gamma ramp size must match current ramp size");
            return;
        }

        XRRCrtcGamma* gamma = XRRAllocGamma(ramp->size);

        memcpy(gamma->red,   ramp->red,   ramp->size * sizeof(unsigned short));
        memcpy(gamma->green, ramp->green, ramp->size * sizeof(unsigned short));
        memcpy(gamma->blue,  ramp->blue,  ramp->size * sizeof(unsigned short));

        XRRSetCrtcGamma(_desktop_window.x11.display, monitor->x11.crtc, gamma);
        XRRFreeGamma(gamma);
    }
    else if (_desktop_window.x11.vidmode.available)
    {
        XF86VidModeSetGammaRamp(_desktop_window.x11.display,
                                _desktop_window.x11.screen,
                                ramp->size,
                                (unsigned short*) ramp->red,
                                (unsigned short*) ramp->green,
                                (unsigned short*) ramp->blue);
    }
    else
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "X11: Gamma ramp access not supported by server");
    }
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW native API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI RRCrtc desktop_windowGetX11Adapter(DESKTOP_WINDOWmonitor* handle)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(None);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_X11)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "X11: Platform not initialized");
        return None;
    }

    return monitor->x11.crtc;
}

DESKTOP_WINDOWAPI RROutput desktop_windowGetX11Monitor(DESKTOP_WINDOWmonitor* handle)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(None);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_X11)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "X11: Platform not initialized");
        return None;
    }

    return monitor->x11.output;
}

#endif // _DESKTOP_WINDOW_X11

