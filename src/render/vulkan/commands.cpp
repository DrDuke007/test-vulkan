#include "render/vulkan/commands.hpp"
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

void Work::wait_for(Receipt previous_work)
{
    wait_list.push_back(previous_work);
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

    VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool                 = command_pool.vk_handle;
    ai.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount          = 1;
    VK_CHECK(vkAllocateCommandBuffers(device.device, &ai, &work.command_buffer));

    command_pool.free_list.push_back(work.command_buffer);

    return work;
}

GraphicsWork Device::get_graphics_work(WorkPool &work_pool)
{
    return {.work = create_work(*this, work_pool, WorkPool::POOL_TYPE_GRAPHICS)};
}

ComputeWork Device::get_compute_work(WorkPool &work_pool)
{
    return {.work = create_work(*this, work_pool, WorkPool::POOL_TYPE_COMPUTE)};
}

TransferWork Device::get_transfer_work(WorkPool &work_pool)
{
    return {.work = create_work(*this, work_pool, WorkPool::POOL_TYPE_TRANSFER)};
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

// Submission
Receipt Device::submit(const Work &work, u32 queue_family_idx, Receipt *reuse_receipt)
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
    wait_list.reserve(work.wait_list.size());
    for (const auto &wait : work.wait_list)
    {
        if (wait.semaphore != VK_NULL_HANDLE)
        {
            wait_list.push_back(wait.semaphore);
        }
    }

    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info            = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.waitSemaphoreCount      = wait_list.size();
    submit_info.pWaitSemaphores         = wait_list.data();
    submit_info.pWaitDstStageMask       = &stage;
    submit_info.commandBufferCount      = 1;
    submit_info.pCommandBuffers         = &work.command_buffer;
    submit_info.signalSemaphoreCount    = 1;
    submit_info.pSignalSemaphores       = &receipt.semaphore;

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queue_family_idx, 0, &queue);

    VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, receipt.fence));

    return receipt;
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

Receipt Device::acquire_next_swapchain(Surface &surface, Receipt *reuse_receipt)
{
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

    if (res != VK_SUBOPTIMAL_KHR)
    {
        VK_CHECK(res);
    }

    return receipt;
}

void Device::present(Receipt receipt, Surface &surface, u32 queue_family_idx)
{
    VkPresentInfoKHR present_i   = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_i.waitSemaphoreCount = 1;
    present_i.pWaitSemaphores    = &receipt.semaphore;
    present_i.swapchainCount     = 1;
    present_i.pSwapchains        = &surface.swapchain;
    present_i.pImageIndices      = &surface.current_image;

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queue_family_idx, 0, &queue);

    auto res = vkQueuePresentKHR(queue, &present_i);
    if (res != VK_SUBOPTIMAL_KHR && res != VK_ERROR_OUT_OF_DATE_KHR)
    {
        VK_CHECK(res);
    }
}
}
