#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDKeys.h>

#define DESKTOP_WINDOW_COCOA_JOYSTICK_STATE         _DESKTOP_WINDOWjoystickNS ns;
#define DESKTOP_WINDOW_COCOA_LIBRARY_JOYSTICK_STATE

// Cocoa-specific per-joystick data
//
typedef struct _DESKTOP_WINDOWjoystickNS
{
    IOHIDDeviceRef      device;
    CFMutableArrayRef   axes;
    CFMutableArrayRef   buttons;
    CFMutableArrayRef   hats;
} _DESKTOP_WINDOWjoystickNS;

DESKTOP_WINDOWbool _desktop_windowInitJoysticksCocoa(void);
void _desktop_windowTerminateJoysticksCocoa(void);
DESKTOP_WINDOWbool _desktop_windowPollJoystickCocoa(_DESKTOP_WINDOWjoystick* js, int mode);
const char* _desktop_windowGetMappingNameCocoa(void);
void _desktop_windowUpdateGamepadGUIDCocoa(char* guid);

