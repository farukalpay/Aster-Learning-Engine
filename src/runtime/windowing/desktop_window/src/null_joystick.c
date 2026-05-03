#include "internal.h"


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowInitJoysticksNull(void)
{
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateJoysticksNull(void)
{
}

DESKTOP_WINDOWbool _desktop_windowPollJoystickNull(_DESKTOP_WINDOWjoystick* js, int mode)
{
    return DESKTOP_WINDOW_FALSE;
}

const char* _desktop_windowGetMappingNameNull(void)
{
    return "";
}

void _desktop_windowUpdateGamepadGUIDNull(char* guid)
{
}

