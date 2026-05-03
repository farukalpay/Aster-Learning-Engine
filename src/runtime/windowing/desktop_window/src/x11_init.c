#include "internal.h"

#if defined(_DESKTOP_WINDOW_X11)

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>


// Translate the X11 KeySyms for a key to a DESKTOP_WINDOW key code
// NOTE: This is only used as a fallback, in case the XKB method fails
//       It is layout-dependent and will fail partially on most non-US layouts
//
static int translateKeySyms(const KeySym* keysyms, int width)
{
    if (width > 1)
    {
        switch (keysyms[1])
        {
            case XK_KP_0:           return DESKTOP_WINDOW_KEY_KP_0;
            case XK_KP_1:           return DESKTOP_WINDOW_KEY_KP_1;
            case XK_KP_2:           return DESKTOP_WINDOW_KEY_KP_2;
            case XK_KP_3:           return DESKTOP_WINDOW_KEY_KP_3;
            case XK_KP_4:           return DESKTOP_WINDOW_KEY_KP_4;
            case XK_KP_5:           return DESKTOP_WINDOW_KEY_KP_5;
            case XK_KP_6:           return DESKTOP_WINDOW_KEY_KP_6;
            case XK_KP_7:           return DESKTOP_WINDOW_KEY_KP_7;
            case XK_KP_8:           return DESKTOP_WINDOW_KEY_KP_8;
            case XK_KP_9:           return DESKTOP_WINDOW_KEY_KP_9;
            case XK_KP_Separator:
            case XK_KP_Decimal:     return DESKTOP_WINDOW_KEY_KP_DECIMAL;
            case XK_KP_Equal:       return DESKTOP_WINDOW_KEY_KP_EQUAL;
            case XK_KP_Enter:       return DESKTOP_WINDOW_KEY_KP_ENTER;
            default:                break;
        }
    }

    switch (keysyms[0])
    {
        case XK_Escape:         return DESKTOP_WINDOW_KEY_ESCAPE;
        case XK_Tab:            return DESKTOP_WINDOW_KEY_TAB;
        case XK_Shift_L:        return DESKTOP_WINDOW_KEY_LEFT_SHIFT;
        case XK_Shift_R:        return DESKTOP_WINDOW_KEY_RIGHT_SHIFT;
        case XK_Control_L:      return DESKTOP_WINDOW_KEY_LEFT_CONTROL;
        case XK_Control_R:      return DESKTOP_WINDOW_KEY_RIGHT_CONTROL;
        case XK_Meta_L:
        case XK_Alt_L:          return DESKTOP_WINDOW_KEY_LEFT_ALT;
        case XK_Mode_switch: // Mapped to Alt_R on many keyboards
        case XK_ISO_Level3_Shift: // AltGr on at least some machines
        case XK_Meta_R:
        case XK_Alt_R:          return DESKTOP_WINDOW_KEY_RIGHT_ALT;
        case XK_Super_L:        return DESKTOP_WINDOW_KEY_LEFT_SUPER;
        case XK_Super_R:        return DESKTOP_WINDOW_KEY_RIGHT_SUPER;
        case XK_Menu:           return DESKTOP_WINDOW_KEY_MENU;
        case XK_Num_Lock:       return DESKTOP_WINDOW_KEY_NUM_LOCK;
        case XK_Caps_Lock:      return DESKTOP_WINDOW_KEY_CAPS_LOCK;
        case XK_Print:          return DESKTOP_WINDOW_KEY_PRINT_SCREEN;
        case XK_Scroll_Lock:    return DESKTOP_WINDOW_KEY_SCROLL_LOCK;
        case XK_Pause:          return DESKTOP_WINDOW_KEY_PAUSE;
        case XK_Delete:         return DESKTOP_WINDOW_KEY_DELETE;
        case XK_BackSpace:      return DESKTOP_WINDOW_KEY_BACKSPACE;
        case XK_Return:         return DESKTOP_WINDOW_KEY_ENTER;
        case XK_Home:           return DESKTOP_WINDOW_KEY_HOME;
        case XK_End:            return DESKTOP_WINDOW_KEY_END;
        case XK_Page_Up:        return DESKTOP_WINDOW_KEY_PAGE_UP;
        case XK_Page_Down:      return DESKTOP_WINDOW_KEY_PAGE_DOWN;
        case XK_Insert:         return DESKTOP_WINDOW_KEY_INSERT;
        case XK_Left:           return DESKTOP_WINDOW_KEY_LEFT;
        case XK_Right:          return DESKTOP_WINDOW_KEY_RIGHT;
        case XK_Down:           return DESKTOP_WINDOW_KEY_DOWN;
        case XK_Up:             return DESKTOP_WINDOW_KEY_UP;
        case XK_F1:             return DESKTOP_WINDOW_KEY_F1;
        case XK_F2:             return DESKTOP_WINDOW_KEY_F2;
        case XK_F3:             return DESKTOP_WINDOW_KEY_F3;
        case XK_F4:             return DESKTOP_WINDOW_KEY_F4;
        case XK_F5:             return DESKTOP_WINDOW_KEY_F5;
        case XK_F6:             return DESKTOP_WINDOW_KEY_F6;
        case XK_F7:             return DESKTOP_WINDOW_KEY_F7;
        case XK_F8:             return DESKTOP_WINDOW_KEY_F8;
        case XK_F9:             return DESKTOP_WINDOW_KEY_F9;
        case XK_F10:            return DESKTOP_WINDOW_KEY_F10;
        case XK_F11:            return DESKTOP_WINDOW_KEY_F11;
        case XK_F12:            return DESKTOP_WINDOW_KEY_F12;
        case XK_F13:            return DESKTOP_WINDOW_KEY_F13;
        case XK_F14:            return DESKTOP_WINDOW_KEY_F14;
        case XK_F15:            return DESKTOP_WINDOW_KEY_F15;
        case XK_F16:            return DESKTOP_WINDOW_KEY_F16;
        case XK_F17:            return DESKTOP_WINDOW_KEY_F17;
        case XK_F18:            return DESKTOP_WINDOW_KEY_F18;
        case XK_F19:            return DESKTOP_WINDOW_KEY_F19;
        case XK_F20:            return DESKTOP_WINDOW_KEY_F20;
        case XK_F21:            return DESKTOP_WINDOW_KEY_F21;
        case XK_F22:            return DESKTOP_WINDOW_KEY_F22;
        case XK_F23:            return DESKTOP_WINDOW_KEY_F23;
        case XK_F24:            return DESKTOP_WINDOW_KEY_F24;
        case XK_F25:            return DESKTOP_WINDOW_KEY_F25;

        // Numeric keypad
        case XK_KP_Divide:      return DESKTOP_WINDOW_KEY_KP_DIVIDE;
        case XK_KP_Multiply:    return DESKTOP_WINDOW_KEY_KP_MULTIPLY;
        case XK_KP_Subtract:    return DESKTOP_WINDOW_KEY_KP_SUBTRACT;
        case XK_KP_Add:         return DESKTOP_WINDOW_KEY_KP_ADD;

        // These should have been detected in secondary keysym test above!
        case XK_KP_Insert:      return DESKTOP_WINDOW_KEY_KP_0;
        case XK_KP_End:         return DESKTOP_WINDOW_KEY_KP_1;
        case XK_KP_Down:        return DESKTOP_WINDOW_KEY_KP_2;
        case XK_KP_Page_Down:   return DESKTOP_WINDOW_KEY_KP_3;
        case XK_KP_Left:        return DESKTOP_WINDOW_KEY_KP_4;
        case XK_KP_Right:       return DESKTOP_WINDOW_KEY_KP_6;
        case XK_KP_Home:        return DESKTOP_WINDOW_KEY_KP_7;
        case XK_KP_Up:          return DESKTOP_WINDOW_KEY_KP_8;
        case XK_KP_Page_Up:     return DESKTOP_WINDOW_KEY_KP_9;
        case XK_KP_Delete:      return DESKTOP_WINDOW_KEY_KP_DECIMAL;
        case XK_KP_Equal:       return DESKTOP_WINDOW_KEY_KP_EQUAL;
        case XK_KP_Enter:       return DESKTOP_WINDOW_KEY_KP_ENTER;

        // Last resort: Check for printable keys (should not happen if the XKB
        // extension is available). This will give a layout dependent mapping
        // (which is wrong, and we may miss some keys, especially on non-US
        // keyboards), but it's better than nothing...
        case XK_a:              return DESKTOP_WINDOW_KEY_A;
        case XK_b:              return DESKTOP_WINDOW_KEY_B;
        case XK_c:              return DESKTOP_WINDOW_KEY_C;
        case XK_d:              return DESKTOP_WINDOW_KEY_D;
        case XK_e:              return DESKTOP_WINDOW_KEY_E;
        case XK_f:              return DESKTOP_WINDOW_KEY_F;
        case XK_g:              return DESKTOP_WINDOW_KEY_G;
        case XK_h:              return DESKTOP_WINDOW_KEY_H;
        case XK_i:              return DESKTOP_WINDOW_KEY_I;
        case XK_j:              return DESKTOP_WINDOW_KEY_J;
        case XK_k:              return DESKTOP_WINDOW_KEY_K;
        case XK_l:              return DESKTOP_WINDOW_KEY_L;
        case XK_m:              return DESKTOP_WINDOW_KEY_M;
        case XK_n:              return DESKTOP_WINDOW_KEY_N;
        case XK_o:              return DESKTOP_WINDOW_KEY_O;
        case XK_p:              return DESKTOP_WINDOW_KEY_P;
        case XK_q:              return DESKTOP_WINDOW_KEY_Q;
        case XK_r:              return DESKTOP_WINDOW_KEY_R;
        case XK_s:              return DESKTOP_WINDOW_KEY_S;
        case XK_t:              return DESKTOP_WINDOW_KEY_T;
        case XK_u:              return DESKTOP_WINDOW_KEY_U;
        case XK_v:              return DESKTOP_WINDOW_KEY_V;
        case XK_w:              return DESKTOP_WINDOW_KEY_W;
        case XK_x:              return DESKTOP_WINDOW_KEY_X;
        case XK_y:              return DESKTOP_WINDOW_KEY_Y;
        case XK_z:              return DESKTOP_WINDOW_KEY_Z;
        case XK_1:              return DESKTOP_WINDOW_KEY_1;
        case XK_2:              return DESKTOP_WINDOW_KEY_2;
        case XK_3:              return DESKTOP_WINDOW_KEY_3;
        case XK_4:              return DESKTOP_WINDOW_KEY_4;
        case XK_5:              return DESKTOP_WINDOW_KEY_5;
        case XK_6:              return DESKTOP_WINDOW_KEY_6;
        case XK_7:              return DESKTOP_WINDOW_KEY_7;
        case XK_8:              return DESKTOP_WINDOW_KEY_8;
        case XK_9:              return DESKTOP_WINDOW_KEY_9;
        case XK_0:              return DESKTOP_WINDOW_KEY_0;
        case XK_space:          return DESKTOP_WINDOW_KEY_SPACE;
        case XK_minus:          return DESKTOP_WINDOW_KEY_MINUS;
        case XK_equal:          return DESKTOP_WINDOW_KEY_EQUAL;
        case XK_bracketleft:    return DESKTOP_WINDOW_KEY_LEFT_BRACKET;
        case XK_bracketright:   return DESKTOP_WINDOW_KEY_RIGHT_BRACKET;
        case XK_backslash:      return DESKTOP_WINDOW_KEY_BACKSLASH;
        case XK_semicolon:      return DESKTOP_WINDOW_KEY_SEMICOLON;
        case XK_apostrophe:     return DESKTOP_WINDOW_KEY_APOSTROPHE;
        case XK_grave:          return DESKTOP_WINDOW_KEY_GRAVE_ACCENT;
        case XK_comma:          return DESKTOP_WINDOW_KEY_COMMA;
        case XK_period:         return DESKTOP_WINDOW_KEY_PERIOD;
        case XK_slash:          return DESKTOP_WINDOW_KEY_SLASH;
        case XK_less:           return DESKTOP_WINDOW_KEY_WORLD_1; // At least in some layouts...
        default:                break;
    }

    // No matching translation was found
    return DESKTOP_WINDOW_KEY_UNKNOWN;
}

// Create key code translation tables
//
static void createKeyTables(void)
{
    int scancodeMin, scancodeMax;

    memset(_desktop_window.x11.keycodes, -1, sizeof(_desktop_window.x11.keycodes));
    memset(_desktop_window.x11.scancodes, -1, sizeof(_desktop_window.x11.scancodes));

    if (_desktop_window.x11.xkb.available)
    {
        // Use XKB to determine physical key locations independently of the
        // current keyboard layout

        XkbDescPtr desc = XkbGetMap(_desktop_window.x11.display, 0, XkbUseCoreKbd);
        XkbGetNames(_desktop_window.x11.display, XkbKeyNamesMask | XkbKeyAliasesMask, desc);

        scancodeMin = desc->min_key_code;
        scancodeMax = desc->max_key_code;

        const struct
        {
            int key;
            char* name;
        } keymap[] =
        {
            { DESKTOP_WINDOW_KEY_GRAVE_ACCENT, "TLDE" },
            { DESKTOP_WINDOW_KEY_1, "AE01" },
            { DESKTOP_WINDOW_KEY_2, "AE02" },
            { DESKTOP_WINDOW_KEY_3, "AE03" },
            { DESKTOP_WINDOW_KEY_4, "AE04" },
            { DESKTOP_WINDOW_KEY_5, "AE05" },
            { DESKTOP_WINDOW_KEY_6, "AE06" },
            { DESKTOP_WINDOW_KEY_7, "AE07" },
            { DESKTOP_WINDOW_KEY_8, "AE08" },
            { DESKTOP_WINDOW_KEY_9, "AE09" },
            { DESKTOP_WINDOW_KEY_0, "AE10" },
            { DESKTOP_WINDOW_KEY_MINUS, "AE11" },
            { DESKTOP_WINDOW_KEY_EQUAL, "AE12" },
            { DESKTOP_WINDOW_KEY_Q, "AD01" },
            { DESKTOP_WINDOW_KEY_W, "AD02" },
            { DESKTOP_WINDOW_KEY_E, "AD03" },
            { DESKTOP_WINDOW_KEY_R, "AD04" },
            { DESKTOP_WINDOW_KEY_T, "AD05" },
            { DESKTOP_WINDOW_KEY_Y, "AD06" },
            { DESKTOP_WINDOW_KEY_U, "AD07" },
            { DESKTOP_WINDOW_KEY_I, "AD08" },
            { DESKTOP_WINDOW_KEY_O, "AD09" },
            { DESKTOP_WINDOW_KEY_P, "AD10" },
            { DESKTOP_WINDOW_KEY_LEFT_BRACKET, "AD11" },
            { DESKTOP_WINDOW_KEY_RIGHT_BRACKET, "AD12" },
            { DESKTOP_WINDOW_KEY_A, "AC01" },
            { DESKTOP_WINDOW_KEY_S, "AC02" },
            { DESKTOP_WINDOW_KEY_D, "AC03" },
            { DESKTOP_WINDOW_KEY_F, "AC04" },
            { DESKTOP_WINDOW_KEY_G, "AC05" },
            { DESKTOP_WINDOW_KEY_H, "AC06" },
            { DESKTOP_WINDOW_KEY_J, "AC07" },
            { DESKTOP_WINDOW_KEY_K, "AC08" },
            { DESKTOP_WINDOW_KEY_L, "AC09" },
            { DESKTOP_WINDOW_KEY_SEMICOLON, "AC10" },
            { DESKTOP_WINDOW_KEY_APOSTROPHE, "AC11" },
            { DESKTOP_WINDOW_KEY_Z, "AB01" },
            { DESKTOP_WINDOW_KEY_X, "AB02" },
            { DESKTOP_WINDOW_KEY_C, "AB03" },
            { DESKTOP_WINDOW_KEY_V, "AB04" },
            { DESKTOP_WINDOW_KEY_B, "AB05" },
            { DESKTOP_WINDOW_KEY_N, "AB06" },
            { DESKTOP_WINDOW_KEY_M, "AB07" },
            { DESKTOP_WINDOW_KEY_COMMA, "AB08" },
            { DESKTOP_WINDOW_KEY_PERIOD, "AB09" },
            { DESKTOP_WINDOW_KEY_SLASH, "AB10" },
            { DESKTOP_WINDOW_KEY_BACKSLASH, "BKSL" },
            { DESKTOP_WINDOW_KEY_WORLD_1, "LSGT" },
            { DESKTOP_WINDOW_KEY_SPACE, "SPCE" },
            { DESKTOP_WINDOW_KEY_ESCAPE, "ESC" },
            { DESKTOP_WINDOW_KEY_ENTER, "RTRN" },
            { DESKTOP_WINDOW_KEY_TAB, "TAB" },
            { DESKTOP_WINDOW_KEY_BACKSPACE, "BKSP" },
            { DESKTOP_WINDOW_KEY_INSERT, "INS" },
            { DESKTOP_WINDOW_KEY_DELETE, "DELE" },
            { DESKTOP_WINDOW_KEY_RIGHT, "RGHT" },
            { DESKTOP_WINDOW_KEY_LEFT, "LEFT" },
            { DESKTOP_WINDOW_KEY_DOWN, "DOWN" },
            { DESKTOP_WINDOW_KEY_UP, "UP" },
            { DESKTOP_WINDOW_KEY_PAGE_UP, "PGUP" },
            { DESKTOP_WINDOW_KEY_PAGE_DOWN, "PGDN" },
            { DESKTOP_WINDOW_KEY_HOME, "HOME" },
            { DESKTOP_WINDOW_KEY_END, "END" },
            { DESKTOP_WINDOW_KEY_CAPS_LOCK, "CAPS" },
            { DESKTOP_WINDOW_KEY_SCROLL_LOCK, "SCLK" },
            { DESKTOP_WINDOW_KEY_NUM_LOCK, "NMLK" },
            { DESKTOP_WINDOW_KEY_PRINT_SCREEN, "PRSC" },
            { DESKTOP_WINDOW_KEY_PAUSE, "PAUS" },
            { DESKTOP_WINDOW_KEY_F1, "FK01" },
            { DESKTOP_WINDOW_KEY_F2, "FK02" },
            { DESKTOP_WINDOW_KEY_F3, "FK03" },
            { DESKTOP_WINDOW_KEY_F4, "FK04" },
            { DESKTOP_WINDOW_KEY_F5, "FK05" },
            { DESKTOP_WINDOW_KEY_F6, "FK06" },
            { DESKTOP_WINDOW_KEY_F7, "FK07" },
            { DESKTOP_WINDOW_KEY_F8, "FK08" },
            { DESKTOP_WINDOW_KEY_F9, "FK09" },
            { DESKTOP_WINDOW_KEY_F10, "FK10" },
            { DESKTOP_WINDOW_KEY_F11, "FK11" },
            { DESKTOP_WINDOW_KEY_F12, "FK12" },
            { DESKTOP_WINDOW_KEY_F13, "FK13" },
            { DESKTOP_WINDOW_KEY_F14, "FK14" },
            { DESKTOP_WINDOW_KEY_F15, "FK15" },
            { DESKTOP_WINDOW_KEY_F16, "FK16" },
            { DESKTOP_WINDOW_KEY_F17, "FK17" },
            { DESKTOP_WINDOW_KEY_F18, "FK18" },
            { DESKTOP_WINDOW_KEY_F19, "FK19" },
            { DESKTOP_WINDOW_KEY_F20, "FK20" },
            { DESKTOP_WINDOW_KEY_F21, "FK21" },
            { DESKTOP_WINDOW_KEY_F22, "FK22" },
            { DESKTOP_WINDOW_KEY_F23, "FK23" },
            { DESKTOP_WINDOW_KEY_F24, "FK24" },
            { DESKTOP_WINDOW_KEY_F25, "FK25" },
            { DESKTOP_WINDOW_KEY_KP_0, "KP0" },
            { DESKTOP_WINDOW_KEY_KP_1, "KP1" },
            { DESKTOP_WINDOW_KEY_KP_2, "KP2" },
            { DESKTOP_WINDOW_KEY_KP_3, "KP3" },
            { DESKTOP_WINDOW_KEY_KP_4, "KP4" },
            { DESKTOP_WINDOW_KEY_KP_5, "KP5" },
            { DESKTOP_WINDOW_KEY_KP_6, "KP6" },
            { DESKTOP_WINDOW_KEY_KP_7, "KP7" },
            { DESKTOP_WINDOW_KEY_KP_8, "KP8" },
            { DESKTOP_WINDOW_KEY_KP_9, "KP9" },
            { DESKTOP_WINDOW_KEY_KP_DECIMAL, "KPDL" },
            { DESKTOP_WINDOW_KEY_KP_DIVIDE, "KPDV" },
            { DESKTOP_WINDOW_KEY_KP_MULTIPLY, "KPMU" },
            { DESKTOP_WINDOW_KEY_KP_SUBTRACT, "KPSU" },
            { DESKTOP_WINDOW_KEY_KP_ADD, "KPAD" },
            { DESKTOP_WINDOW_KEY_KP_ENTER, "KPEN" },
            { DESKTOP_WINDOW_KEY_KP_EQUAL, "KPEQ" },
            { DESKTOP_WINDOW_KEY_LEFT_SHIFT, "LFSH" },
            { DESKTOP_WINDOW_KEY_LEFT_CONTROL, "LCTL" },
            { DESKTOP_WINDOW_KEY_LEFT_ALT, "LALT" },
            { DESKTOP_WINDOW_KEY_LEFT_SUPER, "LWIN" },
            { DESKTOP_WINDOW_KEY_RIGHT_SHIFT, "RTSH" },
            { DESKTOP_WINDOW_KEY_RIGHT_CONTROL, "RCTL" },
            { DESKTOP_WINDOW_KEY_RIGHT_ALT, "RALT" },
            { DESKTOP_WINDOW_KEY_RIGHT_ALT, "LVL3" },
            { DESKTOP_WINDOW_KEY_RIGHT_ALT, "MDSW" },
            { DESKTOP_WINDOW_KEY_RIGHT_SUPER, "RWIN" },
            { DESKTOP_WINDOW_KEY_MENU, "MENU" }
        };

        // Find the X11 key code -> DESKTOP_WINDOW key code mapping
        for (int scancode = scancodeMin;  scancode <= scancodeMax;  scancode++)
        {
            int key = DESKTOP_WINDOW_KEY_UNKNOWN;

            // Map the key name to a DESKTOP_WINDOW key code. Note: We use the US
            // keyboard layout. Because function keys aren't mapped correctly
            // when using traditional KeySym translations, they are mapped
            // here instead.
            for (int i = 0;  i < sizeof(keymap) / sizeof(keymap[0]);  i++)
            {
                if (strncmp(desc->names->keys[scancode].name,
                            keymap[i].name,
                            XkbKeyNameLength) == 0)
                {
                    key = keymap[i].key;
                    break;
                }
            }

            // Fall back to key aliases in case the key name did not match
            for (int i = 0;  i < desc->names->num_key_aliases;  i++)
            {
                if (key != DESKTOP_WINDOW_KEY_UNKNOWN)
                    break;

                if (strncmp(desc->names->key_aliases[i].real,
                            desc->names->keys[scancode].name,
                            XkbKeyNameLength) != 0)
                {
                    continue;
                }

                for (int j = 0;  j < sizeof(keymap) / sizeof(keymap[0]);  j++)
                {
                    if (strncmp(desc->names->key_aliases[i].alias,
                                keymap[j].name,
                                XkbKeyNameLength) == 0)
                    {
                        key = keymap[j].key;
                        break;
                    }
                }
            }

            _desktop_window.x11.keycodes[scancode] = key;
        }

        XkbFreeNames(desc, XkbKeyNamesMask, True);
        XkbFreeKeyboard(desc, 0, True);
    }
    else
        XDisplayKeycodes(_desktop_window.x11.display, &scancodeMin, &scancodeMax);

    int width;
    KeySym* keysyms = XGetKeyboardMapping(_desktop_window.x11.display,
                                          scancodeMin,
                                          scancodeMax - scancodeMin + 1,
                                          &width);

    for (int scancode = scancodeMin;  scancode <= scancodeMax;  scancode++)
    {
        // Translate the un-translated key codes using traditional X11 KeySym
        // lookups
        if (_desktop_window.x11.keycodes[scancode] < 0)
        {
            const size_t base = (scancode - scancodeMin) * width;
            _desktop_window.x11.keycodes[scancode] = translateKeySyms(&keysyms[base], width);
        }

        // Store the reverse translation for faster key name lookup
        if (_desktop_window.x11.keycodes[scancode] > 0)
            _desktop_window.x11.scancodes[_desktop_window.x11.keycodes[scancode]] = scancode;
    }

    XFree(keysyms);
}

// Check whether the IM has a usable style
//
static DESKTOP_WINDOWbool hasUsableInputMethodStyle(void)
{
    DESKTOP_WINDOWbool found = DESKTOP_WINDOW_FALSE;
    XIMStyles* styles = NULL;

    if (XGetIMValues(_desktop_window.x11.im, XNQueryInputStyle, &styles, NULL) != NULL)
        return DESKTOP_WINDOW_FALSE;

    for (unsigned int i = 0;  i < styles->count_styles;  i++)
    {
        if (styles->supported_styles[i] == (XIMPreeditNothing | XIMStatusNothing))
        {
            found = DESKTOP_WINDOW_TRUE;
            break;
        }
    }

    XFree(styles);
    return found;
}

static void inputMethodDestroyCallback(XIM im, XPointer clientData, XPointer callData)
{
    _desktop_window.x11.im = NULL;
}

static void inputMethodInstantiateCallback(Display* display,
                                           XPointer clientData,
                                           XPointer callData)
{
    if (_desktop_window.x11.im)
        return;

    _desktop_window.x11.im = XOpenIM(_desktop_window.x11.display, 0, NULL, NULL);
    if (_desktop_window.x11.im)
    {
        if (!hasUsableInputMethodStyle())
        {
            XCloseIM(_desktop_window.x11.im);
            _desktop_window.x11.im = NULL;
        }
    }

    if (_desktop_window.x11.im)
    {
        XIMCallback callback;
        callback.callback = (XIMProc) inputMethodDestroyCallback;
        callback.client_data = NULL;
        XSetIMValues(_desktop_window.x11.im, XNDestroyCallback, &callback, NULL);

        for (_DESKTOP_WINDOWwindow* window = _desktop_window.windowListHead;  window;  window = window->next)
            _desktop_windowCreateInputContextX11(window);
    }
}

// Return the atom ID only if it is listed in the specified array
//
static Atom getAtomIfSupported(Atom* supportedAtoms,
                               unsigned long atomCount,
                               const char* atomName)
{
    const Atom atom = XInternAtom(_desktop_window.x11.display, atomName, False);

    for (unsigned long i = 0;  i < atomCount;  i++)
    {
        if (supportedAtoms[i] == atom)
            return atom;
    }

    return None;
}

// Check whether the running window manager is EWMH-compliant
//
static void detectEWMH(void)
{
    // First we read the _NET_SUPPORTING_WM_CHECK property on the root window

    Window* windowFromRoot = NULL;
    if (!_desktop_windowGetWindowPropertyX11(_desktop_window.x11.root,
                                   _desktop_window.x11.NET_SUPPORTING_WM_CHECK,
                                   XA_WINDOW,
                                   (unsigned char**) &windowFromRoot))
    {
        return;
    }

    _desktop_windowGrabErrorHandlerX11();

    // If it exists, it should be the XID of a top-level window
    // Then we look for the same property on that window

    Window* windowFromChild = NULL;
    if (!_desktop_windowGetWindowPropertyX11(*windowFromRoot,
                                   _desktop_window.x11.NET_SUPPORTING_WM_CHECK,
                                   XA_WINDOW,
                                   (unsigned char**) &windowFromChild))
    {
        XFree(windowFromRoot);
        return;
    }

    _desktop_windowReleaseErrorHandlerX11();

    // If the property exists, it should contain the XID of the window

    if (*windowFromRoot != *windowFromChild)
    {
        XFree(windowFromRoot);
        XFree(windowFromChild);
        return;
    }

    XFree(windowFromRoot);
    XFree(windowFromChild);

    // We are now fairly sure that an EWMH-compliant WM is currently running
    // We can now start querying the WM about what features it supports by
    // looking in the _NET_SUPPORTED property on the root window
    // It should contain a list of supported EWMH protocol and state atoms

    Atom* supportedAtoms = NULL;
    const unsigned long atomCount =
        _desktop_windowGetWindowPropertyX11(_desktop_window.x11.root,
                                  _desktop_window.x11.NET_SUPPORTED,
                                  XA_ATOM,
                                  (unsigned char**) &supportedAtoms);

    // See which of the atoms we support that are supported by the WM

    _desktop_window.x11.NET_WM_STATE =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WM_STATE");
    _desktop_window.x11.NET_WM_STATE_ABOVE =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WM_STATE_ABOVE");
    _desktop_window.x11.NET_WM_STATE_FULLSCREEN =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WM_STATE_FULLSCREEN");
    _desktop_window.x11.NET_WM_STATE_MAXIMIZED_VERT =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WM_STATE_MAXIMIZED_VERT");
    _desktop_window.x11.NET_WM_STATE_MAXIMIZED_HORZ =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WM_STATE_MAXIMIZED_HORZ");
    _desktop_window.x11.NET_WM_STATE_DEMANDS_ATTENTION =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WM_STATE_DEMANDS_ATTENTION");
    _desktop_window.x11.NET_WM_FULLSCREEN_MONITORS =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WM_FULLSCREEN_MONITORS");
    _desktop_window.x11.NET_WM_WINDOW_TYPE =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WM_WINDOW_TYPE");
    _desktop_window.x11.NET_WM_WINDOW_TYPE_NORMAL =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WM_WINDOW_TYPE_NORMAL");
    _desktop_window.x11.NET_WORKAREA =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_WORKAREA");
    _desktop_window.x11.NET_CURRENT_DESKTOP =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_CURRENT_DESKTOP");
    _desktop_window.x11.NET_ACTIVE_WINDOW =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_ACTIVE_WINDOW");
    _desktop_window.x11.NET_FRAME_EXTENTS =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_FRAME_EXTENTS");
    _desktop_window.x11.NET_REQUEST_FRAME_EXTENTS =
        getAtomIfSupported(supportedAtoms, atomCount, "_NET_REQUEST_FRAME_EXTENTS");

    if (supportedAtoms)
        XFree(supportedAtoms);
}

// Look for and initialize supported X11 extensions
//
static DESKTOP_WINDOWbool initExtensions(void)
{
#if defined(__OpenBSD__) || defined(__NetBSD__)
    _desktop_window.x11.vidmode.handle = _desktop_windowPlatformLoadModule("libXxf86vm.so");
#else
    _desktop_window.x11.vidmode.handle = _desktop_windowPlatformLoadModule("libXxf86vm.so.1");
#endif
    if (_desktop_window.x11.vidmode.handle)
    {
        _desktop_window.x11.vidmode.QueryExtension = (PFN_XF86VidModeQueryExtension)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.vidmode.handle, "XF86VidModeQueryExtension");
        _desktop_window.x11.vidmode.GetGammaRamp = (PFN_XF86VidModeGetGammaRamp)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.vidmode.handle, "XF86VidModeGetGammaRamp");
        _desktop_window.x11.vidmode.SetGammaRamp = (PFN_XF86VidModeSetGammaRamp)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.vidmode.handle, "XF86VidModeSetGammaRamp");
        _desktop_window.x11.vidmode.GetGammaRampSize = (PFN_XF86VidModeGetGammaRampSize)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.vidmode.handle, "XF86VidModeGetGammaRampSize");

        _desktop_window.x11.vidmode.available =
            XF86VidModeQueryExtension(_desktop_window.x11.display,
                                      &_desktop_window.x11.vidmode.eventBase,
                                      &_desktop_window.x11.vidmode.errorBase);
    }

#if defined(__CYGWIN__)
    _desktop_window.x11.xi.handle = _desktop_windowPlatformLoadModule("libXi-6.so");
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    _desktop_window.x11.xi.handle = _desktop_windowPlatformLoadModule("libXi.so");
#else
    _desktop_window.x11.xi.handle = _desktop_windowPlatformLoadModule("libXi.so.6");
#endif
    if (_desktop_window.x11.xi.handle)
    {
        _desktop_window.x11.xi.QueryVersion = (PFN_XIQueryVersion)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xi.handle, "XIQueryVersion");
        _desktop_window.x11.xi.SelectEvents = (PFN_XISelectEvents)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xi.handle, "XISelectEvents");

        if (XQueryExtension(_desktop_window.x11.display,
                            "XInputExtension",
                            &_desktop_window.x11.xi.majorOpcode,
                            &_desktop_window.x11.xi.eventBase,
                            &_desktop_window.x11.xi.errorBase))
        {
            _desktop_window.x11.xi.major = 2;
            _desktop_window.x11.xi.minor = 0;

            if (XIQueryVersion(_desktop_window.x11.display,
                               &_desktop_window.x11.xi.major,
                               &_desktop_window.x11.xi.minor) == Success)
            {
                _desktop_window.x11.xi.available = DESKTOP_WINDOW_TRUE;
            }
        }
    }

#if defined(__CYGWIN__)
    _desktop_window.x11.randr.handle = _desktop_windowPlatformLoadModule("libXrandr-2.so");
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    _desktop_window.x11.randr.handle = _desktop_windowPlatformLoadModule("libXrandr.so");
#else
    _desktop_window.x11.randr.handle = _desktop_windowPlatformLoadModule("libXrandr.so.2");
#endif
    if (_desktop_window.x11.randr.handle)
    {
        _desktop_window.x11.randr.AllocGamma = (PFN_XRRAllocGamma)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRAllocGamma");
        _desktop_window.x11.randr.FreeGamma = (PFN_XRRFreeGamma)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRFreeGamma");
        _desktop_window.x11.randr.FreeCrtcInfo = (PFN_XRRFreeCrtcInfo)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRFreeCrtcInfo");
        _desktop_window.x11.randr.FreeGamma = (PFN_XRRFreeGamma)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRFreeGamma");
        _desktop_window.x11.randr.FreeOutputInfo = (PFN_XRRFreeOutputInfo)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRFreeOutputInfo");
        _desktop_window.x11.randr.FreeScreenResources = (PFN_XRRFreeScreenResources)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRFreeScreenResources");
        _desktop_window.x11.randr.GetCrtcGamma = (PFN_XRRGetCrtcGamma)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRGetCrtcGamma");
        _desktop_window.x11.randr.GetCrtcGammaSize = (PFN_XRRGetCrtcGammaSize)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRGetCrtcGammaSize");
        _desktop_window.x11.randr.GetCrtcInfo = (PFN_XRRGetCrtcInfo)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRGetCrtcInfo");
        _desktop_window.x11.randr.GetOutputInfo = (PFN_XRRGetOutputInfo)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRGetOutputInfo");
        _desktop_window.x11.randr.GetOutputPrimary = (PFN_XRRGetOutputPrimary)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRGetOutputPrimary");
        _desktop_window.x11.randr.GetScreenResourcesCurrent = (PFN_XRRGetScreenResourcesCurrent)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRGetScreenResourcesCurrent");
        _desktop_window.x11.randr.QueryExtension = (PFN_XRRQueryExtension)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRQueryExtension");
        _desktop_window.x11.randr.QueryVersion = (PFN_XRRQueryVersion)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRQueryVersion");
        _desktop_window.x11.randr.SelectInput = (PFN_XRRSelectInput)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRSelectInput");
        _desktop_window.x11.randr.SetCrtcConfig = (PFN_XRRSetCrtcConfig)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRSetCrtcConfig");
        _desktop_window.x11.randr.SetCrtcGamma = (PFN_XRRSetCrtcGamma)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRSetCrtcGamma");
        _desktop_window.x11.randr.UpdateConfiguration = (PFN_XRRUpdateConfiguration)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.randr.handle, "XRRUpdateConfiguration");

        if (XRRQueryExtension(_desktop_window.x11.display,
                              &_desktop_window.x11.randr.eventBase,
                              &_desktop_window.x11.randr.errorBase))
        {
            if (XRRQueryVersion(_desktop_window.x11.display,
                                &_desktop_window.x11.randr.major,
                                &_desktop_window.x11.randr.minor))
            {
                // The DESKTOP_WINDOW RandR path requires at least version 1.3
                if (_desktop_window.x11.randr.major > 1 || _desktop_window.x11.randr.minor >= 3)
                    _desktop_window.x11.randr.available = DESKTOP_WINDOW_TRUE;
            }
            else
            {
                _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                                "X11: Failed to query RandR version");
            }
        }
    }

    if (_desktop_window.x11.randr.available)
    {
        XRRScreenResources* sr = XRRGetScreenResourcesCurrent(_desktop_window.x11.display,
                                                              _desktop_window.x11.root);

        if (!sr->ncrtc || !XRRGetCrtcGammaSize(_desktop_window.x11.display, sr->crtcs[0]))
        {
            // This is likely an older Nvidia driver with broken gamma support
            // Flag it as useless and fall back to xf86vm gamma, if available
            _desktop_window.x11.randr.gammaBroken = DESKTOP_WINDOW_TRUE;
        }

        if (!sr->ncrtc)
        {
            // A system without CRTCs is likely a system with broken RandR
            // Disable the RandR monitor path and fall back to core functions
            _desktop_window.x11.randr.monitorBroken = DESKTOP_WINDOW_TRUE;
        }

        XRRFreeScreenResources(sr);
    }

    if (_desktop_window.x11.randr.available && !_desktop_window.x11.randr.monitorBroken)
    {
        XRRSelectInput(_desktop_window.x11.display, _desktop_window.x11.root,
                       RROutputChangeNotifyMask);
    }

#if defined(__CYGWIN__)
    _desktop_window.x11.xcursor.handle = _desktop_windowPlatformLoadModule("libXcursor-1.so");
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    _desktop_window.x11.xcursor.handle = _desktop_windowPlatformLoadModule("libXcursor.so");
#else
    _desktop_window.x11.xcursor.handle = _desktop_windowPlatformLoadModule("libXcursor.so.1");
#endif
    if (_desktop_window.x11.xcursor.handle)
    {
        _desktop_window.x11.xcursor.ImageCreate = (PFN_XcursorImageCreate)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xcursor.handle, "XcursorImageCreate");
        _desktop_window.x11.xcursor.ImageDestroy = (PFN_XcursorImageDestroy)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xcursor.handle, "XcursorImageDestroy");
        _desktop_window.x11.xcursor.ImageLoadCursor = (PFN_XcursorImageLoadCursor)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xcursor.handle, "XcursorImageLoadCursor");
        _desktop_window.x11.xcursor.GetTheme = (PFN_XcursorGetTheme)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xcursor.handle, "XcursorGetTheme");
        _desktop_window.x11.xcursor.GetDefaultSize = (PFN_XcursorGetDefaultSize)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xcursor.handle, "XcursorGetDefaultSize");
        _desktop_window.x11.xcursor.LibraryLoadImage = (PFN_XcursorLibraryLoadImage)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xcursor.handle, "XcursorLibraryLoadImage");
    }

#if defined(__CYGWIN__)
    _desktop_window.x11.xinerama.handle = _desktop_windowPlatformLoadModule("libXinerama-1.so");
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    _desktop_window.x11.xinerama.handle = _desktop_windowPlatformLoadModule("libXinerama.so");
#else
    _desktop_window.x11.xinerama.handle = _desktop_windowPlatformLoadModule("libXinerama.so.1");
#endif
    if (_desktop_window.x11.xinerama.handle)
    {
        _desktop_window.x11.xinerama.IsActive = (PFN_XineramaIsActive)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xinerama.handle, "XineramaIsActive");
        _desktop_window.x11.xinerama.QueryExtension = (PFN_XineramaQueryExtension)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xinerama.handle, "XineramaQueryExtension");
        _desktop_window.x11.xinerama.QueryScreens = (PFN_XineramaQueryScreens)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xinerama.handle, "XineramaQueryScreens");

        if (XineramaQueryExtension(_desktop_window.x11.display,
                                   &_desktop_window.x11.xinerama.major,
                                   &_desktop_window.x11.xinerama.minor))
        {
            if (XineramaIsActive(_desktop_window.x11.display))
                _desktop_window.x11.xinerama.available = DESKTOP_WINDOW_TRUE;
        }
    }

    _desktop_window.x11.xkb.major = 1;
    _desktop_window.x11.xkb.minor = 0;
    _desktop_window.x11.xkb.available =
        XkbQueryExtension(_desktop_window.x11.display,
                          &_desktop_window.x11.xkb.majorOpcode,
                          &_desktop_window.x11.xkb.eventBase,
                          &_desktop_window.x11.xkb.errorBase,
                          &_desktop_window.x11.xkb.major,
                          &_desktop_window.x11.xkb.minor);

    if (_desktop_window.x11.xkb.available)
    {
        Bool supported;

        if (XkbSetDetectableAutoRepeat(_desktop_window.x11.display, True, &supported))
        {
            if (supported)
                _desktop_window.x11.xkb.detectable = DESKTOP_WINDOW_TRUE;
        }

        XkbStateRec state;
        if (XkbGetState(_desktop_window.x11.display, XkbUseCoreKbd, &state) == Success)
            _desktop_window.x11.xkb.group = (unsigned int)state.group;

        XkbSelectEventDetails(_desktop_window.x11.display, XkbUseCoreKbd, XkbStateNotify,
                              XkbGroupStateMask, XkbGroupStateMask);
    }

    if (_desktop_window.hints.init.x11.xcbVulkanSurface)
    {
#if defined(__CYGWIN__)
        _desktop_window.x11.x11xcb.handle = _desktop_windowPlatformLoadModule("libX11-xcb-1.so");
#elif defined(__OpenBSD__) || defined(__NetBSD__)
        _desktop_window.x11.x11xcb.handle = _desktop_windowPlatformLoadModule("libX11-xcb.so");
#else
        _desktop_window.x11.x11xcb.handle = _desktop_windowPlatformLoadModule("libX11-xcb.so.1");
#endif
    }

    if (_desktop_window.x11.x11xcb.handle)
    {
        _desktop_window.x11.x11xcb.GetXCBConnection = (PFN_XGetXCBConnection)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.x11xcb.handle, "XGetXCBConnection");
    }

#if defined(__CYGWIN__)
    _desktop_window.x11.xrender.handle = _desktop_windowPlatformLoadModule("libXrender-1.so");
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    _desktop_window.x11.xrender.handle = _desktop_windowPlatformLoadModule("libXrender.so");
#else
    _desktop_window.x11.xrender.handle = _desktop_windowPlatformLoadModule("libXrender.so.1");
#endif
    if (_desktop_window.x11.xrender.handle)
    {
        _desktop_window.x11.xrender.QueryExtension = (PFN_XRenderQueryExtension)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xrender.handle, "XRenderQueryExtension");
        _desktop_window.x11.xrender.QueryVersion = (PFN_XRenderQueryVersion)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xrender.handle, "XRenderQueryVersion");
        _desktop_window.x11.xrender.FindVisualFormat = (PFN_XRenderFindVisualFormat)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xrender.handle, "XRenderFindVisualFormat");

        if (XRenderQueryExtension(_desktop_window.x11.display,
                                  &_desktop_window.x11.xrender.errorBase,
                                  &_desktop_window.x11.xrender.eventBase))
        {
            if (XRenderQueryVersion(_desktop_window.x11.display,
                                    &_desktop_window.x11.xrender.major,
                                    &_desktop_window.x11.xrender.minor))
            {
                _desktop_window.x11.xrender.available = DESKTOP_WINDOW_TRUE;
            }
        }
    }

#if defined(__CYGWIN__)
    _desktop_window.x11.xshape.handle = _desktop_windowPlatformLoadModule("libXext-6.so");
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    _desktop_window.x11.xshape.handle = _desktop_windowPlatformLoadModule("libXext.so");
#else
    _desktop_window.x11.xshape.handle = _desktop_windowPlatformLoadModule("libXext.so.6");
#endif
    if (_desktop_window.x11.xshape.handle)
    {
        _desktop_window.x11.xshape.QueryExtension = (PFN_XShapeQueryExtension)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xshape.handle, "XShapeQueryExtension");
        _desktop_window.x11.xshape.ShapeCombineRegion = (PFN_XShapeCombineRegion)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xshape.handle, "XShapeCombineRegion");
        _desktop_window.x11.xshape.QueryVersion = (PFN_XShapeQueryVersion)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xshape.handle, "XShapeQueryVersion");
        _desktop_window.x11.xshape.ShapeCombineMask = (PFN_XShapeCombineMask)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xshape.handle, "XShapeCombineMask");

        if (XShapeQueryExtension(_desktop_window.x11.display,
            &_desktop_window.x11.xshape.errorBase,
            &_desktop_window.x11.xshape.eventBase))
        {
            if (XShapeQueryVersion(_desktop_window.x11.display,
                &_desktop_window.x11.xshape.major,
                &_desktop_window.x11.xshape.minor))
            {
                _desktop_window.x11.xshape.available = DESKTOP_WINDOW_TRUE;
            }
        }
    }

    // Update the key code LUT
    // FIXME: We should listen to XkbMapNotify events to track changes to
    // the keyboard mapping.
    createKeyTables();

    // String format atoms
    _desktop_window.x11.NULL_ = XInternAtom(_desktop_window.x11.display, "NULL", False);
    _desktop_window.x11.UTF8_STRING = XInternAtom(_desktop_window.x11.display, "UTF8_STRING", False);
    _desktop_window.x11.ATOM_PAIR = XInternAtom(_desktop_window.x11.display, "ATOM_PAIR", False);

    // Custom selection property atom
    _desktop_window.x11.DESKTOP_WINDOW_SELECTION =
        XInternAtom(_desktop_window.x11.display, "DESKTOP_WINDOW_SELECTION", False);

    // ICCCM standard clipboard atoms
    _desktop_window.x11.TARGETS = XInternAtom(_desktop_window.x11.display, "TARGETS", False);
    _desktop_window.x11.MULTIPLE = XInternAtom(_desktop_window.x11.display, "MULTIPLE", False);
    _desktop_window.x11.PRIMARY = XInternAtom(_desktop_window.x11.display, "PRIMARY", False);
    _desktop_window.x11.INCR = XInternAtom(_desktop_window.x11.display, "INCR", False);
    _desktop_window.x11.CLIPBOARD = XInternAtom(_desktop_window.x11.display, "CLIPBOARD", False);

    // Clipboard manager atoms
    _desktop_window.x11.CLIPBOARD_MANAGER =
        XInternAtom(_desktop_window.x11.display, "CLIPBOARD_MANAGER", False);
    _desktop_window.x11.SAVE_TARGETS =
        XInternAtom(_desktop_window.x11.display, "SAVE_TARGETS", False);

    // Xdnd (drag and drop) atoms
    _desktop_window.x11.XdndAware = XInternAtom(_desktop_window.x11.display, "XdndAware", False);
    _desktop_window.x11.XdndEnter = XInternAtom(_desktop_window.x11.display, "XdndEnter", False);
    _desktop_window.x11.XdndPosition = XInternAtom(_desktop_window.x11.display, "XdndPosition", False);
    _desktop_window.x11.XdndStatus = XInternAtom(_desktop_window.x11.display, "XdndStatus", False);
    _desktop_window.x11.XdndActionCopy = XInternAtom(_desktop_window.x11.display, "XdndActionCopy", False);
    _desktop_window.x11.XdndDrop = XInternAtom(_desktop_window.x11.display, "XdndDrop", False);
    _desktop_window.x11.XdndFinished = XInternAtom(_desktop_window.x11.display, "XdndFinished", False);
    _desktop_window.x11.XdndSelection = XInternAtom(_desktop_window.x11.display, "XdndSelection", False);
    _desktop_window.x11.XdndTypeList = XInternAtom(_desktop_window.x11.display, "XdndTypeList", False);
    _desktop_window.x11.text_uri_list = XInternAtom(_desktop_window.x11.display, "text/uri-list", False);

    // ICCCM, EWMH and Motif window property atoms
    // These can be set safely even without WM support
    // The EWMH atoms that require WM support are handled in detectEWMH
    _desktop_window.x11.WM_PROTOCOLS =
        XInternAtom(_desktop_window.x11.display, "WM_PROTOCOLS", False);
    _desktop_window.x11.WM_STATE =
        XInternAtom(_desktop_window.x11.display, "WM_STATE", False);
    _desktop_window.x11.WM_DELETE_WINDOW =
        XInternAtom(_desktop_window.x11.display, "WM_DELETE_WINDOW", False);
    _desktop_window.x11.NET_SUPPORTED =
        XInternAtom(_desktop_window.x11.display, "_NET_SUPPORTED", False);
    _desktop_window.x11.NET_SUPPORTING_WM_CHECK =
        XInternAtom(_desktop_window.x11.display, "_NET_SUPPORTING_WM_CHECK", False);
    _desktop_window.x11.NET_WM_ICON =
        XInternAtom(_desktop_window.x11.display, "_NET_WM_ICON", False);
    _desktop_window.x11.NET_WM_PING =
        XInternAtom(_desktop_window.x11.display, "_NET_WM_PING", False);
    _desktop_window.x11.NET_WM_PID =
        XInternAtom(_desktop_window.x11.display, "_NET_WM_PID", False);
    _desktop_window.x11.NET_WM_NAME =
        XInternAtom(_desktop_window.x11.display, "_NET_WM_NAME", False);
    _desktop_window.x11.NET_WM_ICON_NAME =
        XInternAtom(_desktop_window.x11.display, "_NET_WM_ICON_NAME", False);
    _desktop_window.x11.NET_WM_BYPASS_COMPOSITOR =
        XInternAtom(_desktop_window.x11.display, "_NET_WM_BYPASS_COMPOSITOR", False);
    _desktop_window.x11.NET_WM_WINDOW_OPACITY =
        XInternAtom(_desktop_window.x11.display, "_NET_WM_WINDOW_OPACITY", False);
    _desktop_window.x11.MOTIF_WM_HINTS =
        XInternAtom(_desktop_window.x11.display, "_MOTIF_WM_HINTS", False);

    // The compositing manager selection name contains the screen number
    {
        char name[32];
        snprintf(name, sizeof(name), "_NET_WM_CM_S%u", _desktop_window.x11.screen);
        _desktop_window.x11.NET_WM_CM_Sx = XInternAtom(_desktop_window.x11.display, name, False);
    }

    // Detect whether an EWMH-conformant window manager is running
    detectEWMH();

    return DESKTOP_WINDOW_TRUE;
}

// Retrieve system content scale via folklore heuristics
//
static void getSystemContentScale(float* xscale, float* yscale)
{
    // Start by assuming the default X11 DPI
    // NOTE: Some desktop environments (KDE) may remove the Xft.dpi field when it
    //       would be set to 96, so assume that is the case if we cannot find it
    float xdpi = 96.f, ydpi = 96.f;

    // NOTE: Basing the scale on Xft.dpi where available should provide the most
    //       consistent user experience (matches Qt, Gtk, etc), although not
    //       always the most accurate one
    char* rms = XResourceManagerString(_desktop_window.x11.display);
    if (rms)
    {
        XrmDatabase db = XrmGetStringDatabase(rms);
        if (db)
        {
            XrmValue value;
            char* type = NULL;

            if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value))
            {
                if (type && strcmp(type, "String") == 0)
                    xdpi = ydpi = atof(value.addr);
            }

            XrmDestroyDatabase(db);
        }
    }

    *xscale = xdpi / 96.f;
    *yscale = ydpi / 96.f;
}

// Create a blank cursor for hidden and disabled cursor modes
//
static Cursor createHiddenCursor(void)
{
    unsigned char pixels[16 * 16 * 4] = { 0 };
    DESKTOP_WINDOWimage image = { 16, 16, pixels };
    return _desktop_windowCreateNativeCursorX11(&image, 0, 0);
}

// Create a helper window for IPC
//
static Window createHelperWindow(void)
{
    XSetWindowAttributes wa;
    wa.event_mask = PropertyChangeMask;

    return XCreateWindow(_desktop_window.x11.display, _desktop_window.x11.root,
                         0, 0, 1, 1, 0, 0,
                         InputOnly,
                         DefaultVisual(_desktop_window.x11.display, _desktop_window.x11.screen),
                         CWEventMask, &wa);
}

// Create the pipe for empty events without assumuing the OS has pipe2(2)
//
static DESKTOP_WINDOWbool createEmptyEventPipe(void)
{
    if (pipe(_desktop_window.x11.emptyEventPipe) != 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "X11: Failed to create empty event pipe: %s",
                        strerror(errno));
        return DESKTOP_WINDOW_FALSE;
    }

    for (int i = 0; i < 2; i++)
    {
        const int sf = fcntl(_desktop_window.x11.emptyEventPipe[i], F_GETFL, 0);
        const int df = fcntl(_desktop_window.x11.emptyEventPipe[i], F_GETFD, 0);

        if (sf == -1 || df == -1 ||
            fcntl(_desktop_window.x11.emptyEventPipe[i], F_SETFL, sf | O_NONBLOCK) == -1 ||
            fcntl(_desktop_window.x11.emptyEventPipe[i], F_SETFD, df | FD_CLOEXEC) == -1)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                            "X11: Failed to set flags for empty event pipe: %s",
                            strerror(errno));
            return DESKTOP_WINDOW_FALSE;
        }
    }

    return DESKTOP_WINDOW_TRUE;
}

// X error handler
//
static int errorHandler(Display *display, XErrorEvent* event)
{
    if (_desktop_window.x11.display != display)
        return 0;

    _desktop_window.x11.errorCode = event->error_code;
    return 0;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Sets the X error handler callback
//
void _desktop_windowGrabErrorHandlerX11(void)
{
    assert(_desktop_window.x11.errorHandler == NULL);
    _desktop_window.x11.errorCode = Success;
    _desktop_window.x11.errorHandler = XSetErrorHandler(errorHandler);
}

// Clears the X error handler callback
//
void _desktop_windowReleaseErrorHandlerX11(void)
{
    // Synchronize to make sure all commands are processed
    XSync(_desktop_window.x11.display, False);
    XSetErrorHandler(_desktop_window.x11.errorHandler);
    _desktop_window.x11.errorHandler = NULL;
}

// Reports the specified error, appending information about the last X error
//
void _desktop_windowInputErrorX11(int error, const char* message)
{
    char buffer[_DESKTOP_WINDOW_MESSAGE_SIZE];
    XGetErrorText(_desktop_window.x11.display, _desktop_window.x11.errorCode,
                  buffer, sizeof(buffer));

    _desktop_windowInputError(error, "%s: %s", message, buffer);
}

// Creates a native cursor object from the specified image and hotspot
//
Cursor _desktop_windowCreateNativeCursorX11(const DESKTOP_WINDOWimage* image, int xhot, int yhot)
{
    Cursor cursor;

    if (!_desktop_window.x11.xcursor.handle)
        return None;

    XcursorImage* native = XcursorImageCreate(image->width, image->height);
    if (native == NULL)
        return None;

    native->xhot = xhot;
    native->yhot = yhot;

    unsigned char* source = (unsigned char*) image->pixels;
    XcursorPixel* target = native->pixels;

    for (int i = 0;  i < image->width * image->height;  i++, target++, source += 4)
    {
        unsigned int alpha = source[3];

        *target = (alpha << 24) |
                  ((unsigned char) ((source[0] * alpha) / 255) << 16) |
                  ((unsigned char) ((source[1] * alpha) / 255) <<  8) |
                  ((unsigned char) ((source[2] * alpha) / 255) <<  0);
    }

    cursor = XcursorImageLoadCursor(_desktop_window.x11.display, native);
    XcursorImageDestroy(native);

    return cursor;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowConnectX11(int platformID, _DESKTOP_WINDOWplatform* platform)
{
    const _DESKTOP_WINDOWplatform x11 =
    {
        .platformID = DESKTOP_WINDOW_PLATFORM_X11,
        .init = _desktop_windowInitX11,
        .terminate = _desktop_windowTerminateX11,
        .getCursorPos = _desktop_windowGetCursorPosX11,
        .setCursorPos = _desktop_windowSetCursorPosX11,
        .setCursorMode = _desktop_windowSetCursorModeX11,
        .setRawMouseMotion = _desktop_windowSetRawMouseMotionX11,
        .rawMouseMotionSupported = _desktop_windowRawMouseMotionSupportedX11,
        .createCursor = _desktop_windowCreateCursorX11,
        .createStandardCursor = _desktop_windowCreateStandardCursorX11,
        .destroyCursor = _desktop_windowDestroyCursorX11,
        .setCursor = _desktop_windowSetCursorX11,
        .getScancodeName = _desktop_windowGetScancodeNameX11,
        .getKeyScancode = _desktop_windowGetKeyScancodeX11,
        .setClipboardString = _desktop_windowSetClipboardStringX11,
        .getClipboardString = _desktop_windowGetClipboardStringX11,
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
        .freeMonitor = _desktop_windowFreeMonitorX11,
        .getMonitorPos = _desktop_windowGetMonitorPosX11,
        .getMonitorContentScale = _desktop_windowGetMonitorContentScaleX11,
        .getMonitorWorkarea = _desktop_windowGetMonitorWorkareaX11,
        .getVideoModes = _desktop_windowGetVideoModesX11,
        .getVideoMode = _desktop_windowGetVideoModeX11,
        .getGammaRamp = _desktop_windowGetGammaRampX11,
        .setGammaRamp = _desktop_windowSetGammaRampX11,
        .createWindow = _desktop_windowCreateWindowX11,
        .destroyWindow = _desktop_windowDestroyWindowX11,
        .setWindowTitle = _desktop_windowSetWindowTitleX11,
        .setWindowIcon = _desktop_windowSetWindowIconX11,
        .getWindowPos = _desktop_windowGetWindowPosX11,
        .setWindowPos = _desktop_windowSetWindowPosX11,
        .getWindowSize = _desktop_windowGetWindowSizeX11,
        .setWindowSize = _desktop_windowSetWindowSizeX11,
        .setWindowSizeLimits = _desktop_windowSetWindowSizeLimitsX11,
        .setWindowAspectRatio = _desktop_windowSetWindowAspectRatioX11,
        .getFramebufferSize = _desktop_windowGetFramebufferSizeX11,
        .getWindowFrameSize = _desktop_windowGetWindowFrameSizeX11,
        .getWindowContentScale = _desktop_windowGetWindowContentScaleX11,
        .iconifyWindow = _desktop_windowIconifyWindowX11,
        .restoreWindow = _desktop_windowRestoreWindowX11,
        .maximizeWindow = _desktop_windowMaximizeWindowX11,
        .showWindow = _desktop_windowShowWindowX11,
        .hideWindow = _desktop_windowHideWindowX11,
        .requestWindowAttention = _desktop_windowRequestWindowAttentionX11,
        .focusWindow = _desktop_windowFocusWindowX11,
        .setWindowMonitor = _desktop_windowSetWindowMonitorX11,
        .windowFocused = _desktop_windowWindowFocusedX11,
        .windowIconified = _desktop_windowWindowIconifiedX11,
        .windowVisible = _desktop_windowWindowVisibleX11,
        .windowMaximized = _desktop_windowWindowMaximizedX11,
        .windowHovered = _desktop_windowWindowHoveredX11,
        .framebufferTransparent = _desktop_windowFramebufferTransparentX11,
        .getWindowOpacity = _desktop_windowGetWindowOpacityX11,
        .setWindowResizable = _desktop_windowSetWindowResizableX11,
        .setWindowDecorated = _desktop_windowSetWindowDecoratedX11,
        .setWindowFloating = _desktop_windowSetWindowFloatingX11,
        .setWindowOpacity = _desktop_windowSetWindowOpacityX11,
        .setWindowMousePassthrough = _desktop_windowSetWindowMousePassthroughX11,
        .pollEvents = _desktop_windowPollEventsX11,
        .waitEvents = _desktop_windowWaitEventsX11,
        .waitEventsTimeout = _desktop_windowWaitEventsTimeoutX11,
        .postEmptyEvent = _desktop_windowPostEmptyEventX11,
        .getEGLPlatform = _desktop_windowGetEGLPlatformX11,
        .getEGLNativeDisplay = _desktop_windowGetEGLNativeDisplayX11,
        .getEGLNativeWindow = _desktop_windowGetEGLNativeWindowX11,
        .getRequiredInstanceExtensions = _desktop_windowGetRequiredInstanceExtensionsX11,
        .getPhysicalDevicePresentationSupport = _desktop_windowGetPhysicalDevicePresentationSupportX11,
        .createWindowSurface = _desktop_windowCreateWindowSurfaceX11
    };

    // HACK: If the application has left the locale as "C" then both wide
    //       character text input and explicit UTF-8 input via XIM will break
    //       This sets the CTYPE part of the current locale from the environment
    //       in the hope that it is set to something more sane than "C"
    if (strcmp(setlocale(LC_CTYPE, NULL), "C") == 0)
        setlocale(LC_CTYPE, "");

#if defined(__CYGWIN__)
    void* module = _desktop_windowPlatformLoadModule("libX11-6.so");
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    void* module = _desktop_windowPlatformLoadModule("libX11.so");
#else
    void* module = _desktop_windowPlatformLoadModule("libX11.so.6");
#endif
    if (!module)
    {
        if (platformID == DESKTOP_WINDOW_PLATFORM_X11)
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "X11: Failed to load Xlib");

        return DESKTOP_WINDOW_FALSE;
    }

    PFN_XInitThreads XInitThreads = (PFN_XInitThreads)
        _desktop_windowPlatformGetModuleSymbol(module, "XInitThreads");
    PFN_XrmInitialize XrmInitialize = (PFN_XrmInitialize)
        _desktop_windowPlatformGetModuleSymbol(module, "XrmInitialize");
    PFN_XOpenDisplay XOpenDisplay = (PFN_XOpenDisplay)
        _desktop_windowPlatformGetModuleSymbol(module, "XOpenDisplay");
    if (!XInitThreads || !XrmInitialize || !XOpenDisplay)
    {
        if (platformID == DESKTOP_WINDOW_PLATFORM_X11)
            _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "X11: Failed to load Xlib entry point");

        _desktop_windowPlatformFreeModule(module);
        return DESKTOP_WINDOW_FALSE;
    }

    XInitThreads();
    XrmInitialize();

    Display* display = XOpenDisplay(NULL);
    if (!display)
    {
        if (platformID == DESKTOP_WINDOW_PLATFORM_X11)
        {
            const char* name = getenv("DISPLAY");
            if (name)
            {
                _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE,
                                "X11: Failed to open display %s", name);
            }
            else
            {
                _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_UNAVAILABLE,
                                "X11: The DISPLAY environment variable is missing");
            }
        }

        _desktop_windowPlatformFreeModule(module);
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_window.x11.display = display;
    _desktop_window.x11.xlib.handle = module;

    *platform = x11;
    return DESKTOP_WINDOW_TRUE;
}

int _desktop_windowInitX11(void)
{
    _desktop_window.x11.xlib.AllocClassHint = (PFN_XAllocClassHint)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XAllocClassHint");
    _desktop_window.x11.xlib.AllocSizeHints = (PFN_XAllocSizeHints)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XAllocSizeHints");
    _desktop_window.x11.xlib.AllocWMHints = (PFN_XAllocWMHints)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XAllocWMHints");
    _desktop_window.x11.xlib.ChangeProperty = (PFN_XChangeProperty)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XChangeProperty");
    _desktop_window.x11.xlib.ChangeWindowAttributes = (PFN_XChangeWindowAttributes)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XChangeWindowAttributes");
    _desktop_window.x11.xlib.CheckIfEvent = (PFN_XCheckIfEvent)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XCheckIfEvent");
    _desktop_window.x11.xlib.CheckTypedWindowEvent = (PFN_XCheckTypedWindowEvent)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XCheckTypedWindowEvent");
    _desktop_window.x11.xlib.CloseDisplay = (PFN_XCloseDisplay)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XCloseDisplay");
    _desktop_window.x11.xlib.CloseIM = (PFN_XCloseIM)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XCloseIM");
    _desktop_window.x11.xlib.ConvertSelection = (PFN_XConvertSelection)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XConvertSelection");
    _desktop_window.x11.xlib.CreateColormap = (PFN_XCreateColormap)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XCreateColormap");
    _desktop_window.x11.xlib.CreateFontCursor = (PFN_XCreateFontCursor)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XCreateFontCursor");
    _desktop_window.x11.xlib.CreateIC = (PFN_XCreateIC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XCreateIC");
    _desktop_window.x11.xlib.CreateRegion = (PFN_XCreateRegion)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XCreateRegion");
    _desktop_window.x11.xlib.CreateWindow = (PFN_XCreateWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XCreateWindow");
    _desktop_window.x11.xlib.DefineCursor = (PFN_XDefineCursor)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XDefineCursor");
    _desktop_window.x11.xlib.DeleteContext = (PFN_XDeleteContext)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XDeleteContext");
    _desktop_window.x11.xlib.DeleteProperty = (PFN_XDeleteProperty)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XDeleteProperty");
    _desktop_window.x11.xlib.DestroyIC = (PFN_XDestroyIC)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XDestroyIC");
    _desktop_window.x11.xlib.DestroyRegion = (PFN_XDestroyRegion)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XDestroyRegion");
    _desktop_window.x11.xlib.DestroyWindow = (PFN_XDestroyWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XDestroyWindow");
    _desktop_window.x11.xlib.DisplayKeycodes = (PFN_XDisplayKeycodes)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XDisplayKeycodes");
    _desktop_window.x11.xlib.EventsQueued = (PFN_XEventsQueued)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XEventsQueued");
    _desktop_window.x11.xlib.FilterEvent = (PFN_XFilterEvent)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XFilterEvent");
    _desktop_window.x11.xlib.FindContext = (PFN_XFindContext)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XFindContext");
    _desktop_window.x11.xlib.Flush = (PFN_XFlush)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XFlush");
    _desktop_window.x11.xlib.Free = (PFN_XFree)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XFree");
    _desktop_window.x11.xlib.FreeColormap = (PFN_XFreeColormap)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XFreeColormap");
    _desktop_window.x11.xlib.FreeCursor = (PFN_XFreeCursor)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XFreeCursor");
    _desktop_window.x11.xlib.FreeEventData = (PFN_XFreeEventData)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XFreeEventData");
    _desktop_window.x11.xlib.GetErrorText = (PFN_XGetErrorText)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetErrorText");
    _desktop_window.x11.xlib.GetEventData = (PFN_XGetEventData)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetEventData");
    _desktop_window.x11.xlib.GetICValues = (PFN_XGetICValues)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetICValues");
    _desktop_window.x11.xlib.GetIMValues = (PFN_XGetIMValues)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetIMValues");
    _desktop_window.x11.xlib.GetInputFocus = (PFN_XGetInputFocus)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetInputFocus");
    _desktop_window.x11.xlib.GetKeyboardMapping = (PFN_XGetKeyboardMapping)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetKeyboardMapping");
    _desktop_window.x11.xlib.GetScreenSaver = (PFN_XGetScreenSaver)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetScreenSaver");
    _desktop_window.x11.xlib.GetSelectionOwner = (PFN_XGetSelectionOwner)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetSelectionOwner");
    _desktop_window.x11.xlib.GetVisualInfo = (PFN_XGetVisualInfo)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetVisualInfo");
    _desktop_window.x11.xlib.GetWMNormalHints = (PFN_XGetWMNormalHints)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetWMNormalHints");
    _desktop_window.x11.xlib.GetWindowAttributes = (PFN_XGetWindowAttributes)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetWindowAttributes");
    _desktop_window.x11.xlib.GetWindowProperty = (PFN_XGetWindowProperty)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGetWindowProperty");
    _desktop_window.x11.xlib.GrabPointer = (PFN_XGrabPointer)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XGrabPointer");
    _desktop_window.x11.xlib.IconifyWindow = (PFN_XIconifyWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XIconifyWindow");
    _desktop_window.x11.xlib.InternAtom = (PFN_XInternAtom)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XInternAtom");
    _desktop_window.x11.xlib.LookupString = (PFN_XLookupString)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XLookupString");
    _desktop_window.x11.xlib.MapRaised = (PFN_XMapRaised)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XMapRaised");
    _desktop_window.x11.xlib.MapWindow = (PFN_XMapWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XMapWindow");
    _desktop_window.x11.xlib.MoveResizeWindow = (PFN_XMoveResizeWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XMoveResizeWindow");
    _desktop_window.x11.xlib.MoveWindow = (PFN_XMoveWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XMoveWindow");
    _desktop_window.x11.xlib.NextEvent = (PFN_XNextEvent)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XNextEvent");
    _desktop_window.x11.xlib.OpenIM = (PFN_XOpenIM)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XOpenIM");
    _desktop_window.x11.xlib.PeekEvent = (PFN_XPeekEvent)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XPeekEvent");
    _desktop_window.x11.xlib.Pending = (PFN_XPending)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XPending");
    _desktop_window.x11.xlib.QueryExtension = (PFN_XQueryExtension)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XQueryExtension");
    _desktop_window.x11.xlib.QueryPointer = (PFN_XQueryPointer)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XQueryPointer");
    _desktop_window.x11.xlib.RaiseWindow = (PFN_XRaiseWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XRaiseWindow");
    _desktop_window.x11.xlib.RegisterIMInstantiateCallback = (PFN_XRegisterIMInstantiateCallback)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XRegisterIMInstantiateCallback");
    _desktop_window.x11.xlib.ResizeWindow = (PFN_XResizeWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XResizeWindow");
    _desktop_window.x11.xlib.ResourceManagerString = (PFN_XResourceManagerString)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XResourceManagerString");
    _desktop_window.x11.xlib.SaveContext = (PFN_XSaveContext)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSaveContext");
    _desktop_window.x11.xlib.SelectInput = (PFN_XSelectInput)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSelectInput");
    _desktop_window.x11.xlib.SendEvent = (PFN_XSendEvent)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSendEvent");
    _desktop_window.x11.xlib.SetClassHint = (PFN_XSetClassHint)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetClassHint");
    _desktop_window.x11.xlib.SetErrorHandler = (PFN_XSetErrorHandler)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetErrorHandler");
    _desktop_window.x11.xlib.SetICFocus = (PFN_XSetICFocus)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetICFocus");
    _desktop_window.x11.xlib.SetIMValues = (PFN_XSetIMValues)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetIMValues");
    _desktop_window.x11.xlib.SetInputFocus = (PFN_XSetInputFocus)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetInputFocus");
    _desktop_window.x11.xlib.SetLocaleModifiers = (PFN_XSetLocaleModifiers)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetLocaleModifiers");
    _desktop_window.x11.xlib.SetScreenSaver = (PFN_XSetScreenSaver)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetScreenSaver");
    _desktop_window.x11.xlib.SetSelectionOwner = (PFN_XSetSelectionOwner)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetSelectionOwner");
    _desktop_window.x11.xlib.SetWMHints = (PFN_XSetWMHints)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetWMHints");
    _desktop_window.x11.xlib.SetWMNormalHints = (PFN_XSetWMNormalHints)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetWMNormalHints");
    _desktop_window.x11.xlib.SetWMProtocols = (PFN_XSetWMProtocols)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSetWMProtocols");
    _desktop_window.x11.xlib.SupportsLocale = (PFN_XSupportsLocale)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSupportsLocale");
    _desktop_window.x11.xlib.Sync = (PFN_XSync)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XSync");
    _desktop_window.x11.xlib.TranslateCoordinates = (PFN_XTranslateCoordinates)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XTranslateCoordinates");
    _desktop_window.x11.xlib.UndefineCursor = (PFN_XUndefineCursor)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XUndefineCursor");
    _desktop_window.x11.xlib.UngrabPointer = (PFN_XUngrabPointer)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XUngrabPointer");
    _desktop_window.x11.xlib.UnmapWindow = (PFN_XUnmapWindow)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XUnmapWindow");
    _desktop_window.x11.xlib.UnsetICFocus = (PFN_XUnsetICFocus)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XUnsetICFocus");
    _desktop_window.x11.xlib.VisualIDFromVisual = (PFN_XVisualIDFromVisual)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XVisualIDFromVisual");
    _desktop_window.x11.xlib.WarpPointer = (PFN_XWarpPointer)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XWarpPointer");
    _desktop_window.x11.xkb.FreeKeyboard = (PFN_XkbFreeKeyboard)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XkbFreeKeyboard");
    _desktop_window.x11.xkb.FreeNames = (PFN_XkbFreeNames)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XkbFreeNames");
    _desktop_window.x11.xkb.GetMap = (PFN_XkbGetMap)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XkbGetMap");
    _desktop_window.x11.xkb.GetNames = (PFN_XkbGetNames)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XkbGetNames");
    _desktop_window.x11.xkb.GetState = (PFN_XkbGetState)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XkbGetState");
    _desktop_window.x11.xkb.KeycodeToKeysym = (PFN_XkbKeycodeToKeysym)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XkbKeycodeToKeysym");
    _desktop_window.x11.xkb.QueryExtension = (PFN_XkbQueryExtension)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XkbQueryExtension");
    _desktop_window.x11.xkb.SelectEventDetails = (PFN_XkbSelectEventDetails)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XkbSelectEventDetails");
    _desktop_window.x11.xkb.SetDetectableAutoRepeat = (PFN_XkbSetDetectableAutoRepeat)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XkbSetDetectableAutoRepeat");
    _desktop_window.x11.xrm.DestroyDatabase = (PFN_XrmDestroyDatabase)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XrmDestroyDatabase");
    _desktop_window.x11.xrm.GetResource = (PFN_XrmGetResource)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XrmGetResource");
    _desktop_window.x11.xrm.GetStringDatabase = (PFN_XrmGetStringDatabase)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XrmGetStringDatabase");
    _desktop_window.x11.xrm.UniqueQuark = (PFN_XrmUniqueQuark)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XrmUniqueQuark");
    _desktop_window.x11.xlib.UnregisterIMInstantiateCallback = (PFN_XUnregisterIMInstantiateCallback)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "XUnregisterIMInstantiateCallback");
    _desktop_window.x11.xlib.utf8LookupString = (PFN_Xutf8LookupString)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "Xutf8LookupString");
    _desktop_window.x11.xlib.utf8SetWMProperties = (PFN_Xutf8SetWMProperties)
        _desktop_windowPlatformGetModuleSymbol(_desktop_window.x11.xlib.handle, "Xutf8SetWMProperties");

    if (_desktop_window.x11.xlib.utf8LookupString && _desktop_window.x11.xlib.utf8SetWMProperties)
        _desktop_window.x11.xlib.utf8 = DESKTOP_WINDOW_TRUE;

    _desktop_window.x11.screen = DefaultScreen(_desktop_window.x11.display);
    _desktop_window.x11.root = RootWindow(_desktop_window.x11.display, _desktop_window.x11.screen);
    _desktop_window.x11.context = XUniqueContext();

    getSystemContentScale(&_desktop_window.x11.contentScaleX, &_desktop_window.x11.contentScaleY);

    if (!createEmptyEventPipe())
        return DESKTOP_WINDOW_FALSE;

    if (!initExtensions())
        return DESKTOP_WINDOW_FALSE;

    _desktop_window.x11.helperWindowHandle = createHelperWindow();
    _desktop_window.x11.hiddenCursorHandle = createHiddenCursor();

    if (XSupportsLocale() && _desktop_window.x11.xlib.utf8)
    {
        XSetLocaleModifiers("");

        // If an IM is already present our callback will be called right away
        XRegisterIMInstantiateCallback(_desktop_window.x11.display,
                                       NULL, NULL, NULL,
                                       inputMethodInstantiateCallback,
                                       NULL);
    }

    _desktop_windowPollMonitorsX11();
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateX11(void)
{
    if (_desktop_window.x11.helperWindowHandle)
    {
        if (XGetSelectionOwner(_desktop_window.x11.display, _desktop_window.x11.CLIPBOARD) ==
            _desktop_window.x11.helperWindowHandle)
        {
            _desktop_windowPushSelectionToManagerX11();
        }

        XDestroyWindow(_desktop_window.x11.display, _desktop_window.x11.helperWindowHandle);
        _desktop_window.x11.helperWindowHandle = None;
    }

    if (_desktop_window.x11.hiddenCursorHandle)
    {
        XFreeCursor(_desktop_window.x11.display, _desktop_window.x11.hiddenCursorHandle);
        _desktop_window.x11.hiddenCursorHandle = (Cursor) 0;
    }

    _desktop_window_free(_desktop_window.x11.primarySelectionString);
    _desktop_window_free(_desktop_window.x11.clipboardString);

    XUnregisterIMInstantiateCallback(_desktop_window.x11.display,
                                     NULL, NULL, NULL,
                                     inputMethodInstantiateCallback,
                                     NULL);

    if (_desktop_window.x11.im)
    {
        XCloseIM(_desktop_window.x11.im);
        _desktop_window.x11.im = NULL;
    }

    if (_desktop_window.x11.display)
    {
        XCloseDisplay(_desktop_window.x11.display);
        _desktop_window.x11.display = NULL;
    }

    if (_desktop_window.x11.x11xcb.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.x11.x11xcb.handle);
        _desktop_window.x11.x11xcb.handle = NULL;
    }

    if (_desktop_window.x11.xcursor.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.x11.xcursor.handle);
        _desktop_window.x11.xcursor.handle = NULL;
    }

    if (_desktop_window.x11.randr.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.x11.randr.handle);
        _desktop_window.x11.randr.handle = NULL;
    }

    if (_desktop_window.x11.xinerama.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.x11.xinerama.handle);
        _desktop_window.x11.xinerama.handle = NULL;
    }

    if (_desktop_window.x11.xrender.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.x11.xrender.handle);
        _desktop_window.x11.xrender.handle = NULL;
    }

    if (_desktop_window.x11.vidmode.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.x11.vidmode.handle);
        _desktop_window.x11.vidmode.handle = NULL;
    }

    if (_desktop_window.x11.xi.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.x11.xi.handle);
        _desktop_window.x11.xi.handle = NULL;
    }

    _desktop_windowTerminateOSMesa();
    // NOTE: These need to be unloaded after XCloseDisplay, as they register
    //       cleanup callbacks that get called by that function
    _desktop_windowTerminateEGL();
    _desktop_windowTerminateGLX();

    if (_desktop_window.x11.xlib.handle)
    {
        _desktop_windowPlatformFreeModule(_desktop_window.x11.xlib.handle);
        _desktop_window.x11.xlib.handle = NULL;
    }

    if (_desktop_window.x11.emptyEventPipe[0] || _desktop_window.x11.emptyEventPipe[1])
    {
        close(_desktop_window.x11.emptyEventPipe[0]);
        close(_desktop_window.x11.emptyEventPipe[1]);
    }
}

#endif // _DESKTOP_WINDOW_X11

