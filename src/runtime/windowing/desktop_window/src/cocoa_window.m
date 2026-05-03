#include "internal.h"

#if defined(_DESKTOP_WINDOW_COCOA)

#include <float.h>
#include <string.h>

// HACK: This enum value is missing from framework headers on OS X 10.11 despite
//       having been (according to documentation) added in Mac OS X 10.7
#define NSWindowCollectionBehaviorFullScreenNone (1 << 9)

// Returns whether the cursor is in the content area of the specified window
//
static DESKTOP_WINDOWbool cursorInContentArea(_DESKTOP_WINDOWwindow* window)
{
    const NSPoint pos = [window->ns.object mouseLocationOutsideOfEventStream];
    return [window->ns.view mouse:pos inRect:[window->ns.view frame]];
}

// Hides the cursor if not already hidden
//
static void hideCursor(_DESKTOP_WINDOWwindow* window)
{
    if (!_desktop_window.ns.cursorHidden)
    {
        [NSCursor hide];
        _desktop_window.ns.cursorHidden = DESKTOP_WINDOW_TRUE;
    }
}

// Shows the cursor if not already shown
//
static void showCursor(_DESKTOP_WINDOWwindow* window)
{
    if (_desktop_window.ns.cursorHidden)
    {
        [NSCursor unhide];
        _desktop_window.ns.cursorHidden = DESKTOP_WINDOW_FALSE;
    }
}

// Updates the cursor image according to its cursor mode
//
static void updateCursorImage(_DESKTOP_WINDOWwindow* window)
{
    if (window->cursorMode == DESKTOP_WINDOW_CURSOR_NORMAL)
    {
        showCursor(window);

        if (window->cursor)
            [(NSCursor*) window->cursor->ns.object set];
        else
            [[NSCursor arrowCursor] set];
    }
    else
        hideCursor(window);
}

// Apply chosen cursor mode to a focused window
//
static void updateCursorMode(_DESKTOP_WINDOWwindow* window)
{
    if (window->cursorMode == DESKTOP_WINDOW_CURSOR_DISABLED)
    {
        _desktop_window.ns.disabledCursorWindow = window;
        _desktop_windowGetCursorPosCocoa(window,
                               &_desktop_window.ns.restoreCursorPosX,
                               &_desktop_window.ns.restoreCursorPosY);
        _desktop_windowCenterCursorInContentArea(window);
        CGAssociateMouseAndMouseCursorPosition(false);
    }
    else if (_desktop_window.ns.disabledCursorWindow == window)
    {
        _desktop_window.ns.disabledCursorWindow = NULL;
        _desktop_windowSetCursorPosCocoa(window,
                               _desktop_window.ns.restoreCursorPosX,
                               _desktop_window.ns.restoreCursorPosY);
        // NOTE: The matching CGAssociateMouseAndMouseCursorPosition call is
        //       made in _desktop_windowSetCursorPosCocoa as part of a workaround
    }

    if (cursorInContentArea(window))
        updateCursorImage(window);
}

// Make the specified window and its video mode active on its monitor
//
static void acquireMonitor(_DESKTOP_WINDOWwindow* window)
{
    _desktop_windowSetVideoModeCocoa(window->monitor, &window->videoMode);
    const CGRect bounds = CGDisplayBounds(window->monitor->ns.displayID);
    const NSRect frame = NSMakeRect(bounds.origin.x,
                                    _desktop_windowTransformYCocoa(bounds.origin.y + bounds.size.height - 1),
                                    bounds.size.width,
                                    bounds.size.height);

    [window->ns.object setFrame:frame display:YES];

    _desktop_windowInputMonitorWindow(window->monitor, window);
}

// Remove the window and restore the original video mode
//
static void releaseMonitor(_DESKTOP_WINDOWwindow* window)
{
    if (window->monitor->window != window)
        return;

    _desktop_windowInputMonitorWindow(window->monitor, NULL);
    _desktop_windowRestoreVideoModeCocoa(window->monitor);
}

// Translates macOS key modifiers into DESKTOP_WINDOW ones
//
static int translateFlags(NSUInteger flags)
{
    int mods = 0;

    if (flags & NSEventModifierFlagShift)
        mods |= DESKTOP_WINDOW_MOD_SHIFT;
    if (flags & NSEventModifierFlagControl)
        mods |= DESKTOP_WINDOW_MOD_CONTROL;
    if (flags & NSEventModifierFlagOption)
        mods |= DESKTOP_WINDOW_MOD_ALT;
    if (flags & NSEventModifierFlagCommand)
        mods |= DESKTOP_WINDOW_MOD_SUPER;
    if (flags & NSEventModifierFlagCapsLock)
        mods |= DESKTOP_WINDOW_MOD_CAPS_LOCK;

    return mods;
}

// Translates a macOS keycode to a DESKTOP_WINDOW keycode
//
static int translateKey(unsigned int key)
{
    if (key >= sizeof(_desktop_window.ns.keycodes) / sizeof(_desktop_window.ns.keycodes[0]))
        return DESKTOP_WINDOW_KEY_UNKNOWN;

    return _desktop_window.ns.keycodes[key];
}

// Translate a DESKTOP_WINDOW keycode to a Cocoa modifier flag
//
static NSUInteger translateKeyToModifierFlag(int key)
{
    switch (key)
    {
        case DESKTOP_WINDOW_KEY_LEFT_SHIFT:
        case DESKTOP_WINDOW_KEY_RIGHT_SHIFT:
            return NSEventModifierFlagShift;
        case DESKTOP_WINDOW_KEY_LEFT_CONTROL:
        case DESKTOP_WINDOW_KEY_RIGHT_CONTROL:
            return NSEventModifierFlagControl;
        case DESKTOP_WINDOW_KEY_LEFT_ALT:
        case DESKTOP_WINDOW_KEY_RIGHT_ALT:
            return NSEventModifierFlagOption;
        case DESKTOP_WINDOW_KEY_LEFT_SUPER:
        case DESKTOP_WINDOW_KEY_RIGHT_SUPER:
            return NSEventModifierFlagCommand;
        case DESKTOP_WINDOW_KEY_CAPS_LOCK:
            return NSEventModifierFlagCapsLock;
    }

    return 0;
}

// Defines a constant for empty ranges in NSTextInputClient
//
static const NSRange kEmptyRange = { NSNotFound, 0 };


//------------------------------------------------------------------------
// Delegate for window related notifications
//------------------------------------------------------------------------

@interface DESKTOP_WINDOWWindowDelegate : NSObject
{
    _DESKTOP_WINDOWwindow* window;
}

- (instancetype)initWithGlfwWindow:(_DESKTOP_WINDOWwindow *)initWindow;

@end

@implementation DESKTOP_WINDOWWindowDelegate

- (instancetype)initWithGlfwWindow:(_DESKTOP_WINDOWwindow *)initWindow
{
    self = [super init];
    if (self != nil)
        window = initWindow;

    return self;
}

- (BOOL)windowShouldClose:(id)sender
{
    _desktop_windowInputWindowCloseRequest(window);
    return NO;
}

- (void)windowDidResize:(NSNotification *)notification
{
    if (window->context.source == DESKTOP_WINDOW_NATIVE_CONTEXT_API)
        [window->context.nsgl.object update];

    if (_desktop_window.ns.disabledCursorWindow == window)
        _desktop_windowCenterCursorInContentArea(window);

    const int maximized = [window->ns.object isZoomed];
    if (window->ns.maximized != maximized)
    {
        window->ns.maximized = maximized;
        _desktop_windowInputWindowMaximize(window, maximized);
    }

    const NSRect contentRect = [window->ns.view frame];
    const NSRect fbRect = [window->ns.view convertRectToBacking:contentRect];

    if (fbRect.size.width != window->ns.fbWidth ||
        fbRect.size.height != window->ns.fbHeight)
    {
        window->ns.fbWidth  = fbRect.size.width;
        window->ns.fbHeight = fbRect.size.height;
        _desktop_windowInputFramebufferSize(window, fbRect.size.width, fbRect.size.height);
    }

    if (contentRect.size.width != window->ns.width ||
        contentRect.size.height != window->ns.height)
    {
        window->ns.width  = contentRect.size.width;
        window->ns.height = contentRect.size.height;
        _desktop_windowInputWindowSize(window, contentRect.size.width, contentRect.size.height);
    }
}

- (void)windowDidMove:(NSNotification *)notification
{
    if (window->context.source == DESKTOP_WINDOW_NATIVE_CONTEXT_API)
        [window->context.nsgl.object update];

    if (_desktop_window.ns.disabledCursorWindow == window)
        _desktop_windowCenterCursorInContentArea(window);

    int x, y;
    _desktop_windowGetWindowPosCocoa(window, &x, &y);
    _desktop_windowInputWindowPos(window, x, y);
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    if (window->monitor)
        releaseMonitor(window);

    _desktop_windowInputWindowIconify(window, DESKTOP_WINDOW_TRUE);
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    if (window->monitor)
        acquireMonitor(window);

    _desktop_windowInputWindowIconify(window, DESKTOP_WINDOW_FALSE);
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    if (_desktop_window.ns.disabledCursorWindow == window)
        _desktop_windowCenterCursorInContentArea(window);

    _desktop_windowInputWindowFocus(window, DESKTOP_WINDOW_TRUE);
    updateCursorMode(window);
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    if (window->monitor && window->autoIconify)
        _desktop_windowIconifyWindowCocoa(window);

    _desktop_windowInputWindowFocus(window, DESKTOP_WINDOW_FALSE);
}

- (void)windowDidChangeOcclusionState:(NSNotification* )notification
{
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1090
    if ([window->ns.object respondsToSelector:@selector(occlusionState)])
    {
        if ([window->ns.object occlusionState] & NSWindowOcclusionStateVisible)
            window->ns.occluded = DESKTOP_WINDOW_FALSE;
        else
            window->ns.occluded = DESKTOP_WINDOW_TRUE;
    }
#endif
}

@end


//------------------------------------------------------------------------
// Content view class for the DESKTOP_WINDOW window
//------------------------------------------------------------------------

@interface DESKTOP_WINDOWContentView : NSView <NSTextInputClient>
{
    _DESKTOP_WINDOWwindow* window;
    NSTrackingArea* trackingArea;
    NSMutableAttributedString* markedText;
}

- (instancetype)initWithGlfwWindow:(_DESKTOP_WINDOWwindow *)initWindow;

@end

@implementation DESKTOP_WINDOWContentView

- (instancetype)initWithGlfwWindow:(_DESKTOP_WINDOWwindow *)initWindow
{
    self = [super init];
    if (self != nil)
    {
        window = initWindow;
        trackingArea = nil;
        markedText = [[NSMutableAttributedString alloc] init];

        [self updateTrackingAreas];
        [self registerForDraggedTypes:@[NSPasteboardTypeURL]];
    }

    return self;
}

- (void)dealloc
{
    [trackingArea release];
    [markedText release];
    [super dealloc];
}

- (BOOL)isOpaque
{
    return [window->ns.object isOpaque];
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (void)updateLayer
{
    if (window->context.source == DESKTOP_WINDOW_NATIVE_CONTEXT_API)
        [window->context.nsgl.object update];

    _desktop_windowInputWindowDamage(window);
}

- (void)cursorUpdate:(NSEvent *)event
{
    updateCursorImage(window);
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (void)mouseDown:(NSEvent *)event
{
    _desktop_windowInputMouseClick(window,
                         DESKTOP_WINDOW_MOUSE_BUTTON_LEFT,
                         DESKTOP_WINDOW_PRESS,
                         translateFlags([event modifierFlags]));
}

- (void)mouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)mouseUp:(NSEvent *)event
{
    _desktop_windowInputMouseClick(window,
                         DESKTOP_WINDOW_MOUSE_BUTTON_LEFT,
                         DESKTOP_WINDOW_RELEASE,
                         translateFlags([event modifierFlags]));
}

- (void)mouseMoved:(NSEvent *)event
{
    if (window->cursorMode == DESKTOP_WINDOW_CURSOR_DISABLED)
    {
        const double dx = [event deltaX] - window->ns.cursorWarpDeltaX;
        const double dy = [event deltaY] - window->ns.cursorWarpDeltaY;

        _desktop_windowInputCursorPos(window,
                            window->virtualCursorPosX + dx,
                            window->virtualCursorPosY + dy);
    }
    else
    {
        const NSRect contentRect = [window->ns.view frame];
        // NOTE: The returned location uses base 0,1 not 0,0
        const NSPoint pos = [event locationInWindow];

        _desktop_windowInputCursorPos(window, pos.x, contentRect.size.height - pos.y);
    }

    window->ns.cursorWarpDeltaX = 0;
    window->ns.cursorWarpDeltaY = 0;
}

- (void)rightMouseDown:(NSEvent *)event
{
    _desktop_windowInputMouseClick(window,
                         DESKTOP_WINDOW_MOUSE_BUTTON_RIGHT,
                         DESKTOP_WINDOW_PRESS,
                         translateFlags([event modifierFlags]));
}

- (void)rightMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)rightMouseUp:(NSEvent *)event
{
    _desktop_windowInputMouseClick(window,
                         DESKTOP_WINDOW_MOUSE_BUTTON_RIGHT,
                         DESKTOP_WINDOW_RELEASE,
                         translateFlags([event modifierFlags]));
}

- (void)otherMouseDown:(NSEvent *)event
{
    _desktop_windowInputMouseClick(window,
                         (int) [event buttonNumber],
                         DESKTOP_WINDOW_PRESS,
                         translateFlags([event modifierFlags]));
}

- (void)otherMouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

- (void)otherMouseUp:(NSEvent *)event
{
    _desktop_windowInputMouseClick(window,
                         (int) [event buttonNumber],
                         DESKTOP_WINDOW_RELEASE,
                         translateFlags([event modifierFlags]));
}

- (void)mouseExited:(NSEvent *)event
{
    if (window->cursorMode == DESKTOP_WINDOW_CURSOR_HIDDEN)
        showCursor(window);

    _desktop_windowInputCursorEnter(window, DESKTOP_WINDOW_FALSE);
}

- (void)mouseEntered:(NSEvent *)event
{
    if (window->cursorMode == DESKTOP_WINDOW_CURSOR_HIDDEN)
        hideCursor(window);

    _desktop_windowInputCursorEnter(window, DESKTOP_WINDOW_TRUE);
}

- (void)viewDidChangeBackingProperties
{
    const NSRect contentRect = [window->ns.view frame];
    const NSRect fbRect = [window->ns.view convertRectToBacking:contentRect];
    const float xscale = fbRect.size.width / contentRect.size.width;
    const float yscale = fbRect.size.height / contentRect.size.height;

    if (xscale != window->ns.xscale || yscale != window->ns.yscale)
    {
        if (window->ns.scaleFramebuffer && window->ns.layer)
            [window->ns.layer setContentsScale:[window->ns.object backingScaleFactor]];

        window->ns.xscale = xscale;
        window->ns.yscale = yscale;
        _desktop_windowInputWindowContentScale(window, xscale, yscale);
    }

    if (fbRect.size.width != window->ns.fbWidth ||
        fbRect.size.height != window->ns.fbHeight)
    {
        window->ns.fbWidth  = fbRect.size.width;
        window->ns.fbHeight = fbRect.size.height;
        _desktop_windowInputFramebufferSize(window, fbRect.size.width, fbRect.size.height);
    }
}

- (void)drawRect:(NSRect)rect
{
    _desktop_windowInputWindowDamage(window);
}

- (void)updateTrackingAreas
{
    if (trackingArea != nil)
    {
        [self removeTrackingArea:trackingArea];
        [trackingArea release];
    }

    const NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited |
                                          NSTrackingActiveInKeyWindow |
                                          NSTrackingEnabledDuringMouseDrag |
                                          NSTrackingCursorUpdate |
                                          NSTrackingInVisibleRect |
                                          NSTrackingAssumeInside;

    trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                options:options
                                                  owner:self
                                               userInfo:nil];

    [self addTrackingArea:trackingArea];
    [super updateTrackingAreas];
}

- (void)keyDown:(NSEvent *)event
{
    const int key = translateKey([event keyCode]);
    const int mods = translateFlags([event modifierFlags]);

    _desktop_windowInputKey(window, key, [event keyCode], DESKTOP_WINDOW_PRESS, mods);

    [self interpretKeyEvents:@[event]];
}

- (void)flagsChanged:(NSEvent *)event
{
    int action;
    const unsigned int modifierFlags =
        [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
    const int key = translateKey([event keyCode]);
    const int mods = translateFlags(modifierFlags);
    const NSUInteger keyFlag = translateKeyToModifierFlag(key);

    if (keyFlag & modifierFlags)
    {
        if (window->keys[key] == DESKTOP_WINDOW_PRESS)
            action = DESKTOP_WINDOW_RELEASE;
        else
            action = DESKTOP_WINDOW_PRESS;
    }
    else
        action = DESKTOP_WINDOW_RELEASE;

    _desktop_windowInputKey(window, key, [event keyCode], action, mods);
}

- (void)keyUp:(NSEvent *)event
{
    const int key = translateKey([event keyCode]);
    const int mods = translateFlags([event modifierFlags]);
    _desktop_windowInputKey(window, key, [event keyCode], DESKTOP_WINDOW_RELEASE, mods);
}

- (void)scrollWheel:(NSEvent *)event
{
    double deltaX = [event scrollingDeltaX];
    double deltaY = [event scrollingDeltaY];

    if ([event hasPreciseScrollingDeltas])
    {
        deltaX *= 0.1;
        deltaY *= 0.1;
    }

    if (fabs(deltaX) > 0.0 || fabs(deltaY) > 0.0)
        _desktop_windowInputScroll(window, deltaX, deltaY);
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    // HACK: We don't know what to say here because we don't know what the
    //       application wants to do with the paths
    return NSDragOperationGeneric;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    const NSRect contentRect = [window->ns.view frame];
    // NOTE: The returned location uses base 0,1 not 0,0
    const NSPoint pos = [sender draggingLocation];
    _desktop_windowInputCursorPos(window, pos.x, contentRect.size.height - pos.y);

    NSPasteboard* pasteboard = [sender draggingPasteboard];
    NSDictionary* options = @{NSPasteboardURLReadingFileURLsOnlyKey:@YES};
    NSArray* urls = [pasteboard readObjectsForClasses:@[[NSURL class]]
                                              options:options];
    const NSUInteger count = [urls count];
    if (count)
    {
        char** paths = _desktop_window_calloc(count, sizeof(char*));

        for (NSUInteger i = 0;  i < count;  i++)
            paths[i] = _desktop_window_strdup([urls[i] fileSystemRepresentation]);

        _desktop_windowInputDrop(window, (int) count, (const char**) paths);

        for (NSUInteger i = 0;  i < count;  i++)
            _desktop_window_free(paths[i]);
        _desktop_window_free(paths);
    }

    return YES;
}

- (BOOL)hasMarkedText
{
    return [markedText length] > 0;
}

- (NSRange)markedRange
{
    if ([markedText length] > 0)
        return NSMakeRange(0, [markedText length] - 1);
    else
        return kEmptyRange;
}

- (NSRange)selectedRange
{
    return kEmptyRange;
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange
{
    [markedText release];
    if ([string isKindOfClass:[NSAttributedString class]])
        markedText = [[NSMutableAttributedString alloc] initWithAttributedString:string];
    else
        markedText = [[NSMutableAttributedString alloc] initWithString:string];
}

- (void)unmarkText
{
    [[markedText mutableString] setString:@""];
}

- (NSArray*)validAttributesForMarkedText
{
    return [NSArray array];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:(NSRangePointer)actualRange
{
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actualRange
{
    const NSRect frame = [window->ns.view frame];
    return NSMakeRect(frame.origin.x, frame.origin.y, 0.0, 0.0);
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    NSString* characters;
    NSEvent* event = [NSApp currentEvent];
    const int mods = translateFlags([event modifierFlags]);
    const int plain = !(mods & DESKTOP_WINDOW_MOD_SUPER);

    if ([string isKindOfClass:[NSAttributedString class]])
        characters = [string string];
    else
        characters = (NSString*) string;

    NSRange range = NSMakeRange(0, [characters length]);
    while (range.length)
    {
        uint32_t codepoint = 0;

        if ([characters getBytes:&codepoint
                       maxLength:sizeof(codepoint)
                      usedLength:NULL
                        encoding:NSUTF32StringEncoding
                         options:0
                           range:range
                  remainingRange:&range])
        {
            if (codepoint >= 0xf700 && codepoint <= 0xf7ff)
                continue;

            _desktop_windowInputChar(window, codepoint, mods, plain);
        }
    }
}

- (void)doCommandBySelector:(SEL)selector
{
}

@end


//------------------------------------------------------------------------
// DESKTOP_WINDOW window class
//------------------------------------------------------------------------

@interface DESKTOP_WINDOWWindow : NSWindow {}
@end

@implementation DESKTOP_WINDOWWindow

- (BOOL)canBecomeKeyWindow
{
    // Required for NSWindowStyleMaskBorderless windows
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}

@end


// Create the Cocoa window
//
static DESKTOP_WINDOWbool createNativeWindow(_DESKTOP_WINDOWwindow* window,
                                   const _DESKTOP_WINDOWwndconfig* wndconfig,
                                   const _DESKTOP_WINDOWfbconfig* fbconfig)
{
    window->ns.delegate = [[DESKTOP_WINDOWWindowDelegate alloc] initWithGlfwWindow:window];
    if (window->ns.delegate == nil)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to create window delegate");
        return DESKTOP_WINDOW_FALSE;
    }

    NSRect contentRect;

    if (window->monitor)
    {
        DESKTOP_WINDOWvidmode mode;
        int xpos, ypos;

        _desktop_windowGetVideoModeCocoa(window->monitor, &mode);
        _desktop_windowGetMonitorPosCocoa(window->monitor, &xpos, &ypos);

        contentRect = NSMakeRect(xpos, ypos, mode.width, mode.height);
    }
    else
    {
        if (wndconfig->xpos == DESKTOP_WINDOW_ANY_POSITION ||
            wndconfig->ypos == DESKTOP_WINDOW_ANY_POSITION)
        {
            contentRect = NSMakeRect(0, 0, wndconfig->width, wndconfig->height);
        }
        else
        {
            const int xpos = wndconfig->xpos;
            const int ypos = _desktop_windowTransformYCocoa(wndconfig->ypos + wndconfig->height - 1);
            contentRect = NSMakeRect(xpos, ypos, wndconfig->width, wndconfig->height);
        }
    }

    NSUInteger styleMask = NSWindowStyleMaskMiniaturizable;

    if (window->monitor || !window->decorated)
        styleMask |= NSWindowStyleMaskBorderless;
    else
    {
        styleMask |= (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable);

        if (window->resizable)
            styleMask |= NSWindowStyleMaskResizable;
    }

    window->ns.object = [[DESKTOP_WINDOWWindow alloc]
        initWithContentRect:contentRect
                  styleMask:styleMask
                    backing:NSBackingStoreBuffered
                      defer:NO];

    if (window->ns.object == nil)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "Cocoa: Failed to create window");
        return DESKTOP_WINDOW_FALSE;
    }

    if (window->monitor)
        [window->ns.object setLevel:NSMainMenuWindowLevel + 1];
    else
    {
        if (wndconfig->xpos == DESKTOP_WINDOW_ANY_POSITION ||
            wndconfig->ypos == DESKTOP_WINDOW_ANY_POSITION)
        {
            [(NSWindow*) window->ns.object center];
            _desktop_window.ns.cascadePoint =
                NSPointToCGPoint([window->ns.object cascadeTopLeftFromPoint:
                                NSPointFromCGPoint(_desktop_window.ns.cascadePoint)]);
        }

        if (wndconfig->resizable)
        {
            const NSWindowCollectionBehavior behavior =
                NSWindowCollectionBehaviorFullScreenPrimary |
                NSWindowCollectionBehaviorManaged;
            [window->ns.object setCollectionBehavior:behavior];
        }
        else
        {
            const NSWindowCollectionBehavior behavior =
                NSWindowCollectionBehaviorFullScreenNone;
            [window->ns.object setCollectionBehavior:behavior];
        }

        if (wndconfig->floating)
            [window->ns.object setLevel:NSFloatingWindowLevel];

        if (wndconfig->maximized)
            [window->ns.object zoom:nil];
    }

    if (strlen(wndconfig->ns.frameName))
        [window->ns.object setFrameAutosaveName:@(wndconfig->ns.frameName)];

    window->ns.view = [[DESKTOP_WINDOWContentView alloc] initWithGlfwWindow:window];
    window->ns.scaleFramebuffer = wndconfig->scaleFramebuffer;

    if (fbconfig->transparent)
    {
        [window->ns.object setOpaque:NO];
        [window->ns.object setHasShadow:NO];
        [window->ns.object setBackgroundColor:[NSColor clearColor]];
    }

    [window->ns.object setContentView:window->ns.view];
    [window->ns.object makeFirstResponder:window->ns.view];
    [window->ns.object setTitle:@(wndconfig->title)];
    [window->ns.object setDelegate:window->ns.delegate];
    [window->ns.object setAcceptsMouseMovedEvents:YES];
    [window->ns.object setRestorable:NO];

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101200
    if ([window->ns.object respondsToSelector:@selector(setTabbingMode:)])
        [window->ns.object setTabbingMode:NSWindowTabbingModeDisallowed];
#endif

    _desktop_windowGetWindowSizeCocoa(window, &window->ns.width, &window->ns.height);
    _desktop_windowGetFramebufferSizeCocoa(window, &window->ns.fbWidth, &window->ns.fbHeight);

    return DESKTOP_WINDOW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Transforms a y-coordinate between the CG display and NS screen spaces
//
float _desktop_windowTransformYCocoa(float y)
{
    return CGDisplayBounds(CGMainDisplayID()).size.height - y - 1;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowCreateWindowCocoa(_DESKTOP_WINDOWwindow* window,
                                const _DESKTOP_WINDOWwndconfig* wndconfig,
                                const _DESKTOP_WINDOWctxconfig* ctxconfig,
                                const _DESKTOP_WINDOWfbconfig* fbconfig)
{
    @autoreleasepool {

    if (!createNativeWindow(window, wndconfig, fbconfig))
        return DESKTOP_WINDOW_FALSE;

    if (ctxconfig->client != DESKTOP_WINDOW_NO_API)
    {
        if (ctxconfig->source == DESKTOP_WINDOW_NATIVE_CONTEXT_API)
        {
            if (!_desktop_windowInitNSGL())
                return DESKTOP_WINDOW_FALSE;
            if (!_desktop_windowCreateContextNSGL(window, ctxconfig, fbconfig))
                return DESKTOP_WINDOW_FALSE;
        }
        else if (ctxconfig->source == DESKTOP_WINDOW_EGL_CONTEXT_API)
        {
            // EGL implementation on macOS use CALayer* EGLNativeWindowType so we
            // need to get the layer for EGL window surface creation.
            [window->ns.view setWantsLayer:YES];
            window->ns.layer = [window->ns.view layer];

            if (!_desktop_windowInitEGL())
                return DESKTOP_WINDOW_FALSE;
            if (!_desktop_windowCreateContextEGL(window, ctxconfig, fbconfig))
                return DESKTOP_WINDOW_FALSE;
        }
        else if (ctxconfig->source == DESKTOP_WINDOW_OSMESA_CONTEXT_API)
        {
            if (!_desktop_windowInitOSMesa())
                return DESKTOP_WINDOW_FALSE;
            if (!_desktop_windowCreateContextOSMesa(window, ctxconfig, fbconfig))
                return DESKTOP_WINDOW_FALSE;
        }

        if (!_desktop_windowRefreshContextAttribs(window, ctxconfig))
            return DESKTOP_WINDOW_FALSE;
    }

    if (wndconfig->mousePassthrough)
        _desktop_windowSetWindowMousePassthroughCocoa(window, DESKTOP_WINDOW_TRUE);

    if (window->monitor)
    {
        _desktop_windowShowWindowCocoa(window);
        _desktop_windowFocusWindowCocoa(window);
        acquireMonitor(window);

        if (wndconfig->centerCursor)
            _desktop_windowCenterCursorInContentArea(window);
    }
    else
    {
        if (wndconfig->visible)
        {
            _desktop_windowShowWindowCocoa(window);
            if (wndconfig->focused)
                _desktop_windowFocusWindowCocoa(window);
        }
    }

    return DESKTOP_WINDOW_TRUE;

    } // autoreleasepool
}

void _desktop_windowDestroyWindowCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {

    if (_desktop_window.ns.disabledCursorWindow == window)
        _desktop_window.ns.disabledCursorWindow = NULL;

    [window->ns.object orderOut:nil];

    if (window->monitor)
        releaseMonitor(window);

    if (window->context.destroy)
        window->context.destroy(window);

    [window->ns.object setDelegate:nil];
    [window->ns.delegate release];
    window->ns.delegate = nil;

    [window->ns.view release];
    window->ns.view = nil;

    [window->ns.object close];
    window->ns.object = nil;

    // HACK: Allow Cocoa to catch up before returning
    _desktop_windowPollEventsCocoa();

    } // autoreleasepool
}

void _desktop_windowSetWindowTitleCocoa(_DESKTOP_WINDOWwindow* window, const char* title)
{
    @autoreleasepool {
    NSString* string = @(title);
    [window->ns.object setTitle:string];
    // HACK: Set the miniwindow title explicitly as setTitle: doesn't update it
    //       if the window lacks NSWindowStyleMaskTitled
    [window->ns.object setMiniwindowTitle:string];
    } // autoreleasepool
}

void _desktop_windowSetWindowIconCocoa(_DESKTOP_WINDOWwindow* window,
                             int count, const DESKTOP_WINDOWimage* images)
{
    _desktop_windowInputError(DESKTOP_WINDOW_FEATURE_UNAVAILABLE,
                    "Cocoa: Regular windows do not have icons on macOS");
}

void _desktop_windowGetWindowPosCocoa(_DESKTOP_WINDOWwindow* window, int* xpos, int* ypos)
{
    @autoreleasepool {

    const NSRect contentRect =
        [window->ns.object contentRectForFrameRect:[window->ns.object frame]];

    if (xpos)
        *xpos = contentRect.origin.x;
    if (ypos)
        *ypos = _desktop_windowTransformYCocoa(contentRect.origin.y + contentRect.size.height - 1);

    } // autoreleasepool
}

void _desktop_windowSetWindowPosCocoa(_DESKTOP_WINDOWwindow* window, int x, int y)
{
    @autoreleasepool {

    const NSRect contentRect = [window->ns.view frame];
    const NSRect dummyRect = NSMakeRect(x, _desktop_windowTransformYCocoa(y + contentRect.size.height - 1), 0, 0);
    const NSRect frameRect = [window->ns.object frameRectForContentRect:dummyRect];
    [window->ns.object setFrameOrigin:frameRect.origin];

    } // autoreleasepool
}

void _desktop_windowGetWindowSizeCocoa(_DESKTOP_WINDOWwindow* window, int* width, int* height)
{
    @autoreleasepool {

    const NSRect contentRect = [window->ns.view frame];

    if (width)
        *width = contentRect.size.width;
    if (height)
        *height = contentRect.size.height;

    } // autoreleasepool
}

void _desktop_windowSetWindowSizeCocoa(_DESKTOP_WINDOWwindow* window, int width, int height)
{
    @autoreleasepool {

    if (window->monitor)
    {
        if (window->monitor->window == window)
            acquireMonitor(window);
    }
    else
    {
        NSRect contentRect =
            [window->ns.object contentRectForFrameRect:[window->ns.object frame]];
        contentRect.origin.y += contentRect.size.height - height;
        contentRect.size = NSMakeSize(width, height);
        [window->ns.object setFrame:[window->ns.object frameRectForContentRect:contentRect]
                            display:YES];
    }

    } // autoreleasepool
}

void _desktop_windowSetWindowSizeLimitsCocoa(_DESKTOP_WINDOWwindow* window,
                                   int minwidth, int minheight,
                                   int maxwidth, int maxheight)
{
    @autoreleasepool {

    if (minwidth == DESKTOP_WINDOW_DONT_CARE || minheight == DESKTOP_WINDOW_DONT_CARE)
        [window->ns.object setContentMinSize:NSMakeSize(0, 0)];
    else
        [window->ns.object setContentMinSize:NSMakeSize(minwidth, minheight)];

    if (maxwidth == DESKTOP_WINDOW_DONT_CARE || maxheight == DESKTOP_WINDOW_DONT_CARE)
        [window->ns.object setContentMaxSize:NSMakeSize(DBL_MAX, DBL_MAX)];
    else
        [window->ns.object setContentMaxSize:NSMakeSize(maxwidth, maxheight)];

    } // autoreleasepool
}

void _desktop_windowSetWindowAspectRatioCocoa(_DESKTOP_WINDOWwindow* window, int numer, int denom)
{
    @autoreleasepool {
    if (numer == DESKTOP_WINDOW_DONT_CARE || denom == DESKTOP_WINDOW_DONT_CARE)
        [window->ns.object setResizeIncrements:NSMakeSize(1.0, 1.0)];
    else
        [window->ns.object setContentAspectRatio:NSMakeSize(numer, denom)];
    } // autoreleasepool
}

void _desktop_windowGetFramebufferSizeCocoa(_DESKTOP_WINDOWwindow* window, int* width, int* height)
{
    @autoreleasepool {

    const NSRect contentRect = [window->ns.view frame];
    const NSRect fbRect = [window->ns.view convertRectToBacking:contentRect];

    if (width)
        *width = (int) fbRect.size.width;
    if (height)
        *height = (int) fbRect.size.height;

    } // autoreleasepool
}

void _desktop_windowGetWindowFrameSizeCocoa(_DESKTOP_WINDOWwindow* window,
                                  int* left, int* top,
                                  int* right, int* bottom)
{
    @autoreleasepool {

    const NSRect contentRect = [window->ns.view frame];
    const NSRect frameRect = [window->ns.object frameRectForContentRect:contentRect];

    if (left)
        *left = contentRect.origin.x - frameRect.origin.x;
    if (top)
        *top = frameRect.origin.y + frameRect.size.height -
               contentRect.origin.y - contentRect.size.height;
    if (right)
        *right = frameRect.origin.x + frameRect.size.width -
                 contentRect.origin.x - contentRect.size.width;
    if (bottom)
        *bottom = contentRect.origin.y - frameRect.origin.y;

    } // autoreleasepool
}

void _desktop_windowGetWindowContentScaleCocoa(_DESKTOP_WINDOWwindow* window,
                                     float* xscale, float* yscale)
{
    @autoreleasepool {

    const NSRect points = [window->ns.view frame];
    const NSRect pixels = [window->ns.view convertRectToBacking:points];

    if (xscale)
        *xscale = (float) (pixels.size.width / points.size.width);
    if (yscale)
        *yscale = (float) (pixels.size.height / points.size.height);

    } // autoreleasepool
}

void _desktop_windowIconifyWindowCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    [window->ns.object miniaturize:nil];
    } // autoreleasepool
}

void _desktop_windowRestoreWindowCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    if ([window->ns.object isMiniaturized])
        [window->ns.object deminiaturize:nil];
    else if ([window->ns.object isZoomed])
        [window->ns.object zoom:nil];
    } // autoreleasepool
}

void _desktop_windowMaximizeWindowCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    if (![window->ns.object isZoomed])
        [window->ns.object zoom:nil];
    } // autoreleasepool
}

void _desktop_windowShowWindowCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    [window->ns.object orderFront:nil];
    } // autoreleasepool
}

void _desktop_windowHideWindowCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    [window->ns.object orderOut:nil];
    } // autoreleasepool
}

void _desktop_windowRequestWindowAttentionCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    [NSApp requestUserAttention:NSInformationalRequest];
    } // autoreleasepool
}

void _desktop_windowFocusWindowCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    // Make us the active application
    // HACK: This is here to prevent applications using only hidden windows from
    //       being activated, but should probably not be done every time any
    //       window is shown
    [NSApp activateIgnoringOtherApps:YES];
    [window->ns.object makeKeyAndOrderFront:nil];
    } // autoreleasepool
}

void _desktop_windowSetWindowMonitorCocoa(_DESKTOP_WINDOWwindow* window,
                                _DESKTOP_WINDOWmonitor* monitor,
                                int xpos, int ypos,
                                int width, int height,
                                int refreshRate)
{
    @autoreleasepool {

    if (window->monitor == monitor)
    {
        if (monitor)
        {
            if (monitor->window == window)
                acquireMonitor(window);
        }
        else
        {
            const NSRect contentRect =
                NSMakeRect(xpos, _desktop_windowTransformYCocoa(ypos + height - 1), width, height);
            const NSUInteger styleMask = [window->ns.object styleMask];
            const NSRect frameRect =
                [window->ns.object frameRectForContentRect:contentRect
                                                 styleMask:styleMask];

            [window->ns.object setFrame:frameRect display:YES];
        }

        return;
    }

    if (window->monitor)
        releaseMonitor(window);

    _desktop_windowInputWindowMonitor(window, monitor);

    // HACK: Allow the state cached in Cocoa to catch up to reality
    // TODO: Solve this in a less terrible way
    _desktop_windowPollEventsCocoa();

    NSUInteger styleMask = [window->ns.object styleMask];

    if (window->monitor)
    {
        styleMask &= ~(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable);
        styleMask |= NSWindowStyleMaskBorderless;
    }
    else
    {
        if (window->decorated)
        {
            styleMask &= ~NSWindowStyleMaskBorderless;
            styleMask |= (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable);
        }

        if (window->resizable)
            styleMask |= NSWindowStyleMaskResizable;
        else
            styleMask &= ~NSWindowStyleMaskResizable;
    }

    [window->ns.object setStyleMask:styleMask];
    // HACK: Changing the style mask can cause the first responder to be cleared
    [window->ns.object makeFirstResponder:window->ns.view];

    if (window->monitor)
    {
        [window->ns.object setLevel:NSMainMenuWindowLevel + 1];
        [window->ns.object setHasShadow:NO];

        acquireMonitor(window);
    }
    else
    {
        NSRect contentRect = NSMakeRect(xpos, _desktop_windowTransformYCocoa(ypos + height - 1),
                                        width, height);
        NSRect frameRect = [window->ns.object frameRectForContentRect:contentRect
                                                            styleMask:styleMask];
        [window->ns.object setFrame:frameRect display:YES];

        if (window->numer != DESKTOP_WINDOW_DONT_CARE &&
            window->denom != DESKTOP_WINDOW_DONT_CARE)
        {
            [window->ns.object setContentAspectRatio:NSMakeSize(window->numer,
                                                                window->denom)];
        }

        if (window->minwidth != DESKTOP_WINDOW_DONT_CARE &&
            window->minheight != DESKTOP_WINDOW_DONT_CARE)
        {
            [window->ns.object setContentMinSize:NSMakeSize(window->minwidth,
                                                            window->minheight)];
        }

        if (window->maxwidth != DESKTOP_WINDOW_DONT_CARE &&
            window->maxheight != DESKTOP_WINDOW_DONT_CARE)
        {
            [window->ns.object setContentMaxSize:NSMakeSize(window->maxwidth,
                                                            window->maxheight)];
        }

        if (window->floating)
            [window->ns.object setLevel:NSFloatingWindowLevel];
        else
            [window->ns.object setLevel:NSNormalWindowLevel];

        if (window->resizable)
        {
            const NSWindowCollectionBehavior behavior =
                NSWindowCollectionBehaviorFullScreenPrimary |
                NSWindowCollectionBehaviorManaged;
            [window->ns.object setCollectionBehavior:behavior];
        }
        else
        {
            const NSWindowCollectionBehavior behavior =
                NSWindowCollectionBehaviorFullScreenNone;
            [window->ns.object setCollectionBehavior:behavior];
        }

        [window->ns.object setHasShadow:YES];
        // HACK: Clearing NSWindowStyleMaskTitled resets and disables the window
        //       title property but the miniwindow title property is unaffected
        [window->ns.object setTitle:[window->ns.object miniwindowTitle]];
    }

    } // autoreleasepool
}

DESKTOP_WINDOWbool _desktop_windowWindowFocusedCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    return [window->ns.object isKeyWindow];
    } // autoreleasepool
}

DESKTOP_WINDOWbool _desktop_windowWindowIconifiedCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    return [window->ns.object isMiniaturized];
    } // autoreleasepool
}

DESKTOP_WINDOWbool _desktop_windowWindowVisibleCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    return [window->ns.object isVisible];
    } // autoreleasepool
}

DESKTOP_WINDOWbool _desktop_windowWindowMaximizedCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {

    if (window->resizable)
        return [window->ns.object isZoomed];
    else
        return DESKTOP_WINDOW_FALSE;

    } // autoreleasepool
}

DESKTOP_WINDOWbool _desktop_windowWindowHoveredCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {

    const NSPoint point = [NSEvent mouseLocation];

    if ([NSWindow windowNumberAtPoint:point belowWindowWithWindowNumber:0] !=
        [window->ns.object windowNumber])
    {
        return DESKTOP_WINDOW_FALSE;
    }

    return NSMouseInRect(point,
        [window->ns.object convertRectToScreen:[window->ns.view frame]], NO);

    } // autoreleasepool
}

DESKTOP_WINDOWbool _desktop_windowFramebufferTransparentCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    return ![window->ns.object isOpaque] && ![window->ns.view isOpaque];
    } // autoreleasepool
}

void _desktop_windowSetWindowResizableCocoa(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    @autoreleasepool {

    const NSUInteger styleMask = [window->ns.object styleMask];
    if (enabled)
    {
        [window->ns.object setStyleMask:(styleMask | NSWindowStyleMaskResizable)];
        const NSWindowCollectionBehavior behavior =
            NSWindowCollectionBehaviorFullScreenPrimary |
            NSWindowCollectionBehaviorManaged;
        [window->ns.object setCollectionBehavior:behavior];
    }
    else
    {
        [window->ns.object setStyleMask:(styleMask & ~NSWindowStyleMaskResizable)];
        const NSWindowCollectionBehavior behavior =
            NSWindowCollectionBehaviorFullScreenNone;
        [window->ns.object setCollectionBehavior:behavior];
    }

    } // autoreleasepool
}

void _desktop_windowSetWindowDecoratedCocoa(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    @autoreleasepool {

    NSUInteger styleMask = [window->ns.object styleMask];
    if (enabled)
    {
        styleMask |= (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable);
        styleMask &= ~NSWindowStyleMaskBorderless;
    }
    else
    {
        styleMask |= NSWindowStyleMaskBorderless;
        styleMask &= ~(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable);
    }

    [window->ns.object setStyleMask:styleMask];
    [window->ns.object makeFirstResponder:window->ns.view];

    } // autoreleasepool
}

void _desktop_windowSetWindowFloatingCocoa(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    @autoreleasepool {
    if (enabled)
        [window->ns.object setLevel:NSFloatingWindowLevel];
    else
        [window->ns.object setLevel:NSNormalWindowLevel];
    } // autoreleasepool
}

void _desktop_windowSetWindowMousePassthroughCocoa(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool enabled)
{
    @autoreleasepool {
    [window->ns.object setIgnoresMouseEvents:enabled];
    }
}

float _desktop_windowGetWindowOpacityCocoa(_DESKTOP_WINDOWwindow* window)
{
    @autoreleasepool {
    return (float) [window->ns.object alphaValue];
    } // autoreleasepool
}

void _desktop_windowSetWindowOpacityCocoa(_DESKTOP_WINDOWwindow* window, float opacity)
{
    @autoreleasepool {
    [window->ns.object setAlphaValue:opacity];
    } // autoreleasepool
}

void _desktop_windowSetRawMouseMotionCocoa(_DESKTOP_WINDOWwindow *window, DESKTOP_WINDOWbool enabled)
{
    _desktop_windowInputError(DESKTOP_WINDOW_FEATURE_UNIMPLEMENTED,
                    "Cocoa: Raw mouse motion not yet implemented");
}

DESKTOP_WINDOWbool _desktop_windowRawMouseMotionSupportedCocoa(void)
{
    return DESKTOP_WINDOW_FALSE;
}

void _desktop_windowPollEventsCocoa(void)
{
    @autoreleasepool {

    for (;;)
    {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event == nil)
            break;

        [NSApp sendEvent:event];
    }

    } // autoreleasepool
}

void _desktop_windowWaitEventsCocoa(void)
{
    @autoreleasepool {

    // I wanted to pass NO to dequeue:, and rely on PollEvents to
    // dequeue and send.  For reasons not at all clear to me, passing
    // NO to dequeue: causes this method never to return.
    NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:[NSDate distantFuture]
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];
    [NSApp sendEvent:event];

    _desktop_windowPollEventsCocoa();

    } // autoreleasepool
}

void _desktop_windowWaitEventsTimeoutCocoa(double timeout)
{
    @autoreleasepool {

    NSDate* date = [NSDate dateWithTimeIntervalSinceNow:timeout];
    NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:date
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];
    if (event)
        [NSApp sendEvent:event];

    _desktop_windowPollEventsCocoa();

    } // autoreleasepool
}

void _desktop_windowPostEmptyEventCocoa(void)
{
    @autoreleasepool {

    NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSMakePoint(0, 0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:YES];

    } // autoreleasepool
}

void _desktop_windowGetCursorPosCocoa(_DESKTOP_WINDOWwindow* window, double* xpos, double* ypos)
{
    @autoreleasepool {

    const NSRect contentRect = [window->ns.view frame];
    // NOTE: The returned location uses base 0,1 not 0,0
    const NSPoint pos = [window->ns.object mouseLocationOutsideOfEventStream];

    if (xpos)
        *xpos = pos.x;
    if (ypos)
        *ypos = contentRect.size.height - pos.y;

    } // autoreleasepool
}

void _desktop_windowSetCursorPosCocoa(_DESKTOP_WINDOWwindow* window, double x, double y)
{
    @autoreleasepool {

    updateCursorImage(window);

    const NSRect contentRect = [window->ns.view frame];
    // NOTE: The returned location uses base 0,1 not 0,0
    const NSPoint pos = [window->ns.object mouseLocationOutsideOfEventStream];

    window->ns.cursorWarpDeltaX += x - pos.x;
    window->ns.cursorWarpDeltaY += y - contentRect.size.height + pos.y;

    if (window->monitor)
    {
        CGDisplayMoveCursorToPoint(window->monitor->ns.displayID,
                                   CGPointMake(x, y));
    }
    else
    {
        const NSRect localRect = NSMakeRect(x, contentRect.size.height - y - 1, 0, 0);
        const NSRect globalRect = [window->ns.object convertRectToScreen:localRect];
        const NSPoint globalPoint = globalRect.origin;

        CGWarpMouseCursorPosition(CGPointMake(globalPoint.x,
                                              _desktop_windowTransformYCocoa(globalPoint.y)));
    }

    // HACK: Calling this right after setting the cursor position prevents macOS
    //       from freezing the cursor for a fraction of a second afterwards
    if (window->cursorMode != DESKTOP_WINDOW_CURSOR_DISABLED)
        CGAssociateMouseAndMouseCursorPosition(true);

    } // autoreleasepool
}

void _desktop_windowSetCursorModeCocoa(_DESKTOP_WINDOWwindow* window, int mode)
{
    @autoreleasepool {

    if (mode == DESKTOP_WINDOW_CURSOR_CAPTURED)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_FEATURE_UNIMPLEMENTED,
                        "Cocoa: Captured cursor mode not yet implemented");
    }

    if (_desktop_windowWindowFocusedCocoa(window))
        updateCursorMode(window);

    } // autoreleasepool
}

const char* _desktop_windowGetScancodeNameCocoa(int scancode)
{
    @autoreleasepool {

    if (scancode < 0 || scancode > 0xff)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Invalid scancode %i", scancode);
        return NULL;
    }

    const int key = _desktop_window.ns.keycodes[scancode];
    if (key == DESKTOP_WINDOW_KEY_UNKNOWN)
        return NULL;

    UInt32 deadKeyState = 0;
    UniChar characters[4];
    UniCharCount characterCount = 0;

    if (UCKeyTranslate([(NSData*) _desktop_window.ns.unicodeData bytes],
                       scancode,
                       kUCKeyActionDisplay,
                       0,
                       LMGetKbdType(),
                       kUCKeyTranslateNoDeadKeysBit,
                       &deadKeyState,
                       sizeof(characters) / sizeof(characters[0]),
                       &characterCount,
                       characters) != noErr)
    {
        return NULL;
    }

    if (!characterCount)
        return NULL;

    CFStringRef string = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
                                                            characters,
                                                            characterCount,
                                                            kCFAllocatorNull);
    CFStringGetCString(string,
                       _desktop_window.ns.keynames[key],
                       sizeof(_desktop_window.ns.keynames[key]),
                       kCFStringEncodingUTF8);
    CFRelease(string);

    return _desktop_window.ns.keynames[key];

    } // autoreleasepool
}

int _desktop_windowGetKeyScancodeCocoa(int key)
{
    return _desktop_window.ns.scancodes[key];
}

DESKTOP_WINDOWbool _desktop_windowCreateCursorCocoa(_DESKTOP_WINDOWcursor* cursor,
                                const DESKTOP_WINDOWimage* image,
                                int xhot, int yhot)
{
    @autoreleasepool {

    NSImage* native;
    NSBitmapImageRep* rep;

    rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:image->width
                      pixelsHigh:image->height
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSCalibratedRGBColorSpace
                    bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
                     bytesPerRow:image->width * 4
                    bitsPerPixel:32];

    if (rep == nil)
        return DESKTOP_WINDOW_FALSE;

    memcpy([rep bitmapData], image->pixels, image->width * image->height * 4);

    native = [[NSImage alloc] initWithSize:NSMakeSize(image->width, image->height)];
    [native addRepresentation:rep];

    cursor->ns.object = [[NSCursor alloc] initWithImage:native
                                                hotSpot:NSMakePoint(xhot, yhot)];

    [native release];
    [rep release];

    if (cursor->ns.object == nil)
        return DESKTOP_WINDOW_FALSE;

    return DESKTOP_WINDOW_TRUE;

    } // autoreleasepool
}

DESKTOP_WINDOWbool _desktop_windowCreateStandardCursorCocoa(_DESKTOP_WINDOWcursor* cursor, int shape)
{
    @autoreleasepool {

    SEL cursorSelector = NULL;

    // HACK: Try to use a private message
    switch (shape)
    {
        case DESKTOP_WINDOW_RESIZE_EW_CURSOR:
            cursorSelector = NSSelectorFromString(@"_windowResizeEastWestCursor");
            break;
        case DESKTOP_WINDOW_RESIZE_NS_CURSOR:
            cursorSelector = NSSelectorFromString(@"_windowResizeNorthSouthCursor");
            break;
        case DESKTOP_WINDOW_RESIZE_NWSE_CURSOR:
            cursorSelector = NSSelectorFromString(@"_windowResizeNorthWestSouthEastCursor");
            break;
        case DESKTOP_WINDOW_RESIZE_NESW_CURSOR:
            cursorSelector = NSSelectorFromString(@"_windowResizeNorthEastSouthWestCursor");
            break;
    }

    if (cursorSelector && [NSCursor respondsToSelector:cursorSelector])
    {
        id object = [NSCursor performSelector:cursorSelector];
        if ([object isKindOfClass:[NSCursor class]])
            cursor->ns.object = object;
    }

    if (!cursor->ns.object)
    {
        switch (shape)
        {
            case DESKTOP_WINDOW_ARROW_CURSOR:
                cursor->ns.object = [NSCursor arrowCursor];
                break;
            case DESKTOP_WINDOW_IBEAM_CURSOR:
                cursor->ns.object = [NSCursor IBeamCursor];
                break;
            case DESKTOP_WINDOW_CROSSHAIR_CURSOR:
                cursor->ns.object = [NSCursor crosshairCursor];
                break;
            case DESKTOP_WINDOW_POINTING_HAND_CURSOR:
                cursor->ns.object = [NSCursor pointingHandCursor];
                break;
            case DESKTOP_WINDOW_RESIZE_EW_CURSOR:
                cursor->ns.object = [NSCursor resizeLeftRightCursor];
                break;
            case DESKTOP_WINDOW_RESIZE_NS_CURSOR:
                cursor->ns.object = [NSCursor resizeUpDownCursor];
                break;
            case DESKTOP_WINDOW_RESIZE_ALL_CURSOR:
                cursor->ns.object = [NSCursor closedHandCursor];
                break;
            case DESKTOP_WINDOW_NOT_ALLOWED_CURSOR:
                cursor->ns.object = [NSCursor operationNotAllowedCursor];
                break;
        }
    }

    if (!cursor->ns.object)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_CURSOR_UNAVAILABLE,
                        "Cocoa: Standard cursor shape unavailable");
        return DESKTOP_WINDOW_FALSE;
    }

    [cursor->ns.object retain];
    return DESKTOP_WINDOW_TRUE;

    } // autoreleasepool
}

void _desktop_windowDestroyCursorCocoa(_DESKTOP_WINDOWcursor* cursor)
{
    @autoreleasepool {
    if (cursor->ns.object)
        [(NSCursor*) cursor->ns.object release];
    } // autoreleasepool
}

void _desktop_windowSetCursorCocoa(_DESKTOP_WINDOWwindow* window, _DESKTOP_WINDOWcursor* cursor)
{
    @autoreleasepool {
    if (cursorInContentArea(window))
        updateCursorImage(window);
    } // autoreleasepool
}

void _desktop_windowSetClipboardStringCocoa(const char* string)
{
    @autoreleasepool {
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:@[NSPasteboardTypeString] owner:nil];
    [pasteboard setString:@(string) forType:NSPasteboardTypeString];
    } // autoreleasepool
}

const char* _desktop_windowGetClipboardStringCocoa(void)
{
    @autoreleasepool {

    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];

    if (![[pasteboard types] containsObject:NSPasteboardTypeString])
    {
        _desktop_windowInputError(DESKTOP_WINDOW_FORMAT_UNAVAILABLE,
                        "Cocoa: Failed to retrieve string from pasteboard");
        return NULL;
    }

    NSString* object = [pasteboard stringForType:NSPasteboardTypeString];
    if (!object)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to retrieve object from pasteboard");
        return NULL;
    }

    _desktop_window_free(_desktop_window.ns.clipboardString);
    _desktop_window.ns.clipboardString = _desktop_window_strdup([object UTF8String]);

    return _desktop_window.ns.clipboardString;

    } // autoreleasepool
}

EGLenum _desktop_windowGetEGLPlatformCocoa(EGLint** attribs)
{
    if (_desktop_window.egl.ANGLE_platform_angle)
    {
        int type = 0;

        if (_desktop_window.egl.ANGLE_platform_angle_opengl)
        {
            if (_desktop_window.hints.init.angleType == DESKTOP_WINDOW_ANGLE_PLATFORM_TYPE_OPENGL)
                type = EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE;
        }

        if (_desktop_window.egl.ANGLE_platform_angle_metal)
        {
            if (_desktop_window.hints.init.angleType == DESKTOP_WINDOW_ANGLE_PLATFORM_TYPE_METAL)
                type = EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE;
        }

        if (type)
        {
            *attribs = _desktop_window_calloc(3, sizeof(EGLint));
            (*attribs)[0] = EGL_PLATFORM_ANGLE_TYPE_ANGLE;
            (*attribs)[1] = type;
            (*attribs)[2] = EGL_NONE;
            return EGL_PLATFORM_ANGLE_ANGLE;
        }
    }

    return 0;
}

EGLNativeDisplayType _desktop_windowGetEGLNativeDisplayCocoa(void)
{
    return EGL_DEFAULT_DISPLAY;
}

EGLNativeWindowType _desktop_windowGetEGLNativeWindowCocoa(_DESKTOP_WINDOWwindow* window)
{
    return window->ns.layer;
}

void _desktop_windowGetRequiredInstanceExtensionsCocoa(char** extensions)
{
    if (_desktop_window.vk.KHR_surface && _desktop_window.vk.EXT_metal_surface)
    {
        extensions[0] = "VK_KHR_surface";
        extensions[1] = "VK_EXT_metal_surface";
    }
    else if (_desktop_window.vk.KHR_surface && _desktop_window.vk.MVK_macos_surface)
    {
        extensions[0] = "VK_KHR_surface";
        extensions[1] = "VK_MVK_macos_surface";
    }
}

DESKTOP_WINDOWbool _desktop_windowGetPhysicalDevicePresentationSupportCocoa(VkInstance instance,
                                                        VkPhysicalDevice device,
                                                        uint32_t queuefamily)
{
    return DESKTOP_WINDOW_TRUE;
}

VkResult _desktop_windowCreateWindowSurfaceCocoa(VkInstance instance,
                                       _DESKTOP_WINDOWwindow* window,
                                       const VkAllocationCallbacks* allocator,
                                       VkSurfaceKHR* surface)
{
    @autoreleasepool {

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101100
    // HACK: Dynamically load Core Animation to avoid adding an extra
    //       dependency for the majority who don't use MoltenVK
    NSBundle* bundle = [NSBundle bundleWithPath:@"/System/Library/Frameworks/QuartzCore.framework"];
    if (!bundle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to find QuartzCore.framework");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    // NOTE: Create the layer here as makeBackingLayer should not return nil
    window->ns.layer = [[bundle classNamed:@"CAMetalLayer"] layer];
    if (!window->ns.layer)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to create layer for view");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    if (window->ns.scaleFramebuffer)
        [window->ns.layer setContentsScale:[window->ns.object backingScaleFactor]];

    [window->ns.view setLayer:window->ns.layer];
    [window->ns.view setWantsLayer:YES];

    VkResult err;

    if (_desktop_window.vk.EXT_metal_surface)
    {
        VkMetalSurfaceCreateInfoEXT sci;

        PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
        vkCreateMetalSurfaceEXT = (PFN_vkCreateMetalSurfaceEXT)
            vkGetInstanceProcAddr(instance, "vkCreateMetalSurfaceEXT");
        if (!vkCreateMetalSurfaceEXT)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "Cocoa: Vulkan instance missing VK_EXT_metal_surface extension");
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        sci.pLayer = window->ns.layer;

        err = vkCreateMetalSurfaceEXT(instance, &sci, allocator, surface);
    }
    else
    {
        VkMacOSSurfaceCreateInfoMVK sci;

        PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
        vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)
            vkGetInstanceProcAddr(instance, "vkCreateMacOSSurfaceMVK");
        if (!vkCreateMacOSSurfaceMVK)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "Cocoa: Vulkan instance missing VK_MVK_macos_surface extension");
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        sci.pView = window->ns.view;

        err = vkCreateMacOSSurfaceMVK(instance, &sci, allocator, surface);
    }

    if (err)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to create Vulkan surface: %s",
                        _desktop_windowGetVulkanResultString(err));
    }

    return err;
#else
    return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif

    } // autoreleasepool
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW native API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI id desktop_windowGetCocoaWindow(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(nil);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_COCOA)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE,
                        "Cocoa: Platform not initialized");
        return nil;
    }

    return window->ns.object;
}

DESKTOP_WINDOWAPI id desktop_windowGetCocoaView(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(nil);

    if (_desktop_window.platform.platformID != DESKTOP_WINDOW_PLATFORM_COCOA)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE,
                        "Cocoa: Platform not initialized");
        return nil;
    }

    return window->ns.view;
}

#endif // _DESKTOP_WINDOW_COCOA

