#pragma once


#include "base/types.hpp"
#include "base/vector.hpp"
#include "base/option.hpp"
#include "base/handle.hpp"
#include "render/vulkan/descriptor_set.hpp"
#include "render/vulkan/queues.hpp"
#include "vulkan/vulkan_core.h"

#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace vulkan
{

inline constexpr VkImageUsageFlags depth_attachment_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags color_attachment_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags sampled_image_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
inline constexpr VkImageUsageFlags storage_image_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

struct ImageAccess
{
    VkPipelineStageFlags stage = 0;
    VkAccessFlags access       = 0;
    VkImageLayout layout       = VK_IMAGE_LAYOUT_UNDEFINED;
    QueueType queue            = QueueType::Graphics;
};

enum struct ImageUsage
{
    None,
    GraphicsShaderRead,
    GraphicsShaderReadWrite,
    ComputeShaderRead,
    ComputeShaderReadWrite,
    TransferDst,
    TransferSrc,
    ColorAttachment,
    DepthAttachment,
    Present
};

struct ImageDescription
{
    std::string name = "No name";
    uint3 size;
    VkImageType type                    = VK_IMAGE_TYPE_2D;
    VkFormat format                     = VK_FORMAT_R8G8B8A8_UNORM;
    VkSampleCountFlagBits samples       = VK_SAMPLE_COUNT_1_BIT;
    VkImageUsageFlags usages            = sampled_image_usage;
    VmaMemoryUsage memory_usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    bool operator==(const ImageDescription &b) const = default;
};

struct Image
{
    ImageDescription desc;
    VkImage vk_handle;
    VmaAllocation allocation;
    ImageUsage usage = ImageUsage::None;
    bool is_proxy = false;
    bool operator==(const Image &b) const = default;
};

struct BufferDescription
{
};

struct Buffer
{
};

struct Shader
{
    std::string filename;
    VkShaderModule vkhandle;
    Vec<u8> bytecode;
    bool operator==(const Shader &other) const = default;
};

enum struct PrimitiveTopology
{
    TriangleList,
    PointList
};

struct DepthState
{
    Option<VkCompareOp> test = std::nullopt;
    bool enable_write = false;
    float bias = 0.0f;

    bool operator==(const DepthState &) const = default;
};

struct RasterizationState
{
    bool enable_conservative_rasterization{false};
    bool culling{true};

    bool operator==(const RasterizationState &) const = default;
};

struct InputAssemblyState
{
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    bool operator==(const InputAssemblyState &) const = default;
};

struct RenderState
{
    DepthState depth;
    RasterizationState rasterization;
    InputAssemblyState input_assembly;
    bool alpha_blending = false;

    bool operator==(const RenderState &) const = default;
};

struct RenderAttachment
{
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    bool operator==(const RenderAttachment &) const = default;
};

struct RenderAttachments
{
    /**
       Framebuffers and graphics pipelines are created based on a specific render pass object.
       They must only be used with that render pass object, or one compatible with it.
       Two attachment references are compatible if they have matching format and sample count, or are
       both VK_ATTACHMENT_UNUSED or the pointer that would contain the reference is NULL.
       Two arrays of attachment references are compatible if all corresponding pairs of attachments are compatible.
       If the arrays are of different lengths, attachment references not present in the smaller array are treated as VK_ATTACHMENT_UNUSED.

       In other works, image layouts and store/load ops DO NOT matter.
    **/


    Vec<RenderAttachment> colors;
    Option<RenderAttachment> depth;
};

// Everything needed to build a pipeline except render state which is a separate struct
struct GraphicsState
{
    Handle<Shader> vertex_shader;
    Handle<Shader> fragment_shader;
    RenderAttachments attachments;
    Vec<DescriptorType> descriptors;
};

struct GraphicsProgram
{
    // state to compile the pipeline
    GraphicsState graphics_state;
    Vec<RenderState> render_states;

    // pipeline
    VkPipelineLayout pipeline_layout;
    Vec<VkPipeline> pipelines;
    VkPipelineCache cache;

    // data binded to the program
    DescriptorSet descriptor_set;
};
}
