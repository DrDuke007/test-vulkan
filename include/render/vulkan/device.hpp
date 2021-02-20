#pragma once
#include "base/types.hpp"
#include "base/option.hpp"
#include "base/vector.hpp"
#include "base/pool.hpp"
#include "render/vulkan/commands.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/descriptor_set.hpp"

#include <array>
#include <utility>
#include <string_view>
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

    DescriptorSet global_set;

    Pool<Shader> shaders;
    Pool<GraphicsProgram> graphics_programs;
    Pool<RenderPass> renderpasses;
    Pool<Framebuffer> framebuffers;
    Pool<Image> images;

    /// ---

    static Device create(const Context &context, VkPhysicalDevice physical_device);
    void destroy(const Context &context);

    // Resources
    Receipt signaled_receipt();
    void create_work_pool(WorkPool &work_pool);
    void reset_work_pool(WorkPool &work_pool);
    void destroy_work_pool(WorkPool &work_pool);
    void destroy_receipt(Receipt &receipt);

    Handle<Shader> create_shader(std::string_view path);
    void destroy_shader(Handle<Shader> shader_handle);

    Handle<GraphicsProgram> create_program(const GraphicsState &graphics_state);
    void destroy_program(Handle<GraphicsProgram> program_handle);

    Handle<RenderPass> create_renderpass(const RenderAttachments &render_attachments);
    Handle<RenderPass> find_or_create_renderpass(const RenderAttachments &render_attachments);
    void destroy_renderpass(Handle<RenderPass> renderpass_handle);

    Handle<Framebuffer> create_framebuffer(const FramebufferDescription &desc);
    Handle<Framebuffer> find_or_create_framebuffer(const FramebufferDescription &desc);
    void destroy_framebuffer(Handle<Framebuffer> framebuffer_handle);

    Handle<Image> create_image(const ImageDescription &image_desc, Option<VkImage> proxy = {});
    void destroy_image(Handle<Image> image_handle);

    // Programs
    unsigned compile(Handle<GraphicsProgram> &program_handle, const RenderState &render_state);

    // Command submission
    GraphicsWork get_graphics_work(WorkPool &work_pool);
    ComputeWork  get_compute_work (WorkPool &work_pool);
    TransferWork get_transfer_work(WorkPool &work_pool);

    void wait_for(Receipt &receipt);
    void wait_idle();
    Receipt submit(Work &work, Receipt *reuse_receipt);

    // Swapchain
    std::pair<Receipt, bool> acquire_next_swapchain(Surface &surface, Receipt *reuse_receipt = nullptr);
    bool present(Receipt receipt, Surface &surface, WorkPool::POOL_TYPE pool_type);
};

}
