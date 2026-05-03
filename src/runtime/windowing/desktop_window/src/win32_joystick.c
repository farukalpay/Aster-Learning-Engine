#include "internal.h"

#if defined(_DESKTOP_WINDOW_WIN32)

#include <stdio.h>
#include <math.h>

#define _DESKTOP_WINDOW_TYPE_AXIS     0
#define _DESKTOP_WINDOW_TYPE_SLIDER   1
#define _DESKTOP_WINDOW_TYPE_BUTTON   2
#define _DESKTOP_WINDOW_TYPE_POV      3

// Data produced with DirectInput device object enumeration
//
typedef struct _DESKTOP_WINDOWobjenumWin32
{
    IDirectInputDevice8W*   device;
    _DESKTOP_WINDOWjoyobjectWin32*    objects;
    int                     objectCount;
    int                     axisCount;
    int                     sliderCount;
    int                     buttonCount;
    int                     povCount;
} _DESKTOP_WINDOWobjenumWin32;

// Define local copies of the necessary GUIDs
//
static const GUID _desktop_window_IID_IDirectInput8W =
    {0xbf798031,0x483a,0x4da2,{0xaa,0x99,0x5d,0x64,0xed,0x36,0x97,0x00}};
static const GUID _desktop_window_GUID_XAxis =
    {0xa36d02e0,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID _desktop_window_GUID_YAxis =
    {0xa36d02e1,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID _desktop_window_GUID_ZAxis =
    {0xa36d02e2,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID _desktop_window_GUID_RxAxis =
    {0xa36d02f4,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID _desktop_window_GUID_RyAxis =
    {0xa36d02f5,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID _desktop_window_GUID_RzAxis =
    {0xa36d02e3,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID _desktop_window_GUID_Slider =
    {0xa36d02e4,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};
static const GUID _desktop_window_GUID_POV =
    {0xa36d02f2,0xc9f3,0x11cf,{0xbf,0xc7,0x44,0x45,0x53,0x54,0x00,0x00}};

#define IID_IDirectInput8W _desktop_window_IID_IDirectInput8W
#define GUID_XAxis _desktop_window_GUID_XAxis
#define GUID_YAxis _desktop_window_GUID_YAxis
#define GUID_ZAxis _desktop_window_GUID_ZAxis
#define GUID_RxAxis _desktop_window_GUID_RxAxis
#define GUID_RyAxis _desktop_window_GUID_RyAxis
#define GUID_RzAxis _desktop_window_GUID_RzAxis
#define GUID_Slider _desktop_window_GUID_Slider
#define GUID_POV _desktop_window_GUID_POV

// Object data array for our clone of c_dfDIJoystick
// Generated with https://github.com/elmindreda/c_dfDIJoystick2
//
static DIOBJECTDATAFORMAT _desktop_windowObjectDataFormats[] =
{
    { &GUID_XAxis,DIJOFS_X,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_YAxis,DIJOFS_Y,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_ZAxis,DIJOFS_Z,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_RxAxis,DIJOFS_RX,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_RyAxis,DIJOFS_RY,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_RzAxis,DIJOFS_RZ,DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_Slider,DIJOFS_SLIDER(0),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_Slider,DIJOFS_SLIDER(1),DIDFT_AXIS|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,DIDOI_ASPECTPOSITION },
    { &GUID_POV,DIJOFS_POV(0),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { &GUID_POV,DIJOFS_POV(1),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { &GUID_POV,DIJOFS_POV(2),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { &GUID_POV,DIJOFS_POV(3),DIDFT_POV|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(0),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(1),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(2),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(3),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(4),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(5),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(6),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(7),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(8),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(9),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(10),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(11),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(12),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(13),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(14),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(15),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(16),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(17),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(18),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(19),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(20),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(21),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(22),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(23),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(24),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(25),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(26),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(27),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(28),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(29),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(30),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
    { NULL,DIJOFS_BUTTON(31),DIDFT_BUTTON|DIDFT_OPTIONAL|DIDFT_ANYINSTANCE,0 },
};

// Our clone of c_dfDIJoystick
//
static const DIDATAFORMAT _desktop_windowDataFormat =
{
    sizeof(DIDATAFORMAT),
    sizeof(DIOBJECTDATAFORMAT),
    DIDFT_ABSAXIS,
    sizeof(DIJOYSTATE),
    sizeof(_desktop_windowObjectDataFormats) / sizeof(DIOBJECTDATAFORMAT),
    _desktop_windowObjectDataFormats
};

// Returns a description fitting the specified XInput capabilities
//
static const char* getDeviceDescription(const XINPUT_CAPABILITIES* xic)
{
    switch (xic->SubType)
    {
        case XINPUT_DEVSUBTYPE_WHEEL:
            return "XInput Wheel";
        case XINPUT_DEVSUBTYPE_ARCADE_STICK:
            return "XInput Arcade Stick";
        case XINPUT_DEVSUBTYPE_FLIGHT_STICK:
            return "XInput Flight Stick";
        case XINPUT_DEVSUBTYPE_DANCE_PAD:
            return "XInput Dance Pad";
        case XINPUT_DEVSUBTYPE_GUITAR:
            return "XInput Guitar";
        case XINPUT_DEVSUBTYPE_DRUM_KIT:
            return "XInput Drum Kit";
        case XINPUT_DEVSUBTYPE_GAMEPAD:
        {
            if (xic->Flags & XINPUT_CAPS_WIRELESS)
                return "Wireless Xbox Controller";
            else
                return "Xbox Controller";
        }
    }

    return "Unknown XInput Device";
}

// Lexically compare device objects
//
static int compareJoystickObjects(const void* first, const void* second)
{
    const _DESKTOP_WINDOWjoyobjectWin32* fo = first;
    const _DESKTOP_WINDOWjoyobjectWin32* so = second;

    if (fo->type != so->type)
        return fo->type - so->type;

    return fo->offset - so->offset;
}

// Checks whether the specified device supports XInput
// Technique from FDInputJoystickManager::IsXInputDeviceFast in ZDoom
//
static DESKTOP_WINDOWbool supportsXInput(const GUID* guid)
{
    UINT i, count = 0;
    RAWINPUTDEVICELIST* ridl;
    DESKTOP_WINDOWbool result = DESKTOP_WINDOW_FALSE;

    if (GetRawInputDeviceList(NULL, &count, sizeof(RAWINPUTDEVICELIST)) != 0)
        return DESKTOP_WINDOW_FALSE;

    ridl = _desktop_window_calloc(count, sizeof(RAWINPUTDEVICELIST));

    if (GetRawInputDeviceList(ridl, &count, sizeof(RAWINPUTDEVICELIST)) == (UINT) -1)
    {
        _desktop_window_free(ridl);
        return DESKTOP_WINDOW_FALSE;
    }

    for (i = 0;  i < count;  i++)
    {
        RID_DEVICE_INFO rdi;
        char name[256];
        UINT size;

        if (ridl[i].dwType != RIM_TYPEHID)
            continue;

        ZeroMemory(&rdi, sizeof(rdi));
        rdi.cbSize = sizeof(rdi);
        size = sizeof(rdi);

        if ((INT) GetRawInputDeviceInfoA(ridl[i].hDevice,
                                         RIDI_DEVICEINFO,
                                         &rdi, &size) == -1)
        {
            continue;
        }

        if (MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId) != (LONG) guid->Data1)
            continue;

        memset(name, 0, sizeof(name));
        size = sizeof(name);

        if ((INT) GetRawInputDeviceInfoA(ridl[i].hDevice,
                                         RIDI_DEVICENAME,
                                         name, &size) == -1)
        {
            break;
        }

        name[sizeof(name) - 1] = '\0';
        if (strstr(name, "IG_"))
        {
            result = DESKTOP_WINDOW_TRUE;
            break;
        }
    }

    _desktop_window_free(ridl);
    return result;
}

// Frees all resources associated with the specified joystick
//
static void closeJoystick(_DESKTOP_WINDOWjoystick* js)
{
    _desktop_windowInputJoystick(js, DESKTOP_WINDOW_DISCONNECTED);

    if (js->win32.device)
    {
        IDirectInputDevice8_Unacquire(js->win32.device);
        IDirectInputDevice8_Release(js->win32.device);
    }

    _desktop_window_free(js->win32.objects);
    _desktop_windowFreeJoystick(js);
}

// DirectInput device object enumeration callback
// Insights gleaned from SDL
//
static BOOL CALLBACK deviceObjectCallback(const DIDEVICEOBJECTINSTANCEW* doi,
                                          void* user)
{
    _DESKTOP_WINDOWobjenumWin32* data = user;
    _DESKTOP_WINDOWjoyobjectWin32* object = data->objects + data->objectCount;

    if (DIDFT_GETTYPE(doi->dwType) & DIDFT_AXIS)
    {
        DIPROPRANGE dipr;

        if (memcmp(&doi->guidType, &GUID_Slider, sizeof(GUID)) == 0)
            object->offset = DIJOFS_SLIDER(data->sliderCount);
        else if (memcmp(&doi->guidType, &GUID_XAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_X;
        else if (memcmp(&doi->guidType, &GUID_YAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_Y;
        else if (memcmp(&doi->guidType, &GUID_ZAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_Z;
        else if (memcmp(&doi->guidType, &GUID_RxAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_RX;
        else if (memcmp(&doi->guidType, &GUID_RyAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_RY;
        else if (memcmp(&doi->guidType, &GUID_RzAxis, sizeof(GUID)) == 0)
            object->offset = DIJOFS_RZ;
        else
            return DIENUM_CONTINUE;

        ZeroMemory(&dipr, sizeof(dipr));
        dipr.diph.dwSize = sizeof(dipr);
        dipr.diph.dwHeaderSize = sizeof(dipr.diph);
        dipr.diph.dwObj = doi->dwType;
        dipr.diph.dwHow = DIPH_BYID;
        dipr.lMin = -32768;
        dipr.lMax =  32767;

        if (FAILED(IDirectInputDevice8_SetProperty(data->device,
                                                   DIPROP_RANGE,
                                                   &dipr.diph)))
        {
            return DIENUM_CONTINUE;
        }

        if (memcmp(&doi->guidType, &GUID_Slider, sizeof(GUID)) == 0)
        {
            object->type = _DESKTOP_WINDOW_TYPE_SLIDER;
            data->sliderCount++;
        }
        else
        {
            object->type = _DESKTOP_WINDOW_TYPE_AXIS;
            data->axisCount++;
        }
    }
    else if (DIDFT_GETTYPE(doi->dwType) & DIDFT_BUTTON)
    {
        object->offset = DIJOFS_BUTTON(data->buttonCount);
        object->type = _DESKTOP_WINDOW_TYPE_BUTTON;
        data->buttonCount++;
    }
    else if (DIDFT_GETTYPE(doi->dwType) & DIDFT_POV)
    {
        object->offset = DIJOFS_POV(data->povCount);
        object->type = _DESKTOP_WINDOW_TYPE_POV;
        data->povCount++;
    }

    data->objectCount++;
    return DIENUM_CONTINUE;
}

// DirectInput device enumeration callback
//
static BOOL CALLBACK deviceCallback(const DIDEVICEINSTANCE* di, void* user)
{
    int jid = 0;
    DIDEVCAPS dc;
    DIPROPDWORD dipd;
    IDirectInputDevice8* device;
    _DESKTOP_WINDOWobjenumWin32 data;
    _DESKTOP_WINDOWjoystick* js;
    char guid[33];
    char name[256];

    for (jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
    {
        js = _desktop_window.joysticks + jid;
        if (js->connected)
        {
            if (memcmp(&js->win32.guid, &di->guidInstance, sizeof(GUID)) == 0)
                return DIENUM_CONTINUE;
        }
    }

    if (supportsXInput(&di->guidProduct))
        return DIENUM_CONTINUE;

    if (FAILED(IDirectInput8_CreateDevice(_desktop_window.win32.dinput8.api,
                                          &di->guidInstance,
                                          &device,
                                          NULL)))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "Win32: Failed to create device");
        return DIENUM_CONTINUE;
    }

    if (FAILED(IDirectInputDevice8_SetDataFormat(device, &_desktop_windowDataFormat)))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Win32: Failed to set device data format");

        IDirectInputDevice8_Release(device);
        return DIENUM_CONTINUE;
    }

    ZeroMemory(&dc, sizeof(dc));
    dc.dwSize = sizeof(dc);

    if (FAILED(IDirectInputDevice8_GetCapabilities(device, &dc)))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Win32: Failed to query device capabilities");

        IDirectInputDevice8_Release(device);
        return DIENUM_CONTINUE;
    }

    ZeroMemory(&dipd, sizeof(dipd));
    dipd.diph.dwSize = sizeof(dipd);
    dipd.diph.dwHeaderSize = sizeof(dipd.diph);
    dipd.diph.dwHow = DIPH_DEVICE;
    dipd.dwData = DIPROPAXISMODE_ABS;

    if (FAILED(IDirectInputDevice8_SetProperty(device,
                                               DIPROP_AXISMODE,
                                               &dipd.diph)))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Win32: Failed to set device axis mode");

        IDirectInputDevice8_Release(device);
        return DIENUM_CONTINUE;
    }

    memset(&data, 0, sizeof(data));
    data.device = device;
    data.objects = _desktop_window_calloc(dc.dwAxes + (size_t) dc.dwButtons + dc.dwPOVs,
                                sizeof(_DESKTOP_WINDOWjoyobjectWin32));

    if (FAILED(IDirectInputDevice8_EnumObjects(device,
                                               deviceObjectCallback,
                                               &data,
                                               DIDFT_AXIS | DIDFT_BUTTON | DIDFT_POV)))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Win32: Failed to enumerate device objects");

        IDirectInputDevice8_Release(device);
        _desktop_window_free(data.objects);
        return DIENUM_CONTINUE;
    }

    qsort(data.objects, data.objectCount,
          sizeof(_DESKTOP_WINDOWjoyobjectWin32),
          compareJoystickObjects);

    if (!WideCharToMultiByte(CP_UTF8, 0,
                             di->tszInstanceName, -1,
                             name, sizeof(name),
                             NULL, NULL))
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Win32: Failed to convert joystick name to UTF-8");

        IDirectInputDevice8_Release(device);
        _desktop_window_free(data.objects);
        return DIENUM_STOP;
    }

    // Generate a joystick GUID that matches the SDL 2.0.5+ one
    if (memcmp(&di->guidProduct.Data4[2], "PIDVID", 6) == 0)
    {
        sprintf(guid, "03000000%02x%02x0000%02x%02x000000000000",
                (uint8_t) di->guidProduct.Data1,
                (uint8_t) (di->guidProduct.Data1 >> 8),
                (uint8_t) (di->guidProduct.Data1 >> 16),
                (uint8_t) (di->guidProduct.Data1 >> 24));
    }
    else
    {
        sprintf(guid, "05000000%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x00",
                name[0], name[1], name[2], name[3],
                name[4], name[5], name[6], name[7],
                name[8], name[9], name[10]);
    }

    js = _desktop_windowAllocJoystick(name, guid,
                            data.axisCount + data.sliderCount,
                            data.buttonCount,
                            data.povCount);
    if (!js)
    {
        IDirectInputDevice8_Release(device);
        _desktop_window_free(data.objects);
        return DIENUM_STOP;
    }

    js->win32.device = device;
    js->win32.guid = di->guidInstance;
    js->win32.objects = data.objects;
    js->win32.objectCount = data.objectCount;

    _desktop_windowInputJoystick(js, DESKTOP_WINDOW_CONNECTED);
    return DIENUM_CONTINUE;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Checks for new joysticks after DBT_DEVICEARRIVAL
//
void _desktop_windowDetectJoystickConnectionWin32(void)
{
    if (_desktop_window.win32.xinput.instance)
    {
        DWORD index;

        for (index = 0;  index < XUSER_MAX_COUNT;  index++)
        {
            int jid;
            char guid[33];
            XINPUT_CAPABILITIES xic;
            _DESKTOP_WINDOWjoystick* js;

            for (jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
            {
                if (_desktop_window.joysticks[jid].connected &&
                    _desktop_window.joysticks[jid].win32.device == NULL &&
                    _desktop_window.joysticks[jid].win32.index == index)
                {
                    break;
                }
            }

            if (jid <= DESKTOP_WINDOW_JOYSTICK_LAST)
                continue;

            if (XInputGetCapabilities(index, 0, &xic) != ERROR_SUCCESS)
                continue;

            // Generate a joystick GUID that matches the SDL 2.0.5+ one
            sprintf(guid, "78696e707574%02x000000000000000000",
                    xic.SubType & 0xff);

            js = _desktop_windowAllocJoystick(getDeviceDescription(&xic), guid, 6, 10, 1);
            if (!js)
                continue;

            js->win32.index = index;

            _desktop_windowInputJoystick(js, DESKTOP_WINDOW_CONNECTED);
        }
    }

    if (_desktop_window.win32.dinput8.api)
    {
        if (FAILED(IDirectInput8_EnumDevices(_desktop_window.win32.dinput8.api,
                                             DI8DEVCLASS_GAMECTRL,
                                             deviceCallback,
                                             NULL,
                                             DIEDFL_ALLDEVICES)))
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "Failed to enumerate DirectInput8 devices");
            return;
        }
    }
}

// Checks for joystick disconnection after DBT_DEVICEREMOVECOMPLETE
//
void _desktop_windowDetectJoystickDisconnectionWin32(void)
{
    int jid;

    for (jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
    {
        _DESKTOP_WINDOWjoystick* js = _desktop_window.joysticks + jid;
        if (js->connected)
            _desktop_windowPollJoystickWin32(js, _DESKTOP_WINDOW_POLL_PRESENCE);
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowInitJoysticksWin32(void)
{
    if (_desktop_window.win32.dinput8.instance)
    {
        if (FAILED(DirectInput8Create(_desktop_window.win32.instance,
                                      DIRECTINPUT_VERSION,
                                      &IID_IDirectInput8W,
                                      (void**) &_desktop_window.win32.dinput8.api,
                                      NULL)))
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "Win32: Failed to create interface");
            return DESKTOP_WINDOW_FALSE;
        }
    }

    _desktop_windowDetectJoystickConnectionWin32();
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateJoysticksWin32(void)
{
    int jid;

    for (jid = DESKTOP_WINDOW_JOYSTICK_1;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
        closeJoystick(_desktop_window.joysticks + jid);

    if (_desktop_window.win32.dinput8.api)
        IDirectInput8_Release(_desktop_window.win32.dinput8.api);
}

DESKTOP_WINDOWbool _desktop_windowPollJoystickWin32(_DESKTOP_WINDOWjoystick* js, int mode)
{
    if (js->win32.device)
    {
        int i, ai = 0, bi = 0, pi = 0;
        HRESULT result;
        DIJOYSTATE state = {0};

        IDirectInputDevice8_Poll(js->win32.device);
        result = IDirectInputDevice8_GetDeviceState(js->win32.device,
                                                    sizeof(state),
                                                    &state);
        if (result == DIERR_NOTACQUIRED || result == DIERR_INPUTLOST)
        {
            IDirectInputDevice8_Acquire(js->win32.device);
            IDirectInputDevice8_Poll(js->win32.device);
            result = IDirectInputDevice8_GetDeviceState(js->win32.device,
                                                        sizeof(state),
                                                        &state);
        }

        if (FAILED(result))
        {
            closeJoystick(js);
            return DESKTOP_WINDOW_FALSE;
        }

        if (mode == _DESKTOP_WINDOW_POLL_PRESENCE)
            return DESKTOP_WINDOW_TRUE;

        for (i = 0;  i < js->win32.objectCount;  i++)
        {
            const void* data = (char*) &state + js->win32.objects[i].offset;

            switch (js->win32.objects[i].type)
            {
                case _DESKTOP_WINDOW_TYPE_AXIS:
                case _DESKTOP_WINDOW_TYPE_SLIDER:
                {
                    const float value = (*((LONG*) data) + 0.5f) / 32767.5f;
                    _desktop_windowInputJoystickAxis(js, ai, value);
                    ai++;
                    break;
                }

                case _DESKTOP_WINDOW_TYPE_BUTTON:
                {
                    const char value = (*((BYTE*) data) & 0x80) != 0;
                    _desktop_windowInputJoystickButton(js, bi, value);
                    bi++;
                    break;
                }

                case _DESKTOP_WINDOW_TYPE_POV:
                {
                    const int states[9] =
                    {
                        DESKTOP_WINDOW_HAT_UP,
                        DESKTOP_WINDOW_HAT_RIGHT_UP,
                        DESKTOP_WINDOW_HAT_RIGHT,
                        DESKTOP_WINDOW_HAT_RIGHT_DOWN,
                        DESKTOP_WINDOW_HAT_DOWN,
                        DESKTOP_WINDOW_HAT_LEFT_DOWN,
                        DESKTOP_WINDOW_HAT_LEFT,
                        DESKTOP_WINDOW_HAT_LEFT_UP,
                        DESKTOP_WINDOW_HAT_CENTERED
                    };

                    // Screams of horror are appropriate at this point
                    int stateIndex = LOWORD(*(DWORD*) data) / (45 * DI_DEGREES);
                    if (stateIndex < 0 || stateIndex > 8)
                        stateIndex = 8;

                    _desktop_windowInputJoystickHat(js, pi, states[stateIndex]);
                    pi++;
                    break;
                }
            }
        }
    }
    else
    {
        int i, dpad = 0;
        DWORD result;
        XINPUT_STATE xis;
        const WORD buttons[10] =
        {
            XINPUT_GAMEPAD_A,
            XINPUT_GAMEPAD_B,
            XINPUT_GAMEPAD_X,
            XINPUT_GAMEPAD_Y,
            XINPUT_GAMEPAD_LEFT_SHOULDER,
            XINPUT_GAMEPAD_RIGHT_SHOULDER,
            XINPUT_GAMEPAD_BACK,
            XINPUT_GAMEPAD_START,
            XINPUT_GAMEPAD_LEFT_THUMB,
            XINPUT_GAMEPAD_RIGHT_THUMB
        };

        result = XInputGetState(js->win32.index, &xis);
        if (result != ERROR_SUCCESS)
        {
            if (result == ERROR_DEVICE_NOT_CONNECTED)
                closeJoystick(js);

            return DESKTOP_WINDOW_FALSE;
        }

        if (mode == _DESKTOP_WINDOW_POLL_PRESENCE)
            return DESKTOP_WINDOW_TRUE;

        _desktop_windowInputJoystickAxis(js, 0, (xis.Gamepad.sThumbLX + 0.5f) / 32767.5f);
        _desktop_windowInputJoystickAxis(js, 1, -(xis.Gamepad.sThumbLY + 0.5f) / 32767.5f);
        _desktop_windowInputJoystickAxis(js, 2, (xis.Gamepad.sThumbRX + 0.5f) / 32767.5f);
        _desktop_windowInputJoystickAxis(js, 3, -(xis.Gamepad.sThumbRY + 0.5f) / 32767.5f);
        _desktop_windowInputJoystickAxis(js, 4, xis.Gamepad.bLeftTrigger / 127.5f - 1.f);
        _desktop_windowInputJoystickAxis(js, 5, xis.Gamepad.bRightTrigger / 127.5f - 1.f);

        for (i = 0;  i < 10;  i++)
        {
            const char value = (xis.Gamepad.wButtons & buttons[i]) ? 1 : 0;
            _desktop_windowInputJoystickButton(js, i, value);
        }

        if (xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
            dpad |= DESKTOP_WINDOW_HAT_UP;
        if (xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
            dpad |= DESKTOP_WINDOW_HAT_RIGHT;
        if (xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
            dpad |= DESKTOP_WINDOW_HAT_DOWN;
        if (xis.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
            dpad |= DESKTOP_WINDOW_HAT_LEFT;

        // Treat invalid combinations as neither being pressed
        // while preserving what data can be preserved
        if ((dpad & DESKTOP_WINDOW_HAT_RIGHT) && (dpad & DESKTOP_WINDOW_HAT_LEFT))
            dpad &= ~(DESKTOP_WINDOW_HAT_RIGHT | DESKTOP_WINDOW_HAT_LEFT);
        if ((dpad & DESKTOP_WINDOW_HAT_UP) && (dpad & DESKTOP_WINDOW_HAT_DOWN))
            dpad &= ~(DESKTOP_WINDOW_HAT_UP | DESKTOP_WINDOW_HAT_DOWN);

        _desktop_windowInputJoystickHat(js, 0, dpad);
    }

    return DESKTOP_WINDOW_TRUE;
}

const char* _desktop_windowGetMappingNameWin32(void)
{
    return "Windows";
}

void _desktop_windowUpdateGamepadGUIDWin32(char* guid)
{
    if (strcmp(guid + 20, "504944564944") == 0)
    {
        char original[33];
        strncpy(original, guid, sizeof(original) - 1);
        sprintf(guid, "03000000%.4s0000%.4s000000000000",
                original, original + 4);
    }
}

#endif // _DESKTOP_WINDOW_WIN32

