#include "internal.h"

#if defined(_DESKTOP_WINDOW_COCOA)

#include <sys/param.h> // For MAXPATHLEN

// Needed for _NSGetProgname
#include <crt_externs.h>

// Change to our application bundle's resources directory, if present
//
static void changeToResourcesDirectory(void)
{
    char resourcesPath[MAXPATHLEN];

    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle)
        return;

    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(bundle);

    CFStringRef last = CFURLCopyLastPathComponent(resourcesURL);
    if (CFStringCompare(CFSTR("Resources"), last, 0) != kCFCompareEqualTo)
    {
        CFRelease(last);
        CFRelease(resourcesURL);
        return;
    }

    CFRelease(last);

    if (!CFURLGetFileSystemRepresentation(resourcesURL,
                                          true,
                                          (UInt8*) resourcesPath,
                                          MAXPATHLEN))
    {
        CFRelease(resourcesURL);
        return;
    }

    CFRelease(resourcesURL);

    chdir(resourcesPath);
}

// Set up the menu bar (manually)
// This is nasty, nasty stuff -- calls to undocumented semi-private APIs that
// could go away at any moment, lots of stuff that really should be
// localize(d|able), etc.  Add a nib to save us this horror.
//
static void createMenuBar(void)
{
    NSString* appName = nil;
    NSDictionary* bundleInfo = [[NSBundle mainBundle] infoDictionary];
    NSString* nameKeys[] =
    {
        @"CFBundleDisplayName",
        @"CFBundleName",
        @"CFBundleExecutable",
    };

    // Try to figure out what the calling application is called

    for (size_t i = 0;  i < sizeof(nameKeys) / sizeof(nameKeys[0]);  i++)
    {
        id name = bundleInfo[nameKeys[i]];
        if (name &&
            [name isKindOfClass:[NSString class]] &&
            ![name isEqualToString:@""])
        {
            appName = name;
            break;
        }
    }

    if (!appName)
    {
        char** progname = _NSGetProgname();
        if (progname && *progname)
            appName = @(*progname);
        else
            appName = @"DESKTOP_WINDOW Application";
    }

    NSMenu* bar = [[NSMenu alloc] init];
    [NSApp setMainMenu:bar];

    NSMenuItem* appMenuItem =
        [bar addItemWithTitle:@"" action:NULL keyEquivalent:@""];
    NSMenu* appMenu = [[NSMenu alloc] init];
    [appMenuItem setSubmenu:appMenu];

    [appMenu addItemWithTitle:[NSString stringWithFormat:@"About %@", appName]
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    NSMenu* servicesMenu = [[NSMenu alloc] init];
    [NSApp setServicesMenu:servicesMenu];
    [[appMenu addItemWithTitle:@"Services"
                       action:NULL
                keyEquivalent:@""] setSubmenu:servicesMenu];
    [servicesMenu release];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:[NSString stringWithFormat:@"Hide %@", appName]
                       action:@selector(hide:)
                keyEquivalent:@"h"];
    [[appMenu addItemWithTitle:@"Hide Others"
                       action:@selector(hideOtherApplications:)
                keyEquivalent:@"h"]
        setKeyEquivalentModifierMask:NSEventModifierFlagOption | NSEventModifierFlagCommand];
    [appMenu addItemWithTitle:@"Show All"
                       action:@selector(unhideAllApplications:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:[NSString stringWithFormat:@"Quit %@", appName]
                       action:@selector(terminate:)
                keyEquivalent:@"q"];

    NSMenuItem* windowMenuItem =
        [bar addItemWithTitle:@"" action:NULL keyEquivalent:@""];
    [bar release];
    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [NSApp setWindowsMenu:windowMenu];
    [windowMenuItem setSubmenu:windowMenu];

    [windowMenu addItemWithTitle:@"Minimize"
                          action:@selector(performMiniaturize:)
                   keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom"
                          action:@selector(performZoom:)
                   keyEquivalent:@""];
    [windowMenu addItem:[NSMenuItem separatorItem]];
    [windowMenu addItemWithTitle:@"Bring All to Front"
                          action:@selector(arrangeInFront:)
                   keyEquivalent:@""];

    // TODO: Make this appear at the bottom of the menu (for consistency)
    [windowMenu addItem:[NSMenuItem separatorItem]];
    [[windowMenu addItemWithTitle:@"Enter Full Screen"
                           action:@selector(toggleFullScreen:)
                    keyEquivalent:@"f"]
     setKeyEquivalentModifierMask:NSEventModifierFlagControl | NSEventModifierFlagCommand];

    // Prior to Snow Leopard, we need to use this oddly-named semi-private API
    // to get the application menu working properly.
    SEL setAppleMenuSelector = NSSelectorFromString(@"setAppleMenu:");
    [NSApp performSelector:setAppleMenuSelector withObject:appMenu];
}

// Create key code translation tables
//
static void createKeyTables(void)
{
    memset(_desktop_window.ns.keycodes, -1, sizeof(_desktop_window.ns.keycodes));
    memset(_desktop_window.ns.scancodes, -1, sizeof(_desktop_window.ns.scancodes));

    _desktop_window.ns.keycodes[0x1D] = DESKTOP_WINDOW_KEY_0;
    _desktop_window.ns.keycodes[0x12] = DESKTOP_WINDOW_KEY_1;
    _desktop_window.ns.keycodes[0x13] = DESKTOP_WINDOW_KEY_2;
    _desktop_window.ns.keycodes[0x14] = DESKTOP_WINDOW_KEY_3;
    _desktop_window.ns.keycodes[0x15] = DESKTOP_WINDOW_KEY_4;
    _desktop_window.ns.keycodes[0x17] = DESKTOP_WINDOW_KEY_5;
    _desktop_window.ns.keycodes[0x16] = DESKTOP_WINDOW_KEY_6;
    _desktop_window.ns.keycodes[0x1A] = DESKTOP_WINDOW_KEY_7;
    _desktop_window.ns.keycodes[0x1C] = DESKTOP_WINDOW_KEY_8;
    _desktop_window.ns.keycodes[0x19] = DESKTOP_WINDOW_KEY_9;
    _desktop_window.ns.keycodes[0x00] = DESKTOP_WINDOW_KEY_A;
    _desktop_window.ns.keycodes[0x0B] = DESKTOP_WINDOW_KEY_B;
    _desktop_window.ns.keycodes[0x08] = DESKTOP_WINDOW_KEY_C;
    _desktop_window.ns.keycodes[0x02] = DESKTOP_WINDOW_KEY_D;
    _desktop_window.ns.keycodes[0x0E] = DESKTOP_WINDOW_KEY_E;
    _desktop_window.ns.keycodes[0x03] = DESKTOP_WINDOW_KEY_F;
    _desktop_window.ns.keycodes[0x05] = DESKTOP_WINDOW_KEY_G;
    _desktop_window.ns.keycodes[0x04] = DESKTOP_WINDOW_KEY_H;
    _desktop_window.ns.keycodes[0x22] = DESKTOP_WINDOW_KEY_I;
    _desktop_window.ns.keycodes[0x26] = DESKTOP_WINDOW_KEY_J;
    _desktop_window.ns.keycodes[0x28] = DESKTOP_WINDOW_KEY_K;
    _desktop_window.ns.keycodes[0x25] = DESKTOP_WINDOW_KEY_L;
    _desktop_window.ns.keycodes[0x2E] = DESKTOP_WINDOW_KEY_M;
    _desktop_window.ns.keycodes[0x2D] = DESKTOP_WINDOW_KEY_N;
    _desktop_window.ns.keycodes[0x1F] = DESKTOP_WINDOW_KEY_O;
    _desktop_window.ns.keycodes[0x23] = DESKTOP_WINDOW_KEY_P;
    _desktop_window.ns.keycodes[0x0C] = DESKTOP_WINDOW_KEY_Q;
    _desktop_window.ns.keycodes[0x0F] = DESKTOP_WINDOW_KEY_R;
    _desktop_window.ns.keycodes[0x01] = DESKTOP_WINDOW_KEY_S;
    _desktop_window.ns.keycodes[0x11] = DESKTOP_WINDOW_KEY_T;
    _desktop_window.ns.keycodes[0x20] = DESKTOP_WINDOW_KEY_U;
    _desktop_window.ns.keycodes[0x09] = DESKTOP_WINDOW_KEY_V;
    _desktop_window.ns.keycodes[0x0D] = DESKTOP_WINDOW_KEY_W;
    _desktop_window.ns.keycodes[0x07] = DESKTOP_WINDOW_KEY_X;
    _desktop_window.ns.keycodes[0x10] = DESKTOP_WINDOW_KEY_Y;
    _desktop_window.ns.keycodes[0x06] = DESKTOP_WINDOW_KEY_Z;

    _desktop_window.ns.keycodes[0x27] = DESKTOP_WINDOW_KEY_APOSTROPHE;
    _desktop_window.ns.keycodes[0x2A] = DESKTOP_WINDOW_KEY_BACKSLASH;
    _desktop_window.ns.keycodes[0x2B] = DESKTOP_WINDOW_KEY_COMMA;
    _desktop_window.ns.keycodes[0x18] = DESKTOP_WINDOW_KEY_EQUAL;
    _desktop_window.ns.keycodes[0x32] = DESKTOP_WINDOW_KEY_GRAVE_ACCENT;
    _desktop_window.ns.keycodes[0x21] = DESKTOP_WINDOW_KEY_LEFT_BRACKET;
    _desktop_window.ns.keycodes[0x1B] = DESKTOP_WINDOW_KEY_MINUS;
    _desktop_window.ns.keycodes[0x2F] = DESKTOP_WINDOW_KEY_PERIOD;
    _desktop_window.ns.keycodes[0x1E] = DESKTOP_WINDOW_KEY_RIGHT_BRACKET;
    _desktop_window.ns.keycodes[0x29] = DESKTOP_WINDOW_KEY_SEMICOLON;
    _desktop_window.ns.keycodes[0x2C] = DESKTOP_WINDOW_KEY_SLASH;
    _desktop_window.ns.keycodes[0x0A] = DESKTOP_WINDOW_KEY_WORLD_1;

    _desktop_window.ns.keycodes[0x33] = DESKTOP_WINDOW_KEY_BACKSPACE;
    _desktop_window.ns.keycodes[0x39] = DESKTOP_WINDOW_KEY_CAPS_LOCK;
    _desktop_window.ns.keycodes[0x75] = DESKTOP_WINDOW_KEY_DELETE;
    _desktop_window.ns.keycodes[0x7D] = DESKTOP_WINDOW_KEY_DOWN;
    _desktop_window.ns.keycodes[0x77] = DESKTOP_WINDOW_KEY_END;
    _desktop_window.ns.keycodes[0x24] = DESKTOP_WINDOW_KEY_ENTER;
    _desktop_window.ns.keycodes[0x35] = DESKTOP_WINDOW_KEY_ESCAPE;
    _desktop_window.ns.keycodes[0x7A] = DESKTOP_WINDOW_KEY_F1;
    _desktop_window.ns.keycodes[0x78] = DESKTOP_WINDOW_KEY_F2;
    _desktop_window.ns.keycodes[0x63] = DESKTOP_WINDOW_KEY_F3;
    _desktop_window.ns.keycodes[0x76] = DESKTOP_WINDOW_KEY_F4;
    _desktop_window.ns.keycodes[0x60] = DESKTOP_WINDOW_KEY_F5;
    _desktop_window.ns.keycodes[0x61] = DESKTOP_WINDOW_KEY_F6;
    _desktop_window.ns.keycodes[0x62] = DESKTOP_WINDOW_KEY_F7;
    _desktop_window.ns.keycodes[0x64] = DESKTOP_WINDOW_KEY_F8;
    _desktop_window.ns.keycodes[0x65] = DESKTOP_WINDOW_KEY_F9;
    _desktop_window.ns.keycodes[0x6D] = DESKTOP_WINDOW_KEY_F10;
    _desktop_window.ns.keycodes[0x67] = DESKTOP_WINDOW_KEY_F11;
    _desktop_window.ns.keycodes[0x6F] = DESKTOP_WINDOW_KEY_F12;
    _desktop_window.ns.keycodes[0x69] = DESKTOP_WINDOW_KEY_PRINT_SCREEN;
    _desktop_window.ns.keycodes[0x6B] = DESKTOP_WINDOW_KEY_F14;
    _desktop_window.ns.keycodes[0x71] = DESKTOP_WINDOW_KEY_F15;
    _desktop_window.ns.keycodes[0x6A] = DESKTOP_WINDOW_KEY_F16;
    _desktop_window.ns.keycodes[0x40] = DESKTOP_WINDOW_KEY_F17;
    _desktop_window.ns.keycodes[0x4F] = DESKTOP_WINDOW_KEY_F18;
    _desktop_window.ns.keycodes[0x50] = DESKTOP_WINDOW_KEY_F19;
    _desktop_window.ns.keycodes[0x5A] = DESKTOP_WINDOW_KEY_F20;
    _desktop_window.ns.keycodes[0x73] = DESKTOP_WINDOW_KEY_HOME;
    _desktop_window.ns.keycodes[0x72] = DESKTOP_WINDOW_KEY_INSERT;
    _desktop_window.ns.keycodes[0x7B] = DESKTOP_WINDOW_KEY_LEFT;
    _desktop_window.ns.keycodes[0x3A] = DESKTOP_WINDOW_KEY_LEFT_ALT;
    _desktop_window.ns.keycodes[0x3B] = DESKTOP_WINDOW_KEY_LEFT_CONTROL;
    _desktop_window.ns.keycodes[0x38] = DESKTOP_WINDOW_KEY_LEFT_SHIFT;
    _desktop_window.ns.keycodes[0x37] = DESKTOP_WINDOW_KEY_LEFT_SUPER;
    _desktop_window.ns.keycodes[0x6E] = DESKTOP_WINDOW_KEY_MENU;
    _desktop_window.ns.keycodes[0x47] = DESKTOP_WINDOW_KEY_NUM_LOCK;
    _desktop_window.ns.keycodes[0x79] = DESKTOP_WINDOW_KEY_PAGE_DOWN;
    _desktop_window.ns.keycodes[0x74] = DESKTOP_WINDOW_KEY_PAGE_UP;
    _desktop_window.ns.keycodes[0x7C] = DESKTOP_WINDOW_KEY_RIGHT;
    _desktop_window.ns.keycodes[0x3D] = DESKTOP_WINDOW_KEY_RIGHT_ALT;
    _desktop_window.ns.keycodes[0x3E] = DESKTOP_WINDOW_KEY_RIGHT_CONTROL;
    _desktop_window.ns.keycodes[0x3C] = DESKTOP_WINDOW_KEY_RIGHT_SHIFT;
    _desktop_window.ns.keycodes[0x36] = DESKTOP_WINDOW_KEY_RIGHT_SUPER;
    _desktop_window.ns.keycodes[0x31] = DESKTOP_WINDOW_KEY_SPACE;
    _desktop_window.ns.keycodes[0x30] = DESKTOP_WINDOW_KEY_TAB;
    _desktop_window.ns.keycodes[0x7E] = DESKTOP_WINDOW_KEY_UP;

    _desktop_window.ns.keycodes[0x52] = DESKTOP_WINDOW_KEY_KP_0;
    _desktop_window.ns.keycodes[0x53] = DESKTOP_WINDOW_KEY_KP_1;
    _desktop_window.ns.keycodes[0x54] = DESKTOP_WINDOW_KEY_KP_2;
    _desktop_window.ns.keycodes[0x55] = DESKTOP_WINDOW_KEY_KP_3;
    _desktop_window.ns.keycodes[0x56] = DESKTOP_WINDOW_KEY_KP_4;
    _desktop_window.ns.keycodes[0x57] = DESKTOP_WINDOW_KEY_KP_5;
    _desktop_window.ns.keycodes[0x58] = DESKTOP_WINDOW_KEY_KP_6;
    _desktop_window.ns.keycodes[0x59] = DESKTOP_WINDOW_KEY_KP_7;
    _desktop_window.ns.keycodes[0x5B] = DESKTOP_WINDOW_KEY_KP_8;
    _desktop_window.ns.keycodes[0x5C] = DESKTOP_WINDOW_KEY_KP_9;
    _desktop_window.ns.keycodes[0x45] = DESKTOP_WINDOW_KEY_KP_ADD;
    _desktop_window.ns.keycodes[0x41] = DESKTOP_WINDOW_KEY_KP_DECIMAL;
    _desktop_window.ns.keycodes[0x4B] = DESKTOP_WINDOW_KEY_KP_DIVIDE;
    _desktop_window.ns.keycodes[0x4C] = DESKTOP_WINDOW_KEY_KP_ENTER;
    _desktop_window.ns.keycodes[0x51] = DESKTOP_WINDOW_KEY_KP_EQUAL;
    _desktop_window.ns.keycodes[0x43] = DESKTOP_WINDOW_KEY_KP_MULTIPLY;
    _desktop_window.ns.keycodes[0x4E] = DESKTOP_WINDOW_KEY_KP_SUBTRACT;

    for (int scancode = 0;  scancode < 256;  scancode++)
    {
        // Store the reverse translation for faster key name lookup
        if (_desktop_window.ns.keycodes[scancode] >= 0)
            _desktop_window.ns.scancodes[_desktop_window.ns.keycodes[scancode]] = scancode;
    }
}

// Retrieve Unicode data for the current keyboard layout
//
static DESKTOP_WINDOWbool updateUnicodeData(void)
{
    if (_desktop_window.ns.inputSource)
    {
        CFRelease(_desktop_window.ns.inputSource);
        _desktop_window.ns.inputSource = NULL;
        _desktop_window.ns.unicodeData = nil;
    }

    _desktop_window.ns.inputSource = TISCopyCurrentKeyboardLayoutInputSource();
    if (!_desktop_window.ns.inputSource)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to retrieve keyboard layout input source");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.ns.unicodeData =
        TISGetInputSourceProperty(_desktop_window.ns.inputSource,
                                  kTISPropertyUnicodeKeyLayoutData);
    if (!_desktop_window.ns.unicodeData)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to retrieve keyboard layout Unicode data");
        return DESKTOP_WINDOW_FALSE;
    }

    return DESKTOP_WINDOW_TRUE;
}

// Load HIToolbox.framework and the TIS symbols we need from it
//
static DESKTOP_WINDOWbool initializeTIS(void)
{
    // This works only because Cocoa has already loaded it properly
    _desktop_window.ns.tis.bundle =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.HIToolbox"));
    if (!_desktop_window.ns.tis.bundle)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to load HIToolbox.framework");
        return DESKTOP_WINDOW_FALSE;
    }

    CFStringRef* kPropertyUnicodeKeyLayoutData =
        CFBundleGetDataPointerForName(_desktop_window.ns.tis.bundle,
                                      CFSTR("kTISPropertyUnicodeKeyLayoutData"));
    _desktop_window.ns.tis.CopyCurrentKeyboardLayoutInputSource =
        CFBundleGetFunctionPointerForName(_desktop_window.ns.tis.bundle,
                                          CFSTR("TISCopyCurrentKeyboardLayoutInputSource"));
    _desktop_window.ns.tis.GetInputSourceProperty =
        CFBundleGetFunctionPointerForName(_desktop_window.ns.tis.bundle,
                                          CFSTR("TISGetInputSourceProperty"));
    _desktop_window.ns.tis.GetKbdType =
        CFBundleGetFunctionPointerForName(_desktop_window.ns.tis.bundle,
                                          CFSTR("LMGetKbdType"));

    if (!kPropertyUnicodeKeyLayoutData ||
        !TISCopyCurrentKeyboardLayoutInputSource ||
        !TISGetInputSourceProperty ||
        !LMGetKbdType)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to load TIS API symbols");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.ns.tis.kPropertyUnicodeKeyLayoutData =
        *kPropertyUnicodeKeyLayoutData;

    return updateUnicodeData();
}

@interface DESKTOP_WINDOWHelper : NSObject
@end

@implementation DESKTOP_WINDOWHelper

- (void)selectedKeyboardInputSourceChanged:(NSObject* )object
{
    updateUnicodeData();
}

- (void)doNothing:(id)object
{
}

@end // DESKTOP_WINDOWHelper

@interface DESKTOP_WINDOWApplicationDelegate : NSObject <NSApplicationDelegate>
@end

@implementation DESKTOP_WINDOWApplicationDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    for (_DESKTOP_WINDOWwindow* window = _desktop_window.windowListHead;  window;  window = window->next)
        _desktop_windowInputWindowCloseRequest(window);

    return NSTerminateCancel;
}

- (void)applicationDidChangeScreenParameters:(NSNotification *) notification
{
    for (_DESKTOP_WINDOWwindow* window = _desktop_window.windowListHead;  window;  window = window->next)
    {
        if (window->context.client != DESKTOP_WINDOW_NO_API)
            [window->context.nsgl.object update];
    }

    _desktop_windowPollMonitorsCocoa();
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    if (_desktop_window.hints.init.ns.menubar)
    {
        // Menu bar setup must go between sharedApplication and finishLaunching
        // in order to properly emulate the behavior of NSApplicationMain

        if ([[NSBundle mainBundle] pathForResource:@"MainMenu" ofType:@"nib"])
        {
            [[NSBundle mainBundle] loadNibNamed:@"MainMenu"
                                          owner:NSApp
                                topLevelObjects:&_desktop_window.ns.nibObjects];
        }
        else
            createMenuBar();
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    _desktop_windowPostEmptyEventCocoa();
    [NSApp stop:nil];
}

- (void)applicationDidHide:(NSNotification *)notification
{
    for (int i = 0;  i < _desktop_window.monitorCount;  i++)
        _desktop_windowRestoreVideoModeCocoa(_desktop_window.monitors[i]);
}

@end // DESKTOP_WINDOWApplicationDelegate


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

void* _desktop_windowLoadLocalVulkanLoaderCocoa(void)
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle)
        return NULL;

    CFURLRef frameworksUrl = CFBundleCopyPrivateFrameworksURL(bundle);
    if (!frameworksUrl)
        return NULL;

    CFURLRef loaderUrl = CFURLCreateCopyAppendingPathComponent(
        kCFAllocatorDefault, frameworksUrl, CFSTR("libvulkan.1.dylib"), false);
    if (!loaderUrl)
    {
        CFRelease(frameworksUrl);
        return NULL;
    }

    char path[PATH_MAX];
    void* handle = NULL;

    if (CFURLGetFileSystemRepresentation(loaderUrl, true, (UInt8*) path, sizeof(path) - 1))
        handle = _desktop_windowPlatformLoadModule(path);

    CFRelease(loaderUrl);
    CFRelease(frameworksUrl);
    return handle;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowConnectCocoa(int platformID, _DESKTOP_WINDOWplatform* platform)
{
    const _DESKTOP_WINDOWplatform cocoa =
    {
        .platformID = DESKTOP_WINDOW_PLATFORM_COCOA,
        .init = _desktop_windowInitCocoa,
        .terminate = _desktop_windowTerminateCocoa,
        .getCursorPos = _desktop_windowGetCursorPosCocoa,
        .setCursorPos = _desktop_windowSetCursorPosCocoa,
        .setCursorMode = _desktop_windowSetCursorModeCocoa,
        .setRawMouseMotion = _desktop_windowSetRawMouseMotionCocoa,
        .rawMouseMotionSupported = _desktop_windowRawMouseMotionSupportedCocoa,
        .createCursor = _desktop_windowCreateCursorCocoa,
        .createStandardCursor = _desktop_windowCreateStandardCursorCocoa,
        .destroyCursor = _desktop_windowDestroyCursorCocoa,
        .setCursor = _desktop_windowSetCursorCocoa,
        .getScancodeName = _desktop_windowGetScancodeNameCocoa,
        .getKeyScancode = _desktop_windowGetKeyScancodeCocoa,
        .setClipboardString = _desktop_windowSetClipboardStringCocoa,
        .getClipboardString = _desktop_windowGetClipboardStringCocoa,
        .initJoysticks = _desktop_windowInitJoysticksCocoa,
        .terminateJoysticks = _desktop_windowTerminateJoysticksCocoa,
        .pollJoystick = _desktop_windowPollJoystickCocoa,
        .getMappingName = _desktop_windowGetMappingNameCocoa,
        .updateGamepadGUID = _desktop_windowUpdateGamepadGUIDCocoa,
        .freeMonitor = _desktop_windowFreeMonitorCocoa,
        .getMonitorPos = _desktop_windowGetMonitorPosCocoa,
        .getMonitorContentScale = _desktop_windowGetMonitorContentScaleCocoa,
        .getMonitorWorkarea = _desktop_windowGetMonitorWorkareaCocoa,
        .getVideoModes = _desktop_windowGetVideoModesCocoa,
        .getVideoMode = _desktop_windowGetVideoModeCocoa,
        .getGammaRamp = _desktop_windowGetGammaRampCocoa,
        .setGammaRamp = _desktop_windowSetGammaRampCocoa,
        .createWindow = _desktop_windowCreateWindowCocoa,
        .destroyWindow = _desktop_windowDestroyWindowCocoa,
        .setWindowTitle = _desktop_windowSetWindowTitleCocoa,
        .setWindowIcon = _desktop_windowSetWindowIconCocoa,
        .getWindowPos = _desktop_windowGetWindowPosCocoa,
        .setWindowPos = _desktop_windowSetWindowPosCocoa,
        .getWindowSize = _desktop_windowGetWindowSizeCocoa,
        .setWindowSize = _desktop_windowSetWindowSizeCocoa,
        .setWindowSizeLimits = _desktop_windowSetWindowSizeLimitsCocoa,
        .setWindowAspectRatio = _desktop_windowSetWindowAspectRatioCocoa,
        .getFramebufferSize = _desktop_windowGetFramebufferSizeCocoa,
        .getWindowFrameSize = _desktop_windowGetWindowFrameSizeCocoa,
        .getWindowContentScale = _desktop_windowGetWindowContentScaleCocoa,
        .iconifyWindow = _desktop_windowIconifyWindowCocoa,
        .restoreWindow = _desktop_windowRestoreWindowCocoa,
        .maximizeWindow = _desktop_windowMaximizeWindowCocoa,
        .showWindow = _desktop_windowShowWindowCocoa,
        .hideWindow = _desktop_windowHideWindowCocoa,
        .requestWindowAttention = _desktop_windowRequestWindowAttentionCocoa,
        .focusWindow = _desktop_windowFocusWindowCocoa,
        .setWindowMonitor = _desktop_windowSetWindowMonitorCocoa,
        .windowFocused = _desktop_windowWindowFocusedCocoa,
        .windowIconified = _desktop_windowWindowIconifiedCocoa,
        .windowVisible = _desktop_windowWindowVisibleCocoa,
        .windowMaximized = _desktop_windowWindowMaximizedCocoa,
        .windowHovered = _desktop_windowWindowHoveredCocoa,
        .framebufferTransparent = _desktop_windowFramebufferTransparentCocoa,
        .getWindowOpacity = _desktop_windowGetWindowOpacityCocoa,
        .setWindowResizable = _desktop_windowSetWindowResizableCocoa,
        .setWindowDecorated = _desktop_windowSetWindowDecoratedCocoa,
        .setWindowFloating = _desktop_windowSetWindowFloatingCocoa,
        .setWindowOpacity = _desktop_windowSetWindowOpacityCocoa,
        .setWindowMousePassthrough = _desktop_windowSetWindowMousePassthroughCocoa,
        .pollEvents = _desktop_windowPollEventsCocoa,
        .waitEvents = _desktop_windowWaitEventsCocoa,
        .waitEventsTimeout = _desktop_windowWaitEventsTimeoutCocoa,
        .postEmptyEvent = _desktop_windowPostEmptyEventCocoa,
        .getEGLPlatform = _desktop_windowGetEGLPlatformCocoa,
        .getEGLNativeDisplay = _desktop_windowGetEGLNativeDisplayCocoa,
        .getEGLNativeWindow = _desktop_windowGetEGLNativeWindowCocoa,
        .getRequiredInstanceExtensions = _desktop_windowGetRequiredInstanceExtensionsCocoa,
        .getPhysicalDevicePresentationSupport = _desktop_windowGetPhysicalDevicePresentationSupportCocoa,
        .createWindowSurface = _desktop_windowCreateWindowSurfaceCocoa
    };

    *platform = cocoa;
    return DESKTOP_WINDOW_TRUE;
}

int _desktop_windowInitCocoa(void)
{
    @autoreleasepool {

    _desktop_window.ns.helper = [[DESKTOP_WINDOWHelper alloc] init];

    [NSThread detachNewThreadSelector:@selector(doNothing:)
                             toTarget:_desktop_window.ns.helper
                           withObject:nil];

    [NSApplication sharedApplication];

    _desktop_window.ns.delegate = [[DESKTOP_WINDOWApplicationDelegate alloc] init];
    if (_desktop_window.ns.delegate == nil)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Cocoa: Failed to create application delegate");
        return DESKTOP_WINDOW_FALSE;
    }

    [NSApp setDelegate:_desktop_window.ns.delegate];

    NSEvent* (^block)(NSEvent*) = ^ NSEvent* (NSEvent* event)
    {
        if ([event modifierFlags] & NSEventModifierFlagCommand)
            [[NSApp keyWindow] sendEvent:event];

        return event;
    };

    _desktop_window.ns.keyUpMonitor =
        [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp
                                              handler:block];

    if (_desktop_window.hints.init.ns.chdir)
        changeToResourcesDirectory();

    // Press and Hold prevents some keys from emitting repeated characters
    NSDictionary* defaults = @{@"ApplePressAndHoldEnabled":@NO};
    [[NSUserDefaults standardUserDefaults] registerDefaults:defaults];

    [[NSNotificationCenter defaultCenter]
        addObserver:_desktop_window.ns.helper
           selector:@selector(selectedKeyboardInputSourceChanged:)
               name:NSTextInputContextKeyboardSelectionDidChangeNotification
             object:nil];

    createKeyTables();

    _desktop_window.ns.eventSource = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!_desktop_window.ns.eventSource)
        return DESKTOP_WINDOW_FALSE;

    CGEventSourceSetLocalEventsSuppressionInterval(_desktop_window.ns.eventSource, 0.0);

    if (!initializeTIS())
        return DESKTOP_WINDOW_FALSE;

    _desktop_windowPollMonitorsCocoa();

    if (![[NSRunningApplication currentApplication] isFinishedLaunching])
        [NSApp run];

    // In case we are unbundled, make us a proper UI application
    if (_desktop_window.hints.init.ns.menubar)
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    return DESKTOP_WINDOW_TRUE;

    } // autoreleasepool
}

void _desktop_windowTerminateCocoa(void)
{
    @autoreleasepool {

    if (_desktop_window.ns.inputSource)
    {
        CFRelease(_desktop_window.ns.inputSource);
        _desktop_window.ns.inputSource = NULL;
        _desktop_window.ns.unicodeData = nil;
    }

    if (_desktop_window.ns.eventSource)
    {
        CFRelease(_desktop_window.ns.eventSource);
        _desktop_window.ns.eventSource = NULL;
    }

    if (_desktop_window.ns.delegate)
    {
        [NSApp setDelegate:nil];
        [_desktop_window.ns.delegate release];
        _desktop_window.ns.delegate = nil;
    }

    if (_desktop_window.ns.helper)
    {
        [[NSNotificationCenter defaultCenter]
            removeObserver:_desktop_window.ns.helper
                      name:NSTextInputContextKeyboardSelectionDidChangeNotification
                    object:nil];
        [[NSNotificationCenter defaultCenter]
            removeObserver:_desktop_window.ns.helper];
        [_desktop_window.ns.helper release];
        _desktop_window.ns.helper = nil;
    }

    if (_desktop_window.ns.keyUpMonitor)
        [NSEvent removeMonitor:_desktop_window.ns.keyUpMonitor];

    _desktop_window_free(_desktop_window.ns.clipboardString);

    _desktop_windowTerminateNSGL();
    _desktop_windowTerminateEGL();
    _desktop_windowTerminateOSMesa();

    } // autoreleasepool
}

#endif // _DESKTOP_WINDOW_COCOA

