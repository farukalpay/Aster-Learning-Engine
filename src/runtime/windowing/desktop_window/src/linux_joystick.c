#include "internal.h"

#if defined(DESKTOP_WINDOW_BUILD_LINUX_JOYSTICK)

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SYN_DROPPED // < v2.6.39 kernel headers
// Workaround for CentOS-6, which is supported till 2020-11-30, but still on v2.6.32
#define SYN_DROPPED 3
#endif

// Apply an EV_KEY event to the specified joystick
//
static void handleKeyEvent(_DESKTOP_WINDOWjoystick* js, int code, int value)
{
    _desktop_windowInputJoystickButton(js,
                             js->linjs.keyMap[code - BTN_MISC],
                             value ? DESKTOP_WINDOW_PRESS : DESKTOP_WINDOW_RELEASE);
}

// Apply an EV_ABS event to the specified joystick
//
static void handleAbsEvent(_DESKTOP_WINDOWjoystick* js, int code, int value)
{
    const int index = js->linjs.absMap[code];

    if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
    {
        static const char stateMap[3][3] =
        {
            { DESKTOP_WINDOW_HAT_CENTERED, DESKTOP_WINDOW_HAT_UP,       DESKTOP_WINDOW_HAT_DOWN },
            { DESKTOP_WINDOW_HAT_LEFT,     DESKTOP_WINDOW_HAT_LEFT_UP,  DESKTOP_WINDOW_HAT_LEFT_DOWN },
            { DESKTOP_WINDOW_HAT_RIGHT,    DESKTOP_WINDOW_HAT_RIGHT_UP, DESKTOP_WINDOW_HAT_RIGHT_DOWN },
        };

        const int hat = (code - ABS_HAT0X) / 2;
        const int axis = (code - ABS_HAT0X) % 2;
        int* state = js->linjs.hats[hat];

        // NOTE: Looking at several input drivers, it seems all hat events use
        //       -1 for left / up, 0 for centered and 1 for right / down
        if (value == 0)
            state[axis] = 0;
        else if (value < 0)
            state[axis] = 1;
        else if (value > 0)
            state[axis] = 2;

        _desktop_windowInputJoystickHat(js, index, stateMap[state[0]][state[1]]);
    }
    else
    {
        const struct input_absinfo* info = &js->linjs.absInfo[code];
        float normalized = value;

        const int range = info->maximum - info->minimum;
        if (range)
        {
            // Normalize to 0.0 -> 1.0
            normalized = (normalized - info->minimum) / range;
            // Normalize to -1.0 -> 1.0
            normalized = normalized * 2.0f - 1.0f;
        }

        _desktop_windowInputJoystickAxis(js, index, normalized);
    }
}

// Poll state of absolute axes
//
static void pollAbsState(_DESKTOP_WINDOWjoystick* js)
{
    for (int code = 0;  code < ABS_CNT;  code++)
    {
        if (js->linjs.absMap[code] < 0)
            continue;

        struct input_absinfo* info = &js->linjs.absInfo[code];

        if (ioctl(js->linjs.fd, EVIOCGABS(code), info) < 0)
            continue;

        handleAbsEvent(js, code, info->value);
    }
}

#define isBitSet(bit, arr) (arr[(bit) / 8] & (1 << ((bit) % 8)))

// Attempt to open the specified joystick device
//
static DESKTOP_WINDOWbool openJoystickDevice(const char* path)
{
    for (int jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
    {
        if (!_desktop_window.joysticks[jid].connected)
            continue;
        if (strcmp(_desktop_window.joysticks[jid].linjs.path, path) == 0)
            return DESKTOP_WINDOW_FALSE;
    }

    _DESKTOP_WINDOWjoystickLinux linjs = {0};
    linjs.fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (linjs.fd == -1)
        return DESKTOP_WINDOW_FALSE;

    char evBits[(EV_CNT + 7) / 8] = {0};
    char keyBits[(KEY_CNT + 7) / 8] = {0};
    char absBits[(ABS_CNT + 7) / 8] = {0};
    struct input_id id;

    if (ioctl(linjs.fd, EVIOCGBIT(0, sizeof(evBits)), evBits) < 0 ||
        ioctl(linjs.fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0 ||
        ioctl(linjs.fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0 ||
        ioctl(linjs.fd, EVIOCGID, &id) < 0)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR,
                        "Linux: Failed to query input device: %s",
                        strerror(errno));
        close(linjs.fd);
        return DESKTOP_WINDOW_FALSE;
    }

    // Ensure this device supports the events expected of a joystick
    if (!isBitSet(EV_ABS, evBits))
    {
        close(linjs.fd);
        return DESKTOP_WINDOW_FALSE;
    }

    char name[256] = "";

    if (ioctl(linjs.fd, EVIOCGNAME(sizeof(name)), name) < 0)
        strncpy(name, "Unknown", sizeof(name));

    char guid[33] = "";

    // Generate a joystick GUID that matches the SDL 2.0.5+ one
    if (id.vendor && id.product && id.version)
    {
        sprintf(guid, "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000",
                id.bustype & 0xff, id.bustype >> 8,
                id.vendor & 0xff,  id.vendor >> 8,
                id.product & 0xff, id.product >> 8,
                id.version & 0xff, id.version >> 8);
    }
    else
    {
        sprintf(guid, "%02x%02x0000%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x00",
                id.bustype & 0xff, id.bustype >> 8,
                name[0], name[1], name[2], name[3],
                name[4], name[5], name[6], name[7],
                name[8], name[9], name[10]);
    }

    int axisCount = 0, buttonCount = 0, hatCount = 0;

    for (int code = BTN_MISC;  code < KEY_CNT;  code++)
    {
        if (!isBitSet(code, keyBits))
            continue;

        linjs.keyMap[code - BTN_MISC] = buttonCount;
        buttonCount++;
    }

    for (int code = 0;  code < ABS_CNT;  code++)
    {
        linjs.absMap[code] = -1;
        if (!isBitSet(code, absBits))
            continue;

        if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
        {
            linjs.absMap[code] = hatCount;
            hatCount++;
            // Skip the Y axis
            code++;
        }
        else
        {
            if (ioctl(linjs.fd, EVIOCGABS(code), &linjs.absInfo[code]) < 0)
                continue;

            linjs.absMap[code] = axisCount;
            axisCount++;
        }
    }

    _DESKTOP_WINDOWjoystick* js =
        _desktop_windowAllocJoystick(name, guid, axisCount, buttonCount, hatCount);
    if (!js)
    {
        close(linjs.fd);
        return DESKTOP_WINDOW_FALSE;
    }

    strncpy(linjs.path, path, sizeof(linjs.path) - 1);
    memcpy(&js->linjs, &linjs, sizeof(linjs));

    pollAbsState(js);

    _desktop_windowInputJoystick(js, DESKTOP_WINDOW_CONNECTED);
    return DESKTOP_WINDOW_TRUE;
}

#undef isBitSet

// Frees all resources associated with the specified joystick
//
static void closeJoystick(_DESKTOP_WINDOWjoystick* js)
{
    _desktop_windowInputJoystick(js, DESKTOP_WINDOW_DISCONNECTED);
    close(js->linjs.fd);
    _desktop_windowFreeJoystick(js);
}

// Lexically compare joysticks by name; used by qsort
//
static int compareJoysticks(const void* fp, const void* sp)
{
    const _DESKTOP_WINDOWjoystick* fj = fp;
    const _DESKTOP_WINDOWjoystick* sj = sp;
    return strcmp(fj->linjs.path, sj->linjs.path);
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

void _desktop_windowDetectJoystickConnectionLinux(void)
{
    if (_desktop_window.linjs.inotify <= 0)
        return;

    ssize_t offset = 0;
    char buffer[16384];
    const ssize_t size = read(_desktop_window.linjs.inotify, buffer, sizeof(buffer));

    while (size > offset)
    {
        regmatch_t match;
        const struct inotify_event* e = (struct inotify_event*) (buffer + offset);

        offset += sizeof(struct inotify_event) + e->len;

        if (regexec(&_desktop_window.linjs.regex, e->name, 1, &match, 0) != 0)
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/dev/input/%s", e->name);

        if (e->mask & (IN_CREATE | IN_ATTRIB))
            openJoystickDevice(path);
        else if (e->mask & IN_DELETE)
        {
            for (int jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
            {
                if (strcmp(_desktop_window.joysticks[jid].linjs.path, path) == 0)
                {
                    closeJoystick(_desktop_window.joysticks + jid);
                    break;
                }
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowInitJoysticksLinux(void)
{
    const char* dirname = "/dev/input";

    _desktop_window.linjs.inotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (_desktop_window.linjs.inotify > 0)
    {
        // HACK: Register for IN_ATTRIB to get notified when udev is done
        //       This works well in practice but the true way is libudev

        _desktop_window.linjs.watch = inotify_add_watch(_desktop_window.linjs.inotify,
                                              dirname,
                                              IN_CREATE | IN_ATTRIB | IN_DELETE);
    }

    // Continue without device connection notifications if inotify fails

    _desktop_window.linjs.regexCompiled = (regcomp(&_desktop_window.linjs.regex, "^event[0-9]\\+$", 0) == 0);
    if (!_desktop_window.linjs.regexCompiled)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "Linux: Failed to compile regex");
        return DESKTOP_WINDOW_FALSE;
    }

    int count = 0;

    DIR* dir = opendir(dirname);
    if (dir)
    {
        struct dirent* entry;

        while ((entry = readdir(dir)))
        {
            regmatch_t match;

            if (regexec(&_desktop_window.linjs.regex, entry->d_name, 1, &match, 0) != 0)
                continue;

            char path[PATH_MAX];

            snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);

            if (openJoystickDevice(path))
                count++;
        }

        closedir(dir);
    }

    // Continue with no joysticks if enumeration fails

    qsort(_desktop_window.joysticks, count, sizeof(_DESKTOP_WINDOWjoystick), compareJoysticks);
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateJoysticksLinux(void)
{
    for (int jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
    {
        _DESKTOP_WINDOWjoystick* js = _desktop_window.joysticks + jid;
        if (js->connected)
            closeJoystick(js);
    }

    if (_desktop_window.linjs.inotify > 0)
    {
        if (_desktop_window.linjs.watch > 0)
            inotify_rm_watch(_desktop_window.linjs.inotify, _desktop_window.linjs.watch);

        close(_desktop_window.linjs.inotify);
    }

    if (_desktop_window.linjs.regexCompiled)
        regfree(&_desktop_window.linjs.regex);
}

DESKTOP_WINDOWbool _desktop_windowPollJoystickLinux(_DESKTOP_WINDOWjoystick* js, int mode)
{
    // Read all queued events (non-blocking)
    for (;;)
    {
        struct input_event e;

        errno = 0;
        if (read(js->linjs.fd, &e, sizeof(e)) < 0)
        {
            // Reset the joystick slot if the device was disconnected
            if (errno == ENODEV)
                closeJoystick(js);

            break;
        }

        if (e.type == EV_SYN)
        {
            if (e.code == SYN_DROPPED)
                _desktop_window.linjs.dropped = DESKTOP_WINDOW_TRUE;
            else if (e.code == SYN_REPORT)
            {
                _desktop_window.linjs.dropped = DESKTOP_WINDOW_FALSE;
                pollAbsState(js);
            }
        }

        if (_desktop_window.linjs.dropped)
            continue;

        if (e.type == EV_KEY)
            handleKeyEvent(js, e.code, e.value);
        else if (e.type == EV_ABS)
            handleAbsEvent(js, e.code, e.value);
    }

    return js->connected;
}

const char* _desktop_windowGetMappingNameLinux(void)
{
    return "Linux";
}

void _desktop_windowUpdateGamepadGUIDLinux(char* guid)
{
}

#endif // DESKTOP_WINDOW_BUILD_LINUX_JOYSTICK

