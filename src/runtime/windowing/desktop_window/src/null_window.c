#include "internal.h"

#include <stdlib.h>

static void applySizeLimits(_DESKTOP_WINDOWwindow* window, int* width, int* height)
{
    if (window->numer != DESKTOP_WINDOW_DONT_CARE && window->denom != DESKTOP_WINDOW_DONT_CARE)
    {
        const float ratio = (float) window->numer / (float) window->denom;
        *height = (int) (*width / ratio);
    }

    if (window->minwidth != DESKTOP_WINDOW_DONT_CARE)
        *width = _desktop_window_max(*width, window->minwidth);
    else if (window->maxwidth != DESKTOP_WINDOW_DONT_CARE)
        *width = _desktop_window_min(*width, window->maxwidth);

    if (window->minheight != DESKTOP_WINDOW_DONT_CARE)
        *height = _desktop_window_min(*height, window->minheight);
    else if (window->maxheight != DESKTOP_WINDOW_DONT_CARE)
        *height = _desktop_window_max(*height, window->maxheight);
}

static void fitToMonitor(_DESKTOP_WINDOWwindow* window)
{
    DESKTOP_WINDOWvidmode mode;
    _desktop_windowGetVideoModeNull(window->monitor, &mode);
    _desktop_windowGetMonitorPosNull(window->monitor,
                           &window->null.xpos,
                           &window->null.ypos);
    window->null.width = mode.width;
    window->null.height = mode.height;
}

static void acquireMonitor(_DESKTOP_WINDOWwindow* window)
{
    _desktop_windowInputMonitorWindow(window->monitor, window);
}

static void releaseMonitor(_DESKTOP_WINDOWwindow* window)
{
    if (window->monitor->window != window)
        return;

    _desktop_windowInputMonitorWindow(window->monitor, NULL);
}

static int createNativeWindow(_DESKTOP_WINDOWwindow* window,
                              const _DESKTOP_WINDOWwndconfig* wndconfig,
                              const _DESKTOP_WINDOWfbconfig* fbconfig)
{
    if (window->monitor)
        fitToMonitor(window);
    else
    {
        if (wndconfig->xpos == DESKTOP_WINDOW_ANY_POSITION && wndconfig->ypos == DESKTOP_WINDOW_ANY_POSITION)
        {
            window->null.xpos = 17;
            window->null.ypos = 17;
        }
        else
        {
            window->null.xpos = wndconfig->xpos;
            window->null.ypos = wndconfig->ypos;
        }

        window->null.width = wndconfig->width;
        window->null.height = wndconfig->height;
    }

    window->null.visible = wndconfig->visible;
    window->null.decorated = wndconfig->decorated;
    window->null.maximized = wndconfig->maximized;
    window->null.floating = wndconfig->floating;
    window->null.transparent = fbconfig->transparent;
    window->null.opacity = 1.f;

    return DESKTOP_WINDOW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowCreateWindowNull(_DESKTOP_WINDOWwindow* window,
                               const _DESKTOP_WINDOWwndconfig* wndconfig,
                               const _DESKTOP_WINDOWctxconfig* ctxconfig,
                               const _DESKTOP_WINDOWfbconfig* fbconfig)
{
    if (!createNativeWindow(window, wndconfig, fbconfig))
        return DESKTOP_WINDOW_FALSE;

    if (ctxconfig->client != DESKTOP_WINDOW_NO_API)
    {
        if (ctxconfig->source == DESKTOP_WINDOW_NATIVE_CONTEXT_API ||
            ctxconfig->source == DESKTOP_WINDOW_OSMESA_CONTEXT_API)
        {
            if (!_desktop_windowInitOSMesa())
                return DESKTOP_WINDOW_FALSE;
            if (!_desktop_windowCreateContextOSMesa(window, ctxconfig, fbconfig))
                return DESKTOP_WINDOW_FALSE;
        }
        else if (ctxconfig->source == DESKTOP_WINDOW_EGL_CONTEXT_API)
        {
            if (!_desktop_windowInitEGL())
                return DESKTOP_WINDOW_FALSE;
            if (!_desktop_windowCreateContextEGL(window, ctxconfig, fbconfig))
                return DESKTOP_WINDOW_FALSE;
        }

        if (!_desktop_windowRefreshContextAttribs(window, ctxconfig))
            return DESKTOP_WINDOW_FALSE;
    }

    if (wndconfig->mousePassthrough)
        _desktop_windowSetWindowMousePassthroughNull(window, DESKTOP_WINDOW_TRUE);

    if (window->monitor)
    {
        _desktop_windowShowWindowNull(window);
        _desktop_windowFocusWindowNull(window);
        acquireMonitor(window);

        if (wndconfig->centerCursor)
            _desktop_windowCenterCursorInContentArea(window);
    }
    else
    {
        if (wndconfig->visible)
        {
            _desktop_windowShowWindowNull(window);
            if (wndconfig->focused)
                _desktop_windowFocusWindowNull(window);
        }
    }

    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowDestroyWindowNull(_DESKTOP_WINDOWwindow* window)
{
    if (window->monitor)
        releaseMonitor(window);

    if (_desktop_window.null.focusedWindow == window)
        _desktop_window.null.focusedWindow = NULL;

    if (window->context.destroy)
        window->context.destroy(window);
}

void _desktop_windowSetWindowTitleNull(_DESKTOP_WINDOWwindow* window, const char* title)
{
}

void _desktop_windowSetWindowIconNull(_DESKTOP_WINDOWwindow* window, int count, const DESKTOP_WINDOWimage* images)
{
}

void _desktop_windowSetWindowMonitorNull(_DESKTOP_WINDOWwindow* window,
                               _DESKTOP_WINDOWmonitor* monitor,
                               int xpos, int ypos,
                               int width, int height,
                               int refreshRate)
{
    if (window->monitor == monitor)
    {
        if (!monitor)
        {
            _desktop_windowSetWindowPosNull(window, xpos, ypos);
            _desktop_windowSetWindowSizeNull(window, width, height);
        }

        return;
    }

    if (window->monitor)
        releaseMonitor(window);

    _desktop_windowInputWindowMonitor(window, monitor);

    if (window->monitor)
    {
        window->null.visible = DESKTOP_WINDOW_TRUE;
        acquireMonitor(window);
        fitToMonitor(window);
    }
    else
    {
        _desktop_windowSetWindowPosNull(window, xpos, ypos);
        _desktop_windowSetWindowSizeNull(window, width, height);
    }
}

void _desktop_windowGetWindowPosNull(_DESKTOP_WINDOWwindow* window, int* xpos, int* ypos)
{
    if (xpos)
        *xpos = window->null.xpos;
    if (ypos)
        *ypos = window->null.ypos;
}

void _desktop_windowSetWindowPosNull(_DESKTOP_WINDOWwindow* window, int xpos, int ypos)
{
    if (window->monitor)
        return;

    if (window->null.xpos != xpos || window->null.ypos != ypos)
    {
        window->null.xpos = xpos;
        window->null.ypos = ypos;
        _desktop_windowInputWindowPos(window, xpos, ypos);
    }
}

void _desktop_windowGetWindowSizeNull(_DESKTOP_WINDOWwindow* window, int* width, int* height)
{
    if (width)
        *width = window->null.width;
    if (height)
        *height = window->null.height;
}

void _desktop_windowSetWindowSizeNull(_DESKTOP_WINDOWwindow* window, int width, int height)
{
    if (window->monitor)
        return;

    if (window->null.width != width || window->null.height != height)
    {
        window->null.width = width;
        window->null.height = height;
        _desktop_windowInputFramebufferSize(window, width, height);
        _desktop_windowInputWindowDamage(window);
        _desktop_windowInputWindowSize(window, width, height);
    }
}

void _desktop_windowSetWindowSizeLimitsNull(_DESKTOP_WINDOWwindow* window,
                                  int minwidth, int minheight,
                                  int maxwidth, int maxheight)
{
    int width = window->null.width;
    int height = window->null.height;
    applySizeLimits(window, &width, &height);
    _desktop_windowSetWindowSizeNull(window, width, height);
}

void _desktop_windowSetWindowAspectRatioNull(_DESKTOP_WINDOWwindow* window, int n, int d)
{
    int width = window->null.width;
    int height = window->null.height;
    applySizeLimits(window, &width, &height);
    _desktop_windowSetWindowSizeNull(window, width, height);
}

void _desktop_windowGetFramebufferSizeNull(_DESKTOP_WINDOWwindow* window, int* width, int* height)
{
    if (width)
        *width = window->null.width;
    if (height)
        *height = window->null.height;
}

void _desktop_windowGetWindowFrameSizeNull(_DESKTOP_WINDOWwindow* window,
                                 int* left, int* top,
                                 int* right, int* bottom)
{
    if (window->null.decorated && !window->monitor)
    {
        if (left)
            *left = 1;
        if (top)
            *top = 10;
        if (right)
            *right = 1;
        if (bottom)
            *bottom = 1;
    }
    else
    {
        if (left)
            *left = 0;
        if (top)
            *top = 0;
        if (right)
            *right = 0;
        if (bottom)
            *bottom = 0;
    }
}

void _desktop_windowGetWindowContentScaleNull(_DESKTOP_WINDOWwindow* window, float* xscale, float* yscale)
{
    if (xscale)
        *xscale = 1.f;
    if (yscale)
        *yscale = 1.f;
}

void _desktop_windowIconifyWindowNull(_DESKTOP_WINDOWwindow* window)
{
    if (_desktop_window.null.focusedWindow == window)
    {
        _desktop_window.null.focusedWindow = NULL;
        _desktop_windowInputWindowFocus(window, DESKTOP_WINDOW_FALSE);
    }

    if (!window->null.iconified)
    {
        window->null.iconified = DESKTOP_WINDOW_TRUE;
        _desktop_windowInputWindowIconify(window, DESKTOP_WINDOW_TRUE);

        if (window->monitor)
            releaseMonitor(window);
    }
}

void _desktop_windowRestoreWindowNull(_DESKTOP_WINDOWwindow* window)
{
    if (window->null.iconified)
    {
        window->null.iconified = DESKTOP_WINDOW_FALSE;
        _desktop_windowInputWindowIconify(window, DESKTOP_WINDOW_FALSE);

        if (window->monitor)
            acquireMonitor(window);
    }
    else if (window->null.maximized)
    {
        window->null.maximized = DESKTOP_WINDOW_FALSE;
        _desktop_windowInputWindowMaximize(window, DESKTOP_WINDOW_FALSE);
    }
}

void _desktop_windowMaximizeWindowNull(_DESKTOP_WINDOWwindow* window)
{
    if (!window->null.maximized)
    {
        window->null.maximized = DESKTOP_WINDOW_TRUE;
        _desktop_windowInputWindowMaximize(window, DESKTOP_WINDOW_TRUE);
    }
}

DESKTOP_WINDOWbool _desktop_windowWindowMaximizedNull(_DESKTOP_WINDOWwindow* window)
{
    return window->null.maximized;
}

DESKTOP_WINDOWbool _desktop_windowWindowHoveredNull(_DESKTOP_WINDOWwindow* window)
{
    return _desktop_window.null.xcursor >= window->null.xpos &&
           _desktop_window.null.ycursor >= window->null.ypos &&
           _desktop_window.null.xcursor <= window->null.xpos + window->null.width - 1 &&
           _desktop_window.null.ycursor <= window->null.ypos + window->null.height - 1;
}

DESKTOP_WINDOWbool _desktop_windowFramebufferTransparentNull(_DESKTOP_WINDOWwindow* window)
{
    return window->null.transparent;
}

void _desktop_windowSetWindowResizableNull(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    window->null.resizable = enabled;
}

void _desktop_windowSetWindowDecoratedNull(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    window->null.decorated = enabled;
}

void _desktop_windowSetWindowFloatingNull(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    window->null.floating = enabled;
}

void _desktop_windowSetWindowMousePassthroughNull(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
}

float _desktop_windowGetWindowOpacityNull(_DESKTOP_WINDOWwindow* window)
{
    return window->null.opacity;
}

void _desktop_windowSetWindowOpacityNull(_DESKTOP_WINDOWwindow* window, float opacity)
{
    window->null.opacity = opacity;
}

void _desktop_windowSetRawMouseMotionNull(_DESKTOP_WINDOWwindow *window, DESKTOP_WINDOWbool enabled)
{
}

DESKTOP_WINDOWbool _desktop_windowRawMouseMotionSupportedNull(void)
{
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowShowWindowNull(_DESKTOP_WINDOWwindow* window)
{
    window->null.visible = DESKTOP_WINDOW_TRUE;
}

void _desktop_windowRequestWindowAttentionNull(_DESKTOP_WINDOWwindow* window)
{
}

void _desktop_windowHideWindowNull(_DESKTOP_WINDOWwindow* window)
{
    if (_desktop_window.null.focusedWindow == window)
    {
        _desktop_window.null.focusedWindow = NULL;
        _desktop_windowInputWindowFocus(window, DESKTOP_WINDOW_FALSE);
    }

    window->null.visible = DESKTOP_WINDOW_FALSE;
}

void _desktop_windowFocusWindowNull(_DESKTOP_WINDOWwindow* window)
{
    _DESKTOP_WINDOWwindow* previous;

    if (_desktop_window.null.focusedWindow == window)
        return;

    if (!window->null.visible)
        return;

    previous = _desktop_window.null.focusedWindow;
    _desktop_window.null.focusedWindow = window;

    if (previous)
    {
        _desktop_windowInputWindowFocus(previous, DESKTOP_WINDOW_FALSE);
        if (previous->monitor && previous->autoIconify)
            _desktop_windowIconifyWindowNull(previous);
    }

    _desktop_windowInputWindowFocus(window, DESKTOP_WINDOW_TRUE);
}

DESKTOP_WINDOWbool _desktop_windowWindowFocusedNull(_DESKTOP_WINDOWwindow* window)
{
    return _desktop_window.null.focusedWindow == window;
}

DESKTOP_WINDOWbool _desktop_windowWindowIconifiedNull(_DESKTOP_WINDOWwindow* window)
{
    return window->null.iconified;
}

DESKTOP_WINDOWbool _desktop_windowWindowVisibleNull(_DESKTOP_WINDOWwindow* window)
{
    return window->null.visible;
}

void _desktop_windowPollEventsNull(void)
{
}

void _desktop_windowWaitEventsNull(void)
{
}

void _desktop_windowWaitEventsTimeoutNull(double timeout)
{
}

void _desktop_windowPostEmptyEventNull(void)
{
}

void _desktop_windowGetCursorPosNull(_DESKTOP_WINDOWwindow* window, double* xpos, double* ypos)
{
    if (xpos)
        *xpos = _desktop_window.null.xcursor - window->null.xpos;
    if (ypos)
        *ypos = _desktop_window.null.ycursor - window->null.ypos;
}

void _desktop_windowSetCursorPosNull(_DESKTOP_WINDOWwindow* window, double x, double y)
{
    _desktop_window.null.xcursor = window->null.xpos + (int) x;
    _desktop_window.null.ycursor = window->null.ypos + (int) y;
}

void _desktop_windowSetCursorModeNull(_DESKTOP_WINDOWwindow* window, int mode)
{
}

DESKTOP_WINDOWbool _desktop_windowCreateCursorNull(_DESKTOP_WINDOWcursor* cursor,
                               const DESKTOP_WINDOWimage* image,
                               int xhot, int yhot)
{
    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWbool _desktop_windowCreateStandardCursorNull(_DESKTOP_WINDOWcursor* cursor, int shape)
{
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowDestroyCursorNull(_DESKTOP_WINDOWcursor* cursor)
{
}

void _desktop_windowSetCursorNull(_DESKTOP_WINDOWwindow* window, _DESKTOP_WINDOWcursor* cursor)
{
}

void _desktop_windowSetClipboardStringNull(const char* string)
{
    char* copy = _desktop_window_strdup(string);
    _desktop_window_free(_desktop_window.null.clipboardString);
    _desktop_window.null.clipboardString = copy;
}

const char* _desktop_windowGetClipboardStringNull(void)
{
    return _desktop_window.null.clipboardString;
}

EGLenum _desktop_windowGetEGLPlatformNull(EGLint** attribs)
{
    return 0;
}

EGLNativeDisplayType _desktop_windowGetEGLNativeDisplayNull(void)
{
    return 0;
}

EGLNativeWindowType _desktop_windowGetEGLNativeWindowNull(_DESKTOP_WINDOWwindow* window)
{
    return 0;
}

const char* _desktop_windowGetScancodeNameNull(int scancode)
{
    if (scancode < DESKTOP_WINDOW_NULL_SC_FIRST || scancode > DESKTOP_WINDOW_NULL_SC_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Invalid scancode %i", scancode);
        return NULL;
    }

    switch (scancode)
    {
        case DESKTOP_WINDOW_NULL_SC_APOSTROPHE:
            return "'";
        case DESKTOP_WINDOW_NULL_SC_COMMA:
            return ",";
        case DESKTOP_WINDOW_NULL_SC_MINUS:
        case DESKTOP_WINDOW_NULL_SC_KP_SUBTRACT:
            return "-";
        case DESKTOP_WINDOW_NULL_SC_PERIOD:
        case DESKTOP_WINDOW_NULL_SC_KP_DECIMAL:
            return ".";
        case DESKTOP_WINDOW_NULL_SC_SLASH:
        case DESKTOP_WINDOW_NULL_SC_KP_DIVIDE:
            return "/";
        case DESKTOP_WINDOW_NULL_SC_SEMICOLON:
            return ";";
        case DESKTOP_WINDOW_NULL_SC_EQUAL:
        case DESKTOP_WINDOW_NULL_SC_KP_EQUAL:
            return "=";
        case DESKTOP_WINDOW_NULL_SC_LEFT_BRACKET:
            return "[";
        case DESKTOP_WINDOW_NULL_SC_RIGHT_BRACKET:
            return "]";
        case DESKTOP_WINDOW_NULL_SC_KP_MULTIPLY:
            return "*";
        case DESKTOP_WINDOW_NULL_SC_KP_ADD:
            return "+";
        case DESKTOP_WINDOW_NULL_SC_BACKSLASH:
        case DESKTOP_WINDOW_NULL_SC_WORLD_1:
        case DESKTOP_WINDOW_NULL_SC_WORLD_2:
            return "\\";
        case DESKTOP_WINDOW_NULL_SC_0:
        case DESKTOP_WINDOW_NULL_SC_KP_0:
            return "0";
        case DESKTOP_WINDOW_NULL_SC_1:
        case DESKTOP_WINDOW_NULL_SC_KP_1:
            return "1";
        case DESKTOP_WINDOW_NULL_SC_2:
        case DESKTOP_WINDOW_NULL_SC_KP_2:
            return "2";
        case DESKTOP_WINDOW_NULL_SC_3:
        case DESKTOP_WINDOW_NULL_SC_KP_3:
            return "3";
        case DESKTOP_WINDOW_NULL_SC_4:
        case DESKTOP_WINDOW_NULL_SC_KP_4:
            return "4";
        case DESKTOP_WINDOW_NULL_SC_5:
        case DESKTOP_WINDOW_NULL_SC_KP_5:
            return "5";
        case DESKTOP_WINDOW_NULL_SC_6:
        case DESKTOP_WINDOW_NULL_SC_KP_6:
            return "6";
        case DESKTOP_WINDOW_NULL_SC_7:
        case DESKTOP_WINDOW_NULL_SC_KP_7:
            return "7";
        case DESKTOP_WINDOW_NULL_SC_8:
        case DESKTOP_WINDOW_NULL_SC_KP_8:
            return "8";
        case DESKTOP_WINDOW_NULL_SC_9:
        case DESKTOP_WINDOW_NULL_SC_KP_9:
            return "9";
        case DESKTOP_WINDOW_NULL_SC_A:
            return "a";
        case DESKTOP_WINDOW_NULL_SC_B:
            return "b";
        case DESKTOP_WINDOW_NULL_SC_C:
            return "c";
        case DESKTOP_WINDOW_NULL_SC_D:
            return "d";
        case DESKTOP_WINDOW_NULL_SC_E:
            return "e";
        case DESKTOP_WINDOW_NULL_SC_F:
            return "f";
        case DESKTOP_WINDOW_NULL_SC_G:
            return "g";
        case DESKTOP_WINDOW_NULL_SC_H:
            return "h";
        case DESKTOP_WINDOW_NULL_SC_I:
            return "i";
        case DESKTOP_WINDOW_NULL_SC_J:
            return "j";
        case DESKTOP_WINDOW_NULL_SC_K:
            return "k";
        case DESKTOP_WINDOW_NULL_SC_L:
            return "l";
        case DESKTOP_WINDOW_NULL_SC_M:
            return "m";
        case DESKTOP_WINDOW_NULL_SC_N:
            return "n";
        case DESKTOP_WINDOW_NULL_SC_O:
            return "o";
        case DESKTOP_WINDOW_NULL_SC_P:
            return "p";
        case DESKTOP_WINDOW_NULL_SC_Q:
            return "q";
        case DESKTOP_WINDOW_NULL_SC_R:
            return "r";
        case DESKTOP_WINDOW_NULL_SC_S:
            return "s";
        case DESKTOP_WINDOW_NULL_SC_T:
            return "t";
        case DESKTOP_WINDOW_NULL_SC_U:
            return "u";
        case DESKTOP_WINDOW_NULL_SC_V:
            return "v";
        case DESKTOP_WINDOW_NULL_SC_W:
            return "w";
        case DESKTOP_WINDOW_NULL_SC_X:
            return "x";
        case DESKTOP_WINDOW_NULL_SC_Y:
            return "y";
        case DESKTOP_WINDOW_NULL_SC_Z:
            return "z";
    }

    return NULL;
}

int _desktop_windowGetKeyScancodeNull(int key)
{
    return _desktop_window.null.scancodes[key];
}

void _desktop_windowGetRequiredInstanceExtensionsNull(char** extensions)
{
}

DESKTOP_WINDOWbool _desktop_windowGetPhysicalDevicePresentationSupportNull(VkInstance instance,
                                                       VkPhysicalDevice device,
                                                       uint32_t queuefamily)
{
    return DESKTOP_WINDOW_FALSE;
}

VkResult _desktop_windowCreateWindowSurfaceNull(VkInstance instance,
                                      _DESKTOP_WINDOWwindow* window,
                                      const VkAllocationCallbacks* allocator,
                                      VkSurfaceKHR* surface)
{
    // This seems like the most appropriate error to return here
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

