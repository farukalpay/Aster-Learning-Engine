#include "internal.h"
#include "mappings.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Internal key state used for sticky keys
#define _DESKTOP_WINDOW_STICK 3

// Internal constants for gamepad mapping source types
#define _DESKTOP_WINDOW_JOYSTICK_AXIS     1
#define _DESKTOP_WINDOW_JOYSTICK_BUTTON   2
#define _DESKTOP_WINDOW_JOYSTICK_HATBIT   3

#define DESKTOP_WINDOW_MOD_MASK (DESKTOP_WINDOW_MOD_SHIFT | \
                       DESKTOP_WINDOW_MOD_CONTROL | \
                       DESKTOP_WINDOW_MOD_ALT | \
                       DESKTOP_WINDOW_MOD_SUPER | \
                       DESKTOP_WINDOW_MOD_CAPS_LOCK | \
                       DESKTOP_WINDOW_MOD_NUM_LOCK)

// Initializes the platform joystick API if it has not been already
//
static DESKTOP_WINDOWbool initJoysticks(void)
{
    if (!_desktop_window.joysticksInitialized)
    {
        if (!_desktop_window.platform.initJoysticks())
        {
            _desktop_window.platform.terminateJoysticks();
            return DESKTOP_WINDOW_FALSE;
        }
    }

    return _desktop_window.joysticksInitialized = DESKTOP_WINDOW_TRUE;
}

// Finds a mapping based on joystick GUID
//
static _DESKTOP_WINDOWmapping* findMapping(const char* guid)
{
    int i;

    for (i = 0;  i < _desktop_window.mappingCount;  i++)
    {
        if (strcmp(_desktop_window.mappings[i].guid, guid) == 0)
            return _desktop_window.mappings + i;
    }

    return NULL;
}

// Checks whether a gamepad mapping element is present in the hardware
//
static DESKTOP_WINDOWbool isValidElementForJoystick(const _DESKTOP_WINDOWmapelement* e,
                                          const _DESKTOP_WINDOWjoystick* js)
{
    if (e->type == _DESKTOP_WINDOW_JOYSTICK_HATBIT && (e->index >> 4) >= js->hatCount)
        return DESKTOP_WINDOW_FALSE;
    else if (e->type == _DESKTOP_WINDOW_JOYSTICK_BUTTON && e->index >= js->buttonCount)
        return DESKTOP_WINDOW_FALSE;
    else if (e->type == _DESKTOP_WINDOW_JOYSTICK_AXIS && e->index >= js->axisCount)
        return DESKTOP_WINDOW_FALSE;

    return DESKTOP_WINDOW_TRUE;
}

// Finds a mapping based on joystick GUID and verifies element indices
//
static _DESKTOP_WINDOWmapping* findValidMapping(const _DESKTOP_WINDOWjoystick* js)
{
    _DESKTOP_WINDOWmapping* mapping = findMapping(js->guid);
    if (mapping)
    {
        int i;

        for (i = 0;  i <= DESKTOP_WINDOW_GAMEPAD_BUTTON_LAST;  i++)
        {
            if (!isValidElementForJoystick(mapping->buttons + i, js))
                return NULL;
        }

        for (i = 0;  i <= DESKTOP_WINDOW_GAMEPAD_AXIS_LAST;  i++)
        {
            if (!isValidElementForJoystick(mapping->axes + i, js))
                return NULL;
        }
    }

    return mapping;
}

// Parses an SDL_GameControllerDB line and adds it to the mapping list
//
static DESKTOP_WINDOWbool parseMapping(_DESKTOP_WINDOWmapping* mapping, const char* string)
{
    const char* c = string;
    size_t i, length;
    struct
    {
        const char* name;
        _DESKTOP_WINDOWmapelement* element;
    } fields[] =
    {
        { "platform",      NULL },
        { "a",             mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_A },
        { "b",             mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_B },
        { "x",             mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_X },
        { "y",             mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_Y },
        { "back",          mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_BACK },
        { "start",         mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_START },
        { "guide",         mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_GUIDE },
        { "leftshoulder",  mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_LEFT_BUMPER },
        { "rightshoulder", mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_RIGHT_BUMPER },
        { "leftstick",     mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_LEFT_THUMB },
        { "rightstick",    mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_RIGHT_THUMB },
        { "dpup",          mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_DPAD_UP },
        { "dpright",       mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_DPAD_RIGHT },
        { "dpdown",        mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_DPAD_DOWN },
        { "dpleft",        mapping->buttons + DESKTOP_WINDOW_GAMEPAD_BUTTON_DPAD_LEFT },
        { "lefttrigger",   mapping->axes + DESKTOP_WINDOW_GAMEPAD_AXIS_LEFT_TRIGGER },
        { "righttrigger",  mapping->axes + DESKTOP_WINDOW_GAMEPAD_AXIS_RIGHT_TRIGGER },
        { "leftx",         mapping->axes + DESKTOP_WINDOW_GAMEPAD_AXIS_LEFT_X },
        { "lefty",         mapping->axes + DESKTOP_WINDOW_GAMEPAD_AXIS_LEFT_Y },
        { "rightx",        mapping->axes + DESKTOP_WINDOW_GAMEPAD_AXIS_RIGHT_X },
        { "righty",        mapping->axes + DESKTOP_WINDOW_GAMEPAD_AXIS_RIGHT_Y }
    };

    length = strcspn(c, ",");
    if (length != 32 || c[length] != ',')
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, NULL);
        return DESKTOP_WINDOW_FALSE;
    }

    memcpy(mapping->guid, c, length);
    c += length + 1;

    length = strcspn(c, ",");
    if (length >= sizeof(mapping->name) || c[length] != ',')
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, NULL);
        return DESKTOP_WINDOW_FALSE;
    }

    memcpy(mapping->name, c, length);
    c += length + 1;

    while (*c)
    {
        // TODO: Implement output modifiers
        if (*c == '+' || *c == '-')
            return DESKTOP_WINDOW_FALSE;

        for (i = 0;  i < sizeof(fields) / sizeof(fields[0]);  i++)
        {
            length = strlen(fields[i].name);
            if (strncmp(c, fields[i].name, length) != 0 || c[length] != ':')
                continue;

            c += length + 1;

            if (fields[i].element)
            {
                _DESKTOP_WINDOWmapelement* e = fields[i].element;
                int8_t minimum = -1;
                int8_t maximum = 1;

                if (*c == '+')
                {
                    minimum = 0;
                    c += 1;
                }
                else if (*c == '-')
                {
                    maximum = 0;
                    c += 1;
                }

                if (*c == 'a')
                    e->type = _DESKTOP_WINDOW_JOYSTICK_AXIS;
                else if (*c == 'b')
                    e->type = _DESKTOP_WINDOW_JOYSTICK_BUTTON;
                else if (*c == 'h')
                    e->type = _DESKTOP_WINDOW_JOYSTICK_HATBIT;
                else
                    break;

                if (e->type == _DESKTOP_WINDOW_JOYSTICK_HATBIT)
                {
                    const unsigned long hat = strtoul(c + 1, (char**) &c, 10);
                    const unsigned long bit = strtoul(c + 1, (char**) &c, 10);
                    e->index = (uint8_t) ((hat << 4) | bit);
                }
                else
                    e->index = (uint8_t) strtoul(c + 1, (char**) &c, 10);

                if (e->type == _DESKTOP_WINDOW_JOYSTICK_AXIS)
                {
                    e->axisScale = 2 / (maximum - minimum);
                    e->axisOffset = -(maximum + minimum);

                    if (*c == '~')
                    {
                        e->axisScale = -e->axisScale;
                        e->axisOffset = -e->axisOffset;
                    }
                }
            }
            else
            {
                const char* name = _desktop_window.platform.getMappingName();
                length = strlen(name);
                if (strncmp(c, name, length) != 0)
                    return DESKTOP_WINDOW_FALSE;
            }

            break;
        }

        c += strcspn(c, ",");
        c += strspn(c, ",");
    }

    for (i = 0;  i < 32;  i++)
    {
        if (mapping->guid[i] >= 'A' && mapping->guid[i] <= 'F')
            mapping->guid[i] += 'a' - 'A';
    }

    _desktop_window.platform.updateGamepadGUID(mapping->guid);
    return DESKTOP_WINDOW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                         DESKTOP_WINDOW event API                       //////
//////////////////////////////////////////////////////////////////////////

// Notifies shared code of a physical key event
//
void _desktop_windowInputKey(_DESKTOP_WINDOWwindow* window, int key, int scancode, int action, int mods)
{
    assert(window != NULL);
    assert(key >= 0 || key == DESKTOP_WINDOW_KEY_UNKNOWN);
    assert(key <= DESKTOP_WINDOW_KEY_LAST);
    assert(action == DESKTOP_WINDOW_PRESS || action == DESKTOP_WINDOW_RELEASE);
    assert(mods == (mods & DESKTOP_WINDOW_MOD_MASK));

    if (key >= 0 && key <= DESKTOP_WINDOW_KEY_LAST)
    {
        DESKTOP_WINDOWbool repeated = DESKTOP_WINDOW_FALSE;

        if (action == DESKTOP_WINDOW_RELEASE && window->keys[key] == DESKTOP_WINDOW_RELEASE)
            return;

        if (action == DESKTOP_WINDOW_PRESS && window->keys[key] == DESKTOP_WINDOW_PRESS)
            repeated = DESKTOP_WINDOW_TRUE;

        if (action == DESKTOP_WINDOW_RELEASE && window->stickyKeys)
            window->keys[key] = _DESKTOP_WINDOW_STICK;
        else
            window->keys[key] = (char) action;

        if (repeated)
            action = DESKTOP_WINDOW_REPEAT;
    }

    if (!window->lockKeyMods)
        mods &= ~(DESKTOP_WINDOW_MOD_CAPS_LOCK | DESKTOP_WINDOW_MOD_NUM_LOCK);

    if (window->callbacks.key)
        window->callbacks.key((DESKTOP_WINDOWwindow*) window, key, scancode, action, mods);
}

// Notifies shared code of a Unicode codepoint input event
// The 'plain' parameter determines whether to emit a regular character event
//
void _desktop_windowInputChar(_DESKTOP_WINDOWwindow* window, uint32_t codepoint, int mods, DESKTOP_WINDOWbool plain)
{
    assert(window != NULL);
    assert(mods == (mods & DESKTOP_WINDOW_MOD_MASK));
    assert(plain == DESKTOP_WINDOW_TRUE || plain == DESKTOP_WINDOW_FALSE);

    if (codepoint < 32 || (codepoint > 126 && codepoint < 160))
        return;

    if (!window->lockKeyMods)
        mods &= ~(DESKTOP_WINDOW_MOD_CAPS_LOCK | DESKTOP_WINDOW_MOD_NUM_LOCK);

    if (window->callbacks.charmods)
        window->callbacks.charmods((DESKTOP_WINDOWwindow*) window, codepoint, mods);

    if (plain)
    {
        if (window->callbacks.character)
            window->callbacks.character((DESKTOP_WINDOWwindow*) window, codepoint);
    }
}

// Notifies shared code of a scroll event
//
void _desktop_windowInputScroll(_DESKTOP_WINDOWwindow* window, double xoffset, double yoffset)
{
    assert(window != NULL);
    assert(xoffset > -FLT_MAX);
    assert(xoffset < FLT_MAX);
    assert(yoffset > -FLT_MAX);
    assert(yoffset < FLT_MAX);

    if (window->callbacks.scroll)
        window->callbacks.scroll((DESKTOP_WINDOWwindow*) window, xoffset, yoffset);
}

// Notifies shared code of a mouse button click event
//
void _desktop_windowInputMouseClick(_DESKTOP_WINDOWwindow* window, int button, int action, int mods)
{
    assert(window != NULL);
    assert(button >= 0);
    assert(button <= DESKTOP_WINDOW_MOUSE_BUTTON_LAST);
    assert(action == DESKTOP_WINDOW_PRESS || action == DESKTOP_WINDOW_RELEASE);
    assert(mods == (mods & DESKTOP_WINDOW_MOD_MASK));

    if (button < 0 || button > DESKTOP_WINDOW_MOUSE_BUTTON_LAST)
        return;

    if (!window->lockKeyMods)
        mods &= ~(DESKTOP_WINDOW_MOD_CAPS_LOCK | DESKTOP_WINDOW_MOD_NUM_LOCK);

    if (action == DESKTOP_WINDOW_RELEASE && window->stickyMouseButtons)
        window->mouseButtons[button] = _DESKTOP_WINDOW_STICK;
    else
        window->mouseButtons[button] = (char) action;

    if (window->callbacks.mouseButton)
        window->callbacks.mouseButton((DESKTOP_WINDOWwindow*) window, button, action, mods);
}

// Notifies shared code of a cursor motion event
// The position is specified in content area relative screen coordinates
//
void _desktop_windowInputCursorPos(_DESKTOP_WINDOWwindow* window, double xpos, double ypos)
{
    assert(window != NULL);
    assert(xpos > -FLT_MAX);
    assert(xpos < FLT_MAX);
    assert(ypos > -FLT_MAX);
    assert(ypos < FLT_MAX);

    if (window->virtualCursorPosX == xpos && window->virtualCursorPosY == ypos)
        return;

    window->virtualCursorPosX = xpos;
    window->virtualCursorPosY = ypos;

    if (window->callbacks.cursorPos)
        window->callbacks.cursorPos((DESKTOP_WINDOWwindow*) window, xpos, ypos);
}

// Notifies shared code of a cursor enter/leave event
//
void _desktop_windowInputCursorEnter(_DESKTOP_WINDOWwindow* window, DESKTOP_WINDOWbool entered)
{
    assert(window != NULL);
    assert(entered == DESKTOP_WINDOW_TRUE || entered == DESKTOP_WINDOW_FALSE);

    if (window->callbacks.cursorEnter)
        window->callbacks.cursorEnter((DESKTOP_WINDOWwindow*) window, entered);
}

// Notifies shared code of files or directories dropped on a window
//
void _desktop_windowInputDrop(_DESKTOP_WINDOWwindow* window, int count, const char** paths)
{
    assert(window != NULL);
    assert(count > 0);
    assert(paths != NULL);

    if (window->callbacks.drop)
        window->callbacks.drop((DESKTOP_WINDOWwindow*) window, count, paths);
}

// Notifies shared code of a joystick connection or disconnection
//
void _desktop_windowInputJoystick(_DESKTOP_WINDOWjoystick* js, int event)
{
    assert(js != NULL);
    assert(event == DESKTOP_WINDOW_CONNECTED || event == DESKTOP_WINDOW_DISCONNECTED);

    if (event == DESKTOP_WINDOW_CONNECTED)
        js->connected = DESKTOP_WINDOW_TRUE;
    else if (event == DESKTOP_WINDOW_DISCONNECTED)
        js->connected = DESKTOP_WINDOW_FALSE;

    if (_desktop_window.callbacks.joystick)
        _desktop_window.callbacks.joystick((int) (js - _desktop_window.joysticks), event);
}

// Notifies shared code of the new value of a joystick axis
//
void _desktop_windowInputJoystickAxis(_DESKTOP_WINDOWjoystick* js, int axis, float value)
{
    assert(js != NULL);
    assert(axis >= 0);
    assert(axis < js->axisCount);

    js->axes[axis] = value;
}

// Notifies shared code of the new value of a joystick button
//
void _desktop_windowInputJoystickButton(_DESKTOP_WINDOWjoystick* js, int button, char value)
{
    assert(js != NULL);
    assert(button >= 0);
    assert(button < js->buttonCount);
    assert(value == DESKTOP_WINDOW_PRESS || value == DESKTOP_WINDOW_RELEASE);

    js->buttons[button] = value;
}

// Notifies shared code of the new value of a joystick hat
//
void _desktop_windowInputJoystickHat(_DESKTOP_WINDOWjoystick* js, int hat, char value)
{
    int base;

    assert(js != NULL);
    assert(hat >= 0);
    assert(hat < js->hatCount);

    // Valid hat values only use the least significant nibble
    assert((value & 0xf0) == 0);
    // Valid hat values do not have both bits of an axis set
    assert((value & DESKTOP_WINDOW_HAT_LEFT) == 0 || (value & DESKTOP_WINDOW_HAT_RIGHT) == 0);
    assert((value & DESKTOP_WINDOW_HAT_UP) == 0 || (value & DESKTOP_WINDOW_HAT_DOWN) == 0);

    base = js->buttonCount + hat * 4;

    js->buttons[base + 0] = (value & 0x01) ? DESKTOP_WINDOW_PRESS : DESKTOP_WINDOW_RELEASE;
    js->buttons[base + 1] = (value & 0x02) ? DESKTOP_WINDOW_PRESS : DESKTOP_WINDOW_RELEASE;
    js->buttons[base + 2] = (value & 0x04) ? DESKTOP_WINDOW_PRESS : DESKTOP_WINDOW_RELEASE;
    js->buttons[base + 3] = (value & 0x08) ? DESKTOP_WINDOW_PRESS : DESKTOP_WINDOW_RELEASE;

    js->hats[hat] = value;
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Adds the built-in set of gamepad mappings
//
void _desktop_windowInitGamepadMappings(void)
{
    size_t i;
    const size_t count = sizeof(_desktop_windowDefaultMappings) / sizeof(char*);
    _desktop_window.mappings = _desktop_window_calloc(count, sizeof(_DESKTOP_WINDOWmapping));

    for (i = 0;  i < count;  i++)
    {
        if (parseMapping(&_desktop_window.mappings[_desktop_window.mappingCount], _desktop_windowDefaultMappings[i]))
            _desktop_window.mappingCount++;
    }
}

// Returns an available joystick object with arrays and name allocated
//
_DESKTOP_WINDOWjoystick* _desktop_windowAllocJoystick(const char* name,
                                  const char* guid,
                                  int axisCount,
                                  int buttonCount,
                                  int hatCount)
{
    int jid;
    _DESKTOP_WINDOWjoystick* js;

    for (jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
    {
        if (!_desktop_window.joysticks[jid].allocated)
            break;
    }

    if (jid > DESKTOP_WINDOW_JOYSTICK_LAST)
        return NULL;

    js = _desktop_window.joysticks + jid;
    js->allocated   = DESKTOP_WINDOW_TRUE;
    js->axes        = _desktop_window_calloc(axisCount, sizeof(float));
    js->buttons     = _desktop_window_calloc(buttonCount + (size_t) hatCount * 4, 1);
    js->hats        = _desktop_window_calloc(hatCount, 1);
    js->axisCount   = axisCount;
    js->buttonCount = buttonCount;
    js->hatCount    = hatCount;

    strncpy(js->name, name, sizeof(js->name) - 1);
    strncpy(js->guid, guid, sizeof(js->guid) - 1);
    js->mapping = findValidMapping(js);

    return js;
}

// Frees arrays and name and flags the joystick object as unused
//
void _desktop_windowFreeJoystick(_DESKTOP_WINDOWjoystick* js)
{
    _desktop_window_free(js->axes);
    _desktop_window_free(js->buttons);
    _desktop_window_free(js->hats);
    memset(js, 0, sizeof(_DESKTOP_WINDOWjoystick));
}

// Center the cursor in the content area of the specified window
//
void _desktop_windowCenterCursorInContentArea(_DESKTOP_WINDOWwindow* window)
{
    int width, height;

    _desktop_window.platform.getWindowSize(window, &width, &height);
    _desktop_window.platform.setCursorPos(window, width / 2.0, height / 2.0);
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW public API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI int desktop_windowGetInputMode(DESKTOP_WINDOWwindow* handle, int mode)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(0);

    switch (mode)
    {
        case DESKTOP_WINDOW_CURSOR:
            return window->cursorMode;
        case DESKTOP_WINDOW_STICKY_KEYS:
            return window->stickyKeys;
        case DESKTOP_WINDOW_STICKY_MOUSE_BUTTONS:
            return window->stickyMouseButtons;
        case DESKTOP_WINDOW_LOCK_KEY_MODS:
            return window->lockKeyMods;
        case DESKTOP_WINDOW_RAW_MOUSE_MOTION:
            return window->rawMouseMotion;
    }

    _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid input mode 0x%08X", mode);
    return 0;
}

DESKTOP_WINDOWAPI void desktop_windowSetInputMode(DESKTOP_WINDOWwindow* handle, int mode, int value)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    switch (mode)
    {
        case DESKTOP_WINDOW_CURSOR:
        {
            if (value != DESKTOP_WINDOW_CURSOR_NORMAL &&
                value != DESKTOP_WINDOW_CURSOR_HIDDEN &&
                value != DESKTOP_WINDOW_CURSOR_DISABLED &&
                value != DESKTOP_WINDOW_CURSOR_CAPTURED)
            {
                _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM,
                                "Invalid cursor mode 0x%08X",
                                value);
                return;
            }

            if (window->cursorMode == value)
                return;

            window->cursorMode = value;

            _desktop_window.platform.getCursorPos(window,
                                        &window->virtualCursorPosX,
                                        &window->virtualCursorPosY);
            _desktop_window.platform.setCursorMode(window, value);
            return;
        }

        case DESKTOP_WINDOW_STICKY_KEYS:
        {
            value = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            if (window->stickyKeys == value)
                return;

            if (!value)
            {
                int i;

                // Release all sticky keys
                for (i = 0;  i <= DESKTOP_WINDOW_KEY_LAST;  i++)
                {
                    if (window->keys[i] == _DESKTOP_WINDOW_STICK)
                        window->keys[i] = DESKTOP_WINDOW_RELEASE;
                }
            }

            window->stickyKeys = value;
            return;
        }

        case DESKTOP_WINDOW_STICKY_MOUSE_BUTTONS:
        {
            value = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            if (window->stickyMouseButtons == value)
                return;

            if (!value)
            {
                int i;

                // Release all sticky mouse buttons
                for (i = 0;  i <= DESKTOP_WINDOW_MOUSE_BUTTON_LAST;  i++)
                {
                    if (window->mouseButtons[i] == _DESKTOP_WINDOW_STICK)
                        window->mouseButtons[i] = DESKTOP_WINDOW_RELEASE;
                }
            }

            window->stickyMouseButtons = value;
            return;
        }

        case DESKTOP_WINDOW_LOCK_KEY_MODS:
        {
            window->lockKeyMods = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            return;
        }

        case DESKTOP_WINDOW_RAW_MOUSE_MOTION:
        {
            if (!_desktop_window.platform.rawMouseMotionSupported())
            {
                _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                                "Raw mouse motion is not supported on this system");
                return;
            }

            value = value ? DESKTOP_WINDOW_TRUE : DESKTOP_WINDOW_FALSE;
            if (window->rawMouseMotion == value)
                return;

            window->rawMouseMotion = value;
            _desktop_window.platform.setRawMouseMotion(window, value);
            return;
        }
    }

    _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid input mode 0x%08X", mode);
}

DESKTOP_WINDOWAPI int desktop_windowRawMouseMotionSupported(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);
    return _desktop_window.platform.rawMouseMotionSupported();
}

DESKTOP_WINDOWAPI const char* desktop_windowGetKeyName(int key, int scancode)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (key != DESKTOP_WINDOW_KEY_UNKNOWN)
    {
        if (key < DESKTOP_WINDOW_KEY_SPACE || key > DESKTOP_WINDOW_KEY_LAST)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid key %i", key);
            return NULL;
        }

        if (key != DESKTOP_WINDOW_KEY_KP_EQUAL &&
            (key < DESKTOP_WINDOW_KEY_KP_0 || key > DESKTOP_WINDOW_KEY_KP_ADD) &&
            (key < DESKTOP_WINDOW_KEY_APOSTROPHE || key > DESKTOP_WINDOW_KEY_WORLD_2))
        {
            return NULL;
        }

        scancode = _desktop_window.platform.getKeyScancode(key);
    }

    return _desktop_window.platform.getScancodeName(scancode);
}

DESKTOP_WINDOWAPI int desktop_windowGetKeyScancode(int key)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(0);

    if (key < DESKTOP_WINDOW_KEY_SPACE || key > DESKTOP_WINDOW_KEY_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid key %i", key);
        return -1;
    }

    return _desktop_window.platform.getKeyScancode(key);
}

DESKTOP_WINDOWAPI int desktop_windowGetKey(DESKTOP_WINDOWwindow* handle, int key)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_RELEASE);

    if (key < DESKTOP_WINDOW_KEY_SPACE || key > DESKTOP_WINDOW_KEY_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid key %i", key);
        return DESKTOP_WINDOW_RELEASE;
    }

    if (window->keys[key] == _DESKTOP_WINDOW_STICK)
    {
        // Sticky mode: release key now
        window->keys[key] = DESKTOP_WINDOW_RELEASE;
        return DESKTOP_WINDOW_PRESS;
    }

    return (int) window->keys[key];
}

DESKTOP_WINDOWAPI int desktop_windowGetMouseButton(DESKTOP_WINDOWwindow* handle, int button)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_RELEASE);

    if (button < DESKTOP_WINDOW_MOUSE_BUTTON_1 || button > DESKTOP_WINDOW_MOUSE_BUTTON_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid mouse button %i", button);
        return DESKTOP_WINDOW_RELEASE;
    }

    if (window->mouseButtons[button] == _DESKTOP_WINDOW_STICK)
    {
        // Sticky mode: release mouse button now
        window->mouseButtons[button] = DESKTOP_WINDOW_RELEASE;
        return DESKTOP_WINDOW_PRESS;
    }

    return (int) window->mouseButtons[button];
}

DESKTOP_WINDOWAPI void desktop_windowGetCursorPos(DESKTOP_WINDOWwindow* handle, double* xpos, double* ypos)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (window->cursorMode == DESKTOP_WINDOW_CURSOR_DISABLED)
    {
        if (xpos)
            *xpos = window->virtualCursorPosX;
        if (ypos)
            *ypos = window->virtualCursorPosY;
    }
    else
        _desktop_window.platform.getCursorPos(window, xpos, ypos);
}

DESKTOP_WINDOWAPI void desktop_windowSetCursorPos(DESKTOP_WINDOWwindow* handle, double xpos, double ypos)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (xpos != xpos || xpos < -DBL_MAX || xpos > DBL_MAX ||
        ypos != ypos || ypos < -DBL_MAX || ypos > DBL_MAX)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                        "Invalid cursor position %f %f",
                        xpos, ypos);
        return;
    }

    if (!_desktop_window.platform.windowFocused(window))
        return;

    if (window->cursorMode == DESKTOP_WINDOW_CURSOR_DISABLED)
    {
        // Only update the accumulated position if the cursor is disabled
        window->virtualCursorPosX = xpos;
        window->virtualCursorPosY = ypos;
    }
    else
    {
        // Update system cursor position
        _desktop_window.platform.setCursorPos(window, xpos, ypos);
    }
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWcursor* desktop_windowCreateCursor(const DESKTOP_WINDOWimage* image, int xhot, int yhot)
{
    _DESKTOP_WINDOWcursor* cursor;

    assert(image != NULL);
    assert(image->pixels != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (image->width <= 0 || image->height <= 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Invalid image dimensions for cursor");
        return NULL;
    }

    cursor = _desktop_window_calloc(1, sizeof(_DESKTOP_WINDOWcursor));
    cursor->next = _desktop_window.cursorListHead;
    _desktop_window.cursorListHead = cursor;

    if (!_desktop_window.platform.createCursor(cursor, image, xhot, yhot))
    {
        desktop_windowDestroyCursor((DESKTOP_WINDOWcursor*) cursor);
        return NULL;
    }

    return (DESKTOP_WINDOWcursor*) cursor;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWcursor* desktop_windowCreateStandardCursor(int shape)
{
    _DESKTOP_WINDOWcursor* cursor;

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (shape != DESKTOP_WINDOW_ARROW_CURSOR &&
        shape != DESKTOP_WINDOW_IBEAM_CURSOR &&
        shape != DESKTOP_WINDOW_CROSSHAIR_CURSOR &&
        shape != DESKTOP_WINDOW_POINTING_HAND_CURSOR &&
        shape != DESKTOP_WINDOW_RESIZE_EW_CURSOR &&
        shape != DESKTOP_WINDOW_RESIZE_NS_CURSOR &&
        shape != DESKTOP_WINDOW_RESIZE_NWSE_CURSOR &&
        shape != DESKTOP_WINDOW_RESIZE_NESW_CURSOR &&
        shape != DESKTOP_WINDOW_RESIZE_ALL_CURSOR &&
        shape != DESKTOP_WINDOW_NOT_ALLOWED_CURSOR)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid standard cursor 0x%08X", shape);
        return NULL;
    }

    cursor = _desktop_window_calloc(1, sizeof(_DESKTOP_WINDOWcursor));
    cursor->next = _desktop_window.cursorListHead;
    _desktop_window.cursorListHead = cursor;

    if (!_desktop_window.platform.createStandardCursor(cursor, shape))
    {
        desktop_windowDestroyCursor((DESKTOP_WINDOWcursor*) cursor);
        return NULL;
    }

    return (DESKTOP_WINDOWcursor*) cursor;
}

DESKTOP_WINDOWAPI void desktop_windowDestroyCursor(DESKTOP_WINDOWcursor* handle)
{
    _DESKTOP_WINDOWcursor* cursor = (_DESKTOP_WINDOWcursor*) handle;

    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (cursor == NULL)
        return;

    // Make sure the cursor is not being used by any window
    {
        _DESKTOP_WINDOWwindow* window;

        for (window = _desktop_window.windowListHead;  window;  window = window->next)
        {
            if (window->cursor == cursor)
                desktop_windowSetCursor((DESKTOP_WINDOWwindow*) window, NULL);
        }
    }

    _desktop_window.platform.destroyCursor(cursor);

    // Unlink cursor from global linked list
    {
        _DESKTOP_WINDOWcursor** prev = &_desktop_window.cursorListHead;

        while (*prev != cursor)
            prev = &((*prev)->next);

        *prev = cursor->next;
    }

    _desktop_window_free(cursor);
}

DESKTOP_WINDOWAPI void desktop_windowSetCursor(DESKTOP_WINDOWwindow* windowHandle, DESKTOP_WINDOWcursor* cursorHandle)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) windowHandle;
    _DESKTOP_WINDOWcursor* cursor = (_DESKTOP_WINDOWcursor*) cursorHandle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    window->cursor = cursor;

    _desktop_window.platform.setCursor(window, cursor);
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWkeyfun desktop_windowSetKeyCallback(DESKTOP_WINDOWwindow* handle, DESKTOP_WINDOWkeyfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWkeyfun, window->callbacks.key, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWcharfun desktop_windowSetCharCallback(DESKTOP_WINDOWwindow* handle, DESKTOP_WINDOWcharfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWcharfun, window->callbacks.character, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWcharmodsfun desktop_windowSetCharModsCallback(DESKTOP_WINDOWwindow* handle, DESKTOP_WINDOWcharmodsfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWcharmodsfun, window->callbacks.charmods, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWmousebuttonfun desktop_windowSetMouseButtonCallback(DESKTOP_WINDOWwindow* handle,
                                                      DESKTOP_WINDOWmousebuttonfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWmousebuttonfun, window->callbacks.mouseButton, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWcursorposfun desktop_windowSetCursorPosCallback(DESKTOP_WINDOWwindow* handle,
                                                  DESKTOP_WINDOWcursorposfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWcursorposfun, window->callbacks.cursorPos, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWcursorenterfun desktop_windowSetCursorEnterCallback(DESKTOP_WINDOWwindow* handle,
                                                      DESKTOP_WINDOWcursorenterfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWcursorenterfun, window->callbacks.cursorEnter, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWscrollfun desktop_windowSetScrollCallback(DESKTOP_WINDOWwindow* handle,
                                            DESKTOP_WINDOWscrollfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWscrollfun, window->callbacks.scroll, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWdropfun desktop_windowSetDropCallback(DESKTOP_WINDOWwindow* handle, DESKTOP_WINDOWdropfun cbfun)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(window != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWdropfun, window->callbacks.drop, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI int desktop_windowJoystickPresent(int jid)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);

    if (jid < 0 || jid > DESKTOP_WINDOW_JOYSTICK_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return DESKTOP_WINDOW_FALSE;
    }

    if (!initJoysticks())
        return DESKTOP_WINDOW_FALSE;

    js = _desktop_window.joysticks + jid;
    if (!js->connected)
        return DESKTOP_WINDOW_FALSE;

    return _desktop_window.platform.pollJoystick(js, _DESKTOP_WINDOW_POLL_PRESENCE);
}

DESKTOP_WINDOWAPI const float* desktop_windowGetJoystickAxes(int jid, int* count)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);
    assert(count != NULL);

    *count = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > DESKTOP_WINDOW_JOYSTICK_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _desktop_window.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_desktop_window.platform.pollJoystick(js, _DESKTOP_WINDOW_POLL_AXES))
        return NULL;

    *count = js->axisCount;
    return js->axes;
}

DESKTOP_WINDOWAPI const unsigned char* desktop_windowGetJoystickButtons(int jid, int* count)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);
    assert(count != NULL);

    *count = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > DESKTOP_WINDOW_JOYSTICK_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _desktop_window.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_desktop_window.platform.pollJoystick(js, _DESKTOP_WINDOW_POLL_BUTTONS))
        return NULL;

    if (_desktop_window.hints.init.hatButtons)
        *count = js->buttonCount + js->hatCount * 4;
    else
        *count = js->buttonCount;

    return js->buttons;
}

DESKTOP_WINDOWAPI const unsigned char* desktop_windowGetJoystickHats(int jid, int* count)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);
    assert(count != NULL);

    *count = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > DESKTOP_WINDOW_JOYSTICK_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _desktop_window.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_desktop_window.platform.pollJoystick(js, _DESKTOP_WINDOW_POLL_BUTTONS))
        return NULL;

    *count = js->hatCount;
    return js->hats;
}

DESKTOP_WINDOWAPI const char* desktop_windowGetJoystickName(int jid)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > DESKTOP_WINDOW_JOYSTICK_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _desktop_window.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_desktop_window.platform.pollJoystick(js, _DESKTOP_WINDOW_POLL_PRESENCE))
        return NULL;

    return js->name;
}

DESKTOP_WINDOWAPI const char* desktop_windowGetJoystickGUID(int jid)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > DESKTOP_WINDOW_JOYSTICK_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _desktop_window.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_desktop_window.platform.pollJoystick(js, _DESKTOP_WINDOW_POLL_PRESENCE))
        return NULL;

    return js->guid;
}

DESKTOP_WINDOWAPI void desktop_windowSetJoystickUserPointer(int jid, void* pointer)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);

    _DESKTOP_WINDOW_REQUIRE_INIT();

    js = _desktop_window.joysticks + jid;
    if (!js->allocated)
        return;

    js->userPointer = pointer;
}

DESKTOP_WINDOWAPI void* desktop_windowGetJoystickUserPointer(int jid)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    js = _desktop_window.joysticks + jid;
    if (!js->allocated)
        return NULL;

    return js->userPointer;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWjoystickfun desktop_windowSetJoystickCallback(DESKTOP_WINDOWjoystickfun cbfun)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (!initJoysticks())
        return NULL;

    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWjoystickfun, _desktop_window.callbacks.joystick, cbfun);
    return cbfun;
}

DESKTOP_WINDOWAPI int desktop_windowUpdateGamepadMappings(const char* string)
{
    int jid;
    const char* c = string;

    assert(string != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);

    while (*c)
    {
        if ((*c >= '0' && *c <= '9') ||
            (*c >= 'a' && *c <= 'f') ||
            (*c >= 'A' && *c <= 'F'))
        {
            char line[1024];

            const size_t length = strcspn(c, "\r\n");
            if (length < sizeof(line))
            {
                _DESKTOP_WINDOWmapping mapping = {{0}};

                memcpy(line, c, length);
                line[length] = '\0';

                if (parseMapping(&mapping, line))
                {
                    _DESKTOP_WINDOWmapping* previous = findMapping(mapping.guid);
                    if (previous)
                        *previous = mapping;
                    else
                    {
                        _desktop_window.mappingCount++;
                        _desktop_window.mappings =
                            _desktop_window_realloc(_desktop_window.mappings,
                                          sizeof(_DESKTOP_WINDOWmapping) * _desktop_window.mappingCount);
                        _desktop_window.mappings[_desktop_window.mappingCount - 1] = mapping;
                    }
                }
            }

            c += length;
        }
        else
        {
            c += strcspn(c, "\r\n");
            c += strspn(c, "\r\n");
        }
    }

    for (jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
    {
        _DESKTOP_WINDOWjoystick* js = _desktop_window.joysticks + jid;
        if (js->connected)
            js->mapping = findValidMapping(js);
    }

    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWAPI int desktop_windowJoystickIsGamepad(int jid)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);

    if (jid < 0 || jid > DESKTOP_WINDOW_JOYSTICK_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return DESKTOP_WINDOW_FALSE;
    }

    if (!initJoysticks())
        return DESKTOP_WINDOW_FALSE;

    js = _desktop_window.joysticks + jid;
    if (!js->connected)
        return DESKTOP_WINDOW_FALSE;

    if (!_desktop_window.platform.pollJoystick(js, _DESKTOP_WINDOW_POLL_PRESENCE))
        return DESKTOP_WINDOW_FALSE;

    return js->mapping != NULL;
}

DESKTOP_WINDOWAPI const char* desktop_windowGetGamepadName(int jid)
{
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > DESKTOP_WINDOW_JOYSTICK_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _desktop_window.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_desktop_window.platform.pollJoystick(js, _DESKTOP_WINDOW_POLL_PRESENCE))
        return NULL;

    if (!js->mapping)
        return NULL;

    return js->mapping->name;
}

DESKTOP_WINDOWAPI int desktop_windowGetGamepadState(int jid, DESKTOP_WINDOWgamepadstate* state)
{
    int i;
    _DESKTOP_WINDOWjoystick* js;

    assert(jid >= DESKTOP_WINDOW_JOYSTICK_1);
    assert(jid <= DESKTOP_WINDOW_JOYSTICK_LAST);
    assert(state != NULL);

    memset(state, 0, sizeof(DESKTOP_WINDOWgamepadstate));

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);

    if (jid < 0 || jid > DESKTOP_WINDOW_JOYSTICK_LAST)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return DESKTOP_WINDOW_FALSE;
    }

    if (!initJoysticks())
        return DESKTOP_WINDOW_FALSE;

    js = _desktop_window.joysticks + jid;
    if (!js->connected)
        return DESKTOP_WINDOW_FALSE;

    if (!_desktop_window.platform.pollJoystick(js, _DESKTOP_WINDOW_POLL_ALL))
        return DESKTOP_WINDOW_FALSE;

    if (!js->mapping)
        return DESKTOP_WINDOW_FALSE;

    for (i = 0;  i <= DESKTOP_WINDOW_GAMEPAD_BUTTON_LAST;  i++)
    {
        const _DESKTOP_WINDOWmapelement* e = js->mapping->buttons + i;
        if (e->type == _DESKTOP_WINDOW_JOYSTICK_AXIS)
        {
            const float value = js->axes[e->index] * e->axisScale + e->axisOffset;
            // HACK: This should be baked into the value transform
            // TODO: Bake into transform when implementing output modifiers
            if (e->axisOffset < 0 || (e->axisOffset == 0 && e->axisScale > 0))
            {
                if (value >= 0.f)
                    state->buttons[i] = DESKTOP_WINDOW_PRESS;
            }
            else
            {
                if (value <= 0.f)
                    state->buttons[i] = DESKTOP_WINDOW_PRESS;
            }
        }
        else if (e->type == _DESKTOP_WINDOW_JOYSTICK_HATBIT)
        {
            const unsigned int hat = e->index >> 4;
            const unsigned int bit = e->index & 0xf;
            if (js->hats[hat] & bit)
                state->buttons[i] = DESKTOP_WINDOW_PRESS;
        }
        else if (e->type == _DESKTOP_WINDOW_JOYSTICK_BUTTON)
            state->buttons[i] = js->buttons[e->index];
    }

    for (i = 0;  i <= DESKTOP_WINDOW_GAMEPAD_AXIS_LAST;  i++)
    {
        const _DESKTOP_WINDOWmapelement* e = js->mapping->axes + i;
        if (e->type == _DESKTOP_WINDOW_JOYSTICK_AXIS)
        {
            const float value = js->axes[e->index] * e->axisScale + e->axisOffset;
            state->axes[i] = fminf(fmaxf(value, -1.f), 1.f);
        }
        else if (e->type == _DESKTOP_WINDOW_JOYSTICK_HATBIT)
        {
            const unsigned int hat = e->index >> 4;
            const unsigned int bit = e->index & 0xf;
            if (js->hats[hat] & bit)
                state->axes[i] = 1.f;
            else
                state->axes[i] = -1.f;
        }
        else if (e->type == _DESKTOP_WINDOW_JOYSTICK_BUTTON)
            state->axes[i] = js->buttons[e->index] * 2.f - 1.f;
    }

    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWAPI void desktop_windowSetClipboardString(DESKTOP_WINDOWwindow* handle, const char* string)
{
    assert(string != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT();
    _desktop_window.platform.setClipboardString(string);
}

DESKTOP_WINDOWAPI const char* desktop_windowGetClipboardString(DESKTOP_WINDOWwindow* handle)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);
    return _desktop_window.platform.getClipboardString();
}

DESKTOP_WINDOWAPI double desktop_windowGetTime(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(0.0);
    return (double) (_desktop_windowPlatformGetTimerValue() - _desktop_window.timer.offset) /
        _desktop_windowPlatformGetTimerFrequency();
}

DESKTOP_WINDOWAPI void desktop_windowSetTime(double time)
{
    _DESKTOP_WINDOW_REQUIRE_INIT();

    if (time != time || time < 0.0 || time > 18446744073.0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Invalid time %f", time);
        return;
    }

    _desktop_window.timer.offset = _desktop_windowPlatformGetTimerValue() -
        (uint64_t) (time * _desktop_windowPlatformGetTimerFrequency());
}

DESKTOP_WINDOWAPI uint64_t desktop_windowGetTimerValue(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(0);
    return _desktop_windowPlatformGetTimerValue();
}

DESKTOP_WINDOWAPI uint64_t desktop_windowGetTimerFrequency(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(0);
    return _desktop_windowPlatformGetTimerFrequency();
}

