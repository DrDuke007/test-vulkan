#pragma once
#include "base/vector.hpp"
#include "base/handle.hpp"

#include <vulkan/vulkan.h>

namespace vulkan
{
struct Device;
struct Image;
struct Buffer;
struct GraphicsState;

struct ImageDescriptor
{
    Handle<Image> image_handle;
};

struct BufferDescriptor
{
    Handle<Buffer> buffer_handle;
};

struct DynamicDescriptor
{
    Handle<Buffer> buffer_handle;
    usize offset;
};

struct DescriptorType
{
    static const u32 SampledImage  = 0;
    static const u32 StorageImage  = 1;
    static const u32 StorageBuffer = 2;
    static const u32 DynamicBuffer = 3;

    union
    {
        struct
        {
            u32 count : 24;
            u32 type : 8;
        };
        u32 raw;
    };
};

struct Descriptor
{
    union
    {
        ImageDescriptor image;
        BufferDescriptor buffer;
        DynamicDescriptor dynamic;
    };
};
struct DescriptorSet
{
    VkDescriptorSetLayout layout;
    Vec<VkDescriptorSet> descriptor_set_pool;
    Vec<uint> frame_used_pool;
    Vec<Descriptor> descriptors;
};

void destroy_descriptor_set(Device &device, DescriptorSet &set);
DescriptorSet create_descriptor_set(Device &device, const GraphicsState &graphics_state);
}
