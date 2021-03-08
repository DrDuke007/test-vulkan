#include "render/vulkan/context.hpp"

#include "render/vulkan/operators.hpp"
#include "render/vulkan/utils.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/surface.hpp"
#include "platform/window.hpp"

#include "base/logger.hpp"

namespace vulkan
{
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT /*message_severity*/,
                                                     VkDebugUtilsMessageTypeFlagsEXT /*message_type*/,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                     void * /*unused*/)
{
    logger::error("{}\n", pCallbackData->pMessage);

    if (pCallbackData->objectCount)
    {
        logger::error("Objects:\n");
        for (size_t i = 0; i < pCallbackData->objectCount; i++)
        {
            const auto &object = pCallbackData->pObjects[i];
            logger::error("\t [{}] {}\n", i, (object.pObjectName ? object.pObjectName : "NoName"));
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

    VkInstanceCreateInfo create_info    = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pNext                   = nullptr;
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

        logger::info("Found device: {}\n", physical_props.deviceName);
        if (ctx.main_device == u32_invalid && physical_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            logger::info("Prioritizing device {} because it is a discrete GPU.\n", physical_props.deviceName);
            ctx.main_device = i_device;
        }
    }

    if (ctx.main_device == u32_invalid)
    {
        ctx.main_device = 0;
        logger::info("No discrete GPU found, defaulting to device #0.\n");
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


bool operator==(const VkPipelineShaderStageCreateInfo &a, const VkPipelineShaderStageCreateInfo &b)
{
    return a.flags == b.flags && a.stage == b.stage && a.module == b.module && a.pName == b.pName // TODO: strcmp?
           && a.pSpecializationInfo == b.pSpecializationInfo;                                     // TODO: deep cmp?
}

bool operator==(const VkDescriptorBufferInfo &a, const VkDescriptorBufferInfo &b)
{
    return a.buffer == b.buffer && a.offset == b.offset && a.range == b.range;
}

bool operator==(const VkDescriptorImageInfo &a, const VkDescriptorImageInfo &b)
{
    return a.sampler == b.sampler && a.imageView == b.imageView && a.imageLayout == b.imageLayout;
}

bool operator==(const VkExtent3D &a, const VkExtent3D &b)
{
    return a.width == b.width && a.height == b.height && a.depth == b.depth;
}

bool operator==(const VkImageSubresourceRange &a, const VkImageSubresourceRange &b)
{
    return a.aspectMask == b.aspectMask && a.baseMipLevel == b.baseMipLevel && a.levelCount == b.levelCount
           && a.baseArrayLayer == b.baseArrayLayer && a.layerCount == b.layerCount;
}

bool operator==(const VkImageCreateInfo &a, const VkImageCreateInfo &b)
{
    bool same = a.queueFamilyIndexCount == b.queueFamilyIndexCount;
    if (!same)
    {
        return false;
    }

    if (a.pQueueFamilyIndices && b.pQueueFamilyIndices)
    {
        for (usize i = 0; i < a.queueFamilyIndexCount; i++)
        {
            if (a.pQueueFamilyIndices[i] != b.pQueueFamilyIndices[i])
            {
                return false;
            }
        }
    }
    else
    {
        same = a.pQueueFamilyIndices == b.pQueueFamilyIndices;
    }

    return same && a.flags == b.flags && a.imageType == b.imageType && a.format == b.format && a.extent == b.extent
           && a.mipLevels == b.mipLevels && a.arrayLayers == b.arrayLayers && a.samples == b.samples
           && a.tiling == b.tiling && a.usage == b.usage && a.sharingMode == b.sharingMode
           && a.initialLayout == b.initialLayout;
}

bool operator==(const VkComputePipelineCreateInfo &a, const VkComputePipelineCreateInfo &b)
{
    return a.flags == b.flags && a.stage == b.stage && a.layout == b.layout
           && a.basePipelineHandle == b.basePipelineHandle && a.basePipelineIndex == b.basePipelineIndex;
}

bool operator==(const VkFramebufferCreateInfo &a, const VkFramebufferCreateInfo &b)
{
    if (a.attachmentCount != b.attachmentCount)
    {
        return false;
    }

    for (uint i = 0; i < a.attachmentCount; i++)
    {
        if (a.pAttachments[i] != b.pAttachments[i])
        {
            return false;
        }
    }

    return a.flags == b.flags && a.renderPass == b.renderPass && a.width == b.width && a.height == b.height
           && a.layers == b.layers;
}
}
