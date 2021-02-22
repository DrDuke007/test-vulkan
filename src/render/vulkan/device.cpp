#include "render/vulkan/device.hpp"

#include "base/types.hpp"
#include "base/log.hpp"

#include "render/vulkan/utils.hpp"
#include "render/vulkan/context.hpp"
#include "render/vulkan/surface.hpp"
#include "platform/window.hpp"
#include "vulkan/vulkan_core.h"

#include <array>

namespace vulkan
{

Device Device::create(const Context &context, VkPhysicalDevice physical_device)
{
    Device device = {};
    device.physical_device = physical_device;

#define X(name) device.name = context.name
    X(vkCreateDebugUtilsMessengerEXT);
    X(vkDestroyDebugUtilsMessengerEXT);
    X(vkCmdBeginDebugUtilsLabelEXT);
    X(vkCmdEndDebugUtilsLabelEXT);
    X(vkSetDebugUtilsObjectNameEXT);
#undef X

    vkGetPhysicalDeviceProperties(device.physical_device, &device.physical_props);

    /// --- Create the logical device
    uint installed_device_extensions_count = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device.physical_device,
                                                  nullptr,
                                                  &installed_device_extensions_count,
                                                  nullptr));
    Vec<VkExtensionProperties> installed_device_extensions(installed_device_extensions_count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device.physical_device,
                                                  nullptr,
                                                  &installed_device_extensions_count,
                                                  installed_device_extensions.data()));

    Vec<const char *> device_extensions;
    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    if (is_extension_installed(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, installed_device_extensions))
    {
        device_extensions.push_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
    }

    device.vulkan12_features              = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    device.physical_device_features       = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    device.physical_device_features.pNext = &device.vulkan12_features;
    vkGetPhysicalDeviceFeatures2(device.physical_device, &device.physical_device_features);

    uint queue_families_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device, &queue_families_count, nullptr);
    Vec<VkQueueFamilyProperties> queue_families(queue_families_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device.physical_device, &queue_families_count, queue_families.data());

    Vec<VkDeviceQueueCreateInfo> queue_create_infos;
    float priority = 0.0;

    for (uint32_t i = 0; i < queue_families.size(); i++)
    {
        VkDeviceQueueCreateInfo queue_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex = i;
        queue_info.queueCount       = 1;
        queue_info.pQueuePriorities = &priority;

        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            if (device.graphics_family_idx == u32_invalid)
            {
                queue_create_infos.push_back(queue_info);
                device.graphics_family_idx = i;
            }
        }
        else if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            if (device.compute_family_idx == u32_invalid && (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                queue_create_infos.push_back(queue_info);
                device.compute_family_idx = i;
            }
        }
        else if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            if (device.transfer_family_idx == u32_invalid && (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT))
            {
                queue_create_infos.push_back(queue_info);
                device.transfer_family_idx = i;
            }
        }
    }

    if (device.graphics_family_idx == u32_invalid
        || device.compute_family_idx == u32_invalid
        || device.transfer_family_idx == u32_invalid)
    {
        log::error("Failed to find a graphics, compute and transfer queue.\n");
        // throw std::runtime_error("Failed to find a graphics, compute and transfer queue.");
    }

    VkDeviceCreateInfo dci      = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext                   = &device.physical_device_features;
    dci.flags                   = 0;
    dci.queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.size());
    dci.pQueueCreateInfos       = queue_create_infos.data();
    dci.enabledLayerCount       = 0;
    dci.ppEnabledLayerNames     = nullptr;
    dci.enabledExtensionCount   = static_cast<uint32_t>(device_extensions.size());
    dci.ppEnabledExtensionNames = device_extensions.data();
    dci.pEnabledFeatures        = nullptr;

    VK_CHECK(vkCreateDevice(device.physical_device, &dci, nullptr, &device.device));

    /// --- Init VMA allocator
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.vulkanApiVersion       = VK_API_VERSION_1_2;
    allocator_info.physicalDevice         = device.physical_device;
    allocator_info.device                 = device.device;
    allocator_info.instance               = context.instance;
    allocator_info.flags                  = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    VK_CHECK(vmaCreateAllocator(&allocator_info, &device.allocator));

    /// --- Descriptor sets pool

    std::array pool_sizes{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 16},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          .descriptorCount = 16},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 16},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = 16},
    };

    VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.flags                      = 0;
    pool_info.poolSizeCount              = pool_sizes.size();
    pool_info.pPoolSizes                 = pool_sizes.data();
    pool_info.maxSets                    = 16;

    VK_CHECK(vkCreateDescriptorPool(device.device, &pool_info, nullptr, &device.descriptor_pool));

    device.samplers.resize(1);
    VkSamplerCreateInfo sampler_info = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sampler_info.magFilter           = VK_FILTER_LINEAR;
    sampler_info.minFilter           = VK_FILTER_LINEAR;
    sampler_info.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.compareOp           = VK_COMPARE_OP_NEVER;
    sampler_info.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    sampler_info.minLod              = 0;
    sampler_info.maxLod              = 7;
    sampler_info.maxAnisotropy       = 8.0f;
    sampler_info.anisotropyEnable    = true;
    VK_CHECK(vkCreateSampler(device.device, &sampler_info, nullptr, &device.samplers[0]));

    return device;
}

void Device::destroy(const Context &context)
{
    UNUSED(context);

    if (device == VK_NULL_HANDLE)
        return;

    for (auto &[handle, _] : graphics_programs)
        destroy_program(handle);

    for (auto &[handle, _] : shaders)
        destroy_shader(handle);

    for (auto &[handle, _] : renderpasses)
        destroy_renderpass(handle);

    for (auto &[handle, _] : framebuffers)
        destroy_framebuffer(handle);

    for (auto &[handle, _] : images)
        destroy_image(handle);

    for (auto &[handle, _] : buffers)
        destroy_buffer(handle);

    for (auto sampler : samplers)
        vkDestroySampler(device, sampler, nullptr);

    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
}
}
