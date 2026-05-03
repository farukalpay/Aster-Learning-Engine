#include "internal.h"

#if defined(_DESKTOP_WINDOW_COCOA)

#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Kernel/IOKit/hidsystem/IOHIDUsageTables.h>


// Joystick element information
//
typedef struct _DESKTOP_WINDOWjoyelementNS
{
    IOHIDElementRef native;
    uint32_t        usage;
    int             index;
    long            minimum;
    long            maximum;

} _DESKTOP_WINDOWjoyelementNS;


// Returns the value of the specified element of the specified joystick
//
static long getElementValue(_DESKTOP_WINDOWjoystick* js, _DESKTOP_WINDOWjoyelementNS* element)
{
    IOHIDValueRef valueRef;
    long value = 0;

    if (js->ns.device)
    {
        if (IOHIDDeviceGetValue(js->ns.device,
                                element->native,
                                &valueRef) == kIOReturnSuccess)
        {
            value = IOHIDValueGetIntegerValue(valueRef);
        }
    }

    return value;
}

// Comparison function for matching the SDL element order
//
static CFComparisonResult compareElements(const void* fp,
                                          const void* sp,
                                          void* user)
{
    const _DESKTOP_WINDOWjoyelementNS* fe = fp;
    const _DESKTOP_WINDOWjoyelementNS* se = sp;
    if (fe->usage < se->usage)
        return kCFCompareLessThan;
    if (fe->usage > se->usage)
        return kCFCompareGreaterThan;
    if (fe->index < se->index)
        return kCFCompareLessThan;
    if (fe->index > se->index)
        return kCFCompareGreaterThan;
    return kCFCompareEqualTo;
}

// Removes the specified joystick
//
static void closeJoystick(_DESKTOP_WINDOWjoystick* js)
{
    _desktop_windowInputJoystick(js, DESKTOP_WINDOW_DISCONNECTED);

    for (int i = 0;  i < CFArrayGetCount(js->ns.axes);  i++)
        _desktop_window_free((void*) CFArrayGetValueAtIndex(js->ns.axes, i));
    CFRelease(js->ns.axes);

    for (int i = 0;  i < CFArrayGetCount(js->ns.buttons);  i++)
        _desktop_window_free((void*) CFArrayGetValueAtIndex(js->ns.buttons, i));
    CFRelease(js->ns.buttons);

    for (int i = 0;  i < CFArrayGetCount(js->ns.hats);  i++)
        _desktop_window_free((void*) CFArrayGetValueAtIndex(js->ns.hats, i));
    CFRelease(js->ns.hats);

    _desktop_windowFreeJoystick(js);
}

// Callback for user-initiated joystick addition
//
static void matchCallback(void* context,
                          IOReturn result,
                          void* sender,
                          IOHIDDeviceRef device)
{
    int jid;
    char name[256];
    char guid[33];
    CFTypeRef property;
    uint32_t vendor = 0, product = 0, version = 0;
    _DESKTOP_WINDOWjoystick* js;
    CFMutableArrayRef axes, buttons, hats;

    for (jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
    {
        if (_desktop_window.joysticks[jid].ns.device == device)
            return;
    }

    CFArrayRef elements =
        IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);

    // It is reportedly possible for this to fail on macOS 13 Ventura
    // if the application does not have input monitoring permissions
    if (!elements)
        return;

    axes    = CFArrayCreateMutable(NULL, 0, NULL);
    buttons = CFArrayCreateMutable(NULL, 0, NULL);
    hats    = CFArrayCreateMutable(NULL, 0, NULL);

    property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
    if (property)
    {
        CFStringGetCString(property,
                           name,
                           sizeof(name),
                           kCFStringEncodingUTF8);
    }
    else
        strncpy(name, "Unknown", sizeof(name));

    property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey));
    if (property)
        CFNumberGetValue(property, kCFNumberSInt32Type, &vendor);

    property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey));
    if (property)
        CFNumberGetValue(property, kCFNumberSInt32Type, &product);

    property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVersionNumberKey));
    if (property)
        CFNumberGetValue(property, kCFNumberSInt32Type, &version);

    // Generate a joystick GUID that matches the SDL 2.0.5+ one
    if (vendor && product)
    {
        sprintf(guid, "03000000%02x%02x0000%02x%02x0000%02x%02x0000",
                (uint8_t) vendor, (uint8_t) (vendor >> 8),
                (uint8_t) product, (uint8_t) (product >> 8),
                (uint8_t) version, (uint8_t) (version >> 8));
    }
    else
    {
        sprintf(guid, "05000000%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x00",
                name[0], name[1], name[2], name[3],
                name[4], name[5], name[6], name[7],
                name[8], name[9], name[10]);
    }

    for (CFIndex i = 0;  i < CFArrayGetCount(elements);  i++)
    {
        IOHIDElementRef native = (IOHIDElementRef)
            CFArrayGetValueAtIndex(elements, i);
        if (CFGetTypeID(native) != IOHIDElementGetTypeID())
            continue;

        const IOHIDElementType type = IOHIDElementGetType(native);
        if ((type != kIOHIDElementTypeInput_Axis) &&
            (type != kIOHIDElementTypeInput_Button) &&
            (type != kIOHIDElementTypeInput_Misc))
        {
            continue;
        }

        CFMutableArrayRef target = NULL;

        const uint32_t usage = IOHIDElementGetUsage(native);
        const uint32_t page = IOHIDElementGetUsagePage(native);
        if (page == kHIDPage_GenericDesktop)
        {
            switch (usage)
            {
                case kHIDUsage_GD_X:
                case kHIDUsage_GD_Y:
                case kHIDUsage_GD_Z:
                case kHIDUsage_GD_Rx:
                case kHIDUsage_GD_Ry:
                case kHIDUsage_GD_Rz:
                case kHIDUsage_GD_Slider:
                case kHIDUsage_GD_Dial:
                case kHIDUsage_GD_Wheel:
                    target = axes;
                    break;
                case kHIDUsage_GD_Hatswitch:
                    target = hats;
                    break;
                case kHIDUsage_GD_DPadUp:
                case kHIDUsage_GD_DPadRight:
                case kHIDUsage_GD_DPadDown:
                case kHIDUsage_GD_DPadLeft:
                case kHIDUsage_GD_SystemMainMenu:
                case kHIDUsage_GD_Select:
                case kHIDUsage_GD_Start:
                    target = buttons;
                    break;
            }
        }
        else if (page == kHIDPage_Simulation)
        {
            switch (usage)
            {
                case kHIDUsage_Sim_Accelerator:
                case kHIDUsage_Sim_Brake:
                case kHIDUsage_Sim_Throttle:
                case kHIDUsage_Sim_Rudder:
                case kHIDUsage_Sim_Steering:
                    target = axes;
                    break;
            }
        }
        else if (page == kHIDPage_Button || page == kHIDPage_Consumer)
            target = buttons;

        if (target)
        {
            _DESKTOP_WINDOWjoyelementNS* element = _desktop_window_calloc(1, sizeof(_DESKTOP_WINDOWjoyelementNS));
            element->native  = native;
            element->usage   = usage;
            element->index   = (int) CFArrayGetCount(target);
            element->minimum = IOHIDElementGetLogicalMin(native);
            element->maximum = IOHIDElementGetLogicalMax(native);
            CFArrayAppendValue(target, element);
        }
    }

    CFRelease(elements);

    CFArraySortValues(axes, CFRangeMake(0, CFArrayGetCount(axes)),
                      compareElements, NULL);
    CFArraySortValues(buttons, CFRangeMake(0, CFArrayGetCount(buttons)),
                      compareElements, NULL);
    CFArraySortValues(hats, CFRangeMake(0, CFArrayGetCount(hats)),
                      compareElements, NULL);

    js = _desktop_windowAllocJoystick(name, guid,
                            (int) CFArrayGetCount(axes),
                            (int) CFArrayGetCount(buttons),
                            (int) CFArrayGetCount(hats));

    js->ns.device  = device;
    js->ns.axes    = axes;
    js->ns.buttons = buttons;
    js->ns.hats    = hats;

    _desktop_windowInputJoystick(js, DESKTOP_WINDOW_CONNECTED);
}

// Callback for user-initiated joystick removal
//
static void removeCallback(void* context,
                           IOReturn result,
                           void* sender,
                           IOHIDDeviceRef device)
{
    for (int jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
    {
        if (_desktop_window.joysticks[jid].connected && _desktop_window.joysticks[jid].ns.device == device)
        {
            closeJoystick(&_desktop_window.joysticks[jid]);
            break;
        }
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW platform API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowInitJoysticksCocoa(void)
{
    CFMutableArrayRef matching;
    const long usages[] =
    {
        kHIDUsage_GD_Joystick,
        kHIDUsage_GD_GamePad,
        kHIDUsage_GD_MultiAxisController
    };

    _desktop_window.ns.hidManager = IOHIDManagerCreate(kCFAllocatorDefault,
                                             kIOHIDOptionsTypeNone);

    matching = CFArrayCreateMutable(kCFAllocatorDefault,
                                    0,
                                    &kCFTypeArrayCallBacks);
    if (!matching)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_PLATFORM_ERROR, "Cocoa: Failed to create array");
        return DESKTOP_WINDOW_FALSE;
    }

    for (size_t i = 0;  i < sizeof(usages) / sizeof(long);  i++)
    {
        const long page = kHIDPage_GenericDesktop;

        CFMutableDictionaryRef dict =
            CFDictionaryCreateMutable(kCFAllocatorDefault,
                                      0,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
        if (!dict)
            continue;

        CFNumberRef pageRef = CFNumberCreate(kCFAllocatorDefault,
                                             kCFNumberLongType,
                                             &page);
        CFNumberRef usageRef = CFNumberCreate(kCFAllocatorDefault,
                                              kCFNumberLongType,
                                              &usages[i]);
        if (pageRef && usageRef)
        {
            CFDictionarySetValue(dict,
                                 CFSTR(kIOHIDDeviceUsagePageKey),
                                 pageRef);
            CFDictionarySetValue(dict,
                                 CFSTR(kIOHIDDeviceUsageKey),
                                 usageRef);
            CFArrayAppendValue(matching, dict);
        }

        if (pageRef)
            CFRelease(pageRef);
        if (usageRef)
            CFRelease(usageRef);

        CFRelease(dict);
    }

    IOHIDManagerSetDeviceMatchingMultiple(_desktop_window.ns.hidManager, matching);
    CFRelease(matching);

    IOHIDManagerRegisterDeviceMatchingCallback(_desktop_window.ns.hidManager,
                                               &matchCallback, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(_desktop_window.ns.hidManager,
                                              &removeCallback, NULL);
    IOHIDManagerScheduleWithRunLoop(_desktop_window.ns.hidManager,
                                    CFRunLoopGetMain(),
                                    kCFRunLoopDefaultMode);
    IOHIDManagerOpen(_desktop_window.ns.hidManager, kIOHIDOptionsTypeNone);

    // Execute the run loop once in order to register any initially-attached
    // joysticks
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateJoysticksCocoa(void)
{
    for (int jid = 0;  jid <= DESKTOP_WINDOW_JOYSTICK_LAST;  jid++)
    {
        if (_desktop_window.joysticks[jid].connected)
            closeJoystick(&_desktop_window.joysticks[jid]);
    }

    if (_desktop_window.ns.hidManager)
    {
        CFRelease(_desktop_window.ns.hidManager);
        _desktop_window.ns.hidManager = NULL;
    }
}


DESKTOP_WINDOWbool _desktop_windowPollJoystickCocoa(_DESKTOP_WINDOWjoystick* js, int mode)
{
    if (mode & _DESKTOP_WINDOW_POLL_AXES)
    {
        for (CFIndex i = 0;  i < CFArrayGetCount(js->ns.axes);  i++)
        {
            _DESKTOP_WINDOWjoyelementNS* axis = (_DESKTOP_WINDOWjoyelementNS*)
                CFArrayGetValueAtIndex(js->ns.axes, i);

            const long raw = getElementValue(js, axis);
            // Perform auto calibration
            if (raw < axis->minimum)
                axis->minimum = raw;
            if (raw > axis->maximum)
                axis->maximum = raw;

            const long size = axis->maximum - axis->minimum;
            if (size == 0)
                _desktop_windowInputJoystickAxis(js, (int) i, 0.f);
            else
            {
                const float value = (2.f * (raw - axis->minimum) / size) - 1.f;
                _desktop_windowInputJoystickAxis(js, (int) i, value);
            }
        }
    }

    if (mode & _DESKTOP_WINDOW_POLL_BUTTONS)
    {
        for (CFIndex i = 0;  i < CFArrayGetCount(js->ns.buttons);  i++)
        {
            _DESKTOP_WINDOWjoyelementNS* button = (_DESKTOP_WINDOWjoyelementNS*)
                CFArrayGetValueAtIndex(js->ns.buttons, i);
            const char value = getElementValue(js, button) - button->minimum;
            const int state = (value > 0) ? DESKTOP_WINDOW_PRESS : DESKTOP_WINDOW_RELEASE;
            _desktop_windowInputJoystickButton(js, (int) i, state);
        }

        for (CFIndex i = 0;  i < CFArrayGetCount(js->ns.hats);  i++)
        {
            const int states[9] =
            {
                DESKTOP_WINDOW_HAT_UP,
                DESKTOP_WINDOW_HAT_RIGHT_UP,
                DESKTOP_WINDOW_HAT_RIGHT,
                DESKTOP_WINDOW_HAT_RIGHT_DOWN,
                DESKTOP_WINDOW_HAT_DOWN,
                DESKTOP_WINDOW_HAT_LEFT_DOWN,
                DESKTOP_WINDOW_HAT_LEFT,
                DESKTOP_WINDOW_HAT_LEFT_UP,
                DESKTOP_WINDOW_HAT_CENTERED
            };

            _DESKTOP_WINDOWjoyelementNS* hat = (_DESKTOP_WINDOWjoyelementNS*)
                CFArrayGetValueAtIndex(js->ns.hats, i);
            long state = getElementValue(js, hat) - hat->minimum;
            if (state < 0 || state > 8)
                state = 8;

            _desktop_windowInputJoystickHat(js, (int) i, states[state]);
        }
    }

    return js->connected;
}

const char* _desktop_windowGetMappingNameCocoa(void)
{
    return "Mac OS X";
}

void _desktop_windowUpdateGamepadGUIDCocoa(char* guid)
{
    if ((strncmp(guid + 4, "000000000000", 12) == 0) &&
        (strncmp(guid + 20, "000000000000", 12) == 0))
    {
        char original[33];
        strncpy(original, guid, sizeof(original) - 1);
        sprintf(guid, "03000000%.4s0000%.4s000000000000",
                original, original + 16);
    }
}

#endif // _DESKTOP_WINDOW_COCOA

