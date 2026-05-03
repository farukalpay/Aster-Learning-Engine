#include "internal.h"

#if defined(_DESKTOP_WINDOW_WAYLAND)

#include <errno.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include "wayland-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"

// NOTE: Versions of wayland-scanner prior to 1.17.91 named every global array of
//       wl_interface pointers 'types', making it impossible to combine several unmodified
//       private-code files into a single compilation unit
// HACK: We override this name with a macro for each file, allowing them to coexist

#define types _desktop_window_wayland_types
#include "wayland-client-protocol-code.h"
#undef types

#define types _desktop_window_xdg_shell_types
#include "xdg-shell-client-protocol-code.h"
#undef types

#define types _desktop_window_xdg_decoration_types
#include "xdg-decoration-unstable-v1-client-protocol-code.h"
#undef types

#define types _desktop_window_viewporter_types
#include "viewporter-client-protocol-code.h"
#undef types

#define types _desktop_window_relative_pointer_types
#include "relative-pointer-unstable-v1-client-protocol-code.h"
#undef types

#define types _desktop_window_pointer_constraints_types
#include "pointer-constraints-unstable-v1-client-protocol-code.h"
#undef types

#define types _desktop_window_fractional_scale_types
#include "fractional-scale-v1-client-protocol-code.h"
#undef types

#define types _desktop_window_xdg_activation_types
#include "xdg-activation-v1-client-protocol-code.h"
#undef types

#define types _desktop_window_idle_inhibit_types
#include "idle-inhibit-unstable-v1-client-protocol-code.h"
#undef types

static void wmBaseHandlePing(void* userData,
                             struct xdg_wm_base* wmBase,
                             uint32_t serial)
{
    xdg_wm_base_pong(wmBase, serial);
}

static const struct xdg_wm_base_listener wmBaseListener =
{
    wmBaseHandlePing
};

static void registryHandleGlobal(void* userData,
                                 struct wl_registry* registry,
                                 uint32_t name,
                                 const char* interface,
                                 uint32_t version)
{
    if (strcmp(interface, "wl_compositor") == 0)
    {
        _desktop_window.wl.compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface,
                             _desktop_window_min(3, version));
    }
    else if (strcmp(interface, "wl_subcompositor") == 0)
    {
        _desktop_window.wl.subcompositor =
            wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
    }
    else if (strcmp(interface, "wl_shm") == 0)
    {
        _desktop_window.wl.shm =
            wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, "wl_output") == 0)
    {
        _desktop_windowAddOutputWayland(name, version);
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
        if (!_desktop_window.wl.seat)
        {
            _desktop_window.wl.seat =
                wl_registry_bind(registry, name, &wl_seat_interface,
                                 _desktop_window_min(4, version));
            _desktop_windowAddSeatListenerWayland(_desktop_window.wl.seat);
        }
    }
    else if (strcmp(interface, "wl_data_device_manager") == 0)
    {
        if (!_desktop_window.wl.dataDeviceManager)
        {
            _desktop_window.wl.dataDeviceManager =
                wl_registry_bind(registry, name,
                                 &wl_data_device_manager_interface, 1);
        }
    }
    else if (strcmp(interface, "xdg_wm_base") == 0)
    {
        _desktop_window.wl.wmBase =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(_desktop_window.wl.wmBase, &wmBaseListener, NULL);
    }
    else if (strcmp(interface, "zxdg_decoration_manager_v1") == 0)
    {
        _desktop_window.wl.decorationManager =
            wl_registry_bind(registry, name,
                             &zxdg_decoration_manager_v1_interface,
                             1);
    }
    else if (strcmp(interface, "wp_viewporter") == 0)
    {
        _desktop_window.wl.viewporter =
            wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
    }
    else if (strcmp(interface, "zwp_relative_pointer_manager_v1") == 0)
    {
        _desktop_window.wl.relativePointerManager =
            wl_registry_bind(registry, name,
                             &zwp_relative_pointer_manager_v1_interface,
                             1);
    }
    else if (strcmp(interface, "zwp_pointer_constraints_v1") == 0)
    {
        _desktop_window.wl.pointerConstraints =
            wl_registry_bind(registry, name,
                             &zwp_pointer_constraints_v1_interface,
                             1);
    }
    else if (strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0)
    {
        _desktop_window.wl.idleInhibitManager =
            wl_registry_bind(registry, name,
                             &zwp_idle_inhibit_manager_v1_interface,
                             1);
    }
    else if (strcmp(interface, "xdg_activation_v1") == 0)
    {
        _desktop_window.wl.activationManager =
            wl_registry_bind(registry, name,
                             &xdg_activation_v1_interface,
                             1);
    }
    else if (strcmp(interface, "wp_fractional_scale_manager_v1") == 0)
    {
        _desktop_window.wl.fractionalScaleManager =
            wl_registry_bind(registry, name,
                             &wp_fractional_scale_manager_v1_interface,
                             1);
    }
}

static void registryHandleGlobalRemove(void* userData,
                                       struct wl_registry* registry,
                                       uint32_t name)
{
    for (int i = 0; i < _desktop_window.monitorCount; ++i)
    {
        _DESKTOP_WINDOWmonitor* monitor = _desktop_window.monitors[i];
        if (monitor->wl.name == name)
        {
            _desktop_windowInputMonitor(monitor, DESKTOP_WINDOW_DISCONNECTED, 0);
            return;
        }
    }
}


static const struct wl_registry_listener registryListener =
{
    registryHandleGlobal,
    registryHandleGlobalRemove
};

void libdecorHandleError(struct libdecor* context,
                         enum libdecor_error error,
                         const char* message)
{
    _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                    "Wayland: libdecor error %u: %s",
                    error, message);
}

static const struct libdecor_interface libdecorInterface =
{
    libdecorHandleError
};

static void libdecorReadyCallback(void* userData,
                                  struct wl_callback* callback,
                                  uint32_t time)
{
    _desktop_window.wl.libdecor.ready = DESKTOP_WINDOW_TRUE;

    assert(_desktop_window.wl.libdecor.callback == callback);
    wl_callback_destroy(_desktop_window.wl.libdecor.callback);
    _desktop_window.wl.libdecor.callback = NULL;
}

static const struct wl_callback_listener libdecorReadyListener =
{
    libdecorReadyCallback
};

// Create key code translation tables
//
static void createKeyTables(void)
{
    memset(_desktop_window.wl.keycodes, -1, sizeof(_desktop_window.wl.keycodes));
    memset(_desktop_window.wl.scancodes, -1, sizeof(_desktop_window.wl.scancodes));

    _desktop_window.wl.keycodes[KEY_GRAVE]      = DESKTOP_WINDOW_KEY_GRAVE_ACCENT;
    _desktop_window.wl.keycodes[KEY_1]          = DESKTOP_WINDOW_KEY_1;
    _desktop_window.wl.keycodes[KEY_2]          = DESKTOP_WINDOW_KEY_2;
    _desktop_window.wl.keycodes[KEY_3]          = DESKTOP_WINDOW_KEY_3;
    _desktop_window.wl.keycodes[KEY_4]          = DESKTOP_WINDOW_KEY_4;
    _desktop_window.wl.keycodes[KEY_5]          = DESKTOP_WINDOW_KEY_5;
    _desktop_window.wl.keycodes[KEY_6]          = DESKTOP_WINDOW_KEY_6;
    _desktop_window.wl.keycodes[KEY_7]          = DESKTOP_WINDOW_KEY_7;
    _desktop_window.wl.keycodes[KEY_8]          = DESKTOP_WINDOW_KEY_8;
    _desktop_window.wl.keycodes[KEY_9]          = DESKTOP_WINDOW_KEY_9;
    _desktop_window.wl.keycodes[KEY_0]          = DESKTOP_WINDOW_KEY_0;
    _desktop_window.wl.keycodes[KEY_SPACE]      = DESKTOP_WINDOW_KEY_SPACE;
    _desktop_window.wl.keycodes[KEY_MINUS]      = DESKTOP_WINDOW_KEY_MINUS;
    _desktop_window.wl.keycodes[KEY_EQUAL]      = DESKTOP_WINDOW_KEY_EQUAL;
    _desktop_window.wl.keycodes[KEY_Q]          = DESKTOP_WINDOW_KEY_Q;
    _desktop_window.wl.keycodes[KEY_W]          = DESKTOP_WINDOW_KEY_W;
    _desktop_window.wl.keycodes[KEY_E]          = DESKTOP_WINDOW_KEY_E;
    _desktop_window.wl.keycodes[KEY_R]          = DESKTOP_WINDOW_KEY_R;
    _desktop_window.wl.keycodes[KEY_T]          = DESKTOP_WINDOW_KEY_T;
    _desktop_window.wl.keycodes[KEY_Y]          = DESKTOP_WINDOW_KEY_Y;
    _desktop_window.wl.keycodes[KEY_U]          = DESKTOP_WINDOW_KEY_U;
    _desktop_window.wl.keycodes[KEY_I]          = DESKTOP_WINDOW_KEY_I;
    _desktop_window.wl.keycodes[KEY_O]          = DESKTOP_WINDOW_KEY_O;
    _desktop_window.wl.keycodes[KEY_P]          = DESKTOP_WINDOW_KEY_P;
    _desktop_window.wl.keycodes[KEY_LEFTBRACE]  = DESKTOP_WINDOW_KEY_LEFT_BRACKET;
    _desktop_window.wl.keycodes[KEY_RIGHTBRACE] = DESKTOP_WINDOW_KEY_RIGHT_BRACKET;
    _desktop_window.wl.keycodes[KEY_A]          = DESKTOP_WINDOW_KEY_A;
    _desktop_window.wl.keycodes[KEY_S]          = DESKTOP_WINDOW_KEY_S;
    _desktop_window.wl.keycodes[KEY_D]          = DESKTOP_WINDOW_KEY_D;
    _desktop_window.wl.keycodes[KEY_F]          = DESKTOP_WINDOW_KEY_F;
    _desktop_window.wl.keycodes[KEY_G]          = DESKTOP_WINDOW_KEY_G;
    _desktop_window.wl.keycodes[KEY_H]          = DESKTOP_WINDOW_KEY_H;
    _desktop_window.wl.keycodes[KEY_J]          = DESKTOP_WINDOW_KEY_J;
    _desktop_window.wl.keycodes[KEY_K]          = DESKTOP_WINDOW_KEY_K;
    _desktop_window.wl.keycodes[KEY_L]          = DESKTOP_WINDOW_KEY_L;
    _desktop_window.wl.keycodes[KEY_SEMICOLON]  = DESKTOP_WINDOW_KEY_SEMICOLON;
    _desktop_window.wl.keycodes[KEY_APOSTROPHE] = DESKTOP_WINDOW_KEY_APOSTROPHE;
    _desktop_window.wl.keycodes[KEY_Z]          = DESKTOP_WINDOW_KEY_Z;
    _desktop_window.wl.keycodes[KEY_X]          = DESKTOP_WINDOW_KEY_X;
    _desktop_window.wl.keycodes[KEY_C]          = DESKTOP_WINDOW_KEY_C;
    _desktop_window.wl.keycodes[KEY_V]          = DESKTOP_WINDOW_KEY_V;
    _desktop_window.wl.keycodes[KEY_B]          = DESKTOP_WINDOW_KEY_B;
    _desktop_window.wl.keycodes[KEY_N]          = DESKTOP_WINDOW_KEY_N;
    _desktop_window.wl.keycodes[KEY_M]          = DESKTOP_WINDOW_KEY_M;
    _desktop_window.wl.keycodes[KEY_COMMA]      = DESKTOP_WINDOW_KEY_COMMA;
    _desktop_window.wl.keycodes[KEY_DOT]        = DESKTOP_WINDOW_KEY_PERIOD;
    _desktop_window.wl.keycodes[KEY_SLASH]      = DESKTOP_WINDOW_KEY_SLASH;
    _desktop_window.wl.keycodes[KEY_BACKSLASH]  = DESKTOP_WINDOW_KEY_BACKSLASH;
    _desktop_window.wl.keycodes[KEY_ESC]        = DESKTOP_WINDOW_KEY_ESCAPE;
    _desktop_window.wl.keycodes[KEY_TAB]        = DESKTOP_WINDOW_KEY_TAB;
    _desktop_window.wl.keycodes[KEY_LEFTSHIFT]  = DESKTOP_WINDOW_KEY_LEFT_SHIFT;
    _desktop_window.wl.keycodes[KEY_RIGHTSHIFT] = DESKTOP_WINDOW_KEY_RIGHT_SHIFT;
    _desktop_window.wl.keycodes[KEY_LEFTCTRL]   = DESKTOP_WINDOW_KEY_LEFT_CONTROL;
    _desktop_window.wl.keycodes[KEY_RIGHTCTRL]  = DESKTOP_WINDOW_KEY_RIGHT_CONTROL;
    _desktop_window.wl.keycodes[KEY_LEFTALT]    = DESKTOP_WINDOW_KEY_LEFT_ALT;
    _desktop_window.wl.keycodes[KEY_RIGHTALT]   = DESKTOP_WINDOW_KEY_RIGHT_ALT;
    _desktop_window.wl.keycodes[KEY_LEFTMETA]   = DESKTOP_WINDOW_KEY_LEFT_SUPER;
    _desktop_window.wl.keycodes[KEY_RIGHTMETA]  = DESKTOP_WINDOW_KEY_RIGHT_SUPER;
    _desktop_window.wl.keycodes[KEY_COMPOSE]    = DESKTOP_WINDOW_KEY_MENU;
    _desktop_window.wl.keycodes[KEY_NUMLOCK]    = DESKTOP_WINDOW_KEY_NUM_LOCK;
    _desktop_window.wl.keycodes[KEY_CAPSLOCK]   = DESKTOP_WINDOW_KEY_CAPS_LOCK;
    _desktop_window.wl.keycodes[KEY_PRINT]      = DESKTOP_WINDOW_KEY_PRINT_SCREEN;
    _desktop_window.wl.keycodes[KEY_SCROLLLOCK] = DESKTOP_WINDOW_KEY_SCROLL_LOCK;
    _desktop_window.wl.keycodes[KEY_PAUSE]      = DESKTOP_WINDOW_KEY_PAUSE;
    _desktop_window.wl.keycodes[KEY_DELETE]     = DESKTOP_WINDOW_KEY_DELETE;
    _desktop_window.wl.keycodes[KEY_BACKSPACE]  = DESKTOP_WINDOW_KEY_BACKSPACE;
    _desktop_window.wl.keycodes[KEY_ENTER]      = DESKTOP_WINDOW_KEY_ENTER;
    _desktop_window.wl.keycodes[KEY_HOME]       = DESKTOP_WINDOW_KEY_HOME;
    _desktop_window.wl.keycodes[KEY_END]        = DESKTOP_WINDOW_KEY_END;
    _desktop_window.wl.keycodes[KEY_PAGEUP]     = DESKTOP_WINDOW_KEY_PAGE_UP;
    _desktop_window.wl.keycodes[KEY_PAGEDOWN]   = DESKTOP_WINDOW_KEY_PAGE_DOWN;
    _desktop_window.wl.keycodes[KEY_INSERT]     = DESKTOP_WINDOW_KEY_INSERT;
    _desktop_window.wl.keycodes[KEY_LEFT]       = DESKTOP_WINDOW_KEY_LEFT;
    _desktop_window.wl.keycodes[KEY_RIGHT]      = DESKTOP_WINDOW_KEY_RIGHT;
    _desktop_window.wl.keycodes[KEY_DOWN]       = DESKTOP_WINDOW_KEY_DOWN;
    _desktop_window.wl.keycodes[KEY_UP]         = DESKTOP_WINDOW_KEY_UP;
    _desktop_window.wl.keycodes[KEY_F1]         = DESKTOP_WINDOW_KEY_F1;
    _desktop_window.wl.keycodes[KEY_F2]         = DESKTOP_WINDOW_KEY_F2;
    _desktop_window.wl.keycodes[KEY_F3]         = DESKTOP_WINDOW_KEY_F3;
    _desktop_window.wl.keycodes[KEY_F4]         = DESKTOP_WINDOW_KEY_F4;
    _desktop_window.wl.keycodes[KEY_F5]         = DESKTOP_WINDOW_KEY_F5;
    _desktop_window.wl.keycodes[KEY_F6]         = DESKTOP_WINDOW_KEY_F6;
    _desktop_window.wl.keycodes[KEY_F7]         = DESKTOP_WINDOW_KEY_F7;
    _desktop_window.wl.keycodes[KEY_F8]         = DESKTOP_WINDOW_KEY_F8;
    _desktop_window.wl.keycodes[KEY_F9]         = DESKTOP_WINDOW_KEY_F9;
    _desktop_window.wl.keycodes[KEY_F10]        = DESKTOP_WINDOW_KEY_F10;
    _desktop_window.wl.keycodes[KEY_F11]        = DESKTOP_WINDOW_KEY_F11;
    _desktop_window.wl.keycodes[KEY_F12]        = DESKTOP_WINDOW_KEY_F12;
    _desktop_window.wl.keycodes[KEY_F13]        = DESKTOP_WINDOW_KEY_F13;
    _desktop_window.wl.keycodes[KEY_F14]        = DESKTOP_WINDOW_KEY_F14;
    _desktop_window.wl.keycodes[KEY_F15]        = DESKTOP_WINDOW_KEY_F15;
    _desktop_window.wl.keycodes[KEY_F16]        = DESKTOP_WINDOW_KEY_F16;
    _desktop_window.wl.keycodes[KEY_F17]        = DESKTOP_WINDOW_KEY_F17;
    _desktop_window.wl.keycodes[KEY_F18]        = DESKTOP_WINDOW_KEY_F18;
    _desktop_window.wl.keycodes[KEY_F19]        = DESKTOP_WINDOW_KEY_F19;
    _desktop_window.wl.keycodes[KEY_F20]        = DESKTOP_WINDOW_KEY_F20;
    _desktop_window.wl.keycodes[KEY_F21]        = DESKTOP_WINDOW_KEY_F21;
    _desktop_window.wl.keycodes[KEY_F22]        = DESKTOP_WINDOW_KEY_F22;
    _desktop_window.wl.keycodes[KEY_F23]        = DESKTOP_WINDOW_KEY_F23;
    _desktop_window.wl.keycodes[KEY_F24]        = DESKTOP_WINDOW_KEY_F24;
    _desktop_window.wl.keycodes[KEY_KPSLASH]    = DESKTOP_WINDOW_KEY_KP_DIVIDE;
    _desktop_window.wl.keycodes[KEY_KPASTERISK] = DESKTOP_WINDOW_KEY_KP_MULTIPLY;
    _desktop_window.wl.keycodes[KEY_KPMINUS]    = DESKTOP_WINDOW_KEY_KP_SUBTRACT;
    _desktop_window.wl.keycodes[KEY_KPPLUS]     = DESKTOP_WINDOW_KEY_KP_ADD;
    _desktop_window.wl.keycodes[KEY_KP0]        = DESKTOP_WINDOW_KEY_KP_0;
    _desktop_window.wl.keycodes[KEY_KP1]        = DESKTOP_WINDOW_KEY_KP_1;
    _desktop_window.wl.keycodes[KEY_KP2]        = DESKTOP_WINDOW_KEY_KP_2;
    _desktop_window.wl.keycodes[KEY_KP3]        = DESKTOP_WINDOW_KEY_KP_3;
    _desktop_window.wl.keycodes[KEY_KP4]        = DESKTOP_WINDOW_KEY_KP_4;
    _desktop_window.wl.keycodes[KEY_KP5]        = DESKTOP_WINDOW_KEY_KP_5;
    _desktop_window.wl.keycodes[KEY_KP6]        = DESKTOP_WINDOW_KEY_KP_6;
    _desktop_window.wl.keycodes[KEY_KP7]        = DESKTOP_WINDOW_KEY_KP_7;
    _desktop_window.wl.keycodes[KEY_KP8]        = DESKTOP_WINDOW_KEY_KP_8;
    _desktop_window.wl.keycodes[KEY_KP9]        = DESKTOP_WINDOW_KEY_KP_9;
    _desktop_window.wl.keycodes[KEY_KPDOT]      = DESKTOP_WINDOW_KEY_KP_DECIMAL;
    _desktop_window.wl.keycodes[KEY_KPEQUAL]    = DESKTOP_WINDOW_KEY_KP_EQUAL;
    _desktop_window.wl.keycodes[KEY_KPENTER]    = DESKTOP_WINDOW_KEY_KP_ENTER;
    _desktop_window.wl.keycodes[KEY_102ND]      = DESKTOP_WINDOW_KEY_WORLD_2;

    for (int scancode = 0;  scancode < 256;  scancode++)
    {
        if (_desktop_window.wl.keycodes[scancode] > 0)
            _desktop_window.wl.scancodes[_desktop_window.wl.keycodes[scancode]] = scancode;
    }
}

static DESKTOP_WINDOWbool loadCursorTheme(void)
{
    int cursorSize = 16;

    const char* sizeString = getenv("XCURSOR_SIZE");
    if (sizeString)
    {
        errno = 0;
        const long cursorSizeLong = strtol(sizeString, NULL, 10);
        if (errno == 0 && cursorSizeLong > 0 && cursorSizeLong < INT_MAX)
            cursorSize = (int) cursorSizeLong;
    }

    const char* themeName = getenv("XCURSOR_THEME");

    _desktop_window.wl.cursorTheme = wl_cursor_theme_load(themeName, cursorSize, _desktop_window.wl.shm);
    if (!_desktop_window.wl.cursorTheme)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Failed to load default cursor theme");
        return DESKTOP_WINDOW_FALSE;
    }

    // If this happens to be NULL, we just fallback to the scale=1 version.
    _desktop_window.wl.cursorThemeHiDPI =
        wl_cursor_theme_load(themeName, cursorSize * 2, _desktop_window.wl.shm);

    _desktop_window.wl.cursorSurface = wl_compositor_create_surface(_desktop_window.wl.compositor);
    _desktop_window.wl.cursorTimerfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    return DESKTOP_WINDOW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowConnectWayland(int platformID, _DESKTOP_WINDOWplatform* platform)
{
    const _DESKTOP_WINDOWplatform wayland =
    {
        .platformID = DESKTOP_WINDOW_PLATFORM_WAYLAND,
        .init = _desktop_windowInitWayland,
        .terminate = _desktop_windowTerminateWayland,
        .getCursorPos = _desktop_windowGetCursorPosWayland,
        .setCursorPos = _desktop_windowSetCursorPosWayland,
        .setCursorMode = _desktop_windowSetCursorModeWayland,
        .setRawMouseMotion = _desktop_windowSetRawMouseMotionWayland,
        .rawMouseMotionSupported = _desktop_windowRawMouseMotionSupportedWayland,
        .createCursor = _desktop_windowCreateCursorWayland,
        .createStandardCursor = _desktop_windowCreateStandardCursorWayland,
        .destroyCursor = _desktop_windowDestroyCursorWayland,
        .setCursor = _desktop_windowSetCursorWayland,
        .getScancodeName = _desktop_windowGetScancodeNameWayland,
        .getKeyScancode = _desktop_windowGetKeyScancodeWayland,
        .setClipboardString = _desktop_windowSetClipboardStringWayland,
        .getClipboardString = _desktop_windowGetClipboardStringWayland,
#if defined(DESKTOP_WINDOW_BUILD_LINUX_JOYSTICK)
        .initJoysticks = _desktop_windowInitJoysticksLinux,
        .terminateJoysticks = _desktop_windowTerminateJoysticksLinux,
        .pollJoystick = _desktop_windowPollJoystickLinux,
        .getMappingName = _desktop_windowGetMappingNameLinux,
        .updateGamepadGUID = _desktop_windowUpdateGamepadGUIDLinux,
#else
        .initJoysticks = _desktop_windowInitJoysticksNull,
        .terminateJoysticks = _desktop_windowTerminateJoysticksNull,
        .pollJoystick = _desktop_windowPollJoystickNull,
        .getMappingName = _desktop_windowGetMappingNameNull,
        .updateGamepadGUID = _desktop_windowUpdateGamepadGUIDNull,
#endif
        .freeMonitor = _desktop_windowFreeMonitorWayland,
        .getMonitorPos = _desktop_windowGetMonitorPosWayland,
        .getMonitorContentScale = _desktop_windowGetMonitorContentScaleWayland,
        .getMonitorWorkarea = _desktop_windowGetMonitorWorkareaWayland,
        .getVideoModes = _desktop_windowGetVideoModesWayland,
        .getVideoMode = _desktop_windowGetVideoModeWayland,
        .getGammaRamp = _desktop_windowGetGammaRampWayland,
        .setGammaRamp = _desktop_windowSetGammaRampWayland,
        .createWindow = _desktop_windowCreateWindowWayland,
        .destroyWindow = _desktop_windowDestroyWindowWayland,
        .setWindowTitle = _desktop_windowSetWindowTitleWayland,
        .setWindowIcon = _desktop_windowSetWindowIconWayland,
        .getWindowPos = _desktop_windowGetWindowPosWayland,
        .setWindowPos = _desktop_windowSetWindowPosWayland,
        .getWindowSize = _desktop_windowGetWindowSizeWayland,
        .setWindowSize = _desktop_windowSetWindowSizeWayland,
        .setWindowSizeLimits = _desktop_windowSetWindowSizeLimitsWayland,
        .setWindowAspectRatio = _desktop_windowSetWindowAspectRatioWayland,
        .getFramebufferSize = _desktop_windowGetFramebufferSizeWayland,
        .getWindowFrameSize = _desktop_windowGetWindowFrameSizeWayland,
        .getWindowContentScale = _desktop_windowGetWindowContentScaleWayland,
        .iconifyWindow = _desktop_windowIconifyWindowWayland,
        .restoreWindow = _desktop_windowRestoreWindowWayland,
        .maximizeWindow = _desktop_windowMaximizeWindowWayland,
        .showWindow = _desktop_windowShowWindowWayland,
        .hideWindow = _desktop_windowHideWindowWayland,
        .requestWindowAttention = _desktop_windowRequestWindowAttentionWayland,
        .focusWindow = _desktop_windowFocusWindowWayland,
        .setWindowMonitor = _desktop_windowSetWindowMonitorWayland,
        .windowFocused = _desktop_windowWindowFocusedWayland,
        .windowIconified = _desktop_windowWindowIconifiedWayland,
        .windowVisible = _desktop_windowWindowVisibleWayland,
        .windowMaximized = _desktop_windowWindowMaximizedWayland,
        .windowHovered = _desktop_windowWindowHoveredWayland,
        .framebufferTransparent = _desktop_windowFramebufferTransparentWayland,
        .getWindowOpacity = _desktop_windowGetWindowOpacityWayland,
        .setWindowResizable = _desktop_windowSetWindowResizableWayland,
        .setWindowDecorated = _desktop_windowSetWindowDecoratedWayland,
        .setWindowFloating = _desktop_windowSetWindowFloatingWayland,
        .setWindowOpacity = _desktop_windowSetWindowOpacityWayland,
        .setWindowMousePassthrough = _desktop_windowSetWindowMousePassthroughWayland,
        .pollEvents = _desktop_windowPollEventsWayland,
        .waitEvents = _desktop_windowWaitEventsWayland,
        .waitEventsTimeout = _desktop_windowWaitEventsTimeoutWayland,
        .postEmptyEvent = _desktop_windowPostEmptyEventWayland,
        .getEGLPlatform = _desktop_windowGetEGLPlatformWayland,
        .getEGLNativeDisplay = _desktop_windowGetEGLNativeDisplayWayland,
        .getEGLNativeWindow = _desktop_windowGetEGLNativeWindowWayland,
        .getRequiredInstanceExtensions = _desktop_windowGetRequiredInstanceExtensionsWayland,
        .getPhysicalDevicePresentationSupport = _desktop_windowGetPhysicalDevicePresentationSupportWayland,
        .createWindowSurface = _desktop_windowCreateWindowSurfaceWayland
    };

    void* module = _desktop_windowPlatformLoadModule("libwayland-client.so.0");
    if (!module)
    {
        if (platformID == DESKTOP_WINDOW_PLATFORM_WAYLAND)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "Wayland: Failed to load libwayland-client");
        }

        return DESKTOP_WINDOW_FALSE;
    }

    PFN_wl_display_connect wl_display_connect = (PFN_wl_display_connect)
        _desktop_windowPlatformGetModuleSymbol(module, "wl_display_connect");
    if (!wl_display_connect)
    {
        if (platformID == DESKTOP_WINDOW_PLATFORM_WAYLAND)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "Wayland: Failed to load libwayland-client entry point");
        }

        _desktop_windowPlatformFreeModule(module);
        return DESKTOP_WINDOW_FALSE;
    }

    struct wl_display* display = wl_display_connect(NULL);
    if (!display)
    {
        if (platformID == DESKTOP_WINDOW_PLATFORM_WAYLAND)
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "Wayland: Failed to connect to display");

        _desktop_windowPlatformFreeModule(module);
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.wl.display = display;
    _desktop_window.wl.client.handle = module;

    *platform = wayland;
    return DESKTOP_WINDOW_TRUE;
}

int _desktop_windowInitWayland(void)
{
    // These must be set before any failure checks
    _desktop_window.wl.keyRepeatTimerfd = -1;
    _desktop_window.wl.cursorTimerfd = -1;

    _desktop_window.wl.tag = desktop_windowGetVersionString();

    _desktop_window.wl.client.display_flush = (PFN_wl_display_flush)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_display_flush");
    _desktop_window.wl.client.display_cancel_read = (PFN_wl_display_cancel_read)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_display_cancel_read");
    _desktop_window.wl.client.display_dispatch_pending = (PFN_wl_display_dispatch_pending)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_display_dispatch_pending");
    _desktop_window.wl.client.display_read_events = (PFN_wl_display_read_events)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_display_read_events");
    _desktop_window.wl.client.display_disconnect = (PFN_wl_display_disconnect)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_display_disconnect");
    _desktop_window.wl.client.display_roundtrip = (PFN_wl_display_roundtrip)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_display_roundtrip");
    _desktop_window.wl.client.display_get_fd = (PFN_wl_display_get_fd)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_display_get_fd");
    _desktop_window.wl.client.display_prepare_read = (PFN_wl_display_prepare_read)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_display_prepare_read");
    _desktop_window.wl.client.proxy_marshal = (PFN_wl_proxy_marshal)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_marshal");
    _desktop_window.wl.client.proxy_add_listener = (PFN_wl_proxy_add_listener)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_add_listener");
    _desktop_window.wl.client.proxy_destroy = (PFN_wl_proxy_destroy)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_destroy");
    _desktop_window.wl.client.proxy_marshal_constructor = (PFN_wl_proxy_marshal_constructor)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_marshal_constructor");
    _desktop_window.wl.client.proxy_marshal_constructor_versioned = (PFN_wl_proxy_marshal_constructor_versioned)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_marshal_constructor_versioned");
    _desktop_window.wl.client.proxy_get_user_data = (PFN_wl_proxy_get_user_data)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_get_user_data");
    _desktop_window.wl.client.proxy_set_user_data = (PFN_wl_proxy_set_user_data)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_set_user_data");
    _desktop_window.wl.client.proxy_get_tag = (PFN_wl_proxy_get_tag)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_get_tag");
    _desktop_window.wl.client.proxy_set_tag = (PFN_wl_proxy_set_tag)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_set_tag");
    _desktop_window.wl.client.proxy_get_version = (PFN_wl_proxy_get_version)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_get_version");
    _desktop_window.wl.client.proxy_marshal_flags = (PFN_wl_proxy_marshal_flags)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.client.handle, "wl_proxy_marshal_flags");

    if (!_desktop_window.wl.client.display_flush ||
        !_desktop_window.wl.client.display_cancel_read ||
        !_desktop_window.wl.client.display_dispatch_pending ||
        !_desktop_window.wl.client.display_read_events ||
        !_desktop_window.wl.client.display_disconnect ||
        !_desktop_window.wl.client.display_roundtrip ||
        !_desktop_window.wl.client.display_get_fd ||
        !_desktop_window.wl.client.display_prepare_read ||
        !_desktop_window.wl.client.proxy_marshal ||
        !_desktop_window.wl.client.proxy_add_listener ||
        !_desktop_window.wl.client.proxy_destroy ||
        !_desktop_window.wl.client.proxy_marshal_constructor ||
        !_desktop_window.wl.client.proxy_marshal_constructor_versioned ||
        !_desktop_window.wl.client.proxy_get_user_data ||
        !_desktop_window.wl.client.proxy_set_user_data ||
        !_desktop_window.wl.client.proxy_get_tag ||
        !_desktop_window.wl.client.proxy_set_tag)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Failed to load libwayland-client entry point");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.wl.cursor.handle = _desktop_windowPlatformLoadModule("libwayland-cursor.so.0");
    if (!_desktop_window.wl.cursor.handle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Failed to load libwayland-cursor");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.wl.cursor.theme_load = (PFN_wl_cursor_theme_load)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.cursor.handle, "wl_cursor_theme_load");
    _desktop_window.wl.cursor.theme_destroy = (PFN_wl_cursor_theme_destroy)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.cursor.handle, "wl_cursor_theme_destroy");
    _desktop_window.wl.cursor.theme_get_cursor = (PFN_wl_cursor_theme_get_cursor)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.cursor.handle, "wl_cursor_theme_get_cursor");
    _desktop_window.wl.cursor.image_get_buffer = (PFN_wl_cursor_image_get_buffer)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.cursor.handle, "wl_cursor_image_get_buffer");

    _desktop_window.wl.egl.handle = _desktop_windowPlatformLoadModule("libwayland-egl.so.1");
    if (!_desktop_window.wl.egl.handle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Failed to load libwayland-egl");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.wl.egl.window_create = (PFN_wl_egl_window_create)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.egl.handle, "wl_egl_window_create");
    _desktop_window.wl.egl.window_destroy = (PFN_wl_egl_window_destroy)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.egl.handle, "wl_egl_window_destroy");
    _desktop_window.wl.egl.window_resize = (PFN_wl_egl_window_resize)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.egl.handle, "wl_egl_window_resize");

    _desktop_window.wl.xkb.handle = _desktop_windowPlatformLoadModule("libxkbcommon.so.0");
    if (!_desktop_window.wl.xkb.handle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Failed to load libxkbcommon");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.wl.xkb.context_new = (PFN_xkb_context_new)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_context_new");
    _desktop_window.wl.xkb.context_unref = (PFN_xkb_context_unref)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_context_unref");
    _desktop_window.wl.xkb.keymap_new_from_string = (PFN_xkb_keymap_new_from_string)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_keymap_new_from_string");
    _desktop_window.wl.xkb.keymap_unref = (PFN_xkb_keymap_unref)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_keymap_unref");
    _desktop_window.wl.xkb.keymap_mod_get_index = (PFN_xkb_keymap_mod_get_index)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_keymap_mod_get_index");
    _desktop_window.wl.xkb.keymap_key_repeats = (PFN_xkb_keymap_key_repeats)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_keymap_key_repeats");
    _desktop_window.wl.xkb.keymap_key_get_syms_by_level = (PFN_xkb_keymap_key_get_syms_by_level)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_keymap_key_get_syms_by_level");
    _desktop_window.wl.xkb.state_new = (PFN_xkb_state_new)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_state_new");
    _desktop_window.wl.xkb.state_unref = (PFN_xkb_state_unref)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_state_unref");
    _desktop_window.wl.xkb.state_key_get_syms = (PFN_xkb_state_key_get_syms)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_state_key_get_syms");
    _desktop_window.wl.xkb.state_update_mask = (PFN_xkb_state_update_mask)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_state_update_mask");
    _desktop_window.wl.xkb.state_key_get_layout = (PFN_xkb_state_key_get_layout)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_state_key_get_layout");
    _desktop_window.wl.xkb.state_mod_index_is_active = (PFN_xkb_state_mod_index_is_active)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_state_mod_index_is_active");
    _desktop_window.wl.xkb.compose_table_new_from_locale = (PFN_xkb_compose_table_new_from_locale)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_compose_table_new_from_locale");
    _desktop_window.wl.xkb.compose_table_unref = (PFN_xkb_compose_table_unref)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_compose_table_unref");
    _desktop_window.wl.xkb.compose_state_new = (PFN_xkb_compose_state_new)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_compose_state_new");
    _desktop_window.wl.xkb.compose_state_unref = (PFN_xkb_compose_state_unref)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_compose_state_unref");
    _desktop_window.wl.xkb.compose_state_feed = (PFN_xkb_compose_state_feed)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_compose_state_feed");
    _desktop_window.wl.xkb.compose_state_get_status = (PFN_xkb_compose_state_get_status)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_compose_state_get_status");
    _desktop_window.wl.xkb.compose_state_get_one_sym = (PFN_xkb_compose_state_get_one_sym)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.xkb.handle, "xkb_compose_state_get_one_sym");

    if (!_desktop_window.wl.xkb.context_new ||
        !_desktop_window.wl.xkb.context_unref ||
        !_desktop_window.wl.xkb.keymap_new_from_string ||
        !_desktop_window.wl.xkb.keymap_unref ||
        !_desktop_window.wl.xkb.keymap_mod_get_index ||
        !_desktop_window.wl.xkb.keymap_key_repeats ||
        !_desktop_window.wl.xkb.keymap_key_get_syms_by_level ||
        !_desktop_window.wl.xkb.state_new ||
        !_desktop_window.wl.xkb.state_unref ||
        !_desktop_window.wl.xkb.state_key_get_syms ||
        !_desktop_window.wl.xkb.state_update_mask ||
        !_desktop_window.wl.xkb.state_key_get_layout ||
        !_desktop_window.wl.xkb.state_mod_index_is_active ||
        !_desktop_window.wl.xkb.compose_table_new_from_locale ||
        !_desktop_window.wl.xkb.compose_table_unref ||
        !_desktop_window.wl.xkb.compose_state_new ||
        !_desktop_window.wl.xkb.compose_state_unref ||
        !_desktop_window.wl.xkb.compose_state_feed ||
        !_desktop_window.wl.xkb.compose_state_get_status ||
        !_desktop_window.wl.xkb.compose_state_get_one_sym)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Failed to load all entry points from libxkbcommon");
        return DESKTOP_WINDOW_FALSE;
    }

    if (_desktop_window.hints.init.wl.libdecorMode == DESKTOP_WINDOW_WAYLAND_PREFER_LIBDECOR)
        _desktop_window.wl.libdecor.handle = _desktop_windowPlatformLoadModule("libdecor-0.so.0");

    if (_desktop_window.wl.libdecor.handle)
    {
        _desktop_window.wl.libdecor.libdecor_new_ = (PFN_libdecor_new)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_new");
        _desktop_window.wl.libdecor.libdecor_unref_ = (PFN_libdecor_unref)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_unref");
        _desktop_window.wl.libdecor.libdecor_get_fd_ = (PFN_libdecor_get_fd)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_get_fd");
        _desktop_window.wl.libdecor.libdecor_dispatch_ = (PFN_libdecor_dispatch)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_dispatch");
        _desktop_window.wl.libdecor.libdecor_decorate_ = (PFN_libdecor_decorate)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_decorate");
        _desktop_window.wl.libdecor.libdecor_frame_unref_ = (PFN_libdecor_frame_unref)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_unref");
        _desktop_window.wl.libdecor.libdecor_frame_set_app_id_ = (PFN_libdecor_frame_set_app_id)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_set_app_id");
        _desktop_window.wl.libdecor.libdecor_frame_set_title_ = (PFN_libdecor_frame_set_title)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_set_title");
        _desktop_window.wl.libdecor.libdecor_frame_set_minimized_ = (PFN_libdecor_frame_set_minimized)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_set_minimized");
        _desktop_window.wl.libdecor.libdecor_frame_set_fullscreen_ = (PFN_libdecor_frame_set_fullscreen)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_set_fullscreen");
        _desktop_window.wl.libdecor.libdecor_frame_unset_fullscreen_ = (PFN_libdecor_frame_unset_fullscreen)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_unset_fullscreen");
        _desktop_window.wl.libdecor.libdecor_frame_map_ = (PFN_libdecor_frame_map)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_map");
        _desktop_window.wl.libdecor.libdecor_frame_commit_ = (PFN_libdecor_frame_commit)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_commit");
        _desktop_window.wl.libdecor.libdecor_frame_set_min_content_size_ = (PFN_libdecor_frame_set_min_content_size)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_set_min_content_size");
        _desktop_window.wl.libdecor.libdecor_frame_set_max_content_size_ = (PFN_libdecor_frame_set_max_content_size)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_set_max_content_size");
        _desktop_window.wl.libdecor.libdecor_frame_set_maximized_ = (PFN_libdecor_frame_set_maximized)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_set_maximized");
        _desktop_window.wl.libdecor.libdecor_frame_unset_maximized_ = (PFN_libdecor_frame_unset_maximized)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_unset_maximized");
        _desktop_window.wl.libdecor.libdecor_frame_set_capabilities_ = (PFN_libdecor_frame_set_capabilities)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_set_capabilities");
        _desktop_window.wl.libdecor.libdecor_frame_unset_capabilities_ = (PFN_libdecor_frame_unset_capabilities)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_unset_capabilities");
        _desktop_window.wl.libdecor.libdecor_frame_set_visibility_ = (PFN_libdecor_frame_set_visibility)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_set_visibility");
        _desktop_window.wl.libdecor.libdecor_frame_get_xdg_toplevel_ = (PFN_libdecor_frame_get_xdg_toplevel)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_frame_get_xdg_toplevel");
        _desktop_window.wl.libdecor.libdecor_configuration_get_content_size_ = (PFN_libdecor_configuration_get_content_size)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_configuration_get_content_size");
        _desktop_window.wl.libdecor.libdecor_configuration_get_window_state_ = (PFN_libdecor_configuration_get_window_state)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_configuration_get_window_state");
        _desktop_window.wl.libdecor.libdecor_state_new_ = (PFN_libdecor_state_new)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_state_new");
        _desktop_window.wl.libdecor.libdecor_state_free_ = (PFN_libdecor_state_free)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.wl.libdecor.handle, "libdecor_state_free");

        if (!_desktop_window.wl.libdecor.libdecor_new_ ||
            !_desktop_window.wl.libdecor.libdecor_unref_ ||
            !_desktop_window.wl.libdecor.libdecor_get_fd_ ||
            !_desktop_window.wl.libdecor.libdecor_dispatch_ ||
            !_desktop_window.wl.libdecor.libdecor_decorate_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_unref_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_set_app_id_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_set_title_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_set_minimized_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_set_fullscreen_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_unset_fullscreen_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_map_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_commit_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_set_min_content_size_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_set_max_content_size_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_set_maximized_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_unset_maximized_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_set_capabilities_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_unset_capabilities_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_set_visibility_ ||
            !_desktop_window.wl.libdecor.libdecor_frame_get_xdg_toplevel_ ||
            !_desktop_window.wl.libdecor.libdecor_configuration_get_content_size_ ||
            !_desktop_window.wl.libdecor.libdecor_configuration_get_window_state_ ||
            !_desktop_window.wl.libdecor.libdecor_state_new_ ||
            !_desktop_window.wl.libdecor.libdecor_state_free_)
        {
            _desktop_windowPlatformFreeModule(_desktop_window.wl.libdecor.handle);
            memset(&_desktop_window.wl.libdecor, 0, sizeof(_desktop_window.wl.libdecor));
        }
    }

    _desktop_window.wl.registry = wl_display_get_registry(_desktop_window.wl.display);
    wl_registry_add_listener(_desktop_window.wl.registry, &registryListener, NULL);

    createKeyTables();

    _desktop_window.wl.xkb.context = xkb_context_new(0);
    if (!_desktop_window.wl.xkb.context)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Failed to initialize xkb context");
        return DESKTOP_WINDOW_FALSE;
    }

    // Sync so we got all registry objects
    wl_display_roundtrip(_desktop_window.wl.display);

    // Sync so we got all initial output events
    wl_display_roundtrip(_desktop_window.wl.display);

    if (_desktop_window.wl.libdecor.handle)
    {
        _desktop_window.wl.libdecor.context = libdecor_new(_desktop_window.wl.display, &libdecorInterface);
        if (_desktop_window.wl.libdecor.context)
        {
            // Perform an initial dispatch and flush to get the init started
            libdecor_dispatch(_desktop_window.wl.libdecor.context, 0);

            // Create sync point to "know" when libdecor is ready for use
            _desktop_window.wl.libdecor.callback = wl_display_sync(_desktop_window.wl.display);
            wl_callback_add_listener(_desktop_window.wl.libdecor.callback,
                                     &libdecorReadyListener,
                                     NULL);
        }
    }

    if (wl_seat_get_version(_desktop_window.wl.seat) >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
    {
        _desktop_window.wl.keyRepeatTimerfd =
            timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    }

    if (!_desktop_window.wl.wmBase)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Failed to find xdg-shell in your compositor");
        return DESKTOP_WINDOW_FALSE;
    }

    if (!_desktop_window.wl.shm)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Wayland: Failed to find wl_shm in your compositor");
        return DESKTOP_WINDOW_FALSE;
    }

    if (!loadCursorTheme())
        return DESKTOP_WINDOW_FALSE;

    if (_desktop_window.wl.seat && _desktop_window.wl.dataDeviceManager)
    {
        _desktop_window.wl.dataDevice =
            wl_data_device_manager_get_data_device(_desktop_window.wl.dataDeviceManager,
                                                   _desktop_window.wl.seat);
        _desktop_windowAddDataDeviceListenerWayland(_desktop_window.wl.dataDevice);
    }

    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateWayland(void)
{
    _desktop_windowTerminateEGL();
    _desktop_windowTerminateOSMesa();

    if (_desktop_window.wl.libdecor.context)
    {
        // Allow libdecor to finish receiving all its requested globals
        // and ensure the associated sync callback object is destroyed
        while (!_desktop_window.wl.libdecor.ready)
            _desktop_windowWaitEventsWayland();

        libdecor_unref(_desktop_window.wl.libdecor.context);
    }

    if (_desktop_window.wl.libdecor.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.wl.libdecor.handle);
        _desktop_window.wl.libdecor.handle = NULL;
    }

    if (_desktop_window.wl.egl.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.wl.egl.handle);
        _desktop_window.wl.egl.handle = NULL;
    }

    if (_desktop_window.wl.xkb.composeState)
        xkb_compose_state_unref(_desktop_window.wl.xkb.composeState);
    if (_desktop_window.wl.xkb.keymap)
        xkb_keymap_unref(_desktop_window.wl.xkb.keymap);
    if (_desktop_window.wl.xkb.state)
        xkb_state_unref(_desktop_window.wl.xkb.state);
    if (_desktop_window.wl.xkb.context)
        xkb_context_unref(_desktop_window.wl.xkb.context);
    if (_desktop_window.wl.xkb.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.wl.xkb.handle);
        _desktop_window.wl.xkb.handle = NULL;
    }

    if (_desktop_window.wl.cursorTheme)
        wl_cursor_theme_destroy(_desktop_window.wl.cursorTheme);
    if (_desktop_window.wl.cursorThemeHiDPI)
        wl_cursor_theme_destroy(_desktop_window.wl.cursorThemeHiDPI);
    if (_desktop_window.wl.cursor.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.wl.cursor.handle);
        _desktop_window.wl.cursor.handle = NULL;
    }

    for (unsigned int i = 0; i < _desktop_window.wl.offerCount; i++)
        wl_data_offer_destroy(_desktop_window.wl.offers[i].offer);

    _desktop_window_free(_desktop_window.wl.offers);

    if (_desktop_window.wl.cursorSurface)
        wl_surface_destroy(_desktop_window.wl.cursorSurface);
    if (_desktop_window.wl.subcompositor)
        wl_subcompositor_destroy(_desktop_window.wl.subcompositor);
    if (_desktop_window.wl.compositor)
        wl_compositor_destroy(_desktop_window.wl.compositor);
    if (_desktop_window.wl.shm)
        wl_shm_destroy(_desktop_window.wl.shm);
    if (_desktop_window.wl.viewporter)
        wp_viewporter_destroy(_desktop_window.wl.viewporter);
    if (_desktop_window.wl.decorationManager)
        zxdg_decoration_manager_v1_destroy(_desktop_window.wl.decorationManager);
    if (_desktop_window.wl.wmBase)
        xdg_wm_base_destroy(_desktop_window.wl.wmBase);
    if (_desktop_window.wl.selectionOffer)
        wl_data_offer_destroy(_desktop_window.wl.selectionOffer);
    if (_desktop_window.wl.dragOffer)
        wl_data_offer_destroy(_desktop_window.wl.dragOffer);
    if (_desktop_window.wl.selectionSource)
        wl_data_source_destroy(_desktop_window.wl.selectionSource);
    if (_desktop_window.wl.dataDevice)
        wl_data_device_destroy(_desktop_window.wl.dataDevice);
    if (_desktop_window.wl.dataDeviceManager)
        wl_data_device_manager_destroy(_desktop_window.wl.dataDeviceManager);
    if (_desktop_window.wl.pointer)
        wl_pointer_destroy(_desktop_window.wl.pointer);
    if (_desktop_window.wl.keyboard)
        wl_keyboard_destroy(_desktop_window.wl.keyboard);
    if (_desktop_window.wl.seat)
        wl_seat_destroy(_desktop_window.wl.seat);
    if (_desktop_window.wl.relativePointerManager)
        zwp_relative_pointer_manager_v1_destroy(_desktop_window.wl.relativePointerManager);
    if (_desktop_window.wl.pointerConstraints)
        zwp_pointer_constraints_v1_destroy(_desktop_window.wl.pointerConstraints);
    if (_desktop_window.wl.idleInhibitManager)
        zwp_idle_inhibit_manager_v1_destroy(_desktop_window.wl.idleInhibitManager);
    if (_desktop_window.wl.activationManager)
        xdg_activation_v1_destroy(_desktop_window.wl.activationManager);
    if (_desktop_window.wl.fractionalScaleManager)
        wp_fractional_scale_manager_v1_destroy(_desktop_window.wl.fractionalScaleManager);
    if (_desktop_window.wl.registry)
        wl_registry_destroy(_desktop_window.wl.registry);
    if (_desktop_window.wl.display)
    {
        wl_display_flush(_desktop_window.wl.display);
        wl_display_disconnect(_desktop_window.wl.display);
    }

    if (_desktop_window.wl.keyRepeatTimerfd >= 0)
        close(_desktop_window.wl.keyRepeatTimerfd);
    if (_desktop_window.wl.cursorTimerfd >= 0)
        close(_desktop_window.wl.cursorTimerfd);

    _desktop_window_free(_desktop_window.wl.clipboardString);
}

#endif // _DESKTOP_WINDOW_WAYLAND

