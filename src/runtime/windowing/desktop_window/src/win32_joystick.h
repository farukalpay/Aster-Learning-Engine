#define DESKTOP_WINDOW_WIN32_JOYSTICK_STATE         _DESKTOP_WINDOWjoystickWin32 win32;
#define DESKTOP_WINDOW_WIN32_LIBRARY_JOYSTICK_STATE

// Joystick element (axis, button or slider)
//
typedef struct _DESKTOP_WINDOWjoyobjectWin32
{
    int                     offset;
    int                     type;
} _DESKTOP_WINDOWjoyobjectWin32;

// Win32-specific per-joystick data
//
typedef struct _DESKTOP_WINDOWjoystickWin32
{
    _DESKTOP_WINDOWjoyobjectWin32*    objects;
    int                     objectCount;
    IDirectInputDevice8W*   device;
    DWORD                   index;
    GUID                    guid;
} _DESKTOP_WINDOWjoystickWin32;

void _desktop_windowDetectJoystickConnectionWin32(void);
void _desktop_windowDetectJoystickDisconnectionWin32(void);

