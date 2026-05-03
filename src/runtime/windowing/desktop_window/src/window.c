#include "internal.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>


//////////////////////////////////////////////////////////////////////////
//////                         DESKTOP_WINDOW event API                       //////
//////////////////////////////////////////////////////////////////////////

// Notifies shared code that a window has lost or received input focus
//
void _desktop_windowInputWindowFocus(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool focused)
{
    assert(window != NULL);
    assert(focused == DESKTOP_WINDOW_TRUE || focused == DESKTOP_WINDOW_FALSE);

    if (window->callbacks.focus)
        window->callbacks.focus((DESKTOP_WINDOWwindow*) window, focused);

    if (!focused)
    {
        int key, button;

        for (key = 0;  key <= DESKTOP_WINDOW_KEY_LAST;  key++)
        {
            if (window->keys[key] == DESKTOP_WINDOW_PRESS)
            {
                const int scancode = _desktop_window.platform.getKeyScancode(key);
                _desktop_windowInputKey(window, key, scancode, DESKTOP_WINDOW_RELEASE, 0);
            }
        }

        for (button = 0;  button <= DESKTOP_WINDOW_MOUSE_BUTTON_LAST;  button++)
        {
            if (window->mouseButtons[button] == DESKTOP_WINDOW_PRESS)
                _desktop_windowInputMouseClick(window, button, DESKTOP_WINDOW_RELEASE, 0);
        }
    }
}

// Notifies shared code that a window has moved
// The position is specified in content area relative screen coordinates
//
void _desktop_windowInputWindowPos(_DESKTOP_WINDOWwindow* window, int x, int y)
{
    assert(window != NULL);

    if (window->callbacks.pos)
        window->callbacks.pos((DESKTOP_WINDOWwindow*) window, x, y);
}

// Notifies shared code that a window has been resized
// The size is specified in screen coordinates
//
void _desktop_windowInputWindowSize(_DESKTOP_WINDOWwindow* window, int width, int height)
{
    assert(window != NULL);
    assert(width >= 0);
    assert(height >= 0);

    if (window->callbacks.size)
        window->callbacks.size((DESKTOP_WINDOWwindow*) window, width, height);
}

// Notifies shared code that a window has been iconified or restored
//
void _desktop_windowInputWindowIconify(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool iconified)
{
    assert(window != NULL);
    assert(iconified == DESKTOP_WINDOW_TRUE || iconified == DESKTOP_WINDOW_FALSE);

    if (window->callbacks.iconify)
        window->callbacks.iconify((DESKTOP_WINDOWwindow*) window, iconified);
}

// Notifies shared code that a window has been maximized or restored
//
void _desktop_windowInputWindowMaximize(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool maximized)
{
    assert(window != NULL);
    assert(maximized == DESKTOP_WINDOW_TRUE || maximized == DESKTOP_WINDOW_FALSE);

    if (window->callbacks.maximize)
        window->callbacks.maximize((DESKTOP_WINDOWwindow*) window, maximized);
}

// Notifies shared code that a window framebuffer has been resized
// The size is specified in pixels
//
void _desktop_windowInputFramebufferSize(_DESKTOP_WINDOWwindow* window, int width, int height)
{
    assert(window != NULL);
    assert(width >= 0);
    assert(height >= 0);

    if (window->callbacks.fbsize)
        window->callbacks.fbsize((DESKTOP_WINDOWwindow*) window, width, height);
}

// Notifies shared code that a window content scale has changed
// The scale is specified as the ratio between the current and default DPI
//
void _desktop_windowInputWindowContentScale(_DESKTOP_WINDOWwindow* window, float xscale, float yscale)
{
    assert(window != NULL);
    assert(xscale > 0.f);
    assert(xscale < FLT_MAX);
    assert(yscale > 0.f);
    assert(yscale < FLT_MAX);

    if (window->callbacks.scale)
        window->callbacks.scale((DESKTOP_WINDOWwindow*) window, xscale, yscale);
}

// Notifies shared code that the window contents needs updating
//
void _desktop_windowInputWindowDamage(_DESKTOP_WINDOWwindow* window)
{
    assert(window != NULL);

    if (window->callbacks.refresh)
        window->callbacks.refresh((DESKTOP_WINDOWwindow*) window);
}

// Notifies shared code that the user wishes to close a window
//
void _desktop_windowInputWindowCloseRequest(_DESKTOP_WINDOWwindow* window)
{
    assert(window != NULL);

    window->shouldClose = DESKTOP_WINDOW_TRUE;

    if (window->callbacks.close)
        window->callbacks.close((DESKTOP_WINDOWwindow*) window);
}

// Notifies shared code that a window has changed its desired monitor
//
void _desktop_windowInputWindowMonitor(_DESKTOP_WINDOWwindow* window, _DESKTOP_WINDOWmonitor* monitor)
{
    assert(window != NULL);
    window->monitor = monitor;
}

//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW public API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindow* desktop_windowCreateWindow(int width, int height,
                                     const char* title,
                                     DESKTOP_WINDOWmonitor* monitor,
                                     DESKTOP_WINDOWwindow* share)
{
    _DESKTOP_WINDOWfbconfig fbconfig;
    _DESKTOP_WINDOWctxconfig ctxconfig;
    _DESKTOP_WINDOWwndconfig wndconfig;
    _DESKTOP_WINDOWwindow* window;

    assert(title != NULL);
    assert(width >= 0);
    assert(height >= 0);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (width <= 0 || height <= 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                        "Invalid window size %ix%i",
                        width, height);

        return NULL;
    }

    fbconfig  = _desktop_window.hints.framebuffer;
    ctxconfig = _desktop_window.hints.context;
    wndconfig = _desktop_window.hints.window;

    wndconfig.width   = width;
    wndconfig.height  = height;
    wndconfig.title   = title;
    ctxconfig.share   = (_DESKTOP_WINDOWwindow*) share;

    if (!_desktop_windowIsValidContextConfig(&ctxconfig))
        return NULL;

    window = _desktop_window_calloc(1, sizeof(_DESKTOP_WINDOWwindow));
    window->next = _desktop_window.windowListHead;
    _desktop_window.windowListHead = window;

    window->videoMode.width       = width;
    window->videoMode.height      = height;
    window->videoMode.redBits     = fbconfig.redBits;
    window->videoMode.greenBits   = fbconfig.greenBits;
    window->videoMode.blueBits    = fbconfig.blueBits;
    window->videoMode.refreshRate = _desktop_window.hints.refreshRate;

    window->monitor          = (_DESKTOP_WINDOWmonitor*) monitor;
    window->resizable        = wndconfig.resizable;
    window->decorated        = wndconfig.decorated;
    window->autoIconify      = wndconfig.autoIconify;
    window->floating         = wndconfig.floating;
    window->focusOnShow      = wndconfig.focusOnShow;
    window->mousePassthrough = wndconfig.mousePassthrough;
    window->cursorMode       = DESKTOP_WINDOW_CURSOR_NORMAL;

    window->doublebuffer = fbconfig.doublebuffer;

    window->minwidth    = DESKTOP_WINDOW_DONT_CARE;
    window->minheight   = DESKTOP_WINDOW_DONT_CARE;
    window->maxwidth    = DESKTOP_WINDOW_DONT_CARE;
    window->maxheight   = DESKTOP_WINDOW_DONT_CARE;
    window->numer       = DESKTOP_WINDOW_DONT_CARE;
    window->denom       = DESKTOP_WINDOW_DONT_CARE;
    window->title       = _desktop_window_strdup(title);

    if (!_desktop_window.platform.createWindow(window, &wndconfig, &ctxconfig, &fbconfig))
    {
        desktop_windowDestroyWindow((DESKTOP_WINDOWwindow*) window);
        return NULL;
    }

    return (DESKTOP_WINDOWwindow*) window;
}

void desktop_windowDefaultWindowHints(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT();

    // The default is DesktopGraphics with minimum version 1.0
    memset(&_desktop_window.hints.context, 0, sizeof(_desktop_window.hints.context));
    _desktop_window.hints.context.client = DESKTOP_WINDOW_DESKTOP_GRAPHICS_API;
    _desktop_window.hints.context.source = DESKTOP_WINDOW_NATIVE_CONTEXT_API;
    _desktop_window.hints.context.major  = 1;
    _desktop_window.hints.context.minor  = 0;

    // The default is a focused, visible, resizable window with decorations
    memset(&_desktop_window.hints.window, 0, sizeof(_desktop_window.hints.window));
    _desktop_window.hints.window.resizable    = DESKTOP_WINDOW_TRUE;
    _desktop_window.hints.window.visible      = DESKTOP_WINDOW_TRUE;
    _desktop_window.hints.window.decorated    = DESKTOP_WINDOW_TRUE;
    _desktop_window.hints.window.focused      = DESKTOP_WINDOW_TRUE;
    _desktop_window.hints.window.autoIconify  = DESKTOP_WINDOW_TRUE;
    _desktop_window.hints.window.centerCursor = DESKTOP_WINDOW_TRUE;
    _desktop_window.hints.window.focusOnShow  = DESKTOP_WINDOW_TRUE;
    _desktop_window.hints.window.xpos         = DESKTOP_WINDOW_ANY_POSITION;
    _desktop_window.hints.window.ypos         = DESKTOP_WINDOW_ANY_POSITION;
    _desktop_window.hints.window.scaleFramebuffer = DESKTOP_WINDOW_TRUE;

    // The default is 24 bits of color, 24 bits of depth and 8 bits of stencil,
    // double buffered
    memset(&_desktop_window.hints.framebuffer, 0, sizeof(_desktop_window.hints.framebuffer));
    _desktop_window.hints.framebuffer.redBits      = 8;
    _desktop_window.hints.framebuffer.greenBits    = 8;
    _desktop_window.hints.framebuffer.blueBits     = 8;
    _desktop_window.hints.framebuffer.alphaBits    = 8;
    _desktop_window.hints.framebuffer.depthBits    = 24;
    _desktop_window.hints.framebuffer.stencilBits  = 8;
    _desktop_window.hints.framebuffer.doublebuffer = DESKTOP_WINDOW_TRUE;

    // The default is to select the highest available refresh rate
    _desktop_window.hints.refreshRate = DESKTOP_WINDOW_DONT_CARE;
}

DESKTOP_WINDOWAPI void desktop_windowWindowHint(int hint, int value)
{
    _DESKTOP_WINDOW_REQUIRE_INIT();

    switch (hint)
    {
        case DESKTOP_WINDOW_RED_BITS:
            _desktop_window.hints.framebuffer.redBits = value;
            return;
        case DESKTOP_WINDOW_GREEN_BITS:
            _desktop_window.hints.framebuffer.greenBits = value;
            return;
        case DESKTOP_WINDOW_BLUE_BITS:
            _desktop_window.hints.framebuffer.blueBits = value;
            return;
        case DESKTOP_WINDOW_ALPHA_BITS:
            _desktop_window.hints.framebuffer.alphaBits = value;
            return;
        case DESKTOP_WINDOW_DEPTH_BITS:
            _desktop_window.hints.framebuffer.depthBits = value;
            return;
        case DESKTOP_WINDOW_STENCIL_BITS:
            _desktop_window.hints.framebuffer.stencilBits = value;
            return;
        case DESKTOP_WINDOW_ACCUM_RED_BITS:
            _desktop_window.hints.framebuffer.accumRedBits = value;
            return;
        case DESKTOP_WINDOW_ACCUM_GREEN_BITS:
            _desktop_window.hints.framebuffer.accumGreenBits = value;
            return;
        case DESKTOP_WINDOW_ACCUM_BLUE_BITS:
            _desktop_window.hints.framebuffer.accumBlueBits = value;
            return;
        case DESKTOP_WINDOW_ACCUM_ALPHA_BITS:
            _desktop_window.hints.framebuffer.accumAlphaBits = value;
            return;
        case DESKTOP_WINDOW_AUX_BUFFERS:
            _desktop_window.hints.framebuffer.auxBuffers = value;
            return;
        case DESKTOP_WINDOW_STEREO:
            _desktop_window.hints.framebuffer.stereo = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_DOUBLEBUFFER:
            _desktop_window.hints.framebuffer.doublebuffer = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_TRANSPARENT_FRAMEBUFFER:
            _desktop_window.hints.framebuffer.transparent = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_SAMPLES:
            _desktop_window.hints.framebuffer.samples = value;
            return;
        case DESKTOP_WINDOW_SRGB_CAPABLE:
            _desktop_window.hints.framebuffer.sRGB = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_RESIZABLE:
            _desktop_window.hints.window.resizable = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_DECORATED:
            _desktop_window.hints.window.decorated = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_FOCUSED:
            _desktop_window.hints.window.focused = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_AUTO_ICONIFY:
            _desktop_window.hints.window.autoIconify = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_FLOATING:
            _desktop_window.hints.window.floating = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_MAXIMIZED:
            _desktop_window.hints.window.maximized = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_VISIBLE:
            _desktop_window.hints.window.visible = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_POSITION_X:
            _desktop_window.hints.window.xpos = value;
            return;
        case DESKTOP_WINDOW_POSITION_Y:
            _desktop_window.hints.window.ypos = value;
            return;
        case DESKTOP_WINDOW_WIN32_KEYBOARD_MENU:
            _desktop_window.hints.window.win32.keymenu = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_WIN32_SHOWDEFAULT:
            _desktop_window.hints.window.win32.showDefault = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_COCOA_GRAPHICS_SWITCHING:
            _desktop_window.hints.context.nsgl.offline = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_SCALE_TO_MONITOR:
            _desktop_window.hints.window.scaleToMonitor = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_SCALE_FRAMEBUFFER:
        case DESKTOP_WINDOW_COCOA_RETINA_FRAMEBUFFER:
            _desktop_window.hints.window.scaleFramebuffer = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_CENTER_CURSOR:
            _desktop_window.hints.window.centerCursor = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_FOCUS_ON_SHOW:
            _desktop_window.hints.window.focusOnShow = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_MOUSE_PASSTHROUGH:
            _desktop_window.hints.window.mousePassthrough = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_CLIENT_API:
            _desktop_window.hints.context.client = value;
            return;
        case DESKTOP_WINDOW_CONTEXT_CREATION_API:
            _desktop_window.hints.context.source = value;
            return;
        case DESKTOP_WINDOW_CONTEXT_VERSION_MAJOR:
            _desktop_window.hints.context.major = value;
            return;
        case DESKTOP_WINDOW_CONTEXT_VERSION_MINOR:
            _desktop_window.hints.context.minor = value;
            return;
        case DESKTOP_WINDOW_CONTEXT_ROBUSTNESS:
            _desktop_window.hints.context.robustness = value;
            return;
        case DESKTOP_WINDOW_DESKTOP_GRAPHICS_FORWARD_COMPAT:
            _desktop_window.hints.context.forward = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_CONTEXT_DEBUG:
            _desktop_window.hints.context.debug = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_CONTEXT_NO_ERROR:
            _desktop_window.hints.context.noerror = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        case DESKTOP_WINDOW_DESKTOP_GRAPHICS_PROFILE:
            _desktop_window.hints.context.profile = value;
            return;
        case DESKTOP_WINDOW_CONTEXT_RELEASE_BEHAVIOR:
            _desktop_window.hints.context.release = value;
            return;
        case DESKTOP_WINDOW_REFRESH_RATE:
            _desktop_window.hints.refreshRate = value;
            return;
    }

    _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid window hint 0x%08X", hint);
}

DESKTOP_WINDOWAPI void desktop_windowWindowHintString(int hint, const char* value)
{
    assert(value != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    switch (hint)
    {
        case DESKTOP_WINDOW_COCOA_FRAME_NAME:
            strncpy(_desktop_window.hints.window.ns.frameName, value,
                    sizeof(_desktop_window.hints.window.ns.frameName) - 1);
            return;
        case DESKTOP_WINDOW_X11_CLASS_NAME:
            strncpy(_desktop_window.hints.window.x11.className, value,
                    sizeof(_desktop_window.hints.window.x11.className) - 1);
            return;
        case DESKTOP_WINDOW_X11_INSTANCE_NAME:
            strncpy(_desktop_window.hints.window.x11.instanceName, value,
                    sizeof(_desktop_window.hints.window.x11.instanceName) - 1);
            return;
        case DESKTOP_WINDOW_WAYLAND_APP_ID:
            strncpy(_desktop_window.hints.window.wl.appId, value,
                    sizeof(_desktop_window.hints.window.wl.appId) - 1);
            return;
    }

    _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid window hint string 0x%08X", hint);
}

DESKTOP_WINDOWAPI void desktop_windowDestroyWindow(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;

    _DESKTOP_WINDOW_REQUIRE_INIT();

    // Allow closing of NULL (to match the behavior of free)
    if (window == NULL)
        return;

    // Clear all callbacks to avoid exposing a half torn-down window object
    memset(&window->callbacks, 0, sizeof(window->callbacks));

    // The window's context must not be current on another thread when the
    // window is destroyed
    if (window == _desktop_windowPlatformGetTls(&_desktop_window.contextSlot))
        desktop_windowMakeContextCurrent(NULL);

    _desktop_window.platform.destroyWindow(window);

    // Unlink window from global linked list
    {
        _DESKTOP_WINDOWwindow** prev = &_desktop_window.windowListHead;

        while (*prev != window)
            prev = &((*prev)->next);

        *prev = window->next;
    }

    _desktop_window_free(window->title);
    _desktop_window_free(window);
}

DESKTOP_WINDOWAPI int desktop_windowWindowShouldClose(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(0);
    return window->shouldClose;
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowShouldClose(DESKTOP_WINDOWwindow* handle, int value)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();
    window->shouldClose = value;
}

DESKTOP_WINDOWAPI const char* desktop_windowGetWindowTitle(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    return window->title;
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowTitle(DESKTOP_WINDOWwindow* handle, const char* title)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);
    assert(title != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    char* prev = window->title;
    window->title = _desktop_window_strdup(title);

    _desktop_window.platform.setWindowTitle(window, title);
    _desktop_window_free(prev);
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowIcon(DESKTOP_WINDOWwindow* handle,
                               int count, const DESKTOP_WINDOWimage* images)
{
    int i;
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;

    assert(window != NULL);
    assert(count >= 0);
    assert(count == 0 || images != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (count < 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Invalid image count for window icon");
        return;
    }

    for (i = 0; i < count; i++)
    {
        assert(images[i].pixels != NULL);

        if (images[i].width <= 0 || images[i].height <= 0)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                            "Invalid image dimensions for window icon");
            return;
        }
    }

    _desktop_window.platform.setWindowIcon(window, count, images);
}

DESKTOP_WINDOWAPI void desktop_windowGetWindowPos(DESKTOP_WINDOWwindow* handle, int* xpos, int* ypos)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.getWindowPos(window, xpos, ypos);
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowPos(DESKTOP_WINDOWwindow* handle, int xpos, int ypos)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (window->monitor)
        return;

    _desktop_window.platform.setWindowPos(window, xpos, ypos);
}

DESKTOP_WINDOWAPI void desktop_windowGetWindowSize(DESKTOP_WINDOWwindow* handle, int* width, int* height)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    if (width)
        *width = 0;
    if (height)
        *height = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.getWindowSize(window, width, height);
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowSize(DESKTOP_WINDOWwindow* handle, int width, int height)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);
    assert(width >= 0);
    assert(height >= 0);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    window->videoMode.width  = width;
    window->videoMode.height = height;

    _desktop_window.platform.setWindowSize(window, width, height);
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowSizeLimits(DESKTOP_WINDOWwindow* handle,
                                     int minwidth, int minheight,
                                     int maxwidth, int maxheight)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (minwidth != DESKTOP_WINDOW_DONT_CARE && minheight != DESKTOP_WINDOW_DONT_CARE)
    {
        if (minwidth < 0 || minheight < 0)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                            "Invalid window minimum size %ix%i",
                            minwidth, minheight);
            return;
        }
    }

    if (maxwidth != DESKTOP_WINDOW_DONT_CARE && maxheight != DESKTOP_WINDOW_DONT_CARE)
    {
        if (maxwidth < 0 || maxheight < 0 ||
            maxwidth < minwidth || maxheight < minheight)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                            "Invalid window maximum size %ix%i",
                            maxwidth, maxheight);
            return;
        }
    }

    window->minwidth  = minwidth;
    window->minheight = minheight;
    window->maxwidth  = maxwidth;
    window->maxheight = maxheight;

    if (window->monitor || !window->resizable)
        return;

    _desktop_window.platform.setWindowSizeLimits(window,
                                       minwidth, minheight,
                                       maxwidth, maxheight);
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowAspectRatio(DESKTOP_WINDOWwindow* handle, int numer, int denom)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);
    assert(numer != 0);
    assert(denom != 0);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (numer != DESKTOP_WINDOW_DONT_CARE && denom != DESKTOP_WINDOW_DONT_CARE)
    {
        if (numer <= 0 || denom <= 0)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                            "Invalid window aspect ratio %i:%i",
                            numer, denom);
            return;
        }
    }

    window->numer = numer;
    window->denom = denom;

    if (window->monitor || !window->resizable)
        return;

    _desktop_window.platform.setWindowAspectRatio(window, numer, denom);
}

DESKTOP_WINDOWAPI void desktop_windowGetFramebufferSize(DESKTOP_WINDOWwindow* handle, int* width, int* height)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    if (width)
        *width = 0;
    if (height)
        *height = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.getFramebufferSize(window, width, height);
}

DESKTOP_WINDOWAPI void desktop_windowGetWindowFrameSize(DESKTOP_WINDOWwindow* handle,
                                    int* left, int* top,
                                    int* right, int* bottom)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    if (left)
        *left = 0;
    if (top)
        *top = 0;
    if (right)
        *right = 0;
    if (bottom)
        *bottom = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.getWindowFrameSize(window, left, top, right, bottom);
}

DESKTOP_WINDOWAPI void desktop_windowGetWindowContentScale(DESKTOP_WINDOWwindow* handle,
                                       float* xscale, float* yscale)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    if (xscale)
        *xscale = 0.f;
    if (yscale)
        *yscale = 0.f;

    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.getWindowContentScale(window, xscale, yscale);
}

DESKTOP_WINDOWAPI float desktop_windowGetWindowOpacity(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(0.f);
    return _desktop_window.platform.getWindowOpacity(window);
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowOpacity(DESKTOP_WINDOWwindow* handle, float opacity)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);
    assert(opacity == opacity);
    assert(opacity >= 0.f);
    assert(opacity <= 1.f);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (opacity != opacity || opacity < 0.f || opacity > 1.f)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Invalid window opacity %f", opacity);
        return;
    }

    _desktop_window.platform.setWindowOpacity(window, opacity);
}

DESKTOP_WINDOWAPI void desktop_windowIconifyWindow(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.iconifyWindow(window);
}

DESKTOP_WINDOWAPI void desktop_windowRestoreWindow(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.restoreWindow(window);
}

DESKTOP_WINDOWAPI void desktop_windowMaximizeWindow(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (window->monitor)
        return;

    _desktop_window.platform.maximizeWindow(window);
}

DESKTOP_WINDOWAPI void desktop_windowShowWindow(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (window->monitor)
        return;

    _desktop_window.platform.showWindow(window);

    if (window->focusOnShow)
        _desktop_window.platform.focusWindow(window);
}

DESKTOP_WINDOWAPI void desktop_windowRequestWindowAttention(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    _desktop_window.platform.requestWindowAttention(window);
}

DESKTOP_WINDOWAPI void desktop_windowHideWindow(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (window->monitor)
        return;

    _desktop_window.platform.hideWindow(window);
}

DESKTOP_WINDOWAPI void desktop_windowFocusWindow(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    _desktop_window.platform.focusWindow(window);
}

DESKTOP_WINDOWAPI int desktop_windowGetWindowAttrib(DESKTOP_WINDOWwindow* handle, int attrib)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(0);

    switch (attrib)
    {
        case DESKTOP_WINDOW_FOCUSED:
            return _desktop_window.platform.windowFocused(window);
        case DESKTOP_WINDOW_ICONIFIED:
            return _desktop_window.platform.windowIconified(window);
        case DESKTOP_WINDOW_VISIBLE:
            return _desktop_window.platform.windowVisible(window);
        case DESKTOP_WINDOW_MAXIMIZED:
            return _desktop_window.platform.windowMaximized(window);
        case DESKTOP_WINDOW_HOVERED:
            return _desktop_window.platform.windowHovered(window);
        case DESKTOP_WINDOW_FOCUS_ON_SHOW:
            return window->focusOnShow;
        case DESKTOP_WINDOW_MOUSE_PASSTHROUGH:
            return window->mousePassthrough;
        case DESKTOP_WINDOW_TRANSPARENT_FRAMEBUFFER:
            return _desktop_window.platform.framebufferTransparent(window);
        case DESKTOP_WINDOW_RESIZABLE:
            return window->resizable;
        case DESKTOP_WINDOW_DECORATED:
            return window->decorated;
        case DESKTOP_WINDOW_FLOATING:
            return window->floating;
        case DESKTOP_WINDOW_AUTO_ICONIFY:
            return window->autoIconify;
        case DESKTOP_WINDOW_DOUBLEBUFFER:
            return window->doublebuffer;
        case DESKTOP_WINDOW_CLIENT_API:
            return window->context.client;
        case DESKTOP_WINDOW_CONTEXT_CREATION_API:
            return window->context.source;
        case DESKTOP_WINDOW_CONTEXT_VERSION_MAJOR:
            return window->context.major;
        case DESKTOP_WINDOW_CONTEXT_VERSION_MINOR:
            return window->context.minor;
        case DESKTOP_WINDOW_CONTEXT_REVISION:
            return window->context.revision;
        case DESKTOP_WINDOW_CONTEXT_ROBUSTNESS:
            return window->context.robustness;
        case DESKTOP_WINDOW_DESKTOP_GRAPHICS_FORWARD_COMPAT:
            return window->context.forward;
        case DESKTOP_WINDOW_CONTEXT_DEBUG:
            return window->context.debug;
        case DESKTOP_WINDOW_DESKTOP_GRAPHICS_PROFILE:
            return window->context.profile;
        case DESKTOP_WINDOW_CONTEXT_RELEASE_BEHAVIOR:
            return window->context.release;
        case DESKTOP_WINDOW_CONTEXT_NO_ERROR:
            return window->context.noerror;
    }

    _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid window attribute 0x%08X", attrib);
    return 0;
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowAttrib(DESKTOP_WINDOWwindow* handle, int attrib, int value)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    value = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;

    switch (attrib)
    {
        case DESKTOP_WINDOW_AUTO_ICONIFY:
            window->autoIconify = value;
            return;

        case DESKTOP_WINDOW_RESIZABLE:
            window->resizable = value;
            if (!window->monitor)
                _desktop_window.platform.setWindowResizable(window, value);
            return;

        case DESKTOP_WINDOW_DECORATED:
            window->decorated = value;
            if (!window->monitor)
                _desktop_window.platform.setWindowDecorated(window, value);
            return;

        case DESKTOP_WINDOW_FLOATING:
            window->floating = value;
            if (!window->monitor)
                _desktop_window.platform.setWindowFloating(window, value);
            return;

        case DESKTOP_WINDOW_FOCUS_ON_SHOW:
            window->focusOnShow = value;
            return;

        case DESKTOP_WINDOW_MOUSE_PASSTHROUGH:
            window->mousePassthrough = value;
            _desktop_window.platform.setWindowMousePassthrough(window, value);
            return;
    }

    _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid window attribute 0x%08X", attrib);
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWmonitor* desktop_windowGetWindowMonitor(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    return (DESKTOP_WINDOWmonitor*) window->monitor;
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowMonitor(DESKTOP_WINDOWwindow* wh,
                                  DESKTOP_WINDOWmonitor* mh,
                                  int xpos, int ypos,
                                  int width, int height,
                                  int refreshRate)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) wh;
    _DESKTOP_WINDOWmonitor* monitor = (_DESKTOP_WINDOWmonitor*) mh;
    assert(window != NULL);
    assert(width >= 0);
    assert(height >= 0);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (width <= 0 || height <= 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                        "Invalid window size %ix%i",
                        width, height);
        return;
    }

    if (refreshRate < 0 && refreshRate != DESKTOP_WINDOW_DONT_CARE)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                        "Invalid refresh rate %i",
                        refreshRate);
        return;
    }

    window->videoMode.width       = width;
    window->videoMode.height      = height;
    window->videoMode.refreshRate = refreshRate;

    _desktop_window.platform.setWindowMonitor(window, monitor,
                                    xpos, ypos, width, height,
                                    refreshRate);
}

DESKTOP_WINDOWAPI void desktop_windowSetWindowUserPointer(DESKTOP_WINDOWwindow* handle, void* pointer)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();
    window->userPointer = pointer;
}

DESKTOP_WINDOWAPI void* desktop_windowGetWindowUserPointer(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    return window->userPointer;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindowposfun desktop_windowSetWindowPosCallback(DESKTOP_WINDOWwindow* handle,
                                                  DESKTOP_WINDOWwindowposfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWwindowposfun, window->callbacks.pos, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindowsizefun desktop_windowSetWindowSizeCallback(DESKTOP_WINDOWwindow* handle,
                                                    DESKTOP_WINDOWwindowsizefun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWwindowsizefun, window->callbacks.size, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindowclosefun desktop_windowSetWindowCloseCallback(DESKTOP_WINDOWwindow* handle,
                                                      DESKTOP_WINDOWwindowclosefun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWwindowclosefun, window->callbacks.close, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindowrefreshfun desktop_windowSetWindowRefreshCallback(DESKTOP_WINDOWwindow* handle,
                                                          DESKTOP_WINDOWwindowrefreshfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWwindowrefreshfun, window->callbacks.refresh, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindowfocusfun desktop_windowSetWindowFocusCallback(DESKTOP_WINDOWwindow* handle,
                                                      DESKTOP_WINDOWwindowfocusfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWwindowfocusfun, window->callbacks.focus, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindowiconifyfun desktop_windowSetWindowIconifyCallback(DESKTOP_WINDOWwindow* handle,
                                                          DESKTOP_WINDOWwindowiconifyfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWwindowiconifyfun, window->callbacks.iconify, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindowmaximizefun desktop_windowSetWindowMaximizeCallback(DESKTOP_WINDOWwindow* handle,
                                                            DESKTOP_WINDOWwindowmaximizefun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWwindowmaximizefun, window->callbacks.maximize, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWframebuffersizefun desktop_windowSetFramebufferSizeCallback(DESKTOP_WINDOWwindow* handle,
                                                              DESKTOP_WINDOWframebuffersizefun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWframebuffersizefun, window->callbacks.fbsize, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWwindowcontentscalefun desktop_windowSetWindowContentScaleCallback(DESKTOP_WINDOWwindow* handle,
                                                                    DESKTOP_WINDOWwindowcontentscalefun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWwindowcontentscalefun, window->callbacks.scale, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI void desktop_windowPollEvents(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.pollEvents();
}

DESKTOP_WINDOWAPI void desktop_windowWaitEvents(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.waitEvents();
}

DESKTOP_WINDOWAPI void desktop_windowWaitEventsTimeout(double timeout)
{
    _DESKTOP_WINDOW_REQUIRE_INIT();
    assert(timeout == timeout);
    assert(timeout >= 0.0);
    assert(timeout <= DBL_MAX);

    if (timeout != timeout || timeout < 0.0 || timeout > DBL_MAX)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Invalid time %f", timeout);
        return;
    }

    _desktop_window.platform.waitEventsTimeout(timeout);
}

DESKTOP_WINDOWAPI void desktop_windowPostEmptyEvent(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.postEmptyEvent();
}

