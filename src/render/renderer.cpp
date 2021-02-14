#include "render/renderer.hpp"

#include "render/vulkan/commands.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"

Renderer Renderer::create(const platform::Window *window)
{
    Renderer renderer = {
        .context = gfx::Context::create(true, window)
    };

    auto &device = renderer.context.device;

    for (auto &work_pool : renderer.work_pools)
    {
        device.create_work_pool(work_pool);
    }

    for (auto &receipt : renderer.rendering_done)
    {
        receipt = device.signaled_receipt();
    }

    return renderer;
}

void Renderer::destroy()
{
    context.device.wait_idle();

    for (auto &work_pool : work_pools)
    {
        context.device.destroy_work_pool(work_pool);
    }

    context.destroy();
}

void Renderer::update()
{
    gfx::Device &device = context.device;
    auto current_frame = frame_count % FRAME_QUEUE_LENGTH;
    auto &image_acquired = this->image_acquired[current_frame];
    auto &rendering_done = this->rendering_done[current_frame];
    auto &work_pool = work_pools[current_frame];

    // wait for fence
    device.wait_for(rendering_done);
    device.reset_work_pool(work_pool);

    // receipt contains the image acquired semaphore
    image_acquired = device.acquire_next_swapchain(*context.surface, &image_acquired);

    // creating a work with a receipt means wait for this semaphore when submitting!
    gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);
    cmd.wait_for(image_acquired);

    // do stuff
    {
        cmd.begin();

        auto swapchain_image = context.surface->images[context.surface->current_image];

        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        // Undefined -> TransferDst
        {
        auto none_access         = gfx::get_src_image_access(gfx::ImageUsage::None);
        auto transfer_dst_access = gfx::get_dst_image_access(gfx::ImageUsage::TransferDst);
        auto b1 = gfx::get_image_barrier(swapchain_image, none_access, transfer_dst_access, range);
        vkCmdPipelineBarrier(cmd.command_buffer, none_access.stage, transfer_dst_access.stage, 0, 0, nullptr, 0, nullptr, 1, &b1);
        }

        // Clear image
        VkClearColorValue clear_color = {.float32 = {1.0f, 0.0f, 0.0f, 1.0f}};
        vkCmdClearColorImage(cmd.command_buffer, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &range);

        // TransferDst -> Present
        {
        auto transfer_dst_access = gfx::get_src_image_access(gfx::ImageUsage::TransferDst);
        auto present_access         = gfx::get_dst_image_access(gfx::ImageUsage::Present);
        auto b1 = gfx::get_image_barrier(swapchain_image, transfer_dst_access, present_access, range);
        vkCmdPipelineBarrier(cmd.command_buffer, transfer_dst_access.stage, present_access.stage, 0, 0, nullptr, 0, nullptr, 1, &b1);
        }

        cmd.end();
    }

    // submit signals a fence and semaphore
    rendering_done = device.submit(cmd, &rendering_done);

    // present will wait for semaphore
    device.present(rendering_done, *context.surface, device.graphics_family_idx);
    frame_count += 1;
}
