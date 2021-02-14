#pragma once
#include "base/types.hpp"
#include "base/vector.hpp"

#include <vulkan/vulkan.h>

namespace platform { struct Window; }

namespace vulkan
{
struct Context;
struct Device;

struct Surface
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    u32 current_image = u32_invalid;
    Vec<VkImage> images;

    static Surface create(const Context &context, const platform::Window &window);
    void destroy(const Context &context);
    void create_swapchain(const Device &device);
    void destroy_swapchain(const Device &device);
};
}
