#include "internal.h"

static void reportDisabledContext(void) {
    _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                              "Native GPU presentation owns graphics context creation in this engine build");
}

DESKTOP_WINDOWbool _desktop_windowInitEGL(void) {
    reportDisabledContext();
    return DESKTOP_WINDOW_FALSE;
}

void _desktop_windowTerminateEGL(void) {}

DESKTOP_WINDOWbool _desktop_windowCreateContextEGL(_DESKTOP_WINDOWwindow* window,
                                                   const _DESKTOP_WINDOWctxconfig* ctxconfig,
                                                   const _DESKTOP_WINDOWfbconfig* fbconfig) {
    (void) window;
    (void) ctxconfig;
    (void) fbconfig;
    reportDisabledContext();
    return DESKTOP_WINDOW_FALSE;
}

DESKTOP_WINDOWbool _desktop_windowInitOSMesa(void) {
    reportDisabledContext();
    return DESKTOP_WINDOW_FALSE;
}

void _desktop_windowTerminateOSMesa(void) {}

DESKTOP_WINDOWbool _desktop_windowCreateContextOSMesa(_DESKTOP_WINDOWwindow* window,
                                                      const _DESKTOP_WINDOWctxconfig* ctxconfig,
                                                      const _DESKTOP_WINDOWfbconfig* fbconfig) {
    (void) window;
    (void) ctxconfig;
    (void) fbconfig;
    reportDisabledContext();
    return DESKTOP_WINDOW_FALSE;
}
