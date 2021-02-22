#include "render/vulkan/commands.hpp"
#include "render/vulkan/descriptor_set.hpp"
#include "render/vulkan/device.hpp"

#include "render/vulkan/surface.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

namespace vulkan
{

/// --- Work

void Work::begin()
{
    VkCommandBufferBeginInfo binfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    binfo.flags                    = 0;
    vkBeginCommandBuffer(command_buffer, &binfo);
}

void Work::end()
{
    VK_CHECK(vkEndCommandBuffer(command_buffer));
}

void Work::wait_for(Receipt previous_work, VkPipelineStageFlags stage_dst)
{
    wait_list.push_back(previous_work);
    wait_stage_list.push_back(stage_dst);
}

void Work::barrier(Handle<Image> image_handle, ImageUsage usage_destination)
{
    auto &image = *device->images.get(image_handle);

    auto src_access = get_src_image_access(image.usage);
    auto dst_access = get_dst_image_access(usage_destination);
    auto b          = get_image_barrier(image.vkhandle, src_access, dst_access, image.full_range);
    vkCmdPipelineBarrier(command_buffer, src_access.stage, dst_access.stage, 0, 0, nullptr, 0, nullptr, 1, &b);

    image.usage = usage_destination;
}


void Work::barriers(Vec<std::pair<Handle<Image>, ImageUsage>> images, Vec<std::pair<Handle<Buffer>, BufferUsage>> buffers)
{
    Vec<VkImageMemoryBarrier> image_barriers;
    Vec<VkBufferMemoryBarrier> buffer_barriers;

    VkPipelineStageFlags src_stage = 0;
    VkPipelineStageFlags dst_stage = 0;

    for (auto &[image_handle, usage_dst] : images)
    {
        auto &image = *device->images.get(image_handle);
        auto src_access = get_src_image_access(image.usage);
        auto dst_access = get_dst_image_access(usage_dst);
        image_barriers.push_back(get_image_barrier(image.vkhandle, src_access, dst_access, image.full_range));
        src_stage |= src_access.stage;
        dst_stage |= dst_access.stage;

        image.usage = usage_dst;
    }

    for (auto &[buffer_handle, usage_dst] : buffers)
    {
        auto &buffer = *device->buffers.get(buffer_handle);
        auto src_access = get_src_buffer_access(buffer.usage);
        auto dst_access = get_dst_buffer_access(usage_dst);
        buffer_barriers.push_back(get_buffer_barrier(buffer.vkhandle, src_access, dst_access, 0, buffer.desc.size));
        src_stage |= src_access.stage;
        dst_stage |= dst_access.stage;

        buffer.usage = usage_dst;
    }

    vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, nullptr, buffer_barriers.size(), buffer_barriers.data(), image_barriers.size(), image_barriers.data());
}

/// --- TransferWork

void TransferWork::copy_buffer(Handle<Buffer> src, Handle<Buffer> dst)
{
    auto &src_buffer = *device->buffers.get(src);
    auto &dst_buffer = *device->buffers.get(dst);

    VkBufferCopy copy = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = std::min(src_buffer.desc.size, dst_buffer.desc.size),
    };

    vkCmdCopyBuffer(command_buffer, src_buffer.vkhandle, dst_buffer.vkhandle, 1, &copy);
}

void TransferWork::fill_buffer(Handle<Buffer> buffer_handle, u32 data)
{
    auto &buffer = *device->buffers.get(buffer_handle);
    vkCmdFillBuffer(command_buffer, buffer.vkhandle, 0, buffer.desc.size, data);
}
/// --- ComputeWork

void ComputeWork::clear_image(Handle<Image> image_handle, VkClearColorValue clear_color)
{
    auto image = *device->images.get(image_handle);

    vkCmdClearColorImage(command_buffer, image.vkhandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &image.full_range);
}

void ComputeWork::bind_buffer(Handle<GraphicsProgram> program_handle, uint slot, Handle<Buffer> buffer_handle)
{
    auto &program = *device->graphics_programs.get(program_handle);
    ::vulkan::bind_buffer(program.descriptor_set, slot, buffer_handle);
}

void ComputeWork::bind_image(Handle<GraphicsProgram> program_handle, uint slot, Handle<Image> image_handle)
{
    auto &program = *device->graphics_programs.get(program_handle);
    ::vulkan::bind_image(program.descriptor_set, slot, image_handle);
}

/// --- GraphicsWork


void GraphicsWork::draw_indexed(const DrawIndexedOptions &options)
{
    vkCmdDrawIndexed(command_buffer, options.vertex_count, options.instance_count, options.index_offset, options.vertex_offset, options.instance_offset);
}

void GraphicsWork::set_scissor(const VkRect2D &rect)
{
    vkCmdSetScissor(command_buffer, 0, 1, &rect);
}

void GraphicsWork::set_viewport(const VkViewport &viewport)
{
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
}

void GraphicsWork::begin_pass(Handle<RenderPass> renderpass_handle, Handle<Framebuffer> framebuffer_handle, Vec<Handle<Image>> attachments, Vec<VkClearValue> clear_values)
{
    auto &renderpass = *device->renderpasses.get(renderpass_handle);
    auto &framebuffer = *device->framebuffers.get(framebuffer_handle);

    Vec<VkImageView> views(attachments.size());
    for (u32 i_attachment = 0; i_attachment < attachments.size(); i_attachment++)
    {
        views[i_attachment] = device->images.get(attachments[i_attachment])->full_view;
    }

    VkRenderPassAttachmentBeginInfo attachments_info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO };
    attachments_info.attachmentCount = views.size();
    attachments_info.pAttachments = views.data();

    VkRenderPassBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    begin_info.pNext = &attachments_info;
    begin_info.renderPass = renderpass.vkhandle;
    begin_info.framebuffer = framebuffer.vkhandle;
    begin_info.renderArea.extent.width = framebuffer.desc.width;
    begin_info.renderArea.extent.height = framebuffer.desc.height;
    begin_info.clearValueCount = clear_values.size();
    begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GraphicsWork::end_pass()
{
    vkCmdEndRenderPass(command_buffer);
}

void GraphicsWork::bind_pipeline(Handle<GraphicsProgram> program_handle, uint pipeline_index)
{
    auto &program = *device->graphics_programs.get(program_handle);
    auto pipeline = program.pipelines[pipeline_index];

    std::array sets {
        find_or_create_descriptor_set(*device, program.descriptor_set)
    };

    Vec<u32> offsets = {};

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program.pipeline_layout, 0, sets.size(), sets.data(), offsets.size(), offsets.data());
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void GraphicsWork::bind_index_buffer(Handle<Buffer> buffer_handle)
{
    auto &buffer = *device->buffers.get(buffer_handle);
    vkCmdBindIndexBuffer(command_buffer, buffer.vkhandle, 0, VK_INDEX_TYPE_UINT16);
}

/// --- Device

// WorkPool
void Device::create_work_pool(WorkPool &work_pool)
{
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = 0,
        .queueFamilyIndex = this->graphics_family_idx,
    };

    VK_CHECK(vkCreateCommandPool(this->device, &pool_info, nullptr, &work_pool.graphics().vk_handle));

    pool_info.queueFamilyIndex = this->compute_family_idx;
    VK_CHECK(vkCreateCommandPool(this->device, &pool_info, nullptr, &work_pool.compute().vk_handle));

    pool_info.queueFamilyIndex = this->transfer_family_idx;
    VK_CHECK(vkCreateCommandPool(this->device, &pool_info, nullptr, &work_pool.transfer().vk_handle));
}

void Device::reset_work_pool(WorkPool &work_pool)
{
    for (auto &command_pool : work_pool.command_pools)
    {
        vkFreeCommandBuffers(this->device, command_pool.vk_handle, command_pool.free_list.size(), command_pool.free_list.data());
        command_pool.free_list.clear();

        VK_CHECK(vkResetCommandPool(this->device, command_pool.vk_handle, 0));
    }
}

void Device::destroy_work_pool(WorkPool &work_pool)
{
    for (auto &command_pool : work_pool.command_pools)
    {
        vkDestroyCommandPool(device, command_pool.vk_handle, nullptr);
    }
}

// Work
static Work create_work(Device &device, WorkPool &work_pool, WorkPool::POOL_TYPE pool_type)
{
    auto &command_pool = work_pool.command_pools[pool_type];

    Work work = {};
    work.device = &device;

    VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool                 = command_pool.vk_handle;
    ai.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount          = 1;
    VK_CHECK(vkAllocateCommandBuffers(device.device, &ai, &work.command_buffer));

    u32 queue_family_idx =
          pool_type == WorkPool::POOL_TYPE_GRAPHICS ? device.graphics_family_idx
        : pool_type == WorkPool::POOL_TYPE_COMPUTE  ? device.compute_family_idx
        : pool_type == WorkPool::POOL_TYPE_TRANSFER ? device.transfer_family_idx
        : u32_invalid;

    assert(queue_family_idx != u32_invalid);
    vkGetDeviceQueue(device.device, queue_family_idx, 0, &work.queue);

    command_pool.free_list.push_back(work.command_buffer);

    return work;
}

GraphicsWork Device::get_graphics_work(WorkPool &work_pool)
{
    return {{{{create_work(*this, work_pool, WorkPool::POOL_TYPE_GRAPHICS)}}}};
}

ComputeWork Device::get_compute_work(WorkPool &work_pool)
{
    return {{{create_work(*this, work_pool, WorkPool::POOL_TYPE_COMPUTE)}}};
}

TransferWork Device::get_transfer_work(WorkPool &work_pool)
{
    return {{create_work(*this, work_pool, WorkPool::POOL_TYPE_TRANSFER)}};
}

// Receipt
Receipt Device::signaled_receipt()
{
    Receipt receipt = {};
    VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(device, &fci, nullptr, &receipt.fence));
    return receipt;
}

void Device::destroy_receipt(Receipt &receipt)
{
    vkDestroyFence(device, receipt.fence, nullptr);
    vkDestroySemaphore(device, receipt.semaphore, nullptr);
    receipt.fence = VK_NULL_HANDLE;
    receipt.semaphore = VK_NULL_HANDLE;
}

// Submission
Receipt Device::submit(Work &work, Receipt *reuse_receipt)
{
    // Create the receipt
    Receipt receipt = {};
    if (reuse_receipt)
    {
        receipt = *reuse_receipt;
    }

    if (receipt.fence == VK_NULL_HANDLE)
    {
        VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VK_CHECK(vkCreateFence(device, &fence_info, nullptr, &receipt.fence));
    }

    if (receipt.semaphore == VK_NULL_HANDLE)
    {
        VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &semaphore_info, nullptr, &receipt.semaphore));
    }

    // Creathe list of semaphores to wait
    Vec<VkSemaphore> wait_list;
    wait_list.reserve(wait_list.size());
    for (const auto &wait : work.wait_list)
    {
        if (wait.semaphore != VK_NULL_HANDLE)
        {
            wait_list.push_back(wait.semaphore);
        }
    }

    //TODO: check if needed
    vkResetFences(device, 1, &receipt.fence);

    VkSubmitInfo submit_info            = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.waitSemaphoreCount      = wait_list.size();
    submit_info.pWaitSemaphores         = wait_list.data();
    submit_info.pWaitDstStageMask       = work.wait_stage_list.data();
    submit_info.commandBufferCount      = 1;
    submit_info.pCommandBuffers         = &work.command_buffer;
    submit_info.signalSemaphoreCount    = 1;
    submit_info.pSignalSemaphores       = &receipt.semaphore;

    VK_CHECK(vkQueueSubmit(work.queue, 1, &submit_info, receipt.fence));

    return receipt;
}

bool Device::present(Receipt receipt, Surface &surface, WorkPool::POOL_TYPE pool_type)
{
    u32 queue_family_idx =
          pool_type == WorkPool::POOL_TYPE_GRAPHICS ? this->graphics_family_idx
        : pool_type == WorkPool::POOL_TYPE_COMPUTE  ? this->compute_family_idx
        : pool_type == WorkPool::POOL_TYPE_TRANSFER ? this->transfer_family_idx
        : u32_invalid;

    assert(queue_family_idx != u32_invalid);

    VkPresentInfoKHR present_i   = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_i.waitSemaphoreCount = 1;
    present_i.pWaitSemaphores    = &receipt.semaphore;
    present_i.swapchainCount     = 1;
    present_i.pSwapchains        = &surface.swapchain;
    present_i.pImageIndices      = &surface.current_image;

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(this->device, queue_family_idx, 0, &queue);

    auto res = vkQueuePresentKHR(queue, &present_i);

    if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return true;
    }
    else
    {
        VK_CHECK(res);
    }

    return false;
}

void Device::wait_for(Receipt &receipt)
{
    assert(receipt.fence != VK_NULL_HANDLE);

    // 10 sec in nanoseconds
    u64 timeout = 10llu*1000llu*1000llu*1000llu;
    auto wait_result = vkWaitForFences(device, 1, &receipt.fence, true, timeout);
    if (wait_result == VK_TIMEOUT)
    {
        throw std::runtime_error("Submitted command buffer more than 10 second ago.");
    }
    VK_CHECK(wait_result);

    // reset the fence for future use
    VK_CHECK(vkResetFences(device, 1, &receipt.fence));
}

void Device::wait_idle()
{
    VK_CHECK(vkDeviceWaitIdle(device));
}

std::pair<Receipt, bool> Device::acquire_next_swapchain(Surface &surface, Receipt *reuse_receipt)
{
    bool error = false;

    Receipt receipt = {};
    if (reuse_receipt)
    {
        receipt = *reuse_receipt;
    }
    if (receipt.semaphore == VK_NULL_HANDLE)
    {
        VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &semaphore_info, nullptr, &receipt.semaphore));
    }

    auto res = vkAcquireNextImageKHR(
        device,
        surface.swapchain,
        std::numeric_limits<uint64_t>::max(),
        receipt.semaphore,
        nullptr,
        &surface.current_image);

    if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        error = true;
    }
    else
    {
        VK_CHECK(res);
    }

    return {receipt, error};
}
}
