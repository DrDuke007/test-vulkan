#include "render/vulkan/descriptor_set.hpp"

#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

#include <algorithm>
#include <ranges>

namespace vulkan
{
DescriptorSet create_descriptor_set(Device &device, const GraphicsState &graphics_state)
{
    DescriptorSet descriptor_set = {};

    Vec<VkDescriptorSetLayoutBinding> bindings;

    uint binding_number = 0;
    for (const auto &descriptor_type : graphics_state.descriptors)
    {
        bindings.emplace_back();
        auto &binding = bindings.back();
        binding.binding = binding_number;
        binding.descriptorType = to_vk(descriptor_type);
        binding.descriptorCount = descriptor_type.count ? descriptor_type.count : 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        binding_number += 1;
    }

    VkDescriptorSetLayoutCreateInfo desc_layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    desc_layout_info.bindingCount = bindings.size();
    desc_layout_info.pBindings    = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device.device, &desc_layout_info, nullptr, &descriptor_set.layout));

    descriptor_set.descriptor_desc = graphics_state.descriptors;

    return descriptor_set;
}

void destroy_descriptor_set(Device &device, DescriptorSet &set)
{
    vkDestroyDescriptorSetLayout(device.device, set.layout, nullptr);
}

void bind_image(DescriptorSet &set, u32 slot, Handle<Image> image_handle)
{
    assert(set.descriptor_desc[slot].type == DescriptorType::SampledImage
           || set.descriptor_desc[slot].type == DescriptorType::StorageImage);
    set.descriptors[slot].image = {image_handle};
}

void bind_buffer(DescriptorSet &set, u32 slot, Handle<Buffer> buffer_handle)
{
    assert(set.descriptor_desc[slot].type == DescriptorType::StorageBuffer);
    set.descriptors[slot].buffer = {buffer_handle};
}

void bind_uniform_buffer(DescriptorSet &set, u32 slot, Handle<Buffer> buffer_handle, usize offset)
{
    assert(set.descriptor_desc[slot].type == DescriptorType::DynamicBuffer);
    set.descriptors[slot].dynamic = {buffer_handle, offset};
}

VkDescriptorSet find_or_create_descriptor_set(Device &device, DescriptorSet &set)
{
    auto hash = hash_value(set.descriptors);

    for (usize i = 0; i < set.hashes.size(); i++)
    {
        if (hash == set.hashes[i])
        {
            return set.vkhandles[i];
        }
    }

    set.hashes.push_back(hash);


    VkDescriptorSetAllocateInfo set_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    set_info.descriptorPool              = device.descriptor_pool;
    set_info.pSetLayouts                 = &set.layout;
    set_info.descriptorSetCount          = 1;

    VkDescriptorSet vkhandle = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device.device, &set_info, &vkhandle));

    set.vkhandles.push_back(vkhandle);

    return vkhandle;
}

};
