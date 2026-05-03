#include "internal.h"

DESKTOP_WINDOWbool _desktop_windowInitNSGL(void) {
    _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                              "Native graphics context creation is disabled in this engine build");
    return DESKTOP_WINDOW_FALSE;
}

void _desktop_windowTerminateNSGL(void) {}

DESKTOP_WINDOWbool _desktop_windowCreateContextNSGL(_DESKTOP_WINDOWwindow* window,
                                                    const _DESKTOP_WINDOWctxconfig* ctxconfig,
                                                    const _DESKTOP_WINDOWfbconfig* fbconfig) {
    (void) window;
    (void) ctxconfig;
    (void) fbconfig;
    _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                              "Native graphics context creation is disabled in this engine build");
    return DESKTOP_WINDOW_FALSE;
}

void _desktop_windowDestroyContextNSGL(_DESKTOP_WINDOWwindow* window) {
    (void) window;
}

DESKTOP_WINDOWAPI id desktop_windowGetNSGLContext(DESKTOP_WINDOWwindow* handle) {
    (void) handle;
    return nil;
}
