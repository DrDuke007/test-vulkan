#pragma once
#include "base/handle.hpp"
#include "base/vector.hpp"
#include "render/vulkan/resources.hpp"
#include "vulkan/vulkan_core.h"

#include <vulkan/vulkan.h>


namespace vulkan
{
/** Send resources to other queues - Async compute example

GraphicsWork cmd = device.get_graphics();
cmd.begin();
cmd.barrier(hdr_buffer, ColorAttachment);
cmd.barrier(depth_buffer, DepthBuffer);
cmd.begin_pass(hdr_buffer, depth_buffer);
cmd.bind_pipeline(simple_pass);
cmd.draw();
cmd.draw();
cmd.draw();
cmd.draw();
cmd.end_pass();
cmd.barrier(depth_buffer, SampledImage);
cmd.dispatch(depth_reduction);
cmd.barrier(gui_offscreen, ColorAttachment);
cmd.begin_pass(gui_offscreen);
cmd.bind_pipeline(gui);
cmd.draw();
cmd.draw();
cmd.end_pass();

auto hdr_transfer = cmd.send_to(compute, hdr_buffer);
auto gui_transfer = cmd.send_to(compute, gui_offscreen);
cmd.end();
auto done = device.submit(cmd);
submit will signal a fence and a semaphore

//  creating a work with a receipt means wait for this semaphore when submitting
ComputeWork compute = device.get_compute(done);
compute.receive(hdr_transfer);
compute.receive(gui_transfer);
compute.begin();
compute.barrier(hdr_buffer, SampledImage);
compute.barrier(gui_offscreen, SampledImage);
compute.bind_pipeline(post_process);
compute.dispatch();
compute.end();

device.submit(compute);
**/

/** Wait for completion
TransferWork cmd;
cmd.begin();
cmd.upload(font_atlas, pixels, size);
cmd.end();

auto done = device.submit(cmd);
// wait for fence
device.wait_for(done);
**/

/** Swapchain

auto last_frame_done = ... ;
device.wait_for(last_frame_done);

auto image_acquired = device.acquire_next_swapchain();

GraphicsWork cmd{image_acquired};
cmd.begin();
... stuff ...
cmd.end();
auto done = device.submit(cmd);
device.present(done);

last_frame_done = done;
**/
struct Device;
struct Image;
struct Surface;

// A request to send a resource to another queue
struct ResourceTransfer
{
    int sender;
    int receiver;
    int resource;
};

// Indicates when work is done, either on CPU or GPU
struct Receipt
{
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE;
};

// Command buffer / Queue abstraction
struct Work
{
    Device *device;

    VkCommandBuffer command_buffer;
    Vec<Receipt> wait_list;
    Vec<VkPipelineStageFlags> wait_stage_list;
    VkQueue queue;

    void begin();
    void end();

    ResourceTransfer send_to(int receiver, int resource);
    void receive(ResourceTransfer transfer);
    void wait_for(Receipt previous_work, VkPipelineStageFlags stage_dst);

    void barrier(Handle<Image> image, ImageUsage usage_destination);
    void clear_barrier(Handle<Image> image, ImageUsage usage_destination);
    void barriers(Vec<std::pair<Handle<Image>, ImageUsage>> images, Vec<std::pair<Handle<Buffer>, BufferUsage>> buffers);
};

struct TransferWork : Work
{
    void copy_buffer(Handle<Buffer> src, Handle<Buffer> dst);
    void copy_buffer_to_image(Handle<Buffer> src, Handle<Image> dst);
    void fill_buffer(Handle<Buffer> buffer_handle, u32 data);
    void transfer();
};

struct ComputeWork : TransferWork
{
    void clear_image(Handle<Image> image, VkClearColorValue clear_color);

    void dispatch();
    void bind_pipeline(Handle<ComputeProgram> program_handle);

    void bind_buffer(Handle<GraphicsProgram> program_handle, uint slot, Handle<Buffer> buffer_handle);
    void bind_image(Handle<GraphicsProgram> program_handle, uint slot, Handle<Image> image_handle);
};

struct GraphicsWork : ComputeWork
{
    struct DrawIndexedOptions
    {
        u32 vertex_count = 0;
        u32 instance_count = 1;
        u32 index_offset = 0;
        i32 vertex_offset = 0;
        u32 instance_offset = 0;
    };
    void draw_indexed(const DrawIndexedOptions &options);

    void set_scissor(const VkRect2D &rect);
    void set_viewport(const VkViewport &viewport);

    void begin_pass(Handle<RenderPass> renderpass_handle, Handle<Framebuffer> framebuffer_handle, Vec<Handle<Image>> attachments, Vec<VkClearValue> clear_values);
    void end_pass();
    void bind_pipeline(Handle<GraphicsProgram> program_handle, uint pipeline_index);
    void bind_index_buffer(Handle<Buffer> buffer_handle);
};
}
