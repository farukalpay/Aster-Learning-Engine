#include "internal.h"

#if defined(_DESKTOP_WINDOW_WIN32)

#include <stdlib.h>

static const GUID _desktop_window_GUID_DEVINTERFACE_HID =
    {0x4d1e55b2,0xf16f,0x11cf,{0x88,0xcb,0x00,0x11,0x11,0x00,0x00,0x30}};

#define GUID_DEVINTERFACE_HID _desktop_window_GUID_DEVINTERFACE_HID

#if defined(_DESKTOP_WINDOW_USE_HYBRID_HPG) || defined(_DESKTOP_WINDOW_USE_OPTIMUS_HPG)

#if defined(_DESKTOP_WINDOW_BUILD_DLL)
 #pragma message("These symbols must be exported by the executable and have no effect in a DLL")
#endif

// Executables (but not DLLs) exporting this symbol with this value will be
// automatically directed to the high-performance GPU on Nvidia Optimus systems
// with up-to-date drivers
//
__declspec(dllexport) DWORD NvOptimusEnablement = 1;

// Executables (but not DLLs) exporting this symbol with this value will be
// automatically directed to the high-performance GPU on AMD PowerXpress systems
// with up-to-date drivers
//
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

#endif // _DESKTOP_WINDOW_USE_HYBRID_HPG

#if defined(_DESKTOP_WINDOW_BUILD_DLL)

// DESKTOP_WINDOW DLL entry point
//
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    return TRUE;
}

#endif // _DESKTOP_WINDOW_BUILD_DLL

// Load necessary libraries (DLLs)
//
static DESKTOP_WINDOWbool loadLibraries(void)
{
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (const WCHAR*) &_desktop_window,
                            (HMODULE*) &_desktop_window.win32.instance))
    {
        _desktop_windowInputErrorWin32(DESKTOP_WINDOW_PLATFORM_ERROR,
                             "Win32: Failed to retrieve own module handle");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.win32.user32.instance = _desktop_windowPlatformLoadModule("user32.dll");
    if (!_desktop_window.win32.user32.instance)
    {
        _desktop_windowInputErrorWin32(DESKTOP_WINDOW_PLATFORM_ERROR,
                             "Win32: Failed to load user32.dll");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.win32.user32.SetProcessDPIAware_ = (PFN_SetProcessDPIAware)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.user32.instance, "SetProcessDPIAware");
    _desktop_window.win32.user32.ChangeWindowMessageFilterEx_ = (PFN_ChangeWindowMessageFilterEx)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.user32.instance, "ChangeWindowMessageFilterEx");
    _desktop_window.win32.user32.EnableNonClientDpiScaling_ = (PFN_EnableNonClientDpiScaling)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.user32.instance, "EnableNonClientDpiScaling");
    _desktop_window.win32.user32.SetProcessDpiAwarenessContext_ = (PFN_SetProcessDpiAwarenessContext)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.user32.instance, "SetProcessDpiAwarenessContext");
    _desktop_window.win32.user32.GetDpiForWindow_ = (PFN_GetDpiForWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.user32.instance, "GetDpiForWindow");
    _desktop_window.win32.user32.AdjustWindowRectExForDpi_ = (PFN_AdjustWindowRectExForDpi)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.user32.instance, "AdjustWindowRectExForDpi");
    _desktop_window.win32.user32.GetSystemMetricsForDpi_ = (PFN_GetSystemMetricsForDpi)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.user32.instance, "GetSystemMetricsForDpi");

    _desktop_window.win32.dinput8.instance = _desktop_windowPlatformLoadModule("dinput8.dll");
    if (_desktop_window.win32.dinput8.instance)
    {
        _desktop_window.win32.dinput8.Create = (PFN_DirectInput8Create)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.dinput8.instance, "DirectInput8Create");
    }

    {
        int i;
        const char* names[] =
        {
            "xinput1_4.dll",
            "xinput1_3.dll",
            "xinput9_1_0.dll",
            "xinput1_2.dll",
            "xinput1_1.dll",
            NULL
        };

        for (i = 0;  names[i];  i++)
        {
            _desktop_window.win32.xinput.instance = _desktop_windowPlatformLoadModule(names[i]);
            if (_desktop_window.win32.xinput.instance)
            {
                _desktop_window.win32.xinput.GetCapabilities = (PFN_XInputGetCapabilities)
                    _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.xinput.instance, "XInputGetCapabilities");
                _desktop_window.win32.xinput.GetState = (PFN_XInputGetState)
                    _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.xinput.instance, "XInputGetState");

                break;
            }
        }
    }

    _desktop_window.win32.dwmapi.instance = _desktop_windowPlatformLoadModule("dwmapi.dll");
    if (_desktop_window.win32.dwmapi.instance)
    {
        _desktop_window.win32.dwmapi.IsCompositionEnabled = (PFN_DwmIsCompositionEnabled)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.dwmapi.instance, "DwmIsCompositionEnabled");
        _desktop_window.win32.dwmapi.Flush = (PFN_DwmFlush)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.dwmapi.instance, "DwmFlush");
        _desktop_window.win32.dwmapi.EnableBlurBehindWindow = (PFN_DwmEnableBlurBehindWindow)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.dwmapi.instance, "DwmEnableBlurBehindWindow");
        _desktop_window.win32.dwmapi.GetColorizationColor = (PFN_DwmGetColorizationColor)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.dwmapi.instance, "DwmGetColorizationColor");
    }

    _desktop_window.win32.shcore.instance = _desktop_windowPlatformLoadModule("shcore.dll");
    if (_desktop_window.win32.shcore.instance)
    {
        _desktop_window.win32.shcore.SetProcessDpiAwareness_ = (PFN_SetProcessDpiAwareness)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.shcore.instance, "SetProcessDpiAwareness");
        _desktop_window.win32.shcore.GetDpiForMonitor_ = (PFN_GetDpiForMonitor)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.shcore.instance, "GetDpiForMonitor");
    }

    _desktop_window.win32.ntdll.instance = _desktop_windowPlatformLoadModule("ntdll.dll");
    if (_desktop_window.win32.ntdll.instance)
    {
        _desktop_window.win32.ntdll.RtlVerifyVersionInfo_ = (PFN_RtlVerifyVersionInfo)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.win32.ntdll.instance, "RtlVerifyVersionInfo");
    }

    return DESKTOP_WINDOW_TRUE;
}

// Unload used libraries (DLLs)
//
static void freeLibraries(void)
{
    if (_desktop_window.win32.xinput.instance)
        _desktop_windowPlatformFreeModule(_desktop_window.win32.xinput.instance);

    if (_desktop_window.win32.dinput8.instance)
        _desktop_windowPlatformFreeModule(_desktop_window.win32.dinput8.instance);

    if (_desktop_window.win32.user32.instance)
        _desktop_windowPlatformFreeModule(_desktop_window.win32.user32.instance);

    if (_desktop_window.win32.dwmapi.instance)
        _desktop_windowPlatformFreeModule(_desktop_window.win32.dwmapi.instance);

    if (_desktop_window.win32.shcore.instance)
        _desktop_windowPlatformFreeModule(_desktop_window.win32.shcore.instance);

    if (_desktop_window.win32.ntdll.instance)
        _desktop_windowPlatformFreeModule(_desktop_window.win32.ntdll.instance);
}

// Create key code translation tables
//
static void createKeyTables(void)
{
    int scancode;

    memset(_desktop_window.win32.keycodes, -1, sizeof(_desktop_window.win32.keycodes));
    memset(_desktop_window.win32.scancodes, -1, sizeof(_desktop_window.win32.scancodes));

    _desktop_window.win32.keycodes[0x00B] = DESKTOP_WINDOW_KEY_0;
    _desktop_window.win32.keycodes[0x002] = DESKTOP_WINDOW_KEY_1;
    _desktop_window.win32.keycodes[0x003] = DESKTOP_WINDOW_KEY_2;
    _desktop_window.win32.keycodes[0x004] = DESKTOP_WINDOW_KEY_3;
    _desktop_window.win32.keycodes[0x005] = DESKTOP_WINDOW_KEY_4;
    _desktop_window.win32.keycodes[0x006] = DESKTOP_WINDOW_KEY_5;
    _desktop_window.win32.keycodes[0x007] = DESKTOP_WINDOW_KEY_6;
    _desktop_window.win32.keycodes[0x008] = DESKTOP_WINDOW_KEY_7;
    _desktop_window.win32.keycodes[0x009] = DESKTOP_WINDOW_KEY_8;
    _desktop_window.win32.keycodes[0x00A] = DESKTOP_WINDOW_KEY_9;
    _desktop_window.win32.keycodes[0x01E] = DESKTOP_WINDOW_KEY_A;
    _desktop_window.win32.keycodes[0x030] = DESKTOP_WINDOW_KEY_B;
    _desktop_window.win32.keycodes[0x02E] = DESKTOP_WINDOW_KEY_C;
    _desktop_window.win32.keycodes[0x020] = DESKTOP_WINDOW_KEY_D;
    _desktop_window.win32.keycodes[0x012] = DESKTOP_WINDOW_KEY_E;
    _desktop_window.win32.keycodes[0x021] = DESKTOP_WINDOW_KEY_F;
    _desktop_window.win32.keycodes[0x022] = DESKTOP_WINDOW_KEY_G;
    _desktop_window.win32.keycodes[0x023] = DESKTOP_WINDOW_KEY_H;
    _desktop_window.win32.keycodes[0x017] = DESKTOP_WINDOW_KEY_I;
    _desktop_window.win32.keycodes[0x024] = DESKTOP_WINDOW_KEY_J;
    _desktop_window.win32.keycodes[0x025] = DESKTOP_WINDOW_KEY_K;
    _desktop_window.win32.keycodes[0x026] = DESKTOP_WINDOW_KEY_L;
    _desktop_window.win32.keycodes[0x032] = DESKTOP_WINDOW_KEY_M;
    _desktop_window.win32.keycodes[0x031] = DESKTOP_WINDOW_KEY_N;
    _desktop_window.win32.keycodes[0x018] = DESKTOP_WINDOW_KEY_O;
    _desktop_window.win32.keycodes[0x019] = DESKTOP_WINDOW_KEY_P;
    _desktop_window.win32.keycodes[0x010] = DESKTOP_WINDOW_KEY_Q;
    _desktop_window.win32.keycodes[0x013] = DESKTOP_WINDOW_KEY_R;
    _desktop_window.win32.keycodes[0x01F] = DESKTOP_WINDOW_KEY_S;
    _desktop_window.win32.keycodes[0x014] = DESKTOP_WINDOW_KEY_T;
    _desktop_window.win32.keycodes[0x016] = DESKTOP_WINDOW_KEY_U;
    _desktop_window.win32.keycodes[0x02F] = DESKTOP_WINDOW_KEY_V;
    _desktop_window.win32.keycodes[0x011] = DESKTOP_WINDOW_KEY_W;
    _desktop_window.win32.keycodes[0x02D] = DESKTOP_WINDOW_KEY_X;
    _desktop_window.win32.keycodes[0x015] = DESKTOP_WINDOW_KEY_Y;
    _desktop_window.win32.keycodes[0x02C] = DESKTOP_WINDOW_KEY_Z;

    _desktop_window.win32.keycodes[0x028] = DESKTOP_WINDOW_KEY_APOSTROPHE;
    _desktop_window.win32.keycodes[0x02B] = DESKTOP_WINDOW_KEY_BACKSLASH;
    _desktop_window.win32.keycodes[0x033] = DESKTOP_WINDOW_KEY_COMMA;
    _desktop_window.win32.keycodes[0x00D] = DESKTOP_WINDOW_KEY_EQUAL;
    _desktop_window.win32.keycodes[0x029] = DESKTOP_WINDOW_KEY_GRAVE_ACCENT;
    _desktop_window.win32.keycodes[0x01A] = DESKTOP_WINDOW_KEY_LEFT_BRACKET;
    _desktop_window.win32.keycodes[0x00C] = DESKTOP_WINDOW_KEY_MINUS;
    _desktop_window.win32.keycodes[0x034] = DESKTOP_WINDOW_KEY_PERIOD;
    _desktop_window.win32.keycodes[0x01B] = DESKTOP_WINDOW_KEY_RIGHT_BRACKET;
    _desktop_window.win32.keycodes[0x027] = DESKTOP_WINDOW_KEY_SEMICOLON;
    _desktop_window.win32.keycodes[0x035] = DESKTOP_WINDOW_KEY_SLASH;
    _desktop_window.win32.keycodes[0x056] = DESKTOP_WINDOW_KEY_WORLD_2;

    _desktop_window.win32.keycodes[0x00E] = DESKTOP_WINDOW_KEY_BACKSPACE;
    _desktop_window.win32.keycodes[0x153] = DESKTOP_WINDOW_KEY_DELETE;
    _desktop_window.win32.keycodes[0x14F] = DESKTOP_WINDOW_KEY_END;
    _desktop_window.win32.keycodes[0x01C] = DESKTOP_WINDOW_KEY_ENTER;
    _desktop_window.win32.keycodes[0x001] = DESKTOP_WINDOW_KEY_ESCAPE;
    _desktop_window.win32.keycodes[0x147] = DESKTOP_WINDOW_KEY_HOME;
    _desktop_window.win32.keycodes[0x152] = DESKTOP_WINDOW_KEY_INSERT;
    _desktop_window.win32.keycodes[0x15D] = DESKTOP_WINDOW_KEY_MENU;
    _desktop_window.win32.keycodes[0x151] = DESKTOP_WINDOW_KEY_PAGE_DOWN;
    _desktop_window.win32.keycodes[0x149] = DESKTOP_WINDOW_KEY_PAGE_UP;
    _desktop_window.win32.keycodes[0x045] = DESKTOP_WINDOW_KEY_PAUSE;
    _desktop_window.win32.keycodes[0x039] = DESKTOP_WINDOW_KEY_SPACE;
    _desktop_window.win32.keycodes[0x00F] = DESKTOP_WINDOW_KEY_TAB;
    _desktop_window.win32.keycodes[0x03A] = DESKTOP_WINDOW_KEY_CAPS_LOCK;
    _desktop_window.win32.keycodes[0x145] = DESKTOP_WINDOW_KEY_NUM_LOCK;
    _desktop_window.win32.keycodes[0x046] = DESKTOP_WINDOW_KEY_SCROLL_LOCK;
    _desktop_window.win32.keycodes[0x03B] = DESKTOP_WINDOW_KEY_F1;
    _desktop_window.win32.keycodes[0x03C] = DESKTOP_WINDOW_KEY_F2;
    _desktop_window.win32.keycodes[0x03D] = DESKTOP_WINDOW_KEY_F3;
    _desktop_window.win32.keycodes[0x03E] = DESKTOP_WINDOW_KEY_F4;
    _desktop_window.win32.keycodes[0x03F] = DESKTOP_WINDOW_KEY_F5;
    _desktop_window.win32.keycodes[0x040] = DESKTOP_WINDOW_KEY_F6;
    _desktop_window.win32.keycodes[0x041] = DESKTOP_WINDOW_KEY_F7;
    _desktop_window.win32.keycodes[0x042] = DESKTOP_WINDOW_KEY_F8;
    _desktop_window.win32.keycodes[0x043] = DESKTOP_WINDOW_KEY_F9;
    _desktop_window.win32.keycodes[0x044] = DESKTOP_WINDOW_KEY_F10;
    _desktop_window.win32.keycodes[0x057] = DESKTOP_WINDOW_KEY_F11;
    _desktop_window.win32.keycodes[0x058] = DESKTOP_WINDOW_KEY_F12;
    _desktop_window.win32.keycodes[0x064] = DESKTOP_WINDOW_KEY_F13;
    _desktop_window.win32.keycodes[0x065] = DESKTOP_WINDOW_KEY_F14;
    _desktop_window.win32.keycodes[0x066] = DESKTOP_WINDOW_KEY_F15;
    _desktop_window.win32.keycodes[0x067] = DESKTOP_WINDOW_KEY_F16;
    _desktop_window.win32.keycodes[0x068] = DESKTOP_WINDOW_KEY_F17;
    _desktop_window.win32.keycodes[0x069] = DESKTOP_WINDOW_KEY_F18;
    _desktop_window.win32.keycodes[0x06A] = DESKTOP_WINDOW_KEY_F19;
    _desktop_window.win32.keycodes[0x06B] = DESKTOP_WINDOW_KEY_F20;
    _desktop_window.win32.keycodes[0x06C] = DESKTOP_WINDOW_KEY_F21;
    _desktop_window.win32.keycodes[0x06D] = DESKTOP_WINDOW_KEY_F22;
    _desktop_window.win32.keycodes[0x06E] = DESKTOP_WINDOW_KEY_F23;
    _desktop_window.win32.keycodes[0x076] = DESKTOP_WINDOW_KEY_F24;
    _desktop_window.win32.keycodes[0x038] = DESKTOP_WINDOW_KEY_LEFT_ALT;
    _desktop_window.win32.keycodes[0x01D] = DESKTOP_WINDOW_KEY_LEFT_CONTROL;
    _desktop_window.win32.keycodes[0x02A] = DESKTOP_WINDOW_KEY_LEFT_SHIFT;
    _desktop_window.win32.keycodes[0x15B] = DESKTOP_WINDOW_KEY_LEFT_SUPER;
    _desktop_window.win32.keycodes[0x137] = DESKTOP_WINDOW_KEY_PRINT_SCREEN;
    _desktop_window.win32.keycodes[0x138] = DESKTOP_WINDOW_KEY_RIGHT_ALT;
    _desktop_window.win32.keycodes[0x11D] = DESKTOP_WINDOW_KEY_RIGHT_CONTROL;
    _desktop_window.win32.keycodes[0x036] = DESKTOP_WINDOW_KEY_RIGHT_SHIFT;
    _desktop_window.win32.keycodes[0x15C] = DESKTOP_WINDOW_KEY_RIGHT_SUPER;
    _desktop_window.win32.keycodes[0x150] = DESKTOP_WINDOW_KEY_DOWN;
    _desktop_window.win32.keycodes[0x14B] = DESKTOP_WINDOW_KEY_LEFT;
    _desktop_window.win32.keycodes[0x14D] = DESKTOP_WINDOW_KEY_RIGHT;
    _desktop_window.win32.keycodes[0x148] = DESKTOP_WINDOW_KEY_UP;

    _desktop_window.win32.keycodes[0x052] = DESKTOP_WINDOW_KEY_KP_0;
    _desktop_window.win32.keycodes[0x04F] = DESKTOP_WINDOW_KEY_KP_1;
    _desktop_window.win32.keycodes[0x050] = DESKTOP_WINDOW_KEY_KP_2;
    _desktop_window.win32.keycodes[0x051] = DESKTOP_WINDOW_KEY_KP_3;
    _desktop_window.win32.keycodes[0x04B] = DESKTOP_WINDOW_KEY_KP_4;
    _desktop_window.win32.keycodes[0x04C] = DESKTOP_WINDOW_KEY_KP_5;
    _desktop_window.win32.keycodes[0x04D] = DESKTOP_WINDOW_KEY_KP_6;
    _desktop_window.win32.keycodes[0x047] = DESKTOP_WINDOW_KEY_KP_7;
    _desktop_window.win32.keycodes[0x048] = DESKTOP_WINDOW_KEY_KP_8;
    _desktop_window.win32.keycodes[0x049] = DESKTOP_WINDOW_KEY_KP_9;
    _desktop_window.win32.keycodes[0x04E] = DESKTOP_WINDOW_KEY_KP_ADD;
    _desktop_window.win32.keycodes[0x053] = DESKTOP_WINDOW_KEY_KP_DECIMAL;
    _desktop_window.win32.keycodes[0x135] = DESKTOP_WINDOW_KEY_KP_DIVIDE;
    _desktop_window.win32.keycodes[0x11C] = DESKTOP_WINDOW_KEY_KP_ENTER;
    _desktop_window.win32.keycodes[0x059] = DESKTOP_WINDOW_KEY_KP_EQUAL;
    _desktop_window.win32.keycodes[0x037] = DESKTOP_WINDOW_KEY_KP_MULTIPLY;
    _desktop_window.win32.keycodes[0x04A] = DESKTOP_WINDOW_KEY_KP_SUBTRACT;

    for (scancode = 0;  scancode < 512;  scancode++)
    {
        if (_desktop_window.win32.keycodes[scancode] > 0)
            _desktop_window.win32.scancodes[_desktop_window.win32.keycodes[scancode]] = scancode;
    }
}

// Window procedure for the hidden helper window
//
static LRESULT CALLBACK helperWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DISPLAYCHANGE:
            _desktop_windowPollMonitorsWin32();
            break;

        case WM_DEVICECHANGE:
        {
            if (!_desktop_window.joysticksInitialized)
                break;

            if (wParam == DBT_DEVICEARRIVAL)
            {
                DEV_BROADCAST_HDR* dbh = (DEV_BROADCAST_HDR*) lParam;
                if (dbh && dbh->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                    _desktop_windowDetectJoystickConnectionWin32();
            }
            else if (wParam == DBT_DEVICEREMOVECOMPLETE)
            {
                DEV_BROADCAST_HDR* dbh = (DEV_BROADCAST_HDR*) lParam;
                if (dbh && dbh->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                    _desktop_windowDetectJoystickDisconnectionWin32();
            }

            break;
        }
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// Creates a dummy window for behind-the-scenes work
//
static DESKTOP_WINDOWbool createHelperWindow(void)
{
    MSG msg;
    WNDCLASSEXW wc = { sizeof(wc) };

    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = (WNDPROC) helperWindowProc;
    wc.hInstance     = _desktop_window.win32.instance;
    wc.lpszClassName = L"DESKTOP_WINDOW3 Helper";

    _desktop_window.win32.helperWindowClass = RegisterClassExW(&wc);
    if (!_desktop_window.win32.helperWindowClass)
    {
        _desktop_windowInputErrorWin32(DESKTOP_WINDOW_PLATFORM_ERROR,
                             "Win32: Failed to register helper window class");
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.win32.helperWindowHandle =
        CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
                        MAKEINTATOM(_desktop_window.win32.helperWindowClass),
                        L"DESKTOP_WINDOW message window",
                        WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                        0, 0, 1, 1,
                        NULL, NULL,
                        _desktop_window.win32.instance,
                        NULL);

    if (!_desktop_window.win32.helperWindowHandle)
    {
        _desktop_windowInputErrorWin32(DESKTOP_WINDOW_PLATFORM_ERROR,
                             "Win32: Failed to create helper window");
        return DESKTOP_WINDOW_FALSE;
    }

    // HACK: The command to the first ShowWindow call is ignored if the parent
    //       process passed along a STARTUPINFO, so clear that with a no-op call
    ShowWindow(_desktop_window.win32.helperWindowHandle, SW_HIDE);

    // Register for HID device notifications
    {
        DEV_BROADCAST_DEVICEINTERFACE_W dbi;
        ZeroMemory(&dbi, sizeof(dbi));
        dbi.dbcc_size = sizeof(dbi);
        dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        dbi.dbcc_classguid = GUID_DEVINTERFACE_HID;

        _desktop_window.win32.deviceNotificationHandle =
            RegisterDeviceNotificationW(_desktop_window.win32.helperWindowHandle,
                                        (DEV_BROADCAST_HDR*) &dbi,
                                        DEVICE_NOTIFY_WINDOW_HANDLE);
    }

    while (PeekMessageW(&msg, _desktop_window.win32.helperWindowHandle, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

   return DESKTOP_WINDOW_TRUE;
}

//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Returns a wide string version of the specified UTF-8 string
//
WCHAR* _desktop_windowCreateWideStringFromUTF8Win32(const char* source)
{
    WCHAR* target;
    int count;

    count = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
    if (!count)
    {
        _desktop_windowInputErrorWin32(DESKTOP_WINDOW_PLATFORM_ERROR,
                             "Win32: Failed to convert string from UTF-8");
        return NULL;
    }

    target = _desktop_window_calloc(count, sizeof(WCHAR));

    if (!MultiByteToWideChar(CP_UTF8, 0, source, -1, target, count))
    {
        _desktop_windowInputErrorWin32(DESKTOP_WINDOW_PLATFORM_ERROR,
                             "Win32: Failed to convert string from UTF-8");
        _desktop_window_free(target);
        return NULL;
    }

    return target;
}

// Returns a UTF-8 string version of the specified wide string
//
char* _desktop_windowCreateUTF8FromWideStringWin32(const WCHAR* source)
{
    char* target;
    int size;

    size = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (!size)
    {
        _desktop_windowInputErrorWin32(DESKTOP_WINDOW_PLATFORM_ERROR,
                             "Win32: Failed to convert string to UTF-8");
        return NULL;
    }

    target = _desktop_window_calloc(size, 1);

    if (!WideCharToMultiByte(CP_UTF8, 0, source, -1, target, size, NULL, NULL))
    {
        _desktop_windowInputErrorWin32(DESKTOP_WINDOW_PLATFORM_ERROR,
                             "Win32: Failed to convert string to UTF-8");
        _desktop_window_free(target);
        return NULL;
    }

    return target;
}

// Reports the specified error, appending information about the last Win32 error
//
void _desktop_windowInputErrorWin32(int error, const char* description)
{
    WCHAR buffer[_DESKTOP_WINDOW_MESSAGE_SIZE] = L"";
    char message[_DESKTOP_WINDOW_MESSAGE_SIZE] = "";

    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS |
                       FORMAT_MESSAGE_MAX_WIDTH_MASK,
                   NULL,
                   GetLastError() & 0xffff,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buffer,
                   sizeof(buffer) / sizeof(WCHAR),
                   NULL);
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, message, sizeof(message), NULL, NULL);

    _desktop_windowInputError(error, "%s: %s", description, message);
}

// Updates key names according to the current keyboard layout
//
void _desktop_windowUpdateKeyNamesWin32(void)
{
    int key;
    BYTE state[256] = {0};

    memset(_desktop_window.win32.keynames, 0, sizeof(_desktop_window.win32.keynames));

    for (key = DESKTOP_WINDOW_KEY_SPACE;  key <= DESKTOP_WINDOW_KEY_LAST;  key++)
    {
        UINT vk;
        int scancode, length;
        WCHAR chars[16];

        scancode = _desktop_window.win32.scancodes[key];
        if (scancode == -1)
            continue;

        if (key >= DESKTOP_WINDOW_KEY_KP_0 && key <= DESKTOP_WINDOW_KEY_KP_ADD)
        {
            const UINT vks[] = {
                VK_NUMPAD0,  VK_NUMPAD1,  VK_NUMPAD2, VK_NUMPAD3,
                VK_NUMPAD4,  VK_NUMPAD5,  VK_NUMPAD6, VK_NUMPAD7,
                VK_NUMPAD8,  VK_NUMPAD9,  VK_DECIMAL, VK_DIVIDE,
                VK_MULTIPLY, VK_SUBTRACT, VK_ADD
            };

            vk = vks[key - DESKTOP_WINDOW_KEY_KP_0];
        }
        else
            vk = MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK);

        length = ToUnicode(vk, scancode, state,
                           chars, sizeof(chars) / sizeof(WCHAR),
                           0);

        if (length == -1)
        {
            // This is a dead key, so we need a second simulated key press
            // to make it output its own character (usually a diacritic)
            length = ToUnicode(vk, scancode, state,
                               chars, sizeof(chars) / sizeof(WCHAR),
                               0);
        }

        if (length < 1)
            continue;

        WideCharToMultiByte(CP_UTF8, 0, chars, 1,
                            _desktop_window.win32.keynames[key],
                            sizeof(_desktop_window.win32.keynames[key]),
                            NULL, NULL);
    }
}

// Replacement for IsWindowsVersionOrGreater, as we cannot rely on the
// application having a correct embedded manifest
//
BOOL _desktop_windowIsWindowsVersionOrGreaterWin32(WORD major, WORD minor, WORD sp)
{
    OSVERSIONINFOEXW osvi = { sizeof(osvi), major, minor, 0, 0, {0}, sp };
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    ULONGLONG cond = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    // HACK: Use RtlVerifyVersionInfo instead of VerifyVersionInfoW as the
    //       latter lies unless the user knew to embed a non-default manifest
    //       announcing support for Windows 10 via supportedOS GUID
    return RtlVerifyVersionInfo(&osvi, mask, cond) == 0;
}

// Checks whether we are on at least the specified build of Windows 10
//
BOOL _desktop_windowIsWindows10BuildOrGreaterWin32(WORD build)
{
    OSVERSIONINFOEXW osvi = { sizeof(osvi), 10, 0, build };
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER;
    ULONGLONG cond = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_BUILDNUMBER, VER_GREATER_EQUAL);
    // HACK: Use RtlVerifyVersionInfo instead of VerifyVersionInfoW as the
    //       latter lies unless the user knew to embed a non-default manifest
    //       announcing support for Windows 10 via supportedOS GUID
    return RtlVerifyVersionInfo(&osvi, mask, cond) == 0;
}

DESKTOP_WINDOWbool _desktop_windowConnectWin32(int platformID, _DESKTOP_WINDOWplatform* platform)
{
    const _DESKTOP_WINDOWplatform win32 =
    {
        .platformID = DESKTOP_WINDOW_PLATFORM_WIN32,
        .init = _desktop_windowInitWin32,
        .terminate = _desktop_windowTerminateWin32,
        .getCursorPos = _desktop_windowGetCursorPosWin32,
        .setCursorPos = _desktop_windowSetCursorPosWin32,
        .setCursorMode = _desktop_windowSetCursorModeWin32,
        .setRawMouseMotion = _desktop_windowSetRawMouseMotionWin32,
        .rawMouseMotionSupported = _desktop_windowRawMouseMotionSupportedWin32,
        .createCursor = _desktop_windowCreateCursorWin32,
        .createStandardCursor = _desktop_windowCreateStandardCursorWin32,
        .destroyCursor = _desktop_windowDestroyCursorWin32,
        .setCursor = _desktop_windowSetCursorWin32,
        .getScancodeName = _desktop_windowGetScancodeNameWin32,
        .getKeyScancode = _desktop_windowGetKeyScancodeWin32,
        .setClipboardString = _desktop_windowSetClipboardStringWin32,
        .getClipboardString = _desktop_windowGetClipboardStringWin32,
        .initJoysticks = _desktop_windowInitJoysticksWin32,
        .terminateJoysticks = _desktop_windowTerminateJoysticksWin32,
        .pollJoystick = _desktop_windowPollJoystickWin32,
        .getMappingName = _desktop_windowGetMappingNameWin32,
        .updateGamepadGUID = _desktop_windowUpdateGamepadGUIDWin32,
        .freeMonitor = _desktop_windowFreeMonitorWin32,
        .getMonitorPos = _desktop_windowGetMonitorPosWin32,
        .getMonitorContentScale = _desktop_windowGetMonitorContentScaleWin32,
        .getMonitorWorkarea = _desktop_windowGetMonitorWorkareaWin32,
        .getVideoModes = _desktop_windowGetVideoModesWin32,
        .getVideoMode = _desktop_windowGetVideoModeWin32,
        .getGammaRamp = _desktop_windowGetGammaRampWin32,
        .setGammaRamp = _desktop_windowSetGammaRampWin32,
        .createWindow = _desktop_windowCreateWindowWin32,
        .destroyWindow = _desktop_windowDestroyWindowWin32,
        .setWindowTitle = _desktop_windowSetWindowTitleWin32,
        .setWindowIcon = _desktop_windowSetWindowIconWin32,
        .getWindowPos = _desktop_windowGetWindowPosWin32,
        .setWindowPos = _desktop_windowSetWindowPosWin32,
        .getWindowSize = _desktop_windowGetWindowSizeWin32,
        .setWindowSize = _desktop_windowSetWindowSizeWin32,
        .setWindowSizeLimits = _desktop_windowSetWindowSizeLimitsWin32,
        .setWindowAspectRatio = _desktop_windowSetWindowAspectRatioWin32,
        .getFramebufferSize = _desktop_windowGetFramebufferSizeWin32,
        .getWindowFrameSize = _desktop_windowGetWindowFrameSizeWin32,
        .getWindowContentScale = _desktop_windowGetWindowContentScaleWin32,
        .iconifyWindow = _desktop_windowIconifyWindowWin32,
        .restoreWindow = _desktop_windowRestoreWindowWin32,
        .maximizeWindow = _desktop_windowMaximizeWindowWin32,
        .showWindow = _desktop_windowShowWindowWin32,
        .hideWindow = _desktop_windowHideWindowWin32,
        .requestWindowAttention = _desktop_windowRequestWindowAttentionWin32,
        .focusWindow = _desktop_windowFocusWindowWin32,
        .setWindowMonitor = _desktop_windowSetWindowMonitorWin32,
        .windowFocused = _desktop_windowWindowFocusedWin32,
        .windowIconified = _desktop_windowWindowIconifiedWin32,
        .windowVisible = _desktop_windowWindowVisibleWin32,
        .windowMaximized = _desktop_windowWindowMaximizedWin32,
        .windowHovered = _desktop_windowWindowHoveredWin32,
        .framebufferTransparent = _desktop_windowFramebufferTransparentWin32,
        .getWindowOpacity = _desktop_windowGetWindowOpacityWin32,
        .setWindowResizable = _desktop_windowSetWindowResizableWin32,
        .setWindowDecorated = _desktop_windowSetWindowDecoratedWin32,
        .setWindowFloating = _desktop_windowSetWindowFloatingWin32,
        .setWindowOpacity = _desktop_windowSetWindowOpacityWin32,
        .setWindowMousePassthrough = _desktop_windowSetWindowMousePassthroughWin32,
        .pollEvents = _desktop_windowPollEventsWin32,
        .waitEvents = _desktop_windowWaitEventsWin32,
        .waitEventsTimeout = _desktop_windowWaitEventsTimeoutWin32,
        .postEmptyEvent = _desktop_windowPostEmptyEventWin32,
        .getEGLPlatform = _desktop_windowGetEGLPlatformWin32,
        .getEGLNativeDisplay = _desktop_windowGetEGLNativeDisplayWin32,
        .getEGLNativeWindow = _desktop_windowGetEGLNativeWindowWin32,
        .getRequiredInstanceExtensions = _desktop_windowGetRequiredInstanceExtensionsWin32,
        .getPhysicalDevicePresentationSupport = _desktop_windowGetPhysicalDevicePresentationSupportWin32,
        .createWindowSurface = _desktop_windowCreateWindowSurfaceWin32
    };

    *platform = win32;
    return DESKTOP_WINDOW_TRUE;
}

int _desktop_windowInitWin32(void)
{
    if (!loadLibraries())
        return DESKTOP_WINDOW_FALSE;

    createKeyTables();
    _desktop_windowUpdateKeyNamesWin32();

    if (_desktop_windowIsWindows10Version1703OrGreaterWin32())
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    else if (IsWindows8Point1OrGreater())
        SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    else if (IsWindowsVistaOrGreater())
        SetProcessDPIAware();

    if (!createHelperWindow())
        return DESKTOP_WINDOW_FALSE;

    _desktop_windowPollMonitorsWin32();
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateWin32(void)
{
    if (_desktop_window.win32.blankCursor)
        DestroyIcon((HICON) _desktop_window.win32.blankCursor);

    if (_desktop_window.win32.deviceNotificationHandle)
        UnregisterDeviceNotification(_desktop_window.win32.deviceNotificationHandle);

    if (_desktop_window.win32.helperWindowHandle)
        DestroyWindow(_desktop_window.win32.helperWindowHandle);
    if (_desktop_window.win32.helperWindowClass)
        UnregisterClassW(MAKEINTATOM(_desktop_window.win32.helperWindowClass), _desktop_window.win32.instance);
    if (_desktop_window.win32.mainWindowClass)
        UnregisterClassW(MAKEINTATOM(_desktop_window.win32.mainWindowClass), _desktop_window.win32.instance);

    _desktop_window_free(_desktop_window.win32.clipboardString);
    _desktop_window_free(_desktop_window.win32.rawInput);

    _desktop_windowTerminateWGL();
    _desktop_windowTerminateEGL();
    _desktop_windowTerminateOSMesa();

    freeLibraries();
}

#endif // _DESKTOP_WINDOW_WIN32

