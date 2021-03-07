#include "render/vulkan/resources.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

namespace vulkan
{
Handle<Buffer> Device::create_buffer(const BufferDescription &buffer_desc)
{
    VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.usage              = buffer_desc.usage;
    buffer_info.size               = buffer_desc.size;
    buffer_info.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.queueFamilyIndexCount = 0;
    buffer_info.pQueueFamilyIndices   = nullptr;

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

    if (vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT name_info = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectHandle                  = reinterpret_cast<u64>(vkhandle);
        name_info.objectType                    = VK_OBJECT_TYPE_BUFFER;
        name_info.pObjectName                   = buffer_desc.name.c_str();
        VK_CHECK(vkSetDebugUtilsObjectNameEXT(device, &name_info));
    }

    u64  gpu_address = 0;
    if (buffer_desc.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo address_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        address_info.buffer = vkhandle;
        gpu_address = vkGetBufferDeviceAddress(device, &address_info);
    }

    return buffers.add({
            .desc = buffer_desc,
            .vkhandle = vkhandle,
            .allocation = allocation,
            .gpu_address = gpu_address,
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

u64 Device::get_buffer_address(Handle<Buffer> buffer_handle)
{
    auto &buffer = *buffers.get(buffer_handle);

        VkBufferDeviceAddressInfo address_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        address_info.buffer = buffer.vkhandle;
        buffer.gpu_address = vkGetBufferDeviceAddress(device, &address_info);

    return buffer.gpu_address;
}

void Device::flush_buffer(Handle<Buffer> buffer_handle)
{
    auto &buffer = *buffers.get(buffer_handle);
    if (buffer.mapped)
    {
        vmaFlushAllocation(allocator, buffer.allocation, 0, buffer.desc.size);
    }
}

}
