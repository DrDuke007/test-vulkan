#pragma once
#include "base/vector.hpp"

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
    VkCommandBuffer command_buffer;
    Vec<Receipt> wait_list;

    void begin();
    void end();

    ResourceTransfer send_to(int receiver, int resource);
    void receive(ResourceTransfer transfer);
    void wait_for(Receipt previous_work);
};

#define WORK_STATIC_DISPATCH                                                   \
  inline void begin() { work.begin(); }                                        \
  inline void end() { work.end(); }                                            \
  inline ResourceTransfer send_to(int receiver, int resource) {                \
    return work.send_to(receiver, resource);                                   \
  }                                                                            \
  inline void receive(ResourceTransfer transfer) { work.receive(transfer); }   \
  inline void wait_for(Receipt previous_work) { work.wait_for(previous_work); }

struct TransferWork
{
    Work work;

    void upload();
    void transfer();

    WORK_STATIC_DISPATCH
};

#define TRANSFER_STATIC_DISPATCH                                               \
  inline void upload() { work.upload(); }                                      \
  inline void transfer() { work.transfer(); }

struct ComputeWork
{
    TransferWork work;

    void dispatch();
    void bind_pipeline();

    WORK_STATIC_DISPATCH
    TRANSFER_STATIC_DISPATCH
};

#define COMPUTE_STATIC_DISPATCH                                                \
  inline void dispatch() { work.dispatch(); }                                  \
  inline void bind_pipeline() { work.bind_pipeline(); }

struct GraphicsWork
{
    ComputeWork work;

    void draw();
    void begin_pass();
    void end_pass();
    // void bind_pipeline();

    WORK_STATIC_DISPATCH
    TRANSFER_STATIC_DISPATCH
    COMPUTE_STATIC_DISPATCH
};
}
