#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

namespace my_app
{
    class Image
    {
        public:
        Image()
            : allocator_(nullptr)
            , image_info_()
            , image_()
            , mem_usage_()
            , allocation_()
        {
        }

        Image(VmaAllocator& allocator, vk::ImageCreateInfo image_info,
              VmaMemoryUsage mem_usage = VMA_MEMORY_USAGE_GPU_ONLY)
            : allocator_(&allocator)
            , image_info_(image_info)
            , image_()
            , mem_usage_(mem_usage)
            , allocation_()
        {
            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = mem_usage_;

            vmaCreateImage(*allocator_,
                           reinterpret_cast<VkImageCreateInfo*>(&image_info_),
                           &allocInfo,
                           reinterpret_cast<VkImage*>(&image_),
                           &allocation_,
                           nullptr);
        }

        void Free() { vmaDestroyImage(*allocator_, image_, allocation_); }

        vk::Image GetImage() const { return image_; }

        VmaMemoryUsage GetMemUsage() const { return mem_usage_; }

        private:
        VmaAllocator* allocator_;
        vk::ImageCreateInfo image_info_;
        vk::Image image_;
        VmaMemoryUsage mem_usage_;
        VmaAllocation allocation_;
    };
}    // namespace my_app
