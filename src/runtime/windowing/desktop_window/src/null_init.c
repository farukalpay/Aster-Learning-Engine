#include "internal.h"

#include <stdlib.h>
#include <string.h>


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowConnectNull(int platformID, _DESKTOP_WINDOWplatform* platform)
{
    const _DESKTOP_WINDOWplatform null =
    {
        .platformID = DESKTOP_WINDOW_PLATFORM_NULL,
        .init = _desktop_windowInitNull,
        .terminate = _desktop_windowTerminateNull,
        .getCursorPos = _desktop_windowGetCursorPosNull,
        .setCursorPos = _desktop_windowSetCursorPosNull,
        .setCursorMode = _desktop_windowSetCursorModeNull,
        .setRawMouseMotion = _desktop_windowSetRawMouseMotionNull,
        .rawMouseMotionSupported = _desktop_windowRawMouseMotionSupportedNull,
        .createCursor = _desktop_windowCreateCursorNull,
        .createStandardCursor = _desktop_windowCreateStandardCursorNull,
        .destroyCursor = _desktop_windowDestroyCursorNull,
        .setCursor = _desktop_windowSetCursorNull,
        .getScancodeName = _desktop_windowGetScancodeNameNull,
        .getKeyScancode = _desktop_windowGetKeyScancodeNull,
        .setClipboardString = _desktop_windowSetClipboardStringNull,
        .getClipboardString = _desktop_windowGetClipboardStringNull,
        .initJoysticks = _desktop_windowInitJoysticksNull,
        .terminateJoysticks = _desktop_windowTerminateJoysticksNull,
        .pollJoystick = _desktop_windowPollJoystickNull,
        .getMappingName = _desktop_windowGetMappingNameNull,
        .updateGamepadGUID = _desktop_windowUpdateGamepadGUIDNull,
        .freeMonitor = _desktop_windowFreeMonitorNull,
        .getMonitorPos = _desktop_windowGetMonitorPosNull,
        .getMonitorContentScale = _desktop_windowGetMonitorContentScaleNull,
        .getMonitorWorkarea = _desktop_windowGetMonitorWorkareaNull,
        .getVideoModes = _desktop_windowGetVideoModesNull,
        .getVideoMode = _desktop_windowGetVideoModeNull,
        .getGammaRamp = _desktop_windowGetGammaRampNull,
        .setGammaRamp = _desktop_windowSetGammaRampNull,
        .createWindow = _desktop_windowCreateWindowNull,
        .destroyWindow = _desktop_windowDestroyWindowNull,
        .setWindowTitle = _desktop_windowSetWindowTitleNull,
        .setWindowIcon = _desktop_windowSetWindowIconNull,
        .getWindowPos = _desktop_windowGetWindowPosNull,
        .setWindowPos = _desktop_windowSetWindowPosNull,
        .getWindowSize = _desktop_windowGetWindowSizeNull,
        .setWindowSize = _desktop_windowSetWindowSizeNull,
        .setWindowSizeLimits = _desktop_windowSetWindowSizeLimitsNull,
        .setWindowAspectRatio = _desktop_windowSetWindowAspectRatioNull,
        .getFramebufferSize = _desktop_windowGetFramebufferSizeNull,
        .getWindowFrameSize = _desktop_windowGetWindowFrameSizeNull,
        .getWindowContentScale = _desktop_windowGetWindowContentScaleNull,
        .iconifyWindow = _desktop_windowIconifyWindowNull,
        .restoreWindow = _desktop_windowRestoreWindowNull,
        .maximizeWindow = _desktop_windowMaximizeWindowNull,
        .showWindow = _desktop_windowShowWindowNull,
        .hideWindow = _desktop_windowHideWindowNull,
        .requestWindowAttention = _desktop_windowRequestWindowAttentionNull,
        .focusWindow = _desktop_windowFocusWindowNull,
        .setWindowMonitor = _desktop_windowSetWindowMonitorNull,
        .windowFocused = _desktop_windowWindowFocusedNull,
        .windowIconified = _desktop_windowWindowIconifiedNull,
        .windowVisible = _desktop_windowWindowVisibleNull,
        .windowMaximized = _desktop_windowWindowMaximizedNull,
        .windowHovered = _desktop_windowWindowHoveredNull,
        .framebufferTransparent = _desktop_windowFramebufferTransparentNull,
        .getWindowOpacity = _desktop_windowGetWindowOpacityNull,
        .setWindowResizable = _desktop_windowSetWindowResizableNull,
        .setWindowDecorated = _desktop_windowSetWindowDecoratedNull,
        .setWindowFloating = _desktop_windowSetWindowFloatingNull,
        .setWindowOpacity = _desktop_windowSetWindowOpacityNull,
        .setWindowMousePassthrough = _desktop_windowSetWindowMousePassthroughNull,
        .pollEvents = _desktop_windowPollEventsNull,
        .waitEvents = _desktop_windowWaitEventsNull,
        .waitEventsTimeout = _desktop_windowWaitEventsTimeoutNull,
        .postEmptyEvent = _desktop_windowPostEmptyEventNull,
        .getEGLPlatform = _desktop_windowGetEGLPlatformNull,
        .getEGLNativeDisplay = _desktop_windowGetEGLNativeDisplayNull,
        .getEGLNativeWindow = _desktop_windowGetEGLNativeWindowNull,
        .getRequiredInstanceExtensions = _desktop_windowGetRequiredInstanceExtensionsNull,
        .getPhysicalDevicePresentationSupport = _desktop_windowGetPhysicalDevicePresentationSupportNull,
        .createWindowSurface = _desktop_windowCreateWindowSurfaceNull
    };

    *platform = null;
    return DESKTOP_WINDOW_TRUE;
}

int _desktop_windowInitNull(void)
{
    int scancode;

    memset(_desktop_window.null.keycodes, -1, sizeof(_desktop_window.null.keycodes));
    memset(_desktop_window.null.scancodes, -1, sizeof(_desktop_window.null.scancodes));

    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_SPACE]         = DESKTOP_WINDOW_KEY_SPACE;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_APOSTROPHE]    = DESKTOP_WINDOW_KEY_APOSTROPHE;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_COMMA]         = DESKTOP_WINDOW_KEY_COMMA;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_MINUS]         = DESKTOP_WINDOW_KEY_MINUS;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_PERIOD]        = DESKTOP_WINDOW_KEY_PERIOD;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_SLASH]         = DESKTOP_WINDOW_KEY_SLASH;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_0]             = DESKTOP_WINDOW_KEY_0;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_1]             = DESKTOP_WINDOW_KEY_1;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_2]             = DESKTOP_WINDOW_KEY_2;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_3]             = DESKTOP_WINDOW_KEY_3;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_4]             = DESKTOP_WINDOW_KEY_4;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_5]             = DESKTOP_WINDOW_KEY_5;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_6]             = DESKTOP_WINDOW_KEY_6;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_7]             = DESKTOP_WINDOW_KEY_7;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_8]             = DESKTOP_WINDOW_KEY_8;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_9]             = DESKTOP_WINDOW_KEY_9;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_SEMICOLON]     = DESKTOP_WINDOW_KEY_SEMICOLON;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_EQUAL]         = DESKTOP_WINDOW_KEY_EQUAL;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_A]             = DESKTOP_WINDOW_KEY_A;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_B]             = DESKTOP_WINDOW_KEY_B;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_C]             = DESKTOP_WINDOW_KEY_C;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_D]             = DESKTOP_WINDOW_KEY_D;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_E]             = DESKTOP_WINDOW_KEY_E;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F]             = DESKTOP_WINDOW_KEY_F;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_G]             = DESKTOP_WINDOW_KEY_G;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_H]             = DESKTOP_WINDOW_KEY_H;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_I]             = DESKTOP_WINDOW_KEY_I;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_J]             = DESKTOP_WINDOW_KEY_J;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_K]             = DESKTOP_WINDOW_KEY_K;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_L]             = DESKTOP_WINDOW_KEY_L;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_M]             = DESKTOP_WINDOW_KEY_M;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_N]             = DESKTOP_WINDOW_KEY_N;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_O]             = DESKTOP_WINDOW_KEY_O;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_P]             = DESKTOP_WINDOW_KEY_P;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_Q]             = DESKTOP_WINDOW_KEY_Q;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_R]             = DESKTOP_WINDOW_KEY_R;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_S]             = DESKTOP_WINDOW_KEY_S;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_T]             = DESKTOP_WINDOW_KEY_T;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_U]             = DESKTOP_WINDOW_KEY_U;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_V]             = DESKTOP_WINDOW_KEY_V;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_W]             = DESKTOP_WINDOW_KEY_W;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_X]             = DESKTOP_WINDOW_KEY_X;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_Y]             = DESKTOP_WINDOW_KEY_Y;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_Z]             = DESKTOP_WINDOW_KEY_Z;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_LEFT_BRACKET]  = DESKTOP_WINDOW_KEY_LEFT_BRACKET;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_BACKSLASH]     = DESKTOP_WINDOW_KEY_BACKSLASH;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_RIGHT_BRACKET] = DESKTOP_WINDOW_KEY_RIGHT_BRACKET;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_GRAVE_ACCENT]  = DESKTOP_WINDOW_KEY_GRAVE_ACCENT;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_WORLD_1]       = DESKTOP_WINDOW_KEY_WORLD_1;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_WORLD_2]       = DESKTOP_WINDOW_KEY_WORLD_2;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_ESCAPE]        = DESKTOP_WINDOW_KEY_ESCAPE;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_ENTER]         = DESKTOP_WINDOW_KEY_ENTER;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_TAB]           = DESKTOP_WINDOW_KEY_TAB;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_BACKSPACE]     = DESKTOP_WINDOW_KEY_BACKSPACE;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_INSERT]        = DESKTOP_WINDOW_KEY_INSERT;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_DELETE]        = DESKTOP_WINDOW_KEY_DELETE;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_RIGHT]         = DESKTOP_WINDOW_KEY_RIGHT;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_LEFT]          = DESKTOP_WINDOW_KEY_LEFT;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_DOWN]          = DESKTOP_WINDOW_KEY_DOWN;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_UP]            = DESKTOP_WINDOW_KEY_UP;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_PAGE_UP]       = DESKTOP_WINDOW_KEY_PAGE_UP;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_PAGE_DOWN]     = DESKTOP_WINDOW_KEY_PAGE_DOWN;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_HOME]          = DESKTOP_WINDOW_KEY_HOME;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_END]           = DESKTOP_WINDOW_KEY_END;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_CAPS_LOCK]     = DESKTOP_WINDOW_KEY_CAPS_LOCK;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_SCROLL_LOCK]   = DESKTOP_WINDOW_KEY_SCROLL_LOCK;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_NUM_LOCK]      = DESKTOP_WINDOW_KEY_NUM_LOCK;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_PRINT_SCREEN]  = DESKTOP_WINDOW_KEY_PRINT_SCREEN;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_PAUSE]         = DESKTOP_WINDOW_KEY_PAUSE;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F1]            = DESKTOP_WINDOW_KEY_F1;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F2]            = DESKTOP_WINDOW_KEY_F2;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F3]            = DESKTOP_WINDOW_KEY_F3;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F4]            = DESKTOP_WINDOW_KEY_F4;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F5]            = DESKTOP_WINDOW_KEY_F5;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F6]            = DESKTOP_WINDOW_KEY_F6;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F7]            = DESKTOP_WINDOW_KEY_F7;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F8]            = DESKTOP_WINDOW_KEY_F8;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F9]            = DESKTOP_WINDOW_KEY_F9;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F10]           = DESKTOP_WINDOW_KEY_F10;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F11]           = DESKTOP_WINDOW_KEY_F11;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F12]           = DESKTOP_WINDOW_KEY_F12;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F13]           = DESKTOP_WINDOW_KEY_F13;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F14]           = DESKTOP_WINDOW_KEY_F14;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F15]           = DESKTOP_WINDOW_KEY_F15;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F16]           = DESKTOP_WINDOW_KEY_F16;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F17]           = DESKTOP_WINDOW_KEY_F17;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F18]           = DESKTOP_WINDOW_KEY_F18;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F19]           = DESKTOP_WINDOW_KEY_F19;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F20]           = DESKTOP_WINDOW_KEY_F20;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F21]           = DESKTOP_WINDOW_KEY_F21;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F22]           = DESKTOP_WINDOW_KEY_F22;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F23]           = DESKTOP_WINDOW_KEY_F23;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F24]           = DESKTOP_WINDOW_KEY_F24;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_F25]           = DESKTOP_WINDOW_KEY_F25;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_0]          = DESKTOP_WINDOW_KEY_KP_0;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_1]          = DESKTOP_WINDOW_KEY_KP_1;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_2]          = DESKTOP_WINDOW_KEY_KP_2;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_3]          = DESKTOP_WINDOW_KEY_KP_3;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_4]          = DESKTOP_WINDOW_KEY_KP_4;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_5]          = DESKTOP_WINDOW_KEY_KP_5;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_6]          = DESKTOP_WINDOW_KEY_KP_6;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_7]          = DESKTOP_WINDOW_KEY_KP_7;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_8]          = DESKTOP_WINDOW_KEY_KP_8;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_9]          = DESKTOP_WINDOW_KEY_KP_9;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_DECIMAL]    = DESKTOP_WINDOW_KEY_KP_DECIMAL;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_DIVIDE]     = DESKTOP_WINDOW_KEY_KP_DIVIDE;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_MULTIPLY]   = DESKTOP_WINDOW_KEY_KP_MULTIPLY;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_SUBTRACT]   = DESKTOP_WINDOW_KEY_KP_SUBTRACT;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_ADD]        = DESKTOP_WINDOW_KEY_KP_ADD;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_ENTER]      = DESKTOP_WINDOW_KEY_KP_ENTER;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_KP_EQUAL]      = DESKTOP_WINDOW_KEY_KP_EQUAL;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_LEFT_SHIFT]    = DESKTOP_WINDOW_KEY_LEFT_SHIFT;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_LEFT_CONTROL]  = DESKTOP_WINDOW_KEY_LEFT_CONTROL;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_LEFT_ALT]      = DESKTOP_WINDOW_KEY_LEFT_ALT;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_LEFT_SUPER]    = DESKTOP_WINDOW_KEY_LEFT_SUPER;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_RIGHT_SHIFT]   = DESKTOP_WINDOW_KEY_RIGHT_SHIFT;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_RIGHT_CONTROL] = DESKTOP_WINDOW_KEY_RIGHT_CONTROL;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_RIGHT_ALT]     = DESKTOP_WINDOW_KEY_RIGHT_ALT;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_RIGHT_SUPER]   = DESKTOP_WINDOW_KEY_RIGHT_SUPER;
    _desktop_window.null.keycodes[DESKTOP_WINDOW_NULL_SC_MENU]          = DESKTOP_WINDOW_KEY_MENU;

    for (scancode = DESKTOP_WINDOW_NULL_SC_FIRST;  scancode < DESKTOP_WINDOW_NULL_SC_LAST;  scancode++)
    {
        if (_desktop_window.null.keycodes[scancode] > 0)
            _desktop_window.null.scancodes[_desktop_window.null.keycodes[scancode]] = scancode;
    }

    _desktop_windowPollMonitorsNull();
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateNull(void)
{
    free(_desktop_window.null.clipboardString);
    _desktop_windowTerminateOSMesa();
    _desktop_windowTerminateEGL();
}

