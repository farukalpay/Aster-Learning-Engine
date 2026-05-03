#include "internal.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>


// NOTE: The global variables below comprise all mutable global data in DESKTOP_WINDOW
//       Any other mutable global variable is a bug

// This contains all mutable state shared between compilation units of DESKTOP_WINDOW
//
_DESKTOP_WINDOWlibrary _desktop_window = { DESKTOP_WINDOW_FALSE };

// These are outside of _desktop_window so they can be used before initialization and
// after termination without special handling when _desktop_window is cleared to zero
//
static _DESKTOP_WINDOWerror _desktop_windowMainThreadError;
static DESKTOP_WINDOWerrorfun _desktop_windowErrorCallback;
static DESKTOP_WINDOWallocator _desktop_windowInitAllocator;
static _DESKTOP_WINDOWinitconfig _desktop_windowInitHints =
{
    .hatButtons = DESKTOP_WINDOW_TRUE,
    .angleType = DESKTOP_WINDOW_ANGLE_PLATFORM_TYPE_NONE,
    .platformID = DESKTOP_WINDOW_ANY_PLATFORM,
    .vulkanLoader = NULL,
    .ns =
    {
        .menubar = DESKTOP_WINDOW_TRUE,
        .chdir = DESKTOP_WINDOW_TRUE
    },
    .x11 =
    {
        .xcbVulkanSurface = DESKTOP_WINDOW_TRUE,
    },
    .wl =
    {
        .libdecorMode = DESKTOP_WINDOW_WAYLAND_PREFER_LIBDECOR
    },
};

// The allocation function used when no custom allocator is set
//
static void* defaultAllocate(size_t size, void* user)
{
    return malloc(size);
}

// The deallocation function used when no custom allocator is set
//
static void defaultDeallocate(void* block, void* user)
{
    free(block);
}

// The reallocation function used when no custom allocator is set
//
static void* defaultReallocate(void* block, size_t size, void* user)
{
    return realloc(block, size);
}

// Terminate the library
//
static void terminate(void)
{
    int i;

    memset(&_desktop_window.callbacks, 0, sizeof(_desktop_window.callbacks));

    while (_desktop_window.windowListHead)
        desktop_windowDestroyWindow((DESKTOP_WINDOWwindow*) _desktop_window.windowListHead);

    while (_desktop_window.cursorListHead)
        desktop_windowDestroyCursor((DESKTOP_WINDOWcursor*) _desktop_window.cursorListHead);

    for (i = 0;  i < _desktop_window.monitorCount;  i++)
    {
        _DESKTOP_WINDOWmonitor* monitor = _desktop_window.monitors[i];
        if (monitor->originalRamp.size)
            _desktop_window.platform.setGammaRamp(monitor, &monitor->originalRamp);
        _desktop_windowFreeMonitor(monitor);
    }

    _desktop_window_free(_desktop_window.monitors);
    _desktop_window.monitors = NULL;
    _desktop_window.monitorCount = 0;

    _desktop_window_free(_desktop_window.mappings);
    _desktop_window.mappings = NULL;
    _desktop_window.mappingCount = 0;

    _desktop_windowTerminateVulkan();
    _desktop_window.platform.terminateJoysticks();
    _desktop_window.platform.terminate();

    _desktop_window.initialized = DESKTOP_WINDOW_FALSE;

    while (_desktop_window.errorListHead)
    {
        _DESKTOP_WINDOWerror* error = _desktop_window.errorListHead;
        _desktop_window.errorListHead = error->next;
        _desktop_window_free(error);
    }

    _desktop_windowPlatformDestroyTls(&_desktop_window.contextSlot);
    _desktop_windowPlatformDestroyTls(&_desktop_window.errorSlot);
    _desktop_windowPlatformDestroyMutex(&_desktop_window.errorLock);

    memset(&_desktop_window, 0, sizeof(_desktop_window));
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Encode a Unicode code point to a UTF-8 stream
// Based on cutef8 by Jeff Bezanson (Public Domain)
//
size_t _desktop_windowEncodeUTF8(char* s, uint32_t codepoint)
{
    size_t count = 0;

    if (codepoint < 0x80)
        s[count++] = (char) codepoint;
    else if (codepoint < 0x800)
    {
        s[count++] = (codepoint >> 6) | 0xc0;
        s[count++] = (codepoint & 0x3f) | 0x80;
    }
    else if (codepoint < 0x10000)
    {
        s[count++] = (codepoint >> 12) | 0xe0;
        s[count++] = ((codepoint >> 6) & 0x3f) | 0x80;
        s[count++] = (codepoint & 0x3f) | 0x80;
    }
    else if (codepoint < 0x110000)
    {
        s[count++] = (codepoint >> 18) | 0xf0;
        s[count++] = ((codepoint >> 12) & 0x3f) | 0x80;
        s[count++] = ((codepoint >> 6) & 0x3f) | 0x80;
        s[count++] = (codepoint & 0x3f) | 0x80;
    }

    return count;
}

// Splits and translates a text/uri-list into separate file paths
// NOTE: This function destroys the provided string
//
char** _desktop_windowParseUriList(char* text, int* count)
{
    const char* prefix = "file://";
    char** paths = NULL;
    char* line;

    *count = 0;

    while ((line = strtok(text, "\r\n")))
    {
        char* path;

        text = NULL;

        if (line[0] == '#')
            continue;

        if (strncmp(line, prefix, strlen(prefix)) == 0)
        {
            line += strlen(prefix);
            // TODO: Validate hostname
            while (*line != '/')
                line++;
        }

        (*count)++;

        path = _desktop_window_calloc(strlen(line) + 1, 1);
        paths = _desktop_window_realloc(paths, *count * sizeof(char*));
        paths[*count - 1] = path;

        while (*line)
        {
            if (line[0] == '%' && line[1] && line[2])
            {
                const char digits[3] = { line[1], line[2], '\0' };
                *path = (char) strtol(digits, NULL, 16);
                line += 2;
            }
            else
                *path = *line;

            path++;
            line++;
        }
    }

    return paths;
}

char* _desktop_window_strdup(const char* source)
{
    const size_t length = strlen(source);
    char* result = _desktop_window_calloc(length + 1, 1);
    strcpy(result, source);
    return result;
}

int _desktop_window_min(int a, int b)
{
    return a < b ? a : b;
}

int _desktop_window_max(int a, int b)
{
    return a > b ? a : b;
}

void* _desktop_window_calloc(size_t count, size_t size)
{
    if (count && size)
    {
        void* block;

        if (count > SIZE_MAX / size)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Allocation size overflow");
            return NULL;
        }

        block = _desktop_window.allocator.allocate(count * size, _desktop_window.allocator.user);
        if (block)
            return memset(block, 0, count * size);
        else
        {
            _desktop_windowInputError(DESKTOP_WINDOW_OUT_OF_MEMORY, NULL);
            return NULL;
        }
    }
    else
        return NULL;
}

void* _desktop_window_realloc(void* block, size_t size)
{
    if (block && size)
    {
        void* resized = _desktop_window.allocator.reallocate(block, size, _desktop_window.allocator.user);
        if (resized)
            return resized;
        else
        {
            _desktop_windowInputError(DESKTOP_WINDOW_OUT_OF_MEMORY, NULL);
            return NULL;
        }
    }
    else if (block)
    {
        _desktop_window_free(block);
        return NULL;
    }
    else
        return _desktop_window_calloc(1, size);
}

void _desktop_window_free(void* block)
{
    if (block)
        _desktop_window.allocator.deallocate(block, _desktop_window.allocator.user);
}


//////////////////////////////////////////////////////////////////////////
//////                         DESKTOP_WINDOW event API                       //////
//////////////////////////////////////////////////////////////////////////

// Notifies shared code of an error
//
void _desktop_windowInputError(int code, const char* format, ...)
{
    _DESKTOP_WINDOWerror* error;
    char description[_DESKTOP_WINDOW_MESSAGE_SIZE];

    if (format)
    {
        va_list vl;

        va_start(vl, format);
        vsnprintf(description, sizeof(description), format, vl);
        va_end(vl);

        description[sizeof(description) - 1] = '\0';
    }
    else
    {
        if (code == DESKTOP_WINDOW_NOT_INITIALIZED)
            strcpy(description, "The DESKTOP_WINDOW library is not initialized");
        else if (code == DESKTOP_WINDOW_NO_CURRENT_CONTEXT)
            strcpy(description, "There is no current context");
        else if (code == DESKTOP_WINDOW_INVALID_ENUM)
            strcpy(description, "Invalid argument for enum parameter");
        else if (code == DESKTOP_WINDOW_INVALID_VALUE)
            strcpy(description, "Invalid value for parameter");
        else if (code == DESKTOP_WINDOW_OUT_OF_MEMORY)
            strcpy(description, "Out of memory");
        else if (code == DESKTOP_WINDOW_API_UNAVAILABLE)
            strcpy(description, "The requested API is unavailable");
        else if (code == DESKTOP_WINDOW_VERSION_UNAVAILABLE)
            strcpy(description, "The requested API version is unavailable");
        else if (code == DESKTOP_WINDOW_PLATFORM_ERROR)
            strcpy(description, "A platform-specific error occurred");
        else if (code == DESKTOP_WINDOW_FORMAT_UNAVAILABLE)
            strcpy(description, "The requested format is unavailable");
        else if (code == DESKTOP_WINDOW_NO_WINDOW_CONTEXT)
            strcpy(description, "The specified window has no context");
        else if (code == DESKTOP_WINDOW_CURSOR_UNAVAILABLE)
            strcpy(description, "The specified cursor shape is unavailable");
        else if (code == DESKTOP_WINDOW_FEATURE_UNAVAILABLE)
            strcpy(description, "The requested feature cannot be implemented for this platform");
        else if (code == DESKTOP_WINDOW_FEATURE_UNIMPLEMENTED)
            strcpy(description, "The requested feature has not yet been implemented for this platform");
        else if (code == DESKTOP_WINDOW_PLATFORM_UNAVAILABLE)
            strcpy(description, "The requested platform is unavailable");
        else
            strcpy(description, "ERROR: UNKNOWN DESKTOP_WINDOW ERROR");
    }

    if (_desktop_window.initialized)
    {
        error = _desktop_windowPlatformGetTls(&_desktop_window.errorSlot);
        if (!error)
        {
            error = _desktop_window_calloc(1, sizeof(_DESKTOP_WINDOWerror));
            _desktop_windowPlatformSetTls(&_desktop_window.errorSlot, error);
            _desktop_windowPlatformLockMutex(&_desktop_window.errorLock);
            error->next = _desktop_window.errorListHead;
            _desktop_window.errorListHead = error;
            _desktop_windowPlatformUnlockMutex(&_desktop_window.errorLock);
        }
    }
    else
        error = &_desktop_windowMainThreadError;

    error->code = code;
    strcpy(error->description, description);

    if (_desktop_windowErrorCallback)
        _desktop_windowErrorCallback(code, description);
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW public API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI int desktop_windowInit(void)
{
    if (_desktop_window.initialized)
        return DESKTOP_WINDOW_TRUE;

    memset(&_desktop_window, 0, sizeof(_desktop_window));
    _desktop_window.hints.init = _desktop_windowInitHints;

    _desktop_window.allocator = _desktop_windowInitAllocator;
    if (!_desktop_window.allocator.allocate)
    {
        _desktop_window.allocator.allocate   = defaultAllocate;
        _desktop_window.allocator.reallocate = defaultReallocate;
        _desktop_window.allocator.deallocate = defaultDeallocate;
    }

    if (!_desktop_windowSelectPlatform(_desktop_window.hints.init.platformID, &_desktop_window.platform))
        return DESKTOP_WINDOW_FALSE;

    if (!_desktop_window.platform.init())
    {
        terminate();
        return DESKTOP_WINDOW_FALSE;
    }

    if (!_desktop_windowPlatformCreateMutex(&_desktop_window.errorLock) ||
        !_desktop_windowPlatformCreateTls(&_desktop_window.errorSlot) ||
        !_desktop_windowPlatformCreateTls(&_desktop_window.contextSlot))
    {
        terminate();
        return DESKTOP_WINDOW_FALSE;
    }

    _desktop_windowPlatformSetTls(&_desktop_window.errorSlot, &_desktop_windowMainThreadError);

    _desktop_windowInitGamepadMappings();

    _desktop_windowPlatformInitTimer();
    _desktop_window.timer.offset = _desktop_windowPlatformGetTimerValue();

    _desktop_window.initialized = DESKTOP_WINDOW_TRUE;

    desktop_windowDefaultWindowHints();
    return DESKTOP_WINDOW_TRUE;
}

DESKTOP_WINDOWAPI void desktop_windowTerminate(void)
{
    if (!_desktop_window.initialized)
        return;

    terminate();
}

DESKTOP_WINDOWAPI void desktop_windowInitHint(int hint, int value)
{
    switch (hint)
    {
        case DESKTOP_WINDOW_JOYSTICK_HAT_BUTTONS:
            _desktop_windowInitHints.hatButtons = value;
            return;
        case DESKTOP_WINDOW_ANGLE_PLATFORM_TYPE:
            _desktop_windowInitHints.angleType = value;
            return;
        case DESKTOP_WINDOW_PLATFORM:
            _desktop_windowInitHints.platformID = value;
            return;
        case DESKTOP_WINDOW_COCOA_CHDIR_RESOURCES:
            _desktop_windowInitHints.ns.chdir = value;
            return;
        case DESKTOP_WINDOW_COCOA_MENUBAR:
            _desktop_windowInitHints.ns.menubar = value;
            return;
        case DESKTOP_WINDOW_X11_XCB_VULKAN_SURFACE:
            _desktop_windowInitHints.x11.xcbVulkanSurface = value;
            return;
        case DESKTOP_WINDOW_WAYLAND_LIBDECOR:
            _desktop_windowInitHints.wl.libdecorMode = value;
            return;
    }

    _desktop_windowInputError(DESKTOP_WINDOW_INVALID_ENUM,
                    "Invalid init hint 0x%08X", hint);
}

DESKTOP_WINDOWAPI void desktop_windowInitAllocator(const DESKTOP_WINDOWallocator* allocator)
{
    if (allocator)
    {
        if (allocator->allocate && allocator->reallocate && allocator->deallocate)
            _desktop_windowInitAllocator = *allocator;
        else
            _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE, "Missing function in allocator");
    }
    else
        memset(&_desktop_windowInitAllocator, 0, sizeof(DESKTOP_WINDOWallocator));
}

DESKTOP_WINDOWAPI void desktop_windowInitVulkanLoader(PFN_vkGetInstanceProcAddr loader)
{
    _desktop_windowInitHints.vulkanLoader = loader;
}

DESKTOP_WINDOWAPI void desktop_windowGetVersion(int* major, int* minor, int* rev)
{
    if (major != NULL)
        *major = DESKTOP_WINDOW_VERSION_MAJOR;
    if (minor != NULL)
        *minor = DESKTOP_WINDOW_VERSION_MINOR;
    if (rev != NULL)
        *rev = DESKTOP_WINDOW_VERSION_REVISION;
}

DESKTOP_WINDOWAPI int desktop_windowGetError(const char** description)
{
    _DESKTOP_WINDOWerror* error;
    int code = DESKTOP_WINDOW_NO_ERROR;

    if (description)
        *description = NULL;

    if (_desktop_window.initialized)
        error = _desktop_windowPlatformGetTls(&_desktop_window.errorSlot);
    else
        error = &_desktop_windowMainThreadError;

    if (error)
    {
        code = error->code;
        error->code = DESKTOP_WINDOW_NO_ERROR;
        if (description && code)
            *description = error->description;
    }

    return code;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWerrorfun desktop_windowSetErrorCallback(DESKTOP_WINDOWerrorfun cbfun)
{
    _DESKTOP_WINDOW_SWAP(DESKTOP_WINDOWerrorfun, _desktop_windowErrorCallback, cbfun);
    return cbfun;
}

