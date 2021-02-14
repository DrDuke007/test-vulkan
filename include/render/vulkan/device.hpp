#pragma once
#include "base/types.hpp"
#include "base/option.hpp"
#include "base/vector.hpp"
#include "render/vulkan/commands.hpp"

#include <array>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace vulkan
{
struct Context;
struct Surface;

struct CommandPool
{
    VkCommandPool vk_handle = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> free_list = {};
};

struct WorkPool
{
    enum POOL_TYPE
    {
        POOL_TYPE_GRAPHICS = 0,
        POOL_TYPE_COMPUTE  = 1,
        POOL_TYPE_TRANSFER = 2
    };

    std::array<CommandPool, 3> command_pools;

    CommandPool &graphics() { return command_pools[POOL_TYPE_GRAPHICS]; }
    CommandPool &compute()  { return command_pools[POOL_TYPE_COMPUTE];  }
    CommandPool &transfer() { return command_pools[POOL_TYPE_TRANSFER]; }
};

struct Device
{
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties physical_props;
    VkPhysicalDeviceVulkan12Features vulkan12_features;
    VkPhysicalDeviceFeatures2 physical_device_features;
    u32 graphics_family_idx = u32_invalid;
    u32 compute_family_idx = u32_invalid;
    u32 transfer_family_idx = u32_invalid;
    VmaAllocator allocator;

    /// ---

    static Device create(const Context &context, VkPhysicalDevice physical_device);
    void destroy(const Context &context);

    // Resources


    // Command submission
    void create_work_pool(WorkPool &work_pool);
    void reset_work_pool(WorkPool &work_pool);
    void destroy_work_pool(WorkPool &work_pool);

    GraphicsWork get_graphics_work(WorkPool &work_pool);
    ComputeWork  get_compute_work (WorkPool &work_pool);
    TransferWork get_transfer_work(WorkPool &work_pool);

    Receipt signaled_receipt();
    Receipt submit(const Work &work, u32 queue_family_idx, Receipt *reuse_receipt);
    inline Receipt submit(const TransferWork &work, Receipt *reuse_receipt = nullptr) { return submit(work.work, transfer_family_idx, reuse_receipt); }
    inline Receipt submit(const ComputeWork &work,  Receipt *reuse_receipt = nullptr) { return submit(work.work.work, compute_family_idx, reuse_receipt); }
    inline Receipt submit(const GraphicsWork &work, Receipt *reuse_receipt = nullptr) { return submit(work.work.work.work, graphics_family_idx, reuse_receipt); }

    void wait_for(Receipt &receipt);
    void wait_idle();

    // Swapchain
    Receipt acquire_next_swapchain(Surface &surface, Receipt *reuse_receipt = nullptr);
    void present(Receipt receipt, Surface &surface, u32 queue_family_idx);
};
}
