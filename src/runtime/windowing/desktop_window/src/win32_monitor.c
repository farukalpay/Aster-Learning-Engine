#include "internal.h"

#if defined(_DESKTOP_WINDOW_WIN32)

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <wchar.h>


// Callback for EnumDisplayMonitors in createMonitor
//
static BOOL CALLBACK monitorCallback(HMONITOR handle,
                                     HDC dc,
                                     RECT* rect,
                                     LPARAM data)
{
    MONITORINFOEXW mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);

    if (GetMonitorInfoW(handle, (MONITORINFO*) &mi))
    {
        _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) data;
        if (wcscmp(mi.szDevice, monitor->win32.adapterName) == 0)
            monitor->win32.handle = handle;
    }

    return TRUE;
}

// Create monitor from an adapter and (optionally) a display
//
static _DESKTOP_WINDOWmonitor* createMonitor(DISPLAY_DEVICEW* adapter,
                                   DISPLAY_DEVICEW* display)
{
    _DESKTOP_WINDOWmonitor* monitor;
    int widthMM, heightMM;
    char* name;
    HDC dc;
    DEVMODEW dm;
    RECT rect;

    if (display)
        name = _desktop_windowCreateUTF8FromWideStringWin32(display->DeviceString);
    else
        name = _desktop_windowCreateUTF8FromWideStringWin32(adapter->DeviceString);
    if (!name)
        return NULL;

    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    EnumDisplaySettingsW(adapter->DeviceName, ENUM_CURRENT_SETTINGS, &dm);

    dc = CreateDCW(L"DISPLAY", adapter->DeviceName, NULL, NULL);

    if (IsWindows8Point1OrGreater())
    {
        widthMM  = GetDeviceCaps(dc, HORZSIZE);
        heightMM = GetDeviceCaps(dc, VERTSIZE);
    }
    else
    {
        widthMM  = (int) (dm.dmPelsWidth * 25.4f / GetDeviceCaps(dc, LOGPIXELSX));
        heightMM = (int) (dm.dmPelsHeight * 25.4f / GetDeviceCaps(dc, LOGPIXELSY));
    }

    DeleteDC(dc);

    monitor = _desktop_windowAllocMonitor(name, widthMM, heightMM);
    _desktop_window_free(name);

    if (adapter->StateFlags & DISPLAY_DEVICE_MODESPRUNED)
        monitor->win32.modesPruned = DESKTOP_WINDOW_TRUE;

    wcscpy(monitor->win32.adapterName, adapter->DeviceName);
    WideCharToMultiByte(CP_UTF8, 0,
                        adapter->DeviceName, -1,
                        monitor->win32.publicAdapterName,
                        sizeof(monitor->win32.publicAdapterName),
                        NULL, NULL);

    if (display)
    {
        wcscpy(monitor->win32.displayName, display->DeviceName);
        WideCharToMultiByte(CP_UTF8, 0,
                            display->DeviceName, -1,
                            monitor->win32.publicDisplayName,
                            sizeof(monitor->win32.publicDisplayName),
                            NULL, NULL);
    }

    rect.left   = dm.dmPosition.x;
    rect.top    = dm.dmPosition.y;
    rect.right  = dm.dmPosition.x + dm.dmPelsWidth;
    rect.bottom = dm.dmPosition.y + dm.dmPelsHeight;

    EnumDisplayMonitors(NULL, &rect, monitorCallback, (LPARAM) monitor);
    return monitor;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Poll for changes in the set of connected monitors
//
void _desktop_windowPollMonitorsWin32(void)
{
    int i, disconnectedCount;
    _DESKTOP_WINDOWmonitor** disconnected = NULL;
    DWORD adapterIndex, displayIndex;
    DISPLAY_DEVICEW adapter, display;
    _DESKTOP_WINDOWmonitor* monitor;

    disconnectedCount = _desktop_window.monitorCount;
    if (disconnectedCount)
    {
        disconnected = _desktop_window_calloc(_desktop_window.monitorCount, sizeof(_DESKTOP_WINDOWmonitor*));
        memcpy(disconnected,
               _desktop_window.monitors,
               _desktop_window.monitorCount * sizeof(_DESKTOP_WINDOWmonitor*));
    }

    for (adapterIndex = 0;  ;  adapterIndex++)
    {
        int type = _DESKTOP_WINDOW_INSERT_LAST;

        ZeroMemory(&adapter, sizeof(adapter));
        adapter.cb = sizeof(adapter);

        if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
            break;

        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;

        if (adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            type = _DESKTOP_WINDOW_INSERT_FIRST;

        for (displayIndex = 0;  ;  displayIndex++)
        {
            ZeroMemory(&display, sizeof(display));
            display.cb = sizeof(display);

            if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
                break;

            if (!(display.StateFlags & DISPLAY_DEVICE_ACTIVE))
                continue;

            for (i = 0;  i < disconnectedCount;  i++)
            {
                if (disconnected[i] &&
                    wcscmp(disconnected[i]->win32.displayName,
                           display.DeviceName) == 0)
                {
                    disconnected[i] = NULL;
                    // handle may have changed, update
                    EnumDisplayMonitors(NULL, NULL, monitorCallback, (LPARAM) _desktop_window.monitors[i]);
                    break;
                }
            }

            if (i < disconnectedCount)
                continue;

            monitor = createMonitor(&adapter, &display);
            if (!monitor)
            {
                _desktop_window_free(disconnected);
                return;
            }

            _desktop_windowInputMonitor(monitor, DESKTOP_WINDOW_CONNECTED, type);

            type = _DESKTOP_WINDOW_INSERT_LAST;
        }

        // HACK: If an active adapter does not have any display devices
        //       (as sometimes happens), add it directly as a monitor
        if (displayIndex == 0)
        {
            for (i = 0;  i < disconnectedCount;  i++)
            {
                if (disconnected[i] &&
                    wcscmp(disconnected[i]->win32.adapterName,
                           adapter.DeviceName) == 0)
                {
                    disconnected[i] = NULL;
                    break;
                }
            }

            if (i < disconnectedCount)
                continue;

            monitor = createMonitor(&adapter, NULL);
            if (!monitor)
            {
                _desktop_window_free(disconnected);
                return;
            }

            _desktop_windowInputMonitor(monitor, DESKTOP_WINDOW_CONNECTED, type);
        }
    }

    for (i = 0;  i < disconnectedCount;  i++)
    {
        if (disconnected[i])
            _desktop_windowInputMonitor(disconnected[i], DESKTOP_WINDOW_DISCONNECTED, 0);
    }

    _desktop_window_free(disconnected);
}

// Change the current video mode
//
void _desktop_windowSetVideoModeWin32(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWvidmode* desired)
{
    DESKTOP_WINDOWvidmode current;
    const DESKTOP_WINDOWvidmode* best;
    DEVMODEW dm;
    LONG result;

    best = _desktop_windowChooseVideoMode(monitor, desired);
    _desktop_windowGetVideoModeWin32(monitor, &current);
    if (_desktop_windowCompareVideoModes(&current, best) == 0)
        return;

    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    dm.dmFields           = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL |
                            DM_DISPLAYFREQUENCY;
    dm.dmPelsWidth        = best->width;
    dm.dmPelsHeight       = best->height;
    dm.dmBitsPerPel       = best->redBits + best->greenBits + best->blueBits;
    dm.dmDisplayFrequency = best->refreshRate;

    if (dm.dmBitsPerPel < 15 || dm.dmBitsPerPel >= 24)
        dm.dmBitsPerPel = 32;

    result = ChangeDisplaySettingsExW(monitor->win32.adapterName,
                                      &dm,
                                      NULL,
                                      CDS_FULLSCREEN,
                                      NULL);
    if (result == DISP_CHANGE_SUCCESSFUL)
        monitor->win32.modeChanged = DESKTOP_WINDOW_TRUE;
    else
    {
        const char* description = "Unknown error";

        if (result == DISP_CHANGE_BADDUALVIEW)
            description = "The system uses DualView";
        else if (result == DISP_CHANGE_BADFLAGS)
            description = "Invalid flags";
        else if (result == DISP_CHANGE_BADMODE)
            description = "Graphics mode not supported";
        else if (result == DISP_CHANGE_BADPARAM)
            description = "Invalid parameter";
        else if (result == DISP_CHANGE_FAILED)
            description = "Graphics mode failed";
        else if (result == DISP_CHANGE_NOTUPDATED)
            description = "Failed to write to registry";
        else if (result == DISP_CHANGE_RESTART)
            description = "Computer restart required";

        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Win32: Failed to set video mode: %s",
                        description);
    }
}

// Restore the previously saved (original) video mode
//
void _desktop_windowRestoreVideoModeWin32(_DESKTOP_WINDOWmonitor* monitor)
{
    if (monitor->win32.modeChanged)
    {
        ChangeDisplaySettingsExW(monitor->win32.adapterName,
                                 NULL, NULL, CDS_FULLSCREEN, NULL);
        monitor->win32.modeChanged = DESKTOP_WINDOW_FALSE;
    }
}

void _desktop_windowGetHMONITORContentScaleWin32(HMONITOR handle, float* xscale, float* yscale)
{
    UINT xdpi, ydpi;

    if (xscale)
        *xscale = 0.f;
    if (yscale)
        *yscale = 0.f;

    if (IsWindows8Point1OrGreater())
    {
        if (GetDpiForMonitor(handle, MDT_EFFECTIVE_DPI, &xdpi, &ydpi) != S_OK)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "Win32: Failed to query monitor DPI");
            return;
        }
    }
    else
    {
        const HDC dc = GetDC(NULL);
        xdpi = GetDeviceCaps(dc, LOGPIXELSX);
        ydpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(NULL, dc);
    }

    if (xscale)
        *xscale = xdpi / (float) USER_DEFAULT_SCREEN_DPI;
    if (yscale)
        *yscale = ydpi / (float) USER_DEFAULT_SCREEN_DPI;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowFreeMonitorWin32(_DESKTOP_WINDOWmonitor* monitor)
{
}

void _desktop_windowGetMonitorPosWin32(_DESKTOP_WINDOWmonitor* monitor, int* xpos, int* ypos)
{
    DEVMODEW dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);

    EnumDisplaySettingsExW(monitor->win32.adapterName,
                           ENUM_CURRENT_SETTINGS,
                           &dm,
                           EDS_ROTATEDMODE);

    if (xpos)
        *xpos = dm.dmPosition.x;
    if (ypos)
        *ypos = dm.dmPosition.y;
}

void _desktop_windowGetMonitorContentScaleWin32(_DESKTOP_WINDOWmonitor* monitor,
                                      float* xscale, float* yscale)
{
    _desktop_windowGetHMONITORContentScaleWin32(monitor->win32.handle, xscale, yscale);
}

void _desktop_windowGetMonitorWorkareaWin32(_DESKTOP_WINDOWmonitor* monitor,
                                  int* xpos, int* ypos,
                                  int* width, int* height)
{
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(monitor->win32.handle, &mi);

    if (xpos)
        *xpos = mi.rcWork.left;
    if (ypos)
        *ypos = mi.rcWork.top;
    if (width)
        *width = mi.rcWork.right - mi.rcWork.left;
    if (height)
        *height = mi.rcWork.bottom - mi.rcWork.top;
}

DESKTOP_WINDOWvidmode* _desktop_windowGetVideoModesWin32(_DESKTOP_WINDOWmonitor* monitor, int* count)
{
    int modeIndex = 0, size = 0;
    DESKTOP_WINDOWvidmode* result = NULL;

    *count = 0;

    for (;;)
    {
        int i;
        DESKTOP_WINDOWvidmode mode;
        DEVMODEW dm;

        ZeroMemory(&dm, sizeof(dm));
        dm.dmSize = sizeof(dm);

        if (!EnumDisplaySettingsW(monitor->win32.adapterName, modeIndex, &dm))
            break;

        modeIndex++;

        // Skip modes with less than 15 BPP
        if (dm.dmBitsPerPel < 15)
            continue;

        mode.width  = dm.dmPelsWidth;
        mode.height = dm.dmPelsHeight;
        mode.refreshRate = dm.dmDisplayFrequency;
        _desktop_windowSplitBPP(dm.dmBitsPerPel,
                      &mode.redBits,
                      &mode.greenBits,
                      &mode.blueBits);

        for (i = 0;  i < *count;  i++)
        {
            if (_desktop_windowCompareVideoModes(result + i, &mode) == 0)
                break;
        }

        // Skip duplicate modes
        if (i < *count)
            continue;

        if (monitor->win32.modesPruned)
        {
            // Skip modes not supported by the connected displays
            if (ChangeDisplaySettingsExW(monitor->win32.adapterName,
                                         &dm,
                                         NULL,
                                         CDS_TEST,
                                         NULL) != DISP_CHANGE_SUCCESSFUL)
            {
                continue;
            }
        }

        if (*count == size)
        {
            size += 128;
            result = (DESKTOP_WINDOWvidmode*) _desktop_window_realloc(result, size * sizeof(DESKTOP_WINDOWvidmode));
        }

        (*count)++;
        result[*count - 1] = mode;
    }

    if (!*count)
    {
        // HACK: Report the current mode if no valid modes were found
        result = _desktop_window_calloc(1, sizeof(DESKTOP_WINDOWvidmode));
        _desktop_windowGetVideoModeWin32(monitor, result);
        *count = 1;
    }

    return result;
}

DESKTOP_WINDOWbool _desktop_windowGetVideoModeWin32(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWvidmode* mode)
{
    DEVMODEW dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);

    if (!EnumDisplaySettingsW(monitor->win32.adapterName, ENUM_CURRENT_SETTINGS, &dm))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "Win32: Failed to query display settings");
        return DESKTOP_WINDOW_FALSE;
    }

    mode->width  = dm.dmPelsWidth;
    mode->height = dm.dmPelsHeight;
    mode->refreshRate = dm.dmDisplayFrequency;
    _desktop_windowSplitBPP(dm.dmBitsPerPel,
                  &mode->redBits,
                  &mode->greenBits,
                  &mode->blueBits);

    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWbool _desktop_windowGetGammaRampWin32(_DESKTOP_WINDOWmonitor* monitor, DESKTOP_WINDOWgammaramp* ramp)
{
    HDC dc;
    WORD values[3][256];

    dc = CreateDCW(L"DISPLAY", monitor->win32.adapterName, NULL, NULL);
    GetDeviceGammaRamp(dc, values);
    DeleteDC(dc);

    _desktop_windowAllocGammaArrays(ramp, 256);

    memcpy(ramp->red,   values[0], sizeof(values[0]));
    memcpy(ramp->green, values[1], sizeof(values[1]));
    memcpy(ramp->blue,  values[2], sizeof(values[2]));

    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowSetGammaRampWin32(_DESKTOP_WINDOWmonitor* monitor, const DESKTOP_WINDOWgammaramp* ramp)
{
    HDC dc;
    WORD values[3][256];

    if (ramp->size != 256)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Win32: Gamma ramp size must be 256");
        return;
    }

    memcpy(values[0], ramp->red,   sizeof(values[0]));
    memcpy(values[1], ramp->green, sizeof(values[1]));
    memcpy(values[2], ramp->blue,  sizeof(values[2]));

    dc = CreateDCW(L"DISPLAY", monitor->win32.adapterName, NULL, NULL);
    SetDeviceGammaRamp(dc, values);
    DeleteDC(dc);
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW native API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI const char* desktop_windowGetWin32Adapter(DESKTOP_WINDOWmonitor* handle)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_WIN32)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "Win32: Platform not initialized");
        return NULL;
    }

    return monitor->win32.publicAdapterName;
}

DESKTOP_WINDOWAPI const char* desktop_windowGetWin32Monitor(DESKTOP_WINDOWmonitor* handle)
{
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_WIN32)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "Win32: Platform not initialized");
        return NULL;
    }

    return monitor->win32.publicDisplayName;
}

#endif // _DESKTOP_WINDOW_WIN32

