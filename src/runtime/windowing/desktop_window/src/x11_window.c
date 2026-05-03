#include "internal.h"

#if defined(_DESKTOP_WINDOW_X11)

#include <X11/cursorfont.h>
#include <X11/Xmd.h>

#include <poll.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

// Action for EWMH client messages
#define _NET_WM_STATE_REMOVE        0
#define _NET_WM_STATE_ADD           1
#define _NET_WM_STATE_TOGGLE        2

// Additional mouse button names for XButtonEvent
#define Button6            6
#define Button7            7

// Motif WM hints flags
#define MWM_HINTS_DECORATIONS   2
#define MWM_DECOR_ALL           1

#define _DESKTOP_WINDOW_XDND_VERSION 5

// Wait for event data to arrive on the X11 display socket
// This avoids blocking other threads via the per-display Xlib lock that also
// covers GLX functions
//
static DESKTOP_WINDOWbool waitForX11Event(double* timeout)
{
    struct pollfd fd = { ConnectionNumber(_desktop_window.x11.display), POLLIN };

    while (!XPending(_desktop_window.x11.display))
    {
        if (!_desktop_windowPollPOSIX(&fd, 1, timeout))
            return DESKTOP_WINDOW_FALSE;
    }

    return DESKTOP_WINDOW_TRUE;
}

// Wait for event data to arrive on any event file descriptor
// This avoids blocking other threads via the per-display Xlib lock that also
// covers GLX functions
//
static DESKTOP_WINDOWbool waitForAnyEvent(double* timeout)
{
    enum { XLIB_FD, PIPE_FD, INOTIFY_FD };
    struct pollfd fds[] =
    {
        [XLIB_FD] = { ConnectionNumber(_desktop_window.x11.display), POLLIN },
        [PIPE_FD] = { _desktop_window.x11.emptyEventPipe[0], POLLIN },
        [INOTIFY_FD] = { -1, POLLIN }
    };

#if defined(DESKTOP_WINDOW_BUILD_LINUX_JOYSTICK)
    if (_desktop_window.joysticksInitialized)
        fds[INOTIFY_FD].fd = _desktop_window.linjs.inotify;
#endif

    while (!XPending(_desktop_window.x11.display))
    {
        if (!_desktop_windowPollPOSIX(fds, sizeof(fds) / sizeof(fds[0]), timeout))
            return DESKTOP_WINDOW_FALSE;

        for (int i = 1; i < sizeof(fds) / sizeof(fds[0]); i++)
        {
            if (fds[i].revents & POLLIN)
                return DESKTOP_WINDOW_TRUE;
        }
    }

    return DESKTOP_WINDOW_TRUE;
}

// Writes a byte to the empty event pipe
//
static void writeEmptyEvent(void)
{
    for (;;)
    {
        const char byte = 0;
        const ssize_t result = write(_desktop_window.x11.emptyEventPipe[1], &byte, 1);
        if (result == 1 || (result == -1 && errno != EINTR))
            break;
    }
}

// Drains available data from the empty event pipe
//
static void drainEmptyEvents(void)
{
    for (;;)
    {
        char dummy[64];
        const ssize_t result = read(_desktop_window.x11.emptyEventPipe[0], dummy, sizeof(dummy));
        if (result == -1 && errno != EINTR)
            break;
    }
}

// Waits until a VisibilityNotify event arrives for the specified window or the
// timeout period elapses (ICCCM section 4.2.2)
//
static DESKTOP_WINDOWbool waitForVisibilityNotify(_DESKTOP_WINDOWwindow* window)
{
    XEvent dummy;
    double timeout = 0.1;

    while (!XCheckTypedWindowEvent(_desktop_window.x11.display,
                                   window->x11.handle,
                                   VisibilityNotify,
                                   &dummy))
    {
        if (!waitForX11Event(&timeout))
            return DESKTOP_WINDOW_FALSE;
    }

    return DESKTOP_WINDOW_TRUE;
}

// Returns whether the window is iconified
//
static int getWindowState(_DESKTOP_WINDOWwindow* window)
{
    int result = WithdrawnState;
    struct {
        CARD32 state;
        Window icon;
    } *state = NULL;

    if (_desktop_windowGetWindowPropertyX11(window->x11.handle,
                                  _desktop_window.x11.WM_STATE,
                                  _desktop_window.x11.WM_STATE,
                                  (unsigned char**) &state) >= 2)
    {
        result = state->state;
    }

    if (state)
        XFree(state);

    return result;
}

// Returns whether the event is a selection event
//
static Bool isSelectionEvent(Display* display, XEvent* event, XPointer pointer)
{
    if (event->xany.window != _desktop_window.x11.helperWindowHandle)
        return False;

    return event->type == SelectionRequest ||
           event->type == SelectionNotify ||
           event->type == SelectionClear;
}

// Returns whether it is a _NET_FRAME_EXTENTS event for the specified window
//
static Bool isFrameExtentsEvent(Display* display, XEvent* event, XPointer pointer)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) pointer;
    return event->type == PropertyNotify &&
           event->xproperty.state == PropertyNewValue &&
           event->xproperty.window == window->x11.handle &&
           event->xproperty.atom == _desktop_window.x11.NET_FRAME_EXTENTS;
}

// Returns whether it is a property event for the specified selection transfer
//
static Bool isSelPropNewValueNotify(Display* display, XEvent* event, XPointer pointer)
{
    XEvent* notification = (XEvent*) pointer;
    return event->type == PropertyNotify &&
           event->xproperty.state == PropertyNewValue &&
           event->xproperty.window == notification->xselection.requestor &&
           event->xproperty.atom == notification->xselection.property;
}

// Translates an X event modifier state mask
//
static int translateState(int state)
{
    int mods = 0;

    if (state & ShiftMask)
        mods |= DESKTOP_WINDOW_MOD_SHIFT;
    if (state & ControlMask)
        mods |= DESKTOP_WINDOW_MOD_CONTROL;
    if (state & Mod1Mask)
        mods |= DESKTOP_WINDOW_MOD_ALT;
    if (state & Mod4Mask)
        mods |= DESKTOP_WINDOW_MOD_SUPER;
    if (state & LockMask)
        mods |= DESKTOP_WINDOW_MOD_CAPS_LOCK;
    if (state & Mod2Mask)
        mods |= DESKTOP_WINDOW_MOD_NUM_LOCK;

    return mods;
}

// Translates an X11 key code to a DESKTOP_WINDOW key token
//
static int translateKey(int scancode)
{
    // Use the pre-filled LUT (see createKeyTables() in x11_init.c)
    if (scancode < 0 || scancode > 255)
        return DESKTOP_WINDOW_KEY_UNKNOWN;

    return _desktop_window.x11.keycodes[scancode];
}

// Sends an EWMH or ICCCM event to the window manager
//
static void sendEventToWM(_DESKTOP_WINDOWwindow* window, Atom type,
                          long a, long b, long c, long d, long e)
{
    XEvent event = { ClientMessage };
    event.xclient.window = window->x11.handle;
    event.xclient.format = 32; // Data is 32-bit longs
    event.xclient.message_type = type;
    event.xclient.data.l[0] = a;
    event.xclient.data.l[1] = b;
    event.xclient.data.l[2] = c;
    event.xclient.data.l[3] = d;
    event.xclient.data.l[4] = e;

    XSendEvent(_desktop_window.x11.display, _desktop_window.x11.root,
               False,
               SubstructureNotifyMask | SubstructureRedirectMask,
               &event);
}

// Updates the normal hints according to the window settings
//
static void updateNormalHints(_DESKTOP_WINDOWwindow* window, int width, int height)
{
    XSizeHints* hints = XAllocSizeHints();

    long supplied;
    XGetWMNormalHints(_desktop_window.x11.display, window->x11.handle, hints, &supplied);

    hints->flags &= ~(PMinSize | PMaxSize | PAspect);

    if (!window->monitor)
    {
        if (window->resizable)
        {
            if (window->minwidth != DESKTOP_WINDOW_DONT_CARE &&
                window->minheight != DESKTOP_WINDOW_DONT_CARE)
            {
                hints->flags |= PMinSize;
                hints->min_width = window->minwidth;
                hints->min_height = window->minheight;
            }

            if (window->maxwidth != DESKTOP_WINDOW_DONT_CARE &&
                window->maxheight != DESKTOP_WINDOW_DONT_CARE)
            {
                hints->flags |= PMaxSize;
                hints->max_width = window->maxwidth;
                hints->max_height = window->maxheight;
            }

            if (window->numer != DESKTOP_WINDOW_DONT_CARE &&
                window->denom != DESKTOP_WINDOW_DONT_CARE)
            {
                hints->flags |= PAspect;
                hints->min_aspect.x = hints->max_aspect.x = window->numer;
                hints->min_aspect.y = hints->max_aspect.y = window->denom;
            }
        }
        else
        {
            hints->flags |= (PMinSize | PMaxSize);
            hints->min_width  = hints->max_width  = width;
            hints->min_height = hints->max_height = height;
        }
    }

    XSetWMNormalHints(_desktop_window.x11.display, window->x11.handle, hints);
    XFree(hints);
}

// Updates the full screen status of the window
//
static void updateWindowMode(_DESKTOP_WINDOWwindow* window)
{
    if (window->monitor)
    {
        if (_desktop_window.x11.xinerama.available &&
            _desktop_window.x11.NET_WM_FULLSCREEN_MONITORS)
        {
            sendEventToWM(window,
                          _desktop_window.x11.NET_WM_FULLSCREEN_MONITORS,
                          window->monitor->x11.index,
                          window->monitor->x11.index,
                          window->monitor->x11.index,
                          window->monitor->x11.index,
                          0);
        }

        if (_desktop_window.x11.NET_WM_STATE && _desktop_window.x11.NET_WM_STATE_FULLSCREEN)
        {
            sendEventToWM(window,
                          _desktop_window.x11.NET_WM_STATE,
                          _NET_WM_STATE_ADD,
                          _desktop_window.x11.NET_WM_STATE_FULLSCREEN,
                          0, 1, 0);
        }
        else
        {
            // This is the butcher's way of removing window decorations
            // Setting the override-redirect attribute on a window makes the
            // window manager ignore the window completely (ICCCM, section 4)
            // The good thing is that this makes undecorated full screen windows
            // easy to do; the bad thing is that we have to do everything
            // manually and some things (like iconify/restore) won't work at
            // all, as those are tasks usually performed by the window manager

            XSetWindowAttributes attributes;
            attributes.override_redirect = True;
            XChangeWindowAttributes(_desktop_window.x11.display,
                                    window->x11.handle,
                                    CWOverrideRedirect,
                                    &attributes);

            window->x11.overrideRedirect = DESKTOP_WINDOW_TRUE;
        }

        // Enable compositor bypass
        if (!window->x11.transparent)
        {
            const unsigned long value = 1;

            XChangeProperty(_desktop_window.x11.display,  window->x11.handle,
                            _desktop_window.x11.NET_WM_BYPASS_COMPOSITOR, XA_CARDINAL, 32,
                            PropModeReplace, (unsigned char*) &value, 1);
        }
    }
    else
    {
        if (_desktop_window.x11.xinerama.available &&
            _desktop_window.x11.NET_WM_FULLSCREEN_MONITORS)
        {
            XDeleteProperty(_desktop_window.x11.display, window->x11.handle,
                            _desktop_window.x11.NET_WM_FULLSCREEN_MONITORS);
        }

        if (_desktop_window.x11.NET_WM_STATE && _desktop_window.x11.NET_WM_STATE_FULLSCREEN)
        {
            sendEventToWM(window,
                          _desktop_window.x11.NET_WM_STATE,
                          _NET_WM_STATE_REMOVE,
                          _desktop_window.x11.NET_WM_STATE_FULLSCREEN,
                          0, 1, 0);
        }
        else
        {
            XSetWindowAttributes attributes;
            attributes.override_redirect = False;
            XChangeWindowAttributes(_desktop_window.x11.display,
                                    window->x11.handle,
                                    CWOverrideRedirect,
                                    &attributes);

            window->x11.overrideRedirect = DESKTOP_WINDOW_FALSE;
        }

        // Disable compositor bypass
        if (!window->x11.transparent)
        {
            XDeleteProperty(_desktop_window.x11.display, window->x11.handle,
                            _desktop_window.x11.NET_WM_BYPASS_COMPOSITOR);
        }
    }
}

// Decode a Unicode code point from a UTF-8 stream
// Based on cutef8 by Jeff Bezanson (Public Domain)
//
static uint32_t decodeUTF8(const char** s)
{
    uint32_t codepoint = 0, count = 0;
    static const uint32_t offsets[] =
    {
        0x00000000u, 0x00003080u, 0x000e2080u,
        0x03c82080u, 0xfa082080u, 0x82082080u
    };

    do
    {
        codepoint = (codepoint << 6) + (unsigned char) **s;
        (*s)++;
        count++;
    } while ((**s & 0xc0) == 0x80);

    assert(count <= 6);
    return codepoint - offsets[count - 1];
}

// Convert the specified Latin-1 string to UTF-8
//
static char* convertLatin1toUTF8(const char* source)
{
    size_t size = 1;
    const char* sp;

    for (sp = source;  *sp;  sp++)
        size += (*sp & 0x80) ? 2 : 1;

    char* target = _desktop_window_calloc(size, 1);
    char* tp = target;

    for (sp = source;  *sp;  sp++)
        tp += _desktop_windowEncodeUTF8(tp, *sp);

    return target;
}

// Updates the cursor image according to its cursor mode
//
static void updateCursorImage(_DESKTOP_WINDOWwindow* window)
{
    if (window->cursorMode == DESKTOP_WINDOW_CURSOR_NORMAL ||
        window->cursorMode == DESKTOP_WINDOW_CURSOR_CAPTURED)
    {
        if (window->cursor)
        {
            XDefineCursor(_desktop_window.x11.display, window->x11.handle,
                          window->cursor->x11.handle);
        }
        else
            XUndefineCursor(_desktop_window.x11.display, window->x11.handle);
    }
    else
    {
        XDefineCursor(_desktop_window.x11.display, window->x11.handle,
                      _desktop_window.x11.hiddenCursorHandle);
    }
}

// Grabs the cursor and confines it to the window
//
static void captureCursor(_DESKTOP_WINDOWwindow* window)
{
    XGrabPointer(_desktop_window.x11.display, window->x11.handle, True,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync,
                 window->x11.handle,
                 None,
                 CurrentTime);
}

// Ungrabs the cursor
//
static void releaseCursor(void)
{
    XUngrabPointer(_desktop_window.x11.display, CurrentTime);
}

// Enable XI2 raw mouse motion events
//
static void enableRawMouseMotion(_DESKTOP_WINDOWwindow* window)
{
    XIEventMask em;
    unsigned char mask[XIMaskLen(XI_RawMotion)] = { 0 };

    em.deviceid = XIAllMasterDevices;
    em.mask_len = sizeof(mask);
    em.mask = mask;
    XISetMask(mask, XI_RawMotion);

    XISelectEvents(_desktop_window.x11.display, _desktop_window.x11.root, &em, 1);
}

// Disable XI2 raw mouse motion events
//
static void disableRawMouseMotion(_DESKTOP_WINDOWwindow* window)
{
    XIEventMask em;
    unsigned char mask[] = { 0 };

    em.deviceid = XIAllMasterDevices;
    em.mask_len = sizeof(mask);
    em.mask = mask;

    XISelectEvents(_desktop_window.x11.display, _desktop_window.x11.root, &em, 1);
}

// Apply disabled cursor mode to a focused window
//
static void disableCursor(_DESKTOP_WINDOWwindow* window)
{
    if (window->rawMouseMotion)
        enableRawMouseMotion(window);

    _desktop_window.x11.disabledCursorWindow = window;
    _desktop_windowGetCursorPosX11(window,
                         &_desktop_window.x11.restoreCursorPosX,
                         &_desktop_window.x11.restoreCursorPosY);
    updateCursorImage(window);
    _desktop_windowCenterCursorInContentArea(window);
    captureCursor(window);
}

// Exit disabled cursor mode for the specified window
//
static void enableCursor(_DESKTOP_WINDOWwindow* window)
{
    if (window->rawMouseMotion)
        disableRawMouseMotion(window);

    _desktop_window.x11.disabledCursorWindow = NULL;
    releaseCursor();
    _desktop_windowSetCursorPosX11(window,
                         _desktop_window.x11.restoreCursorPosX,
                         _desktop_window.x11.restoreCursorPosY);
    updateCursorImage(window);
}

// Clear its handle when the input context has been destroyed
//
static void inputContextDestroyCallback(XIC ic, XPointer clientData, XPointer callData)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) clientData;
    window->x11.ic = NULL;
}

// Create the X11 window (and its colormap)
//
static DESKTOP_WINDOWbool createNativeWindow(_DESKTOP_WINDOWwindow* window,
                                   const _DESKTOP_WINDOWwndconfig* wndconfig,
                                   Visual* visual, int depth)
{
    int width = wndconfig->width;
    int height = wndconfig->height;

    if (wndconfig->scaleToMonitor)
    {
        width *= _desktop_window.x11.contentScaleX;
        height *= _desktop_window.x11.contentScaleY;
    }

    int xpos = 0, ypos = 0;

    if (wndconfig->xpos != DESKTOP_WINDOW_ANY_POSITION && wndconfig->ypos != DESKTOP_WINDOW_ANY_POSITION)
    {
        xpos = wndconfig->xpos;
        ypos = wndconfig->ypos;
    }

    // Create a colormap based on the visual used by the current context
    window->x11.colormap = XCreateColormap(_desktop_window.x11.display,
                                           _desktop_window.x11.root,
                                           visual,
                                           AllocNone);

    window->x11.transparent = _desktop_windowIsVisualTransparentX11(visual);

    XSetWindowAttributes wa = { 0 };
    wa.colormap = window->x11.colormap;
    wa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                    PointerMotionMask | ButtonPressMask | ButtonReleaseMask |
                    ExposureMask | FocusChangeMask | VisibilityChangeMask |
                    EnterWindowMask | LeaveWindowMask | PropertyChangeMask;

    _desktop_windowGrabErrorHandlerX11();

    window->x11.parent = _desktop_window.x11.root;
    window->x11.handle = XCreateWindow(_desktop_window.x11.display,
                                       _desktop_window.x11.root,
                                       xpos, ypos,
                                       width, height,
                                       0,      // Border width
                                       depth,  // Color depth
                                       InputOutput,
                                       visual,
                                       CWBorderPixel | CWColormap | CWEventMask,
                                       &wa);

    _desktop_windowReleaseErrorHandlerX11();

    if (!window->x11.handle)
    {
        _desktop_windowInputErrorX11(DESKTOP_WINDOW_PLATFORM_ERROR,
                           "X11: Failed to create window");
        return DESKTOP_WINDOW_FALSE;
    }

    XSaveContext(_desktop_window.x11.display,
                 window->x11.handle,
                 _desktop_window.x11.context,
                 (XPointer) window);

    if (!wndconfig->decorated)
        _desktop_windowSetWindowDecoratedX11(window, DESKTOP_WINDOW_FALSE);

    if (_desktop_window.x11.NET_WM_STATE && !window->monitor)
    {
        Atom states[3];
        int count = 0;

        if (wndconfig->floating)
        {
            if (_desktop_window.x11.NET_WM_STATE_ABOVE)
                states[count++] = _desktop_window.x11.NET_WM_STATE_ABOVE;
        }

        if (wndconfig->maximized)
        {
            if (_desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT &&
                _desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ)
            {
                states[count++] = _desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT;
                states[count++] = _desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ;
                window->x11.maximized = DESKTOP_WINDOW_TRUE;
            }
        }

        if (count)
        {
            XChangeProperty(_desktop_window.x11.display, window->x11.handle,
                            _desktop_window.x11.NET_WM_STATE, XA_ATOM, 32,
                            PropModeReplace, (unsigned char*) states, count);
        }
    }

    // Declare the WM protocols supported by DESKTOP_WINDOW
    {
        Atom protocols[] =
        {
            _desktop_window.x11.WM_DELETE_WINDOW,
            _desktop_window.x11.NET_WM_PING
        };

        XSetWMProtocols(_desktop_window.x11.display, window->x11.handle,
                        protocols, sizeof(protocols) / sizeof(Atom));
    }

    // Declare our PID
    {
        const long pid = getpid();

        XChangeProperty(_desktop_window.x11.display,  window->x11.handle,
                        _desktop_window.x11.NET_WM_PID, XA_CARDINAL, 32,
                        PropModeReplace,
                        (unsigned char*) &pid, 1);
    }

    if (_desktop_window.x11.NET_WM_WINDOW_TYPE && _desktop_window.x11.NET_WM_WINDOW_TYPE_NORMAL)
    {
        Atom type = _desktop_window.x11.NET_WM_WINDOW_TYPE_NORMAL;
        XChangeProperty(_desktop_window.x11.display,  window->x11.handle,
                        _desktop_window.x11.NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                        PropModeReplace, (unsigned char*) &type, 1);
    }

    // Set ICCCM WM_HINTS property
    {
        XWMHints* hints = XAllocWMHints();
        if (!hints)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_OUT_OF_MEMORY,
                            "X11: Failed to allocate WM hints");
            return DESKTOP_WINDOW_FALSE;
        }

        hints->flags = StateHint;
        hints->initial_state = NormalState;

        XSetWMHints(_desktop_window.x11.display, window->x11.handle, hints);
        XFree(hints);
    }

    // Set ICCCM WM_NORMAL_HINTS property
    {
        XSizeHints* hints = XAllocSizeHints();
        if (!hints)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_OUT_OF_MEMORY, "X11: Failed to allocate size hints");
            return DESKTOP_WINDOW_FALSE;
        }

        if (!wndconfig->resizable)
        {
            hints->flags |= (PMinSize | PMaxSize);
            hints->min_width  = hints->max_width  = width;
            hints->min_height = hints->max_height = height;
        }

        // HACK: Explicitly setting PPosition to any value causes some WMs, notably
        //       Compiz and Metacity, to honor the position of unmapped windows
        if (wndconfig->xpos != DESKTOP_WINDOW_ANY_POSITION && wndconfig->ypos != DESKTOP_WINDOW_ANY_POSITION)
        {
            hints->flags |= PPosition;
            hints->x = 0;
            hints->y = 0;
        }

        hints->flags |= PWinGravity;
        hints->win_gravity = StaticGravity;

        XSetWMNormalHints(_desktop_window.x11.display, window->x11.handle, hints);
        XFree(hints);
    }

    // Set ICCCM WM_CLASS property
    {
        XClassHint* hint = XAllocClassHint();

        if (strlen(wndconfig->x11.instanceName) &&
            strlen(wndconfig->x11.className))
        {
            hint->res_name = (char*) wndconfig->x11.instanceName;
            hint->res_class = (char*) wndconfig->x11.className;
        }
        else
        {
            const char* resourceName = getenv("RESOURCE_NAME");
            if (resourceName && strlen(resourceName))
                hint->res_name = (char*) resourceName;
            else if (strlen(wndconfig->title))
                hint->res_name = (char*) wndconfig->title;
            else
                hint->res_name = (char*) "desktop_window-application";

            if (strlen(wndconfig->title))
                hint->res_class = (char*) wndconfig->title;
            else
                hint->res_class = (char*) "DESKTOP_WINDOW-Application";
        }

        XSetClassHint(_desktop_window.x11.display, window->x11.handle, hint);
        XFree(hint);
    }

    // Announce support for Xdnd (drag and drop)
    {
        const Atom version = _DESKTOP_WINDOW_XDND_VERSION;
        XChangeProperty(_desktop_window.x11.display, window->x11.handle,
                        _desktop_window.x11.XdndAware, XA_ATOM, 32,
                        PropModeReplace, (unsigned char*) &version, 1);
    }

    if (_desktop_window.x11.im)
        _desktop_windowCreateInputContextX11(window);

    _desktop_windowSetWindowTitleX11(window, wndconfig->title);
    _desktop_windowGetWindowPosX11(window, &window->x11.xpos, &window->x11.ypos);
    _desktop_windowGetWindowSizeX11(window, &window->x11.width, &window->x11.height);

    return DESKTOP_WINDOW_TRUE;
}

// Set the specified property to the selection converted to the requested target
//
static Atom writeTargetToProperty(const XSelectionRequestEvent* request)
{
    char* selectionString = NULL;
    const Atom formats[] = { _desktop_window.x11.UTF8_STRING, XA_STRING };
    const int formatCount = sizeof(formats) / sizeof(formats[0]);

    if (request->selection == _desktop_window.x11.PRIMARY)
        selectionString = _desktop_window.x11.primarySelectionString;
    else
        selectionString = _desktop_window.x11.clipboardString;

    if (request->property == None)
    {
        // The requester is a legacy client (ICCCM section 2.2)
        // We don't support legacy clients, so fail here
        return None;
    }

    if (request->target == _desktop_window.x11.TARGETS)
    {
        // The list of supported targets was requested

        const Atom targets[] = { _desktop_window.x11.TARGETS,
                                 _desktop_window.x11.MULTIPLE,
                                 _desktop_window.x11.UTF8_STRING,
                                 XA_STRING };

        XChangeProperty(_desktop_window.x11.display,
                        request->requestor,
                        request->property,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        (unsigned char*) targets,
                        sizeof(targets) / sizeof(targets[0]));

        return request->property;
    }

    if (request->target == _desktop_window.x11.MULTIPLE)
    {
        // Multiple conversions were requested

        Atom* targets;
        const unsigned long count =
            _desktop_windowGetWindowPropertyX11(request->requestor,
                                      request->property,
                                      _desktop_window.x11.ATOM_PAIR,
                                      (unsigned char**) &targets);

        for (unsigned long i = 0;  i < count;  i += 2)
        {
            int j;

            for (j = 0;  j < formatCount;  j++)
            {
                if (targets[i] == formats[j])
                    break;
            }

            if (j < formatCount)
            {
                XChangeProperty(_desktop_window.x11.display,
                                request->requestor,
                                targets[i + 1],
                                targets[i],
                                8,
                                PropModeReplace,
                                (unsigned char *) selectionString,
                                strlen(selectionString));
            }
            else
                targets[i + 1] = None;
        }

        XChangeProperty(_desktop_window.x11.display,
                        request->requestor,
                        request->property,
                        _desktop_window.x11.ATOM_PAIR,
                        32,
                        PropModeReplace,
                        (unsigned char*) targets,
                        count);

        XFree(targets);

        return request->property;
    }

    if (request->target == _desktop_window.x11.SAVE_TARGETS)
    {
        // The request is a check whether we support SAVE_TARGETS
        // It should be handled as a no-op side effect target

        XChangeProperty(_desktop_window.x11.display,
                        request->requestor,
                        request->property,
                        _desktop_window.x11.NULL_,
                        32,
                        PropModeReplace,
                        NULL,
                        0);

        return request->property;
    }

    // Conversion to a data target was requested

    for (int i = 0;  i < formatCount;  i++)
    {
        if (request->target == formats[i])
        {
            // The requested target is one we support

            XChangeProperty(_desktop_window.x11.display,
                            request->requestor,
                            request->property,
                            request->target,
                            8,
                            PropModeReplace,
                            (unsigned char *) selectionString,
                            strlen(selectionString));

            return request->property;
        }
    }

    // The requested target is not supported

    return None;
}

static void handleSelectionRequest(XEvent* event)
{
    const XSelectionRequestEvent* request = &event->xselectionrequest;

    XEvent reply = { SelectionNotify };
    reply.xselection.property = writeTargetToProperty(request);
    reply.xselection.display = request->display;
    reply.xselection.requestor = request->requestor;
    reply.xselection.selection = request->selection;
    reply.xselection.target = request->target;
    reply.xselection.time = request->time;

    XSendEvent(_desktop_window.x11.display, request->requestor, False, 0, &reply);
}

static const char* getSelectionString(Atom selection)
{
    char** selectionString = NULL;
    const Atom targets[] = { _desktop_window.x11.UTF8_STRING, XA_STRING };
    const size_t targetCount = sizeof(targets) / sizeof(targets[0]);

    if (selection == _desktop_window.x11.PRIMARY)
        selectionString = &_desktop_window.x11.primarySelectionString;
    else
        selectionString = &_desktop_window.x11.clipboardString;

    if (XGetSelectionOwner(_desktop_window.x11.display, selection) ==
        _desktop_window.x11.helperWindowHandle)
    {
        // Instead of doing a large number of X round-trips just to put this
        // string into a window property and then read it back, just return it
        return *selectionString;
    }

    _desktop_window_free(*selectionString);
    *selectionString = NULL;

    for (size_t i = 0;  i < targetCount;  i++)
    {
        char* data;
        Atom actualType;
        int actualFormat;
        unsigned long itemCount, bytesAfter;
        XEvent notification, dummy;

        XConvertSelection(_desktop_window.x11.display,
                          selection,
                          targets[i],
                          _desktop_window.x11.DESKTOP_WINDOW_SELECTION,
                          _desktop_window.x11.helperWindowHandle,
                          CurrentTime);

        while (!XCheckTypedWindowEvent(_desktop_window.x11.display,
                                       _desktop_window.x11.helperWindowHandle,
                                       SelectionNotify,
                                       &notification))
        {
            waitForX11Event(NULL);
        }

        if (notification.xselection.property == None)
            continue;

        XCheckIfEvent(_desktop_window.x11.display,
                      &dummy,
                      isSelPropNewValueNotify,
                      (XPointer) &notification);

        XGetWindowProperty(_desktop_window.x11.display,
                           notification.xselection.requestor,
                           notification.xselection.property,
                           0,
                           LONG_MAX,
                           True,
                           AnyPropertyType,
                           &actualType,
                           &actualFormat,
                           &itemCount,
                           &bytesAfter,
                           (unsigned char**) &data);

        if (actualType == _desktop_window.x11.INCR)
        {
            size_t size = 1;
            char* string = NULL;

            for (;;)
            {
                while (!XCheckIfEvent(_desktop_window.x11.display,
                                      &dummy,
                                      isSelPropNewValueNotify,
                                      (XPointer) &notification))
                {
                    waitForX11Event(NULL);
                }

                XFree(data);
                XGetWindowProperty(_desktop_window.x11.display,
                                   notification.xselection.requestor,
                                   notification.xselection.property,
                                   0,
                                   LONG_MAX,
                                   True,
                                   AnyPropertyType,
                                   &actualType,
                                   &actualFormat,
                                   &itemCount,
                                   &bytesAfter,
                                   (unsigned char**) &data);

                if (itemCount)
                {
                    size += itemCount;
                    string = _desktop_window_realloc(string, size);
                    string[size - itemCount - 1] = '\0';
                    strcat(string, data);
                }

                if (!itemCount)
                {
                    if (string)
                    {
                        if (targets[i] == XA_STRING)
                        {
                            *selectionString = convertLatin1toUTF8(string);
                            _desktop_window_free(string);
                        }
                        else
                            *selectionString = string;
                    }

                    break;
                }
            }
        }
        else if (actualType == targets[i])
        {
            if (targets[i] == XA_STRING)
                *selectionString = convertLatin1toUTF8(data);
            else
                *selectionString = _desktop_window_strdup(data);
        }

        XFree(data);

        if (*selectionString)
            break;
    }

    if (!*selectionString)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_FORMAT_UNAVAILABLE,
                        "X11: Failed to convert selection to string");
    }

    return *selectionString;
}

// Make the specified window and its video mode active on its monitor
//
static void acquireMonitor(_DESKTOP_WINDOWwindow* window)
{
    if (_desktop_window.x11.saver.count == 0)
    {
        // Remember old screen saver settings
        XGetScreenSaver(_desktop_window.x11.display,
                        &_desktop_window.x11.saver.timeout,
                        &_desktop_window.x11.saver.interval,
                        &_desktop_window.x11.saver.blanking,
                        &_desktop_window.x11.saver.exposure);

        // Disable screen saver
        XSetScreenSaver(_desktop_window.x11.display, 0, 0, DontPreferBlanking,
                        DefaultExposures);
    }

    if (!window->monitor->window)
        _desktop_window.x11.saver.count++;

    _desktop_windowSetVideoModeX11(window->monitor, &window->videoMode);

    if (window->x11.overrideRedirect)
    {
        int xpos, ypos;
        DESKTOP_WINDOWvidmode mode;

        // Manually position the window over its monitor
        _desktop_windowGetMonitorPosX11(window->monitor, &xpos, &ypos);
        _desktop_windowGetVideoModeX11(window->monitor, &mode);

        XMoveResizeWindow(_desktop_window.x11.display, window->x11.handle,
                          xpos, ypos, mode.width, mode.height);
    }

    _desktop_windowInputMonitorWindow(window->monitor, window);
}

// Remove the window and restore the original video mode
//
static void releaseMonitor(_DESKTOP_WINDOWwindow* window)
{
    if (window->monitor->window != window)
        return;

    _desktop_windowInputMonitorWindow(window->monitor, NULL);
    _desktop_windowRestoreVideoModeX11(window->monitor);

    _desktop_window.x11.saver.count--;

    if (_desktop_window.x11.saver.count == 0)
    {
        // Restore old screen saver settings
        XSetScreenSaver(_desktop_window.x11.display,
                        _desktop_window.x11.saver.timeout,
                        _desktop_window.x11.saver.interval,
                        _desktop_window.x11.saver.blanking,
                        _desktop_window.x11.saver.exposure);
    }
}

// Process the specified X event
//
static void processEvent(XEvent *event)
{
    int keycode = 0;
    Bool filtered = False;

    // HACK: Save scancode as some IMs clear the field in XFilterEvent
    if (event->type == KeyPress || event->type == KeyRelease)
        keycode = event->xkey.keycode;

    filtered = XFilterEvent(event, None);

    if (_desktop_window.x11.randr.available)
    {
        if (event->type == _desktop_window.x11.randr.eventBase + RRNotify)
        {
            XRRUpdateConfiguration(event);
            _desktop_windowPollMonitorsX11();
            return;
        }
    }

    if (_desktop_window.x11.xkb.available)
    {
        if (event->type == _desktop_window.x11.xkb.eventBase + XkbEventCode)
        {
            if (((XkbEvent*) event)->any.xkb_type == XkbStateNotify &&
                (((XkbEvent*) event)->state.changed & XkbGroupStateMask))
            {
                _desktop_window.x11.xkb.group = ((XkbEvent*) event)->state.group;
            }

            return;
        }
    }

    if (event->type == GenericEvent)
    {
        if (_desktop_window.x11.xi.available)
        {
            _DESKTOP_WINDOWwindow* window = _desktop_window.x11.disabledCursorWindow;

            if (window &&
                window->rawMouseMotion &&
                event->xcookie.extension == _desktop_window.x11.xi.majorOpcode &&
                XGetEventData(_desktop_window.x11.display, &event->xcookie) &&
                event->xcookie.evtype == XI_RawMotion)
            {
                XIRawEvent* re = event->xcookie.data;
                if (re->valuators.mask_len)
                {
                    const double* values = re->raw_values;
                    double xpos = window->virtualCursorPosX;
                    double ypos = window->virtualCursorPosY;

                    if (XIMaskIsSet(re->valuators.mask, 0))
                    {
                        xpos += *values;
                        values++;
                    }

                    if (XIMaskIsSet(re->valuators.mask, 1))
                        ypos += *values;

                    _desktop_windowInputCursorPos(window, xpos, ypos);
                }
            }

            XFreeEventData(_desktop_window.x11.display, &event->xcookie);
        }

        return;
    }

    if (event->type == SelectionRequest)
    {
        handleSelectionRequest(event);
        return;
    }

    _DESKTOP_WINDOWwindow* window = NULL;
    if (XFindContext(_desktop_window.x11.display,
                     event->xany.window,
                     _desktop_window.x11.context,
                     (XPointer*) &window) != 0)
    {
        // This is an event for a window that has already been destroyed
        return;
    }

    switch (event->type)
    {
        case ReparentNotify:
        {
            window->x11.parent = event->xreparent.parent;
            return;
        }

        case KeyPress:
        {
            const int key = translateKey(keycode);
            const int mods = translateState(event->xkey.state);
            const int plain = !(mods & (DESKTOP_WINDOW_MOD_CONTROL | DESKTOP_WINDOW_MOD_ALT));

            if (window->x11.ic)
            {
                // HACK: Do not report the key press events duplicated by XIM
                //       Duplicate key releases are filtered out implicitly by
                //       the DESKTOP_WINDOW key repeat logic in _desktop_windowInputKey
                //       A timestamp per key is used to handle simultaneous keys
                // NOTE: Always allow the first event for each key through
                //       (the server never sends a timestamp of zero)
                // NOTE: Timestamp difference is compared to handle wrap-around
                Time diff = event->xkey.time - window->x11.keyPressTimes[keycode];
                if (diff == event->xkey.time || (diff > 0 && diff < ((Time)1 << 31)))
                {
                    if (keycode)
                        _desktop_windowInputKey(window, key, keycode, DESKTOP_WINDOW_PRESS, mods);

                    window->x11.keyPressTimes[keycode] = event->xkey.time;
                }

                if (!filtered)
                {
                    int count;
                    Status status;
                    char buffer[100];
                    char* chars = buffer;

                    count = Xutf8LookupString(window->x11.ic,
                                              &event->xkey,
                                              buffer, sizeof(buffer) - 1,
                                              NULL, &status);

                    if (status == XBufferOverflow)
                    {
                        chars = _desktop_window_calloc(count + 1, 1);
                        count = Xutf8LookupString(window->x11.ic,
                                                  &event->xkey,
                                                  chars, count,
                                                  NULL, &status);
                    }

                    if (status == XLookupChars || status == XLookupBoth)
                    {
                        const char* c = chars;
                        chars[count] = '\0';
                        while (c - chars < count)
                            _desktop_windowInputChar(window, decodeUTF8(&c), mods, plain);
                    }

                    if (chars != buffer)
                        _desktop_window_free(chars);
                }
            }
            else
            {
                KeySym keysym;
                XLookupString(&event->xkey, NULL, 0, &keysym, NULL);

                _desktop_windowInputKey(window, key, keycode, DESKTOP_WINDOW_PRESS, mods);

                const uint32_t codepoint = _desktop_windowKeySym2Unicode(keysym);
                if (codepoint != DESKTOP_WINDOW_INVALID_CODEPOINT)
                    _desktop_windowInputChar(window, codepoint, mods, plain);
            }

            return;
        }

        case KeyRelease:
        {
            const int key = translateKey(keycode);
            const int mods = translateState(event->xkey.state);

            if (!_desktop_window.x11.xkb.detectable)
            {
                // HACK: Key repeat events will arrive as KeyRelease/KeyPress
                //       pairs with similar or identical time stamps
                //       The key repeat logic in _desktop_windowInputKey expects only key
                //       presses to repeat, so detect and discard release events
                if (XEventsQueued(_desktop_window.x11.display, QueuedAfterReading))
                {
                    XEvent next;
                    XPeekEvent(_desktop_window.x11.display, &next);

                    if (next.type == KeyPress &&
                        next.xkey.window == event->xkey.window &&
                        next.xkey.keycode == keycode)
                    {
                        // HACK: The time of repeat events sometimes doesn't
                        //       match that of the press event, so add an
                        //       epsilon
                        //       Toshiyuki Takahashi can press a button
                        //       16 times per second so it's fairly safe to
                        //       assume that no human is pressing the key 50
                        //       times per second (value is ms)
                        if ((next.xkey.time - event->xkey.time) < 20)
                        {
                            // This is very likely a server-generated key repeat
                            // event, so ignore it
                            return;
                        }
                    }
                }
            }

            _desktop_windowInputKey(window, key, keycode, DESKTOP_WINDOW_RELEASE, mods);
            return;
        }

        case ButtonPress:
        {
            const int mods = translateState(event->xbutton.state);

            if (event->xbutton.button == Button1)
                _desktop_windowInputMouseClick(window, DESKTOP_WINDOW_MOUSE_BUTTON_LEFT, DESKTOP_WINDOW_PRESS, mods);
            else if (event->xbutton.button == Button2)
                _desktop_windowInputMouseClick(window, DESKTOP_WINDOW_MOUSE_BUTTON_MIDDLE, DESKTOP_WINDOW_PRESS, mods);
            else if (event->xbutton.button == Button3)
                _desktop_windowInputMouseClick(window, DESKTOP_WINDOW_MOUSE_BUTTON_RIGHT, DESKTOP_WINDOW_PRESS, mods);

            // Modern X provides scroll events as mouse button presses
            else if (event->xbutton.button == Button4)
                _desktop_windowInputScroll(window, 0.0, 1.0);
            else if (event->xbutton.button == Button5)
                _desktop_windowInputScroll(window, 0.0, -1.0);
            else if (event->xbutton.button == Button6)
                _desktop_windowInputScroll(window, 1.0, 0.0);
            else if (event->xbutton.button == Button7)
                _desktop_windowInputScroll(window, -1.0, 0.0);

            else
            {
                // Additional buttons after 7 are treated as regular buttons
                // We subtract 4 to fill the gap left by scroll input above
                _desktop_windowInputMouseClick(window,
                                     event->xbutton.button - Button1 - 4,
                                     DESKTOP_WINDOW_PRESS,
                                     mods);
            }

            return;
        }

        case ButtonRelease:
        {
            const int mods = translateState(event->xbutton.state);

            if (event->xbutton.button == Button1)
            {
                _desktop_windowInputMouseClick(window,
                                     DESKTOP_WINDOW_MOUSE_BUTTON_LEFT,
                                     DESKTOP_WINDOW_RELEASE,
                                     mods);
            }
            else if (event->xbutton.button == Button2)
            {
                _desktop_windowInputMouseClick(window,
                                     DESKTOP_WINDOW_MOUSE_BUTTON_MIDDLE,
                                     DESKTOP_WINDOW_RELEASE,
                                     mods);
            }
            else if (event->xbutton.button == Button3)
            {
                _desktop_windowInputMouseClick(window,
                                     DESKTOP_WINDOW_MOUSE_BUTTON_RIGHT,
                                     DESKTOP_WINDOW_RELEASE,
                                     mods);
            }
            else if (event->xbutton.button > Button7)
            {
                // Additional buttons after 7 are treated as regular buttons
                // We subtract 4 to fill the gap left by scroll input above
                _desktop_windowInputMouseClick(window,
                                     event->xbutton.button - Button1 - 4,
                                     DESKTOP_WINDOW_RELEASE,
                                     mods);
            }

            return;
        }

        case EnterNotify:
        {
            // XEnterWindowEvent is XCrossingEvent
            const int x = event->xcrossing.x;
            const int y = event->xcrossing.y;

            // HACK: This is a workaround for WMs (KWM, Fluxbox) that otherwise
            //       ignore the defined cursor for hidden cursor mode
            if (window->cursorMode == DESKTOP_WINDOW_CURSOR_HIDDEN)
                updateCursorImage(window);

            _desktop_windowInputCursorEnter(window, DESKTOP_WINDOW_TRUE);
            _desktop_windowInputCursorPos(window, x, y);

            window->x11.lastCursorPosX = x;
            window->x11.lastCursorPosY = y;
            return;
        }

        case LeaveNotify:
        {
            _desktop_windowInputCursorEnter(window, DESKTOP_WINDOW_FALSE);
            return;
        }

        case MotionNotify:
        {
            const int x = event->xmotion.x;
            const int y = event->xmotion.y;

            if (x != window->x11.warpCursorPosX ||
                y != window->x11.warpCursorPosY)
            {
                // The cursor was moved by something other than DESKTOP_WINDOW

                if (window->cursorMode == DESKTOP_WINDOW_CURSOR_DISABLED)
                {
                    if (_desktop_window.x11.disabledCursorWindow != window)
                        return;
                    if (window->rawMouseMotion)
                        return;

                    const int dx = x - window->x11.lastCursorPosX;
                    const int dy = y - window->x11.lastCursorPosY;

                    _desktop_windowInputCursorPos(window,
                                        window->virtualCursorPosX + dx,
                                        window->virtualCursorPosY + dy);
                }
                else
                    _desktop_windowInputCursorPos(window, x, y);
            }

            window->x11.lastCursorPosX = x;
            window->x11.lastCursorPosY = y;
            return;
        }

        case ConfigureNotify:
        {
            if (event->xconfigure.width != window->x11.width ||
                event->xconfigure.height != window->x11.height)
            {
                window->x11.width = event->xconfigure.width;
                window->x11.height = event->xconfigure.height;

                _desktop_windowInputFramebufferSize(window,
                                          event->xconfigure.width,
                                          event->xconfigure.height);

                _desktop_windowInputWindowSize(window,
                                     event->xconfigure.width,
                                     event->xconfigure.height);
            }

            int xpos = event->xconfigure.x;
            int ypos = event->xconfigure.y;

            // NOTE: ConfigureNotify events from the server are in local
            //       coordinates, so if we are reparented we need to translate
            //       the position into root (screen) coordinates
            if (!event->xany.send_event && window->x11.parent != _desktop_window.x11.root)
            {
                _desktop_windowGrabErrorHandlerX11();

                Window dummy;
                XTranslateCoordinates(_desktop_window.x11.display,
                                      window->x11.parent,
                                      _desktop_window.x11.root,
                                      xpos, ypos,
                                      &xpos, &ypos,
                                      &dummy);

                _desktop_windowReleaseErrorHandlerX11();
                if (_desktop_window.x11.errorCode == BadWindow)
                    return;
            }

            if (xpos != window->x11.xpos || ypos != window->x11.ypos)
            {
                window->x11.xpos = xpos;
                window->x11.ypos = ypos;

                _desktop_windowInputWindowPos(window, xpos, ypos);
            }

            return;
        }

        case ClientMessage:
        {
            // Custom client message, probably from the window manager

            if (filtered)
                return;

            if (event->xclient.message_type == None)
                return;

            if (event->xclient.message_type == _desktop_window.x11.WM_PROTOCOLS)
            {
                const Atom protocol = event->xclient.data.l[0];
                if (protocol == None)
                    return;

                if (protocol == _desktop_window.x11.WM_DELETE_WINDOW)
                {
                    // The window manager was asked to close the window, for
                    // example by the user pressing a 'close' window decoration
                    // button
                    _desktop_windowInputWindowCloseRequest(window);
                }
                else if (protocol == _desktop_window.x11.NET_WM_PING)
                {
                    // The window manager is pinging the application to ensure
                    // it's still responding to events

                    XEvent reply = *event;
                    reply.xclient.window = _desktop_window.x11.root;

                    XSendEvent(_desktop_window.x11.display, _desktop_window.x11.root,
                               False,
                               SubstructureNotifyMask | SubstructureRedirectMask,
                               &reply);
                }
            }
            else if (event->xclient.message_type == _desktop_window.x11.XdndEnter)
            {
                // A drag operation has entered the window
                unsigned long count;
                Atom* formats = NULL;
                const DESKTOP_WINDOWbool list = event->xclient.data.l[1] & 1;

                _desktop_window.x11.xdnd.source  = event->xclient.data.l[0];
                _desktop_window.x11.xdnd.version = event->xclient.data.l[1] >> 24;
                _desktop_window.x11.xdnd.format  = None;

                if (_desktop_window.x11.xdnd.version > _DESKTOP_WINDOW_XDND_VERSION)
                    return;

                if (list)
                {
                    count = _desktop_windowGetWindowPropertyX11(_desktop_window.x11.xdnd.source,
                                                      _desktop_window.x11.XdndTypeList,
                                                      XA_ATOM,
                                                      (unsigned char**) &formats);
                }
                else
                {
                    count = 3;
                    formats = (Atom*) event->xclient.data.l + 2;
                }

                for (unsigned int i = 0;  i < count;  i++)
                {
                    if (formats[i] == _desktop_window.x11.text_uri_list)
                    {
                        _desktop_window.x11.xdnd.format = _desktop_window.x11.text_uri_list;
                        break;
                    }
                }

                if (list && formats)
                    XFree(formats);
            }
            else if (event->xclient.message_type == _desktop_window.x11.XdndDrop)
            {
                // The drag operation has finished by dropping on the window
                Time time = CurrentTime;

                if (_desktop_window.x11.xdnd.version > _DESKTOP_WINDOW_XDND_VERSION)
                    return;

                if (_desktop_window.x11.xdnd.format)
                {
                    if (_desktop_window.x11.xdnd.version >= 1)
                        time = event->xclient.data.l[2];

                    // Request the chosen format from the source window
                    XConvertSelection(_desktop_window.x11.display,
                                      _desktop_window.x11.XdndSelection,
                                      _desktop_window.x11.xdnd.format,
                                      _desktop_window.x11.XdndSelection,
                                      window->x11.handle,
                                      time);
                }
                else if (_desktop_window.x11.xdnd.version >= 2)
                {
                    XEvent reply = { ClientMessage };
                    reply.xclient.window = _desktop_window.x11.xdnd.source;
                    reply.xclient.message_type = _desktop_window.x11.XdndFinished;
                    reply.xclient.format = 32;
                    reply.xclient.data.l[0] = window->x11.handle;
                    reply.xclient.data.l[1] = 0; // The drag was rejected
                    reply.xclient.data.l[2] = None;

                    XSendEvent(_desktop_window.x11.display, _desktop_window.x11.xdnd.source,
                               False, NoEventMask, &reply);
                    XFlush(_desktop_window.x11.display);
                }
            }
            else if (event->xclient.message_type == _desktop_window.x11.XdndPosition)
            {
                // The drag operation has moved over the window
                const int xabs = (event->xclient.data.l[2] >> 16) & 0xffff;
                const int yabs = (event->xclient.data.l[2]) & 0xffff;
                Window dummy;
                int xpos, ypos;

                if (_desktop_window.x11.xdnd.version > _DESKTOP_WINDOW_XDND_VERSION)
                    return;

                XTranslateCoordinates(_desktop_window.x11.display,
                                      _desktop_window.x11.root,
                                      window->x11.handle,
                                      xabs, yabs,
                                      &xpos, &ypos,
                                      &dummy);

                _desktop_windowInputCursorPos(window, xpos, ypos);

                XEvent reply = { ClientMessage };
                reply.xclient.window = _desktop_window.x11.xdnd.source;
                reply.xclient.message_type = _desktop_window.x11.XdndStatus;
                reply.xclient.format = 32;
                reply.xclient.data.l[0] = window->x11.handle;
                reply.xclient.data.l[2] = 0; // Specify an empty rectangle
                reply.xclient.data.l[3] = 0;

                if (_desktop_window.x11.xdnd.format)
                {
                    // Reply that we are ready to copy the dragged data
                    reply.xclient.data.l[1] = 1; // Accept with no rectangle
                    if (_desktop_window.x11.xdnd.version >= 2)
                        reply.xclient.data.l[4] = _desktop_window.x11.XdndActionCopy;
                }

                XSendEvent(_desktop_window.x11.display, _desktop_window.x11.xdnd.source,
                           False, NoEventMask, &reply);
                XFlush(_desktop_window.x11.display);
            }

            return;
        }

        case SelectionNotify:
        {
            if (event->xselection.property == _desktop_window.x11.XdndSelection)
            {
                // The converted data from the drag operation has arrived
                char* data;
                const unsigned long result =
                    _desktop_windowGetWindowPropertyX11(event->xselection.requestor,
                                              event->xselection.property,
                                              event->xselection.target,
                                              (unsigned char**) &data);

                if (result)
                {
                    int count;
                    char** paths = _desktop_windowParseUriList(data, &count);

                    _desktop_windowInputDrop(window, count, (const char**) paths);

                    for (int i = 0;  i < count;  i++)
                        _desktop_window_free(paths[i]);
                    _desktop_window_free(paths);
                }

                if (data)
                    XFree(data);

                if (_desktop_window.x11.xdnd.version >= 2)
                {
                    XEvent reply = { ClientMessage };
                    reply.xclient.window = _desktop_window.x11.xdnd.source;
                    reply.xclient.message_type = _desktop_window.x11.XdndFinished;
                    reply.xclient.format = 32;
                    reply.xclient.data.l[0] = window->x11.handle;
                    reply.xclient.data.l[1] = result;
                    reply.xclient.data.l[2] = _desktop_window.x11.XdndActionCopy;

                    XSendEvent(_desktop_window.x11.display, _desktop_window.x11.xdnd.source,
                               False, NoEventMask, &reply);
                    XFlush(_desktop_window.x11.display);
                }
            }

            return;
        }

        case FocusIn:
        {
            if (event->xfocus.mode == NotifyGrab ||
                event->xfocus.mode == NotifyUngrab)
            {
                // Ignore focus events from popup indicator windows, window menu
                // key chords and window dragging
                return;
            }

            if (window->cursorMode == DESKTOP_WINDOW_CURSOR_DISABLED)
                disableCursor(window);
            else if (window->cursorMode == DESKTOP_WINDOW_CURSOR_CAPTURED)
                captureCursor(window);

            if (window->x11.ic)
                XSetICFocus(window->x11.ic);

            _desktop_windowInputWindowFocus(window, DESKTOP_WINDOW_TRUE);
            return;
        }

        case FocusOut:
        {
            if (event->xfocus.mode == NotifyGrab ||
                event->xfocus.mode == NotifyUngrab)
            {
                // Ignore focus events from popup indicator windows, window menu
                // key chords and window dragging
                return;
            }

            if (window->cursorMode == DESKTOP_WINDOW_CURSOR_DISABLED)
                enableCursor(window);
            else if (window->cursorMode == DESKTOP_WINDOW_CURSOR_CAPTURED)
                releaseCursor();

            if (window->x11.ic)
                XUnsetICFocus(window->x11.ic);

            if (window->monitor && window->autoIconify)
                _desktop_windowIconifyWindowX11(window);

            _desktop_windowInputWindowFocus(window, DESKTOP_WINDOW_FALSE);
            return;
        }

        case Expose:
        {
            _desktop_windowInputWindowDamage(window);
            return;
        }

        case PropertyNotify:
        {
            if (event->xproperty.state != PropertyNewValue)
                return;

            if (event->xproperty.atom == _desktop_window.x11.WM_STATE)
            {
                const int state = getWindowState(window);
                if (state != IconicState && state != NormalState)
                    return;

                const DESKTOP_WINDOWbool iconified = (state == IconicState);
                if (window->x11.iconified != iconified)
                {
                    if (window->monitor)
                    {
                        if (iconified)
                            releaseMonitor(window);
                        else
                            acquireMonitor(window);
                    }

                    window->x11.iconified = iconified;
                    _desktop_windowInputWindowIconify(window, iconified);
                }
            }
            else if (event->xproperty.atom == _desktop_window.x11.NET_WM_STATE)
            {
                const DESKTOP_WINDOWbool maximized = _desktop_windowWindowMaximizedX11(window);
                if (window->x11.maximized != maximized)
                {
                    window->x11.maximized = maximized;
                    _desktop_windowInputWindowMaximize(window, maximized);
                }
            }

            return;
        }

        case DestroyNotify:
            return;
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Retrieve a single window property of the specified type
// Inspired by fghGetWindowProperty from freeglut
//
unsigned long _desktop_windowGetWindowPropertyX11(Window window,
                                        Atom property,
                                        Atom type,
                                        unsigned char** value)
{
    Atom actualType;
    int actualFormat;
    unsigned long itemCount, bytesAfter;

    XGetWindowProperty(_desktop_window.x11.display,
                       window,
                       property,
                       0,
                       LONG_MAX,
                       False,
                       type,
                       &actualType,
                       &actualFormat,
                       &itemCount,
                       &bytesAfter,
                       value);

    return itemCount;
}

DESKTOP_WINDOWbool _desktop_windowIsVisualTransparentX11(Visual* visual)
{
    if (!_desktop_window.x11.xrender.available)
        return DESKTOP_WINDOW_FALSE;

    XRenderPictFormat* pf = XRenderFindVisualFormat(_desktop_window.x11.display, visual);
    return pf && pf->direct.alphaMask;
}

// Push contents of our selection to clipboard manager
//
void _desktop_windowPushSelectionToManagerX11(void)
{
    XConvertSelection(_desktop_window.x11.display,
                      _desktop_window.x11.CLIPBOARD_MANAGER,
                      _desktop_window.x11.SAVE_TARGETS,
                      None,
                      _desktop_window.x11.helperWindowHandle,
                      CurrentTime);

    for (;;)
    {
        XEvent event;

        while (XCheckIfEvent(_desktop_window.x11.display, &event, isSelectionEvent, NULL))
        {
            switch (event.type)
            {
                case SelectionRequest:
                    handleSelectionRequest(&event);
                    break;

                case SelectionNotify:
                {
                    if (event.xselection.target == _desktop_window.x11.SAVE_TARGETS)
                    {
                        // This means one of two things; either the selection
                        // was not owned, which means there is no clipboard
                        // manager, or the transfer to the clipboard manager has
                        // completed
                        // In either case, it means we are done here
                        return;
                    }

                    break;
                }
            }
        }

        waitForX11Event(NULL);
    }
}

void _desktop_windowCreateInputContextX11(_DESKTOP_WINDOWwindow* window)
{
    XIMCallback callback;
    callback.callback = (XIMProc) inputContextDestroyCallback;
    callback.client_data = (XPointer) window;

    window->x11.ic = XCreateIC(_desktop_window.x11.im,
                               XNInputStyle,
                               XIMPreeditNothing | XIMStatusNothing,
                               XNClientWindow,
                               window->x11.handle,
                               XNFocusWindow,
                               window->x11.handle,
                               XNDestroyCallback,
                               &callback,
                               NULL);

    if (window->x11.ic)
    {
        XWindowAttributes attribs;
        XGetWindowAttributes(_desktop_window.x11.display, window->x11.handle, &attribs);

        unsigned long filter = 0;
        if (XGetICValues(window->x11.ic, XNFilterEvents, &filter, NULL) == NULL)
        {
            XSelectInput(_desktop_window.x11.display,
                         window->x11.handle,
                         attribs.your_event_mask | filter);
        }
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowCreateWindowX11(_DESKTOP_WINDOWwindow* window,
                              const _DESKTOP_WINDOWwndconfig* wndconfig,
                              const _DESKTOP_WINDOWctxconfig* ctxconfig,
                              const _DESKTOP_WINDOWfbconfig* fbconfig)
{
    Visual* visual = NULL;
    int depth;

    if (ctxconfig->client != DESKTOP_WINDOW_NO_API)
    {
        if (ctxconfig->source == DESKTOP_WINDOW_NATIVE_CONTEXT_API)
        {
            if (!_desktop_windowInitGLX())
                return DESKTOP_WINDOW_FALSE;
            if (!_desktop_windowChooseVisualGLX(wndconfig, ctxconfig, fbconfig, &visual, &depth))
                return DESKTOP_WINDOW_FALSE;
        }
        else if (ctxconfig->source == DESKTOP_WINDOW_EGL_CONTEXT_API)
        {
            if (!_desktop_windowInitEGL())
                return DESKTOP_WINDOW_FALSE;
            if (!_desktop_windowChooseVisualEGL(wndconfig, ctxconfig, fbconfig, &visual, &depth))
                return DESKTOP_WINDOW_FALSE;
        }
        else if (ctxconfig->source == DESKTOP_WINDOW_OSMESA_CONTEXT_API)
        {
            if (!_desktop_windowInitOSMesa())
                return DESKTOP_WINDOW_FALSE;
        }
    }

    if (!visual)
    {
        visual = DefaultVisual(_desktop_window.x11.display, _desktop_window.x11.screen);
        depth = DefaultDepth(_desktop_window.x11.display, _desktop_window.x11.screen);
    }

    if (!createNativeWindow(window, wndconfig, visual, depth))
        return DESKTOP_WINDOW_FALSE;

    if (ctxconfig->client != DESKTOP_WINDOW_NO_API)
    {
        if (ctxconfig->source == DESKTOP_WINDOW_NATIVE_CONTEXT_API)
        {
            if (!_desktop_windowCreateContextGLX(window, ctxconfig, fbconfig))
                return DESKTOP_WINDOW_FALSE;
        }
        else if (ctxconfig->source == DESKTOP_WINDOW_EGL_CONTEXT_API)
        {
            if (!_desktop_windowCreateContextEGL(window, ctxconfig, fbconfig))
                return DESKTOP_WINDOW_FALSE;
        }
        else if (ctxconfig->source == DESKTOP_WINDOW_OSMESA_CONTEXT_API)
        {
            if (!_desktop_windowCreateContextOSMesa(window, ctxconfig, fbconfig))
                return DESKTOP_WINDOW_FALSE;
        }

        if (!_desktop_windowRefreshContextAttribs(window, ctxconfig))
            return DESKTOP_WINDOW_FALSE;
    }

    if (wndconfig->mousePassthrough)
        _desktop_windowSetWindowMousePassthroughX11(window, DESKTOP_WINDOW_TRUE);

    if (window->monitor)
    {
        _desktop_windowShowWindowX11(window);
        updateWindowMode(window);
        acquireMonitor(window);

        if (wndconfig->centerCursor)
            _desktop_windowCenterCursorInContentArea(window);
    }
    else
    {
        if (wndconfig->visible)
        {
            _desktop_windowShowWindowX11(window);
            if (wndconfig->focused)
                _desktop_windowFocusWindowX11(window);
        }
    }

    XFlush(_desktop_window.x11.display);
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowDestroyWindowX11(_DESKTOP_WINDOWwindow* window)
{
    if (_desktop_window.x11.disabledCursorWindow == window)
        enableCursor(window);

    if (window->monitor)
        releaseMonitor(window);

    if (window->x11.ic)
    {
        XDestroyIC(window->x11.ic);
        window->x11.ic = NULL;
    }

    if (window->context.destroy)
        window->context.destroy(window);

    if (window->x11.handle)
    {
        XDeleteContext(_desktop_window.x11.display, window->x11.handle, _desktop_window.x11.context);
        XUnmapWindow(_desktop_window.x11.display, window->x11.handle);
        XDestroyWindow(_desktop_window.x11.display, window->x11.handle);
        window->x11.handle = (Window) 0;
    }

    if (window->x11.colormap)
    {
        XFreeColormap(_desktop_window.x11.display, window->x11.colormap);
        window->x11.colormap = (Colormap) 0;
    }

    XFlush(_desktop_window.x11.display);
}

void _desktop_windowSetWindowTitleX11(_DESKTOP_WINDOWwindow* window, const char* title)
{
    if (_desktop_window.x11.xlib.utf8)
    {
        Xutf8SetWMProperties(_desktop_window.x11.display,
                             window->x11.handle,
                             title, title,
                             NULL, 0,
                             NULL, NULL, NULL);
    }

    XChangeProperty(_desktop_window.x11.display,  window->x11.handle,
                    _desktop_window.x11.NET_WM_NAME, _desktop_window.x11.UTF8_STRING, 8,
                    PropModeReplace,
                    (unsigned char*) title, strlen(title));

    XChangeProperty(_desktop_window.x11.display,  window->x11.handle,
                    _desktop_window.x11.NET_WM_ICON_NAME, _desktop_window.x11.UTF8_STRING, 8,
                    PropModeReplace,
                    (unsigned char*) title, strlen(title));

    XFlush(_desktop_window.x11.display);
}

void _desktop_windowSetWindowIconX11(_DESKTOP_WINDOWwindow* window, int count, const DESKTOP_WINDOWimage* images)
{
    if (count)
    {
        int longCount = 0;

        for (int i = 0;  i < count;  i++)
            longCount += 2 + images[i].width * images[i].height;

        unsigned long* icon = _desktop_window_calloc(longCount, sizeof(unsigned long));
        unsigned long* target = icon;

        for (int i = 0;  i < count;  i++)
        {
            *target++ = images[i].width;
            *target++ = images[i].height;

            for (int j = 0;  j < images[i].width * images[i].height;  j++)
            {
                *target++ = (((unsigned long) images[i].pixels[j * 4 + 0]) << 16) |
                            (((unsigned long) images[i].pixels[j * 4 + 1]) <<  8) |
                            (((unsigned long) images[i].pixels[j * 4 + 2]) <<  0) |
                            (((unsigned long) images[i].pixels[j * 4 + 3]) << 24);
            }
        }

        // NOTE: XChangeProperty expects 32-bit values like the image data above to be
        //       placed in the 32 least significant bits of individual longs.  This is
        //       true even if long is 64-bit and a WM protocol calls for "packed" data.
        //       This is because of a historical mistake that then became part of the Xlib
        //       ABI.  Xlib will pack these values into a regular array of 32-bit values
        //       before sending it over the wire.
        XChangeProperty(_desktop_window.x11.display, window->x11.handle,
                        _desktop_window.x11.NET_WM_ICON,
                        XA_CARDINAL, 32,
                        PropModeReplace,
                        (unsigned char*) icon,
                        longCount);

        _desktop_window_free(icon);
    }
    else
    {
        XDeleteProperty(_desktop_window.x11.display, window->x11.handle,
                        _desktop_window.x11.NET_WM_ICON);
    }

    XFlush(_desktop_window.x11.display);
}

void _desktop_windowGetWindowPosX11(_DESKTOP_WINDOWwindow* window, int* xpos, int* ypos)
{
    Window dummy;
    int x, y;

    XTranslateCoordinates(_desktop_window.x11.display, window->x11.handle, _desktop_window.x11.root,
                          0, 0, &x, &y, &dummy);

    if (xpos)
        *xpos = x;
    if (ypos)
        *ypos = y;
}

void _desktop_windowSetWindowPosX11(_DESKTOP_WINDOWwindow* window, int xpos, int ypos)
{
    // HACK: Explicitly setting PPosition to any value causes some WMs, notably
    //       Compiz and Metacity, to honor the position of unmapped windows
    if (!_desktop_windowWindowVisibleX11(window))
    {
        long supplied;
        XSizeHints* hints = XAllocSizeHints();

        if (XGetWMNormalHints(_desktop_window.x11.display, window->x11.handle, hints, &supplied))
        {
            hints->flags |= PPosition;
            hints->x = hints->y = 0;

            XSetWMNormalHints(_desktop_window.x11.display, window->x11.handle, hints);
        }

        XFree(hints);
    }

    XMoveWindow(_desktop_window.x11.display, window->x11.handle, xpos, ypos);
    XFlush(_desktop_window.x11.display);
}

void _desktop_windowGetWindowSizeX11(_DESKTOP_WINDOWwindow* window, int* width, int* height)
{
    XWindowAttributes attribs;
    XGetWindowAttributes(_desktop_window.x11.display, window->x11.handle, &attribs);

    if (width)
        *width = attribs.width;
    if (height)
        *height = attribs.height;
}

void _desktop_windowSetWindowSizeX11(_DESKTOP_WINDOWwindow* window, int width, int height)
{
    if (window->monitor)
    {
        if (window->monitor->window == window)
            acquireMonitor(window);
    }
    else
    {
        if (!window->resizable)
            updateNormalHints(window, width, height);

        XResizeWindow(_desktop_window.x11.display, window->x11.handle, width, height);
    }

    XFlush(_desktop_window.x11.display);
}

void _desktop_windowSetWindowSizeLimitsX11(_DESKTOP_WINDOWwindow* window,
                                 int minwidth, int minheight,
                                 int maxwidth, int maxheight)
{
    int width, height;
    _desktop_windowGetWindowSizeX11(window, &width, &height);
    updateNormalHints(window, width, height);
    XFlush(_desktop_window.x11.display);
}

void _desktop_windowSetWindowAspectRatioX11(_DESKTOP_WINDOWwindow* window, int numer, int denom)
{
    int width, height;
    _desktop_windowGetWindowSizeX11(window, &width, &height);
    updateNormalHints(window, width, height);
    XFlush(_desktop_window.x11.display);
}

void _desktop_windowGetFramebufferSizeX11(_DESKTOP_WINDOWwindow* window, int* width, int* height)
{
    _desktop_windowGetWindowSizeX11(window, width, height);
}

void _desktop_windowGetWindowFrameSizeX11(_DESKTOP_WINDOWwindow* window,
                                int* left, int* top,
                                int* right, int* bottom)
{
    long* extents = NULL;

    if (window->monitor || !window->decorated)
        return;

    if (_desktop_window.x11.NET_FRAME_EXTENTS == None)
        return;

    if (!_desktop_windowWindowVisibleX11(window) &&
        _desktop_window.x11.NET_REQUEST_FRAME_EXTENTS)
    {
        XEvent event;
        double timeout = 0.5;

        // Ensure _NET_FRAME_EXTENTS is set, allowing desktop_windowGetWindowFrameSize to
        // function before the window is mapped
        sendEventToWM(window, _desktop_window.x11.NET_REQUEST_FRAME_EXTENTS,
                      0, 0, 0, 0, 0);

        // HACK: Use a timeout because earlier versions of some window managers
        //       (at least Unity, Fluxbox and Xfwm) failed to send the reply
        //       They have been fixed but broken versions are still in the wild
        //       If you are affected by this and your window manager is NOT
        //       listed above, PLEASE report it to their and our issue trackers
        while (!XCheckIfEvent(_desktop_window.x11.display,
                              &event,
                              isFrameExtentsEvent,
                              (XPointer) window))
        {
            if (!waitForX11Event(&timeout))
            {
                _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                                "X11: The window manager has a broken _NET_REQUEST_FRAME_EXTENTS implementation; please report this issue");
                return;
            }
        }
    }

    if (_desktop_windowGetWindowPropertyX11(window->x11.handle,
                                  _desktop_window.x11.NET_FRAME_EXTENTS,
                                  XA_CARDINAL,
                                  (unsigned char**) &extents) == 4)
    {
        if (left)
            *left = extents[0];
        if (top)
            *top = extents[2];
        if (right)
            *right = extents[1];
        if (bottom)
            *bottom = extents[3];
    }

    if (extents)
        XFree(extents);
}

void _desktop_windowGetWindowContentScaleX11(_DESKTOP_WINDOWwindow* window, float* xscale, float* yscale)
{
    if (xscale)
        *xscale = _desktop_window.x11.contentScaleX;
    if (yscale)
        *yscale = _desktop_window.x11.contentScaleY;
}

void _desktop_windowIconifyWindowX11(_DESKTOP_WINDOWwindow* window)
{
    if (window->x11.overrideRedirect)
    {
        // Override-redirect windows cannot be iconified or restored, as those
        // tasks are performed by the window manager
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "X11: Iconification of full screen windows requires a WM that supports EWMH full screen");
        return;
    }

    XIconifyWindow(_desktop_window.x11.display, window->x11.handle, _desktop_window.x11.screen);
    XFlush(_desktop_window.x11.display);
}

void _desktop_windowRestoreWindowX11(_DESKTOP_WINDOWwindow* window)
{
    if (window->x11.overrideRedirect)
    {
        // Override-redirect windows cannot be iconified or restored, as those
        // tasks are performed by the window manager
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "X11: Iconification of full screen windows requires a WM that supports EWMH full screen");
        return;
    }

    if (_desktop_windowWindowIconifiedX11(window))
    {
        XMapWindow(_desktop_window.x11.display, window->x11.handle);
        waitForVisibilityNotify(window);
    }
    else if (_desktop_windowWindowVisibleX11(window))
    {
        if (_desktop_window.x11.NET_WM_STATE &&
            _desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT &&
            _desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ)
        {
            sendEventToWM(window,
                          _desktop_window.x11.NET_WM_STATE,
                          _NET_WM_STATE_REMOVE,
                          _desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT,
                          _desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ,
                          1, 0);
        }
    }

    XFlush(_desktop_window.x11.display);
}

void _desktop_windowMaximizeWindowX11(_DESKTOP_WINDOWwindow* window)
{
    if (!_desktop_window.x11.NET_WM_STATE ||
        !_desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT ||
        !_desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ)
    {
        return;
    }

    if (_desktop_windowWindowVisibleX11(window))
    {
        sendEventToWM(window,
                    _desktop_window.x11.NET_WM_STATE,
                    _NET_WM_STATE_ADD,
                    _desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT,
                    _desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ,
                    1, 0);
    }
    else
    {
        Atom* states = NULL;
        unsigned long count =
            _desktop_windowGetWindowPropertyX11(window->x11.handle,
                                      _desktop_window.x11.NET_WM_STATE,
                                      XA_ATOM,
                                      (unsigned char**) &states);

        // NOTE: We don't check for failure as this property may not exist yet
        //       and that's fine (and we'll create it implicitly with append)

        Atom missing[2] =
        {
            _desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT,
            _desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ
        };
        unsigned long missingCount = 2;

        for (unsigned long i = 0;  i < count;  i++)
        {
            for (unsigned long j = 0;  j < missingCount;  j++)
            {
                if (states[i] == missing[j])
                {
                    missing[j] = missing[missingCount - 1];
                    missingCount--;
                }
            }
        }

        if (states)
            XFree(states);

        if (!missingCount)
            return;

        XChangeProperty(_desktop_window.x11.display, window->x11.handle,
                        _desktop_window.x11.NET_WM_STATE, XA_ATOM, 32,
                        PropModeAppend,
                        (unsigned char*) missing,
                        missingCount);
    }

    XFlush(_desktop_window.x11.display);
}

void _desktop_windowShowWindowX11(_DESKTOP_WINDOWwindow* window)
{
    if (_desktop_windowWindowVisibleX11(window))
        return;

    XMapWindow(_desktop_window.x11.display, window->x11.handle);
    waitForVisibilityNotify(window);
}

void _desktop_windowHideWindowX11(_DESKTOP_WINDOWwindow* window)
{
    XUnmapWindow(_desktop_window.x11.display, window->x11.handle);
    XFlush(_desktop_window.x11.display);
}

void _desktop_windowRequestWindowAttentionX11(_DESKTOP_WINDOWwindow* window)
{
    if (!_desktop_window.x11.NET_WM_STATE || !_desktop_window.x11.NET_WM_STATE_DEMANDS_ATTENTION)
        return;

    sendEventToWM(window,
                  _desktop_window.x11.NET_WM_STATE,
                  _NET_WM_STATE_ADD,
                  _desktop_window.x11.NET_WM_STATE_DEMANDS_ATTENTION,
                  0, 1, 0);
}

void _desktop_windowFocusWindowX11(_DESKTOP_WINDOWwindow* window)
{
    if (_desktop_window.x11.NET_ACTIVE_WINDOW)
        sendEventToWM(window, _desktop_window.x11.NET_ACTIVE_WINDOW, 1, 0, 0, 0, 0);
    else if (_desktop_windowWindowVisibleX11(window))
    {
        XRaiseWindow(_desktop_window.x11.display, window->x11.handle);
        XSetInputFocus(_desktop_window.x11.display, window->x11.handle,
                       RevertToParent, CurrentTime);
    }

    XFlush(_desktop_window.x11.display);
}

void _desktop_windowSetWindowMonitorX11(_DESKTOP_WINDOWwindow* window,
                              _DESKTOP_WINDOWmonitor* monitor,
                              int xpos, int ypos,
                              int width, int height,
                              int refreshRate)
{
    if (window->monitor == monitor)
    {
        if (monitor)
        {
            if (monitor->window == window)
                acquireMonitor(window);
        }
        else
        {
            if (!window->resizable)
                updateNormalHints(window, width, height);

            XMoveResizeWindow(_desktop_window.x11.display, window->x11.handle,
                              xpos, ypos, width, height);
        }

        XFlush(_desktop_window.x11.display);
        return;
    }

    if (window->monitor)
    {
        _desktop_windowSetWindowDecoratedX11(window, window->decorated);
        _desktop_windowSetWindowFloatingX11(window, window->floating);
        releaseMonitor(window);
    }

    _desktop_windowInputWindowMonitor(window, monitor);
    updateNormalHints(window, width, height);

    if (window->monitor)
    {
        if (!_desktop_windowWindowVisibleX11(window))
        {
            XMapRaised(_desktop_window.x11.display, window->x11.handle);
            waitForVisibilityNotify(window);
        }

        updateWindowMode(window);
        acquireMonitor(window);
    }
    else
    {
        updateWindowMode(window);
        XMoveResizeWindow(_desktop_window.x11.display, window->x11.handle,
                          xpos, ypos, width, height);
    }

    XFlush(_desktop_window.x11.display);
}

DESKTOP_WINDOWbool _desktop_windowWindowFocusedX11(_DESKTOP_WINDOWwindow* window)
{
    Window focused;
    int state;

    XGetInputFocus(_desktop_window.x11.display, &focused, &state);
    return window->x11.handle == focused;
}

DESKTOP_WINDOWbool _desktop_windowWindowIconifiedX11(_DESKTOP_WINDOWwindow* window)
{
    return getWindowState(window) == IconicState;
}

DESKTOP_WINDOWbool _desktop_windowWindowVisibleX11(_DESKTOP_WINDOWwindow* window)
{
    XWindowAttributes wa;
    XGetWindowAttributes(_desktop_window.x11.display, window->x11.handle, &wa);
    return wa.map_state == IsViewable;
}

DESKTOP_WINDOWbool _desktop_windowWindowMaximizedX11(_DESKTOP_WINDOWwindow* window)
{
    Atom* states;
    DESKTOP_WINDOWbool maximized = DESKTOP_WINDOW_FALSE;

    if (!_desktop_window.x11.NET_WM_STATE ||
        !_desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT ||
        !_desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ)
    {
        return maximized;
    }

    const unsigned long count =
        _desktop_windowGetWindowPropertyX11(window->x11.handle,
                                  _desktop_window.x11.NET_WM_STATE,
                                  XA_ATOM,
                                  (unsigned char**) &states);

    for (unsigned long i = 0;  i < count;  i++)
    {
        if (states[i] == _desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT ||
            states[i] == _desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ)
        {
            maximized = DESKTOP_WINDOW_TRUE;
            break;
        }
    }

    if (states)
        XFree(states);

    return maximized;
}

DESKTOP_WINDOWbool _desktop_windowWindowHoveredX11(_DESKTOP_WINDOWwindow* window)
{
    Window w = _desktop_window.x11.root;
    while (w)
    {
        Window root;
        int rootX, rootY, childX, childY;
        unsigned int mask;

        _desktop_windowGrabErrorHandlerX11();

        const Bool result = XQueryPointer(_desktop_window.x11.display, w,
                                          &root, &w, &rootX, &rootY,
                                          &childX, &childY, &mask);

        _desktop_windowReleaseErrorHandlerX11();

        if (_desktop_window.x11.errorCode == BadWindow)
            w = _desktop_window.x11.root;
        else if (!result)
            return DESKTOP_WINDOW_FALSE;
        else if (w == window->x11.handle)
            return DESKTOP_WINDOW_TRUE;
    }

    return DESKTOP_WINDOW_FALSE;
}

DESKTOP_WINDOWbool _desktop_windowFramebufferTransparentX11(_DESKTOP_WINDOWwindow* window)
{
    if (!window->x11.transparent)
        return DESKTOP_WINDOW_FALSE;

    return XGetSelectionOwner(_desktop_window.x11.display, _desktop_window.x11.NET_WM_CM_Sx) != None;
}

void _desktop_windowSetWindowResizableX11(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    int width, height;
    _desktop_windowGetWindowSizeX11(window, &width, &height);
    updateNormalHints(window, width, height);
}

void _desktop_windowSetWindowDecoratedX11(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    struct
    {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    } hints = {0};

    hints.flags = MWM_HINTS_DECORATIONS;
    hints.decorations = enabled ? MWM_DECOR_ALL : 0;

    XChangeProperty(_desktop_window.x11.display, window->x11.handle,
                    _desktop_window.x11.MOTIF_WM_HINTS,
                    _desktop_window.x11.MOTIF_WM_HINTS, 32,
                    PropModeReplace,
                    (unsigned char*) &hints,
                    sizeof(hints) / sizeof(long));
}

void _desktop_windowSetWindowFloatingX11(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    if (!_desktop_window.x11.NET_WM_STATE || !_desktop_window.x11.NET_WM_STATE_ABOVE)
        return;

    if (_desktop_windowWindowVisibleX11(window))
    {
        const long action = enabled ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
        sendEventToWM(window,
                      _desktop_window.x11.NET_WM_STATE,
                      action,
                      _desktop_window.x11.NET_WM_STATE_ABOVE,
                      0, 1, 0);
    }
    else
    {
        Atom* states = NULL;
        const unsigned long count =
            _desktop_windowGetWindowPropertyX11(window->x11.handle,
                                      _desktop_window.x11.NET_WM_STATE,
                                      XA_ATOM,
                                      (unsigned char**) &states);

        // NOTE: We don't check for failure as this property may not exist yet
        //       and that's fine (and we'll create it implicitly with append)

        if (enabled)
        {
            unsigned long i;

            for (i = 0;  i < count;  i++)
            {
                if (states[i] == _desktop_window.x11.NET_WM_STATE_ABOVE)
                    break;
            }

            if (i == count)
            {
                XChangeProperty(_desktop_window.x11.display, window->x11.handle,
                                _desktop_window.x11.NET_WM_STATE, XA_ATOM, 32,
                                PropModeAppend,
                                (unsigned char*) &_desktop_window.x11.NET_WM_STATE_ABOVE,
                                1);
            }
        }
        else if (states)
        {
            for (unsigned long i = 0;  i < count;  i++)
            {
                if (states[i] == _desktop_window.x11.NET_WM_STATE_ABOVE)
                {
                    states[i] = states[count - 1];
                    XChangeProperty(_desktop_window.x11.display, window->x11.handle,
                                    _desktop_window.x11.NET_WM_STATE, XA_ATOM, 32,
                                    PropModeReplace, (unsigned char*) states, count - 1);
                    break;
                }
            }
        }

        if (states)
            XFree(states);
    }

    XFlush(_desktop_window.x11.display);
}

void _desktop_windowSetWindowMousePassthroughX11(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    if (!_desktop_window.x11.xshape.available)
        return;

    if (enabled)
    {
        Region region = XCreateRegion();
        XShapeCombineRegion(_desktop_window.x11.display, window->x11.handle,
                            ShapeInput, 0, 0, region, ShapeSet);
        XDestroyRegion(region);
    }
    else
    {
        XShapeCombineMask(_desktop_window.x11.display, window->x11.handle,
                          ShapeInput, 0, 0, None, ShapeSet);
    }
}

float _desktop_windowGetWindowOpacityX11(_DESKTOP_WINDOWwindow* window)
{
    float opacity = 1.f;

    if (XGetSelectionOwner(_desktop_window.x11.display, _desktop_window.x11.NET_WM_CM_Sx))
    {
        CARD32* value = NULL;

        if (_desktop_windowGetWindowPropertyX11(window->x11.handle,
                                      _desktop_window.x11.NET_WM_WINDOW_OPACITY,
                                      XA_CARDINAL,
                                      (unsigned char**) &value))
        {
            opacity = (float) (*value / (double) 0xffffffffu);
        }

        if (value)
            XFree(value);
    }

    return opacity;
}

void _desktop_windowSetWindowOpacityX11(_DESKTOP_WINDOWwindow* window, float opacity)
{
    const CARD32 value = (CARD32) (0xffffffffu * (double) opacity);
    XChangeProperty(_desktop_window.x11.display, window->x11.handle,
                    _desktop_window.x11.NET_WM_WINDOW_OPACITY, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char*) &value, 1);
}

void _desktop_windowSetRawMouseMotionX11(_DESKTOP_WINDOWwindow *window, DESKTOP_WINDOWbool enabled)
{
    if (!_desktop_window.x11.xi.available)
        return;

    if (_desktop_window.x11.disabledCursorWindow != window)
        return;

    if (enabled)
        enableRawMouseMotion(window);
    else
        disableRawMouseMotion(window);
}

DESKTOP_WINDOWbool _desktop_windowRawMouseMotionSupportedX11(void)
{
    return _desktop_window.x11.xi.available;
}

void _desktop_windowPollEventsX11(void)
{
    drainEmptyEvents();

#if defined(DESKTOP_WINDOW_BUILD_LINUX_JOYSTICK)
    if (_desktop_window.joysticksInitialized)
        _desktop_windowDetectJoystickConnectionLinux();
#endif
    XPending(_desktop_window.x11.display);

    while (QLength(_desktop_window.x11.display))
    {
        XEvent event;
        XNextEvent(_desktop_window.x11.display, &event);
        processEvent(&event);
    }

    _DESKTOP_WINDOWwindow* window = _desktop_window.x11.disabledCursorWindow;
    if (window)
    {
        int width, height;
        _desktop_windowGetWindowSizeX11(window, &width, &height);

        // NOTE: Re-center the cursor only if it has moved since the last call,
        //       to avoid breaking desktop_windowWaitEvents with MotionNotify
        if (window->x11.lastCursorPosX != width / 2 ||
            window->x11.lastCursorPosY != height / 2)
        {
            _desktop_windowSetCursorPosX11(window, width / 2, height / 2);
        }
    }

    XFlush(_desktop_window.x11.display);
}

void _desktop_windowWaitEventsX11(void)
{
    waitForAnyEvent(NULL);
    _desktop_windowPollEventsX11();
}

void _desktop_windowWaitEventsTimeoutX11(double timeout)
{
    waitForAnyEvent(&timeout);
    _desktop_windowPollEventsX11();
}

void _desktop_windowPostEmptyEventX11(void)
{
    writeEmptyEvent();
}

void _desktop_windowGetCursorPosX11(_DESKTOP_WINDOWwindow* window, double* xpos, double* ypos)
{
    Window root, child;
    int rootX, rootY, childX, childY;
    unsigned int mask;

    XQueryPointer(_desktop_window.x11.display, window->x11.handle,
                  &root, &child,
                  &rootX, &rootY, &childX, &childY,
                  &mask);

    if (xpos)
        *xpos = childX;
    if (ypos)
        *ypos = childY;
}

void _desktop_windowSetCursorPosX11(_DESKTOP_WINDOWwindow* window, double x, double y)
{
    // Store the new position so it can be recognized later
    window->x11.warpCursorPosX = (int) x;
    window->x11.warpCursorPosY = (int) y;

    XWarpPointer(_desktop_window.x11.display, None, window->x11.handle,
                 0,0,0,0, (int) x, (int) y);
    XFlush(_desktop_window.x11.display);
}

void _desktop_windowSetCursorModeX11(_DESKTOP_WINDOWwindow* window, int mode)
{
    if (_desktop_windowWindowFocusedX11(window))
    {
        if (mode == DESKTOP_WINDOW_CURSOR_DISABLED)
        {
            _desktop_windowGetCursorPosX11(window,
                                 &_desktop_window.x11.restoreCursorPosX,
                                 &_desktop_window.x11.restoreCursorPosY);
            _desktop_windowCenterCursorInContentArea(window);
            if (window->rawMouseMotion)
                enableRawMouseMotion(window);
        }
        else if (_desktop_window.x11.disabledCursorWindow == window)
        {
            if (window->rawMouseMotion)
                disableRawMouseMotion(window);
        }

        if (mode == DESKTOP_WINDOW_CURSOR_DISABLED || mode == DESKTOP_WINDOW_CURSOR_CAPTURED)
            captureCursor(window);
        else
            releaseCursor();

        if (mode == DESKTOP_WINDOW_CURSOR_DISABLED)
            _desktop_window.x11.disabledCursorWindow = window;
        else if (_desktop_window.x11.disabledCursorWindow == window)
        {
            _desktop_window.x11.disabledCursorWindow = NULL;
            _desktop_windowSetCursorPosX11(window,
                                 _desktop_window.x11.restoreCursorPosX,
                                 _desktop_window.x11.restoreCursorPosY);
        }
    }

    updateCursorImage(window);
    XFlush(_desktop_window.x11.display);
}

const char* _desktop_windowGetScancodeNameX11(int scancode)
{
    if (!_desktop_window.x11.xkb.available)
        return NULL;

    if (scancode < 0 || scancode > 0xff)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Invalid scancode %i", scancode);
        return NULL;
    }

    const int key = _desktop_window.x11.keycodes[scancode];
    if (key == DESKTOP_WINDOW_KEY_UNKNOWN)
        return NULL;

    const KeySym keysym = XkbKeycodeToKeysym(_desktop_window.x11.display,
                                             scancode, _desktop_window.x11.xkb.group, 0);
    if (keysym == NoSymbol)
        return NULL;

    const uint32_t codepoint = _desktop_windowKeySym2Unicode(keysym);
    if (codepoint == DESKTOP_WINDOW_INVALID_CODEPOINT)
        return NULL;

    const size_t count = _desktop_windowEncodeUTF8(_desktop_window.x11.keynames[key], codepoint);
    if (count == 0)
        return NULL;

    _desktop_window.x11.keynames[key][count] = '\0';
    return _desktop_window.x11.keynames[key];
}

int _desktop_windowGetKeyScancodeX11(int key)
{
    return _desktop_window.x11.scancodes[key];
}

DESKTOP_WINDOWbool _desktop_windowCreateCursorX11(_DESKTOP_WINDOWcursor* cursor,
                              const DESKTOP_WINDOWimage* image,
                              int xhot, int yhot)
{
    cursor->x11.handle = _desktop_windowCreateNativeCursorX11(image, xhot, yhot);
    if (!cursor->x11.handle)
        return DESKTOP_WINDOW_FALSE;

    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWbool _desktop_windowCreateStandardCursorX11(_DESKTOP_WINDOWcursor* cursor, int shape)
{
    if (_desktop_window.x11.xcursor.handle)
    {
        char* theme = XcursorGetTheme(_desktop_window.x11.display);
        if (theme)
        {
            const int size = XcursorGetDefaultSize(_desktop_window.x11.display);
            const char* name = NULL;

            switch (shape)
            {
                case DESKTOP_WINDOW_ARROW_CURSOR:
                    name = "default";
                    break;
                case DESKTOP_WINDOW_IBEAM_CURSOR:
                    name = "text";
                    break;
                case DESKTOP_WINDOW_CROSSHAIR_CURSOR:
                    name = "crosshair";
                    break;
                case DESKTOP_WINDOW_POINTING_HAND_CURSOR:
                    name = "pointer";
                    break;
                case DESKTOP_WINDOW_RESIZE_EW_CURSOR:
                    name = "ew-resize";
                    break;
                case DESKTOP_WINDOW_RESIZE_NS_CURSOR:
                    name = "ns-resize";
                    break;
                case DESKTOP_WINDOW_RESIZE_NWSE_CURSOR:
                    name = "nwse-resize";
                    break;
                case DESKTOP_WINDOW_RESIZE_NESW_CURSOR:
                    name = "nesw-resize";
                    break;
                case DESKTOP_WINDOW_RESIZE_ALL_CURSOR:
                    name = "all-scroll";
                    break;
                case DESKTOP_WINDOW_NOT_ALLOWED_CURSOR:
                    name = "not-allowed";
                    break;
            }

            XcursorImage* image = XcursorLibraryLoadImage(name, theme, size);
            if (image)
            {
                cursor->x11.handle = XcursorImageLoadCursor(_desktop_window.x11.display, image);
                XcursorImageDestroy(image);
            }
        }
    }

    if (!cursor->x11.handle)
    {
        unsigned int native = 0;

        switch (shape)
        {
            case DESKTOP_WINDOW_ARROW_CURSOR:
                native = XC_left_ptr;
                break;
            case DESKTOP_WINDOW_IBEAM_CURSOR:
                native = XC_xterm;
                break;
            case DESKTOP_WINDOW_CROSSHAIR_CURSOR:
                native = XC_crosshair;
                break;
            case DESKTOP_WINDOW_POINTING_HAND_CURSOR:
                native = XC_hand2;
                break;
            case DESKTOP_WINDOW_RESIZE_EW_CURSOR:
                native = XC_sb_h_double_arrow;
                break;
            case DESKTOP_WINDOW_RESIZE_NS_CURSOR:
                native = XC_sb_v_double_arrow;
                break;
            case DESKTOP_WINDOW_RESIZE_ALL_CURSOR:
                native = XC_fleur;
                break;
            default:
                _desktop_windowInputError(DESKTOP_WINDOW_CURSOR_UNAVAILABLE,
                                "X11: Standard cursor shape unavailable");
                return DESKTOP_WINDOW_FALSE;
        }

        cursor->x11.handle = XCreateFontCursor(_desktop_window.x11.display, native);
        if (!cursor->x11.handle)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "X11: Failed to create standard cursor");
            return DESKTOP_WINDOW_FALSE;
        }
    }

    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowDestroyCursorX11(_DESKTOP_WINDOWcursor* cursor)
{
    if (cursor->x11.handle)
        XFreeCursor(_desktop_window.x11.display, cursor->x11.handle);
}

void _desktop_windowSetCursorX11(_DESKTOP_WINDOWwindow* window, _DESKTOP_WINDOWcursor* cursor)
{
    if (window->cursorMode == DESKTOP_WINDOW_CURSOR_NORMAL ||
        window->cursorMode == DESKTOP_WINDOW_CURSOR_CAPTURED)
    {
        updateCursorImage(window);
        XFlush(_desktop_window.x11.display);
    }
}

void _desktop_windowSetClipboardStringX11(const char* string)
{
    char* copy = _desktop_window_strdup(string);
    _desktop_window_free(_desktop_window.x11.clipboardString);
    _desktop_window.x11.clipboardString = copy;

    XSetSelectionOwner(_desktop_window.x11.display,
                       _desktop_window.x11.CLIPBOARD,
                       _desktop_window.x11.helperWindowHandle,
                       CurrentTime);

    if (XGetSelectionOwner(_desktop_window.x11.display, _desktop_window.x11.CLIPBOARD) !=
        _desktop_window.x11.helperWindowHandle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "X11: Failed to become owner of clipboard selection");
    }
}

const char* _desktop_windowGetClipboardStringX11(void)
{
    return getSelectionString(_desktop_window.x11.CLIPBOARD);
}

EGLenum _desktop_windowGetEGLPlatformX11(EGLint** attribs)
{
    if (_desktop_window.egl.ANGLE_platform_angle)
    {
        int type = 0;

        if (_desktop_window.egl.ANGLE_platform_angle_desktop_graphics)
        {
            if (_desktop_window.hints.init.angleType == DESKTOP_WINDOW_ANGLE_PLATFORM_TYPE_DESKTOP_GRAPHICS)
                type = EGL_PLATFORM_ANGLE_TYPE_DESKTOP_GRAPHICS_ANGLE;
        }

        if (_desktop_window.egl.ANGLE_platform_angle_vulkan)
        {
            if (_desktop_window.hints.init.angleType == DESKTOP_WINDOW_ANGLE_PLATFORM_TYPE_VULKAN)
                type = EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
        }

        if (type)
        {
            *attribs = _desktop_window_calloc(5, sizeof(EGLint));
            (*attribs)[0] = EGL_PLATFORM_ANGLE_TYPE_ANGLE;
            (*attribs)[1] = type;
            (*attribs)[2] = EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE;
            (*attribs)[3] = EGL_PLATFORM_X11_EXT;
            (*attribs)[4] = EGL_NONE;
            return EGL_PLATFORM_ANGLE_ANGLE;
        }
    }

    if (_desktop_window.egl.EXT_platform_base && _desktop_window.egl.EXT_platform_x11)
        return EGL_PLATFORM_X11_EXT;

    return 0;
}

EGLNativeDisplayType _desktop_windowGetEGLNativeDisplayX11(void)
{
    return _desktop_window.x11.display;
}

EGLNativeWindowType _desktop_windowGetEGLNativeWindowX11(_DESKTOP_WINDOWwindow* window)
{
    if (_desktop_window.egl.platform)
        return &window->x11.handle;
    else
        return (EGLNativeWindowType) window->x11.handle;
}

void _desktop_windowGetRequiredInstanceExtensionsX11(char** extensions)
{
    if (!_desktop_window.vk.KHR_surface)
        return;

    if (!_desktop_window.vk.KHR_xcb_surface || !_desktop_window.x11.x11xcb.handle)
    {
        if (!_desktop_window.vk.KHR_xlib_surface)
            return;
    }

    extensions[0] = "VK_KHR_surface";

    // NOTE: VK_KHR_xcb_surface is preferred due to some early ICDs exposing but
    //       not correctly implementing VK_KHR_xlib_surface
    if (_desktop_window.vk.KHR_xcb_surface && _desktop_window.x11.x11xcb.handle)
        extensions[1] = "VK_KHR_xcb_surface";
    else
        extensions[1] = "VK_KHR_xlib_surface";
}

DESKTOP_WINDOWbool _desktop_windowGetPhysicalDevicePresentationSupportX11(VkInstance instance,
                                                      VkPhysicalDevice device,
                                                      uint32_t queuefamily)
{
    VisualID visualID = XVisualIDFromVisual(DefaultVisual(_desktop_window.x11.display,
                                                          _desktop_window.x11.screen));

    if (_desktop_window.vk.KHR_xcb_surface && _desktop_window.x11.x11xcb.handle)
    {
        PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR
            vkGetPhysicalDeviceXcbPresentationSupportKHR =
            (PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR)
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceXcbPresentationSupportKHR");
        if (!vkGetPhysicalDeviceXcbPresentationSupportKHR)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "X11: Vulkan instance missing VK_KHR_xcb_surface extension");
            return DESKTOP_WINDOW_FALSE;
        }

        xcb_connection_t* connection = XGetXCBConnection(_desktop_window.x11.display);
        if (!connection)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "X11: Failed to retrieve XCB connection");
            return DESKTOP_WINDOW_FALSE;
        }

        return vkGetPhysicalDeviceXcbPresentationSupportKHR(device,
                                                            queuefamily,
                                                            connection,
                                                            visualID);
    }
    else
    {
        PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR
            vkGetPhysicalDeviceXlibPresentationSupportKHR =
            (PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR)
            vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceXlibPresentationSupportKHR");
        if (!vkGetPhysicalDeviceXlibPresentationSupportKHR)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "X11: Vulkan instance missing VK_KHR_xlib_surface extension");
            return DESKTOP_WINDOW_FALSE;
        }

        return vkGetPhysicalDeviceXlibPresentationSupportKHR(device,
                                                             queuefamily,
                                                             _desktop_window.x11.display,
                                                             visualID);
    }
}

VkResult _desktop_windowCreateWindowSurfaceX11(VkInstance instance,
                                     _DESKTOP_WINDOWwindow* window,
                                     const VkAllocationCallbacks* allocator,
                                     VkSurfaceKHR* surface)
{
    if (_desktop_window.vk.KHR_xcb_surface && _desktop_window.x11.x11xcb.handle)
    {
        VkResult err;
        VkXcbSurfaceCreateInfoKHR sci;
        PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;

        xcb_connection_t* connection = XGetXCBConnection(_desktop_window.x11.display);
        if (!connection)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "X11: Failed to retrieve XCB connection");
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        vkCreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)
            vkGetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR");
        if (!vkCreateXcbSurfaceKHR)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "X11: Vulkan instance missing VK_KHR_xcb_surface extension");
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        sci.connection = connection;
        sci.window = window->x11.handle;

        err = vkCreateXcbSurfaceKHR(instance, &sci, allocator, surface);
        if (err)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "X11: Failed to create Vulkan XCB surface: %s",
                            _desktop_windowGetVulkanResultString(err));
        }

        return err;
    }
    else
    {
        VkResult err;
        VkXlibSurfaceCreateInfoKHR sci;
        PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;

        vkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)
            vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
        if (!vkCreateXlibSurfaceKHR)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "X11: Vulkan instance missing VK_KHR_xlib_surface extension");
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        sci.dpy = _desktop_window.x11.display;
        sci.window = window->x11.handle;

        err = vkCreateXlibSurfaceKHR(instance, &sci, allocator, surface);
        if (err)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "X11: Failed to create Vulkan X11 surface: %s",
                            _desktop_windowGetVulkanResultString(err));
        }

        return err;
    }
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW native API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI Display* desktop_windowGetX11Display(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_X11)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "X11: Platform not initialized");
        return NULL;
    }

    return _desktop_window.x11.display;
}

DESKTOP_WINDOWAPI Window desktop_windowGetX11Window(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(None);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_X11)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "X11: Platform not initialized");
        return None;
    }

    return window->x11.handle;
}

DESKTOP_WINDOWAPI void desktop_windowSetX11SelectionString(const char* string)
{
    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_X11)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "X11: Platform not initialized");
        return;
    }

    _desktop_window_free(_desktop_window.x11.primarySelectionString);
    _desktop_window.x11.primarySelectionString = _desktop_window_strdup(string);

    XSetSelectionOwner(_desktop_window.x11.display,
                       _desktop_window.x11.PRIMARY,
                       _desktop_window.x11.helperWindowHandle,
                       CurrentTime);

    if (XGetSelectionOwner(_desktop_window.x11.display, _desktop_window.x11.PRIMARY) !=
        _desktop_window.x11.helperWindowHandle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "X11: Failed to become owner of primary selection");
    }
}

DESKTOP_WINDOWAPI const char* desktop_windowGetX11SelectionString(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_X11)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE, "X11: Platform not initialized");
        return NULL;
    }

    return getSelectionString(_desktop_window.x11.PRIMARY);
}

#endif // _DESKTOP_WINDOW_X11

