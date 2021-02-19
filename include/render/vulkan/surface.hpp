#pragma once
#include "base/types.hpp"
#include "base/vector.hpp"
#include "base/handle.hpp"

#include <vulkan/vulkan.h>

namespace platform { struct Window; }

namespace vulkan
{
struct Context;
struct Device;
struct Image;

struct Surface
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    VkSurfaceFormatKHR format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    u32 current_image = u32_invalid;
    Vec<Handle<Image>> images;

    static Surface create(Context &context, const platform::Window &window);
    void destroy(Context &context);
    void create_swapchain(Device &device);
    void destroy_swapchain(Device &device);
};
}
