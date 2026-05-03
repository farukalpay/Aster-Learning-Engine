#include "internal.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define _DESKTOP_WINDOW_FIND_LOADER    1
#define _DESKTOP_WINDOW_REQUIRE_LOADER 2


//////////////////////////////////////////////////////////////////////////
//////                       DESKTOP_WINDOW internal API                      //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWbool _desktop_windowInitVulkan(int mode)
{
    VkResult err;
    VkExtensionProperties* ep;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
    uint32_t i, count;

    if (_desktop_window.vk.available)
        return DESKTOP_WINDOW_TRUE;

    if (_desktop_window.hints.init.vulkanLoader)
        _desktop_window.vk.GetInstanceProcAddr = _desktop_window.hints.init.vulkanLoader;
    else
    {
#if defined(_DESKTOP_WINDOW_VULKAN_LIBRARY)
        _desktop_window.vk.handle = _desktop_windowPlatformLoadModule(_DESKTOP_WINDOW_VULKAN_LIBRARY);
#elif defined(_DESKTOP_WINDOW_WIN32)
        _desktop_window.vk.handle = _desktop_windowPlatformLoadModule("vulkan-1.dll");
#elif defined(_DESKTOP_WINDOW_COCOA)
        _desktop_window.vk.handle = _desktop_windowPlatformLoadModule("libvulkan.1.dylib");
        if (!_desktop_window.vk.handle)
            _desktop_window.vk.handle = _desktop_windowLoadLocalVulkanLoaderCocoa();
#elif defined(__OpenBSD__) || defined(__NetBSD__)
        _desktop_window.vk.handle = _desktop_windowPlatformLoadModule("libvulkan.so");
#else
        _desktop_window.vk.handle = _desktop_windowPlatformLoadModule("libvulkan.so.1");
#endif
        if (!_desktop_window.vk.handle)
        {
            if (mode == _DESKTOP_WINDOW_REQUIRE_LOADER)
                _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE, "Vulkan: Loader not found");

            return DESKTOP_WINDOW_FALSE;
        }

        _desktop_window.vk.GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
            _desktop_windowPlatformGetModuleSymbol(_desktop_window.vk.handle, "vkGetInstanceProcAddr");
        if (!_desktop_window.vk.GetInstanceProcAddr)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "Vulkan: Loader does not export vkGetInstanceProcAddr");

            _desktop_windowTerminateVulkan();
            return DESKTOP_WINDOW_FALSE;
        }
    }

    vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)
        vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceExtensionProperties");
    if (!vkEnumerateInstanceExtensionProperties)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "Vulkan: Failed to retrieve vkEnumerateInstanceExtensionProperties");

        _desktop_windowTerminateVulkan();
        return DESKTOP_WINDOW_FALSE;
    }

    err = vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    if (err)
    {
        // NOTE: This happens on systems with a loader but without any Vulkan ICD
        if (mode == _DESKTOP_WINDOW_REQUIRE_LOADER)
        {
            _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                            "Vulkan: Failed to query instance extension count: %s",
                            _desktop_windowGetVulkanResultString(err));
        }

        _desktop_windowTerminateVulkan();
        return DESKTOP_WINDOW_FALSE;
    }

    ep = _desktop_window_calloc(count, sizeof(VkExtensionProperties));

    err = vkEnumerateInstanceExtensionProperties(NULL, &count, ep);
    if (err)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "Vulkan: Failed to query instance extensions: %s",
                        _desktop_windowGetVulkanResultString(err));

        _desktop_window_free(ep);
        _desktop_windowTerminateVulkan();
        return DESKTOP_WINDOW_FALSE;
    }

    for (i = 0;  i < count;  i++)
    {
        if (strcmp(ep[i].extensionName, "VK_KHR_surface") == 0)
            _desktop_window.vk.KHR_surface = DESKTOP_WINDOW_TRUE;
        else if (strcmp(ep[i].extensionName, "VK_KHR_win32_surface") == 0)
            _desktop_window.vk.KHR_win32_surface = DESKTOP_WINDOW_TRUE;
        else if (strcmp(ep[i].extensionName, "VK_MVK_macos_surface") == 0)
            _desktop_window.vk.MVK_macos_surface = DESKTOP_WINDOW_TRUE;
        else if (strcmp(ep[i].extensionName, "VK_EXT_metal_surface") == 0)
            _desktop_window.vk.EXT_metal_surface = DESKTOP_WINDOW_TRUE;
        else if (strcmp(ep[i].extensionName, "VK_KHR_xlib_surface") == 0)
            _desktop_window.vk.KHR_xlib_surface = DESKTOP_WINDOW_TRUE;
        else if (strcmp(ep[i].extensionName, "VK_KHR_xcb_surface") == 0)
            _desktop_window.vk.KHR_xcb_surface = DESKTOP_WINDOW_TRUE;
        else if (strcmp(ep[i].extensionName, "VK_KHR_wayland_surface") == 0)
            _desktop_window.vk.KHR_wayland_surface = DESKTOP_WINDOW_TRUE;
    }

    _desktop_window_free(ep);

    _desktop_window.vk.available = DESKTOP_WINDOW_TRUE;

    _desktop_window.platform.getRequiredInstanceExtensions(_desktop_window.vk.extensions);

    return DESKTOP_WINDOW_TRUE;
}

void _desktop_windowTerminateVulkan(void)
{
    if (_desktop_window.vk.handle)
        _desktop_windowPlatformFreeModule(_desktop_window.vk.handle);
}

const char* _desktop_windowGetVulkanResultString(VkResult result)
{
    switch (result)
    {
        case VK_SUCCESS:
            return "Success";
        case VK_NOT_READY:
            return "A fence or query has not yet completed";
        case VK_TIMEOUT:
            return "A wait operation has not completed in the specified time";
        case VK_EVENT_SET:
            return "An event is signaled";
        case VK_EVENT_RESET:
            return "An event is unsignaled";
        case VK_INCOMPLETE:
            return "A return array was too small for the result";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "A host memory allocation has failed";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "A device memory allocation has failed";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "Initialization of an object could not be completed for implementation-specific reasons";
        case VK_ERROR_DEVICE_LOST:
            return "The logical or physical device has been lost";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "Mapping of a memory object has failed";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "A requested layer is not present or could not be loaded";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "A requested extension is not supported";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "A requested feature is not supported";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "The requested version of Vulkan is not supported by the driver or is otherwise incompatible";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "Too many objects of the type have already been created";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "A requested format is not supported on this device";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "A surface is no longer available";
        case VK_SUBOPTIMAL_KHR:
            return "A swapchain no longer matches the surface properties exactly, but can still be used";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "A surface has changed in such a way that it is no longer compatible with the swapchain";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "The display used by a swapchain does not use the same presentable image layout";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "The requested window is already connected to a VkSurfaceKHR, or to some other non-Vulkan API";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "A validation layer found an error";
        default:
            return "ERROR: UNKNOWN VULKAN ERROR";
    }
}


//////////////////////////////////////////////////////////////////////////
//////                        DESKTOP_WINDOW public API                       //////
//////////////////////////////////////////////////////////////////////////

DESKTOP_WINDOWAPI int desktop_windowVulkanSupported(void)
{
    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);
    return _desktop_windowInitVulkan(_DESKTOP_WINDOW_FIND_LOADER);
}

DESKTOP_WINDOWAPI const char** desktop_windowGetRequiredInstanceExtensions(uint32_t* count)
{
    assert(count != NULL);

    *count = 0;

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (!_desktop_windowInitVulkan(_DESKTOP_WINDOW_REQUIRE_LOADER))
        return NULL;

    if (!_desktop_window.vk.extensions[0])
        return NULL;

    *count = 2;
    return (const char**) _desktop_window.vk.extensions;
}

DESKTOP_WINDOWAPI DESKTOP_WINDOWvkproc desktop_windowGetInstanceProcAddress(VkInstance instance,
                                              const char* procname)
{
    DESKTOP_WINDOWvkproc proc;
    assert(procname != NULL);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(NULL);

    if (!_desktop_windowInitVulkan(_DESKTOP_WINDOW_REQUIRE_LOADER))
        return NULL;

    // NOTE: Vulkan 1.0 and 1.1 vkGetInstanceProcAddr cannot return itself
    if (strcmp(procname, "vkGetInstanceProcAddr") == 0)
        return (DESKTOP_WINDOWvkproc) vkGetInstanceProcAddr;

    proc = (DESKTOP_WINDOWvkproc) vkGetInstanceProcAddr(instance, procname);
    if (!proc)
    {
        if (_desktop_window.vk.handle)
            proc = (DESKTOP_WINDOWvkproc) _desktop_windowPlatformGetModuleSymbol(_desktop_window.vk.handle, procname);
    }

    return proc;
}

DESKTOP_WINDOWAPI int desktop_windowGetPhysicalDevicePresentationSupport(VkInstance instance,
                                                     VkPhysicalDevice device,
                                                     uint32_t queuefamily)
{
    assert(instance != VK_NULL_HANDLE);
    assert(device != VK_NULL_HANDLE);

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(DESKTOP_WINDOW_FALSE);

    if (!_desktop_windowInitVulkan(_DESKTOP_WINDOW_REQUIRE_LOADER))
        return DESKTOP_WINDOW_FALSE;

    if (!_desktop_window.vk.extensions[0])
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "Vulkan: Window surface creation extensions not found");
        return DESKTOP_WINDOW_FALSE;
    }

    return _desktop_window.platform.getPhysicalDevicePresentationSupport(instance,
                                                               device,
                                                               queuefamily);
}

DESKTOP_WINDOWAPI VkResult desktop_windowCreateWindowSurface(VkInstance instance,
                                         DESKTOP_WINDOWwindow* handle,
                                         const VkAllocationCallbacks* allocator,
                                         VkSurfaceKHR* surface)
{
    _DESKTOP_WINDOWwindow* window = (_DESKTOP_WINDOWwindow*) handle;
    assert(instance != VK_NULL_HANDLE);
    assert(window != NULL);
    assert(surface != NULL);

    *surface = VK_NULL_HANDLE;

    _DESKTOP_WINDOW_REQUIRE_INIT_OR_RETURN(VK_ERROR_INITIALIZATION_FAILED);

    if (!_desktop_windowInitVulkan(_DESKTOP_WINDOW_REQUIRE_LOADER))
        return VK_ERROR_INITIALIZATION_FAILED;

    if (!_desktop_window.vk.extensions[0])
    {
        _desktop_windowInputError(DESKTOP_WINDOW_API_UNAVAILABLE,
                        "Vulkan: Window surface creation extensions not found");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    if (window->context.client != DESKTOP_WINDOW_NO_API)
    {
        _desktop_windowInputError(DESKTOP_WINDOW_INVALID_VALUE,
                        "Vulkan: Window surface creation requires the window to have the client API set to DESKTOP_WINDOW_NO_API");
        return VK_ERROR_NATIVE_WINDOW_IN_USE_KHR;
    }

    return _desktop_window.platform.createWindowSurface(instance, window, allocator, surface);
}

