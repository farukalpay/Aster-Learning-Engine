#include <linux/input.h>
#include <linux/limits.h>
#include <regex.h>

#define DESKTOP_WINDOW_LINUX_JOYSTICK_STATE         _DESKTOP_WINDOWjoystickLinux linjs;
#define DESKTOP_WINDOW_LINUX_LIBRARY_JOYSTICK_STATE _DESKTOP_WINDOWlibraryLinux  linjs;

// Linux-specific joystick data
//
typedef struct _DESKTOP_WINDOWjoystickLinux
{
    int                     fd;
    char                    path[PATH_MAX];
    int                     keyMap[KEY_CNT - BTN_MISC];
    int                     absMap[ABS_CNT];
    struct input_absinfo    absInfo[ABS_CNT];
    int                     hats[4][2];
} _DESKTOP_WINDOWjoystickLinux;

// Linux-specific joystick API data
//
typedef struct _DESKTOP_WINDOWlibraryLinux
{
    int                     inotify;
    int                     watch;
    regex_t                 regex;
    DESKTOP_WINDOWbool                regexCompiled;
    DESKTOP_WINDOWbool                dropped;
} _DESKTOP_WINDOWlibraryLinux;

void _desktop_windowDetectJoystickConnectionLinux(void);

DESKTOP_WINDOWbool _desktop_windowInitJoysticksLinux(void);
void _desktop_windowTerminateJoysticksLinux(void);
DESKTOP_WINDOWbool _desktop_windowPollJoystickLinux(_DESKTOP_WINDOWjoystick* js, int mode);
const char* _desktop_windowGetMappingNameLinux(void);
void _desktop_windowUpdateGamepadGUIDLinux(char* guid);

