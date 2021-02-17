#include "render/vulkan/descriptor_set.hpp"

#include "render/vulkan/device.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"

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
    desc_layout_info.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device.device, &desc_layout_info, nullptr, &descriptor_set.layout));

    return descriptor_set;
}

void destroy_descriptor_set(Device &device, DescriptorSet &set)
{
    vkDestroyDescriptorSetLayout(device.device, set.layout, nullptr);
}
};
