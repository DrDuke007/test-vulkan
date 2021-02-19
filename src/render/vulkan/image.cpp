#include "render/vulkan/resources.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"


namespace vulkan
{
Handle<Image> Device::create_image(const ImageDescription &image_desc)
{
    VkImageCreateInfo image_info     = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType             = image_desc.type;
    image_info.format                = image_desc.format;
    image_info.extent.width          = image_desc.size.x;
    image_info.extent.height         = image_desc.size.y;
    image_info.extent.depth          = image_desc.size.z;
    image_info.mipLevels             = 1;
    image_info.arrayLayers           = 1;
    image_info.samples               = image_desc.samples;
    image_info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage                 = image_desc.usages;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices   = nullptr;
    image_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    image_info.tiling                = VK_IMAGE_TILING_OPTIMAL;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
    alloc_info.usage     = image_desc.memory_usage;
    alloc_info.pUserData = const_cast<void *>(reinterpret_cast<const void *>(image_desc.name.c_str()));

    VkImage vkhandle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VK_CHECK(vmaCreateImage(allocator,
                            reinterpret_cast<VkImageCreateInfo *>(&image_info),
                            &alloc_info,
                            reinterpret_cast<VkImage *>(&vkhandle),
                            &allocation,
                            nullptr));

    return images.add({
            .desc = image_desc,
            .vkhandle = vkhandle,
            .allocation = allocation,
            .usage = ImageUsage::None,
            .is_proxy = false,
        });
}

void Device::destroy_image(Handle<Image> image_handle)
{
    if (auto *image = images.get(image_handle))
    {
        vmaDestroyImage(allocator, image->vkhandle, image->allocation);
        images.remove(image_handle);
    }
}
};
