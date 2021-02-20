#include "render/vulkan/resources.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

namespace vulkan
{
Handle<Buffer> Device::create_buffer(const BufferDescription &buffer_desc)
{
    std::array queue_indices {transfer_family_idx, compute_family_idx, graphics_family_idx};

    VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.usage              = buffer_desc.usage;
    buffer_info.size               = buffer_desc.size;
    buffer_info.sharingMode        = VK_SHARING_MODE_CONCURRENT;
    buffer_info.queueFamilyIndexCount = queue_indices.size();
    buffer_info.pQueueFamilyIndices   = queue_indices.data();

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage     = buffer_desc.memory_usage;
    alloc_info.flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
    alloc_info.pUserData = const_cast<void *>(reinterpret_cast<const void *>(buffer_desc.name.c_str()));

    VkBuffer vkhandle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;

    VK_CHECK(vmaCreateBuffer(allocator,
                             reinterpret_cast<VkBufferCreateInfo *>(&buffer_info),
                             &alloc_info,
                             reinterpret_cast<VkBuffer *>(&vkhandle),
                             &allocation,
                             nullptr));
    return buffers.add({
            .desc = buffer_desc,
            .vkhandle = vkhandle,
            .allocation = allocation,
        });
}

void Device::destroy_buffer(Handle<Buffer> buffer_handle)
{
    if (auto *buffer = buffers.get(buffer_handle))
    {
        if (buffer->mapped)
        {
            vmaUnmapMemory(allocator, buffer->allocation);
            buffer->mapped = nullptr;
        }

        vmaDestroyBuffer(allocator, buffer->vkhandle, buffer->allocation);
        buffers.remove(buffer_handle);
    }
}


void *Device::map_buffer(Handle<Buffer> buffer_handle)
{
    auto &buffer = *buffers.get(buffer_handle);
    if (!buffer.mapped)
    {
        VK_CHECK(vmaMapMemory(allocator, buffer.allocation, &buffer.mapped));
    }

    return buffer.mapped;
}

}
