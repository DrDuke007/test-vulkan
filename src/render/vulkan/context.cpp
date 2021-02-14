#include "render/vulkan/context.hpp"

#include "render/vulkan/utils.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/surface.hpp"
#include "platform/window.hpp"

#include <fmt/color.h>
#include <fmt/core.h>

namespace vulkan
{
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT /*message_severity*/,
                                                     VkDebugUtilsMessageTypeFlagsEXT /*message_type*/,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                     void * /*unused*/)
{
    auto style = fg(fmt::color::crimson) | fmt::emphasis::bold;
    fmt::print(stderr, style, "{}\n", pCallbackData->pMessage);

    if (pCallbackData->objectCount)
    {
        fmt::print(stderr, style, "Objects:\n");
        for (size_t i = 0; i < pCallbackData->objectCount; i++)
        {
            const auto &object = pCallbackData->pObjects[i];
            fmt::print(stderr, style, "\t [{}] {}\n", i, (object.pObjectName ? object.pObjectName : "NoName"));
        }
    }

    return VK_FALSE;
}

Context Context::create(bool enable_validation, const platform::Window *window)
{
    Context ctx = {};

    /// --- Create Instance
    Vec<const char *> instance_extensions;

    instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    instance_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    instance_extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#else
    assert(false);
#endif

    instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    uint layer_props_count = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_props_count, nullptr));
    Vec<VkLayerProperties> installed_instance_layers(layer_props_count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_props_count, installed_instance_layers.data()));

    Vec<const char *> instance_layers;
    if (enable_validation)
    {
        instance_layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkApplicationInfo app_info  = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName   = "Multi";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "GoodEngine";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 1, 0);
    app_info.apiVersion         = VK_API_VERSION_1_2;

    // TODO: add synchronization once the flag is available in vulkan_core.h
    std::array<VkValidationFeatureEnableEXT, 1> enables{VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT};

    VkValidationFeaturesEXT features       = {.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    features.enabledValidationFeatureCount = enables.size();
    features.pEnabledValidationFeatures    = enables.data();

    VkInstanceCreateInfo create_info    = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pNext                   = &features;
    create_info.flags                   = 0;
    create_info.pApplicationInfo        = &app_info;
    create_info.enabledLayerCount       = static_cast<uint32_t>(instance_layers.size());
    create_info.ppEnabledLayerNames     = instance_layers.data();
    create_info.enabledExtensionCount   = static_cast<uint32_t>(instance_extensions.size());
    create_info.ppEnabledExtensionNames = instance_extensions.data();

    VK_CHECK(vkCreateInstance(&create_info, nullptr, &ctx.instance));

    /// --- Load instance functions
#define X(name) ctx.name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(ctx.instance, #name))
    X(vkCreateDebugUtilsMessengerEXT);
    X(vkDestroyDebugUtilsMessengerEXT);
    X(vkCmdBeginDebugUtilsLabelEXT);
    X(vkCmdEndDebugUtilsLabelEXT);
    X(vkSetDebugUtilsObjectNameEXT);
#undef X

    /// --- Init debug layers
    if (enable_validation)
    {
        VkDebugUtilsMessengerCreateInfoEXT ci = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        ci.flags                              = 0;
        ci.messageSeverity                    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        ci.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        ci.messageType     |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = debug_callback;

        VkDebugUtilsMessengerEXT messenger;
        VK_CHECK(ctx.vkCreateDebugUtilsMessengerEXT(ctx.instance, &ci, nullptr, &messenger));
        ctx.debug_messenger = messenger;
    }


    /// --- Create window surface
    if (window)
    {
        ctx.surface = Surface::create(ctx, *window);
    }

    /// --- Pick devices
    uint physical_devices_count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(ctx.instance, &physical_devices_count, nullptr));
    Vec<VkPhysicalDevice> physical_devices(physical_devices_count);
    VK_CHECK(vkEnumeratePhysicalDevices(ctx.instance, &physical_devices_count, physical_devices.data()));

    for (uint i_device = 0; i_device < physical_devices_count; i_device++)
    {
        VkPhysicalDeviceProperties physical_props;
        vkGetPhysicalDeviceProperties(physical_devices[i_device], &physical_props);

        fmt::print("Found device: {}\n", physical_props.deviceName);
        if (physical_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            fmt::print("Prioritizing device {} because it is a discrete GPU.\n", physical_props.deviceName);
            ctx.main_device = i_device;
        }
    }

    if (ctx.main_device == u32_invalid)
    {
        ctx.main_device = 0;
        fmt::print("No discrete GPU found, defaulting to device #0.\n");
    }

    ctx.device = Device::create(ctx, physical_devices[ctx.main_device]);

    if (ctx.surface)
    {
        VkBool32 surface_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(ctx.device.physical_device, ctx.device.graphics_family_idx, ctx.surface->surface, &surface_support);
        assert(surface_support);
        ctx.surface->create_swapchain(ctx.device);
    }

    return ctx;
}

void Context::destroy()
{
    if (surface)
    {
        surface->destroy(*this);
        surface = std::nullopt;
    }

    device.destroy(*this);

    if (debug_messenger)
    {
        vkDestroyDebugUtilsMessengerEXT(instance, *debug_messenger, nullptr);
        debug_messenger = std::nullopt;
    }

    vkDestroyInstance(instance, nullptr);
}
}
