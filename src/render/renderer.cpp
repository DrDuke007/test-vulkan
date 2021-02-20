#include "render/renderer.hpp"

#include "render/vulkan/commands.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/utils.hpp"
#include "vulkan/vulkan_core.h"


#include <tuple> // for std::tie
#include <imgui/imgui.h>

Renderer Renderer::create(const platform::Window *window)
{
    Renderer renderer = {
        .context = gfx::Context::create(true, window)
    };

    auto &device = renderer.context.device;

    for (auto &work_pool : renderer.work_pools)
    {
        device.create_work_pool(work_pool);
    }

    for (auto &receipt : renderer.rendering_done)
    {
        receipt = device.signaled_receipt();
    }

    auto &surface = *renderer.context.surface;

    gfx::GraphicsState gui_state = {};
    gui_state.vertex_shader   =  device.create_shader("shaders/gui.vert.spv");
    gui_state.fragment_shader =  device.create_shader("shaders/gui.frag.spv");
    gui_state.attachments.colors.push_back({.format = surface.format.format});
    // gui_state.attachments.depth = {.format = VK_FORMAT_D32_SFLOAT};
    gui_state.descriptors = {
        {.type = gfx::DescriptorType::StorageBuffer, .count = 1},
        {.type = gfx::DescriptorType::StorageBuffer, .count = 1},
        {.type = gfx::DescriptorType::SampledImage,  .count = 1},
    };

    renderer.gui_program = device.create_program(gui_state);

    gfx::RenderState state = {};
    uint gui_default = device.compile(renderer.gui_program, state);
    UNUSED(gui_default);

    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    u8 *pixels = nullptr;
    int width  = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    renderer.gui_font_atlas = device.create_image({
            .name = "Font Atlas",
            .size = {static_cast<u32>(width), static_cast<u32>(height), 1},
        });

    renderer.gui_renderpass = device.find_or_create_renderpass(gui_state.attachments);

    Vec<gfx::FramebufferAttachment> fb_attachments = {
        {.width = surface.extent.width, .height = surface.extent.height, .format = surface.format.format}
    };

    renderer.gui_framebuffer = device.create_framebuffer({
            .width = surface.extent.width,
            .height = surface.extent.height,
            .attachments = fb_attachments,
        });

    renderer.gui_vertices = device.create_buffer({
            .name = "Imgui vertices",
            .size = 1_MiB,
            .usage = gfx::storage_buffer_usage,
        });

    renderer.gui_vertices_staging = device.create_buffer({
            .name = "Imgui vertices staging",
            .size = 1_MiB,
            .usage = gfx::source_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
        });


    renderer.gui_indices = device.create_buffer({
            .name = "Imgui indices",
            .size = 1_MiB,
            .usage = gfx::storage_buffer_usage,
        });

    renderer.gui_indices_staging = device.create_buffer({
            .name = "Imgui indices staging",
            .size = 1_MiB,
            .usage = gfx::source_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
        });

    return renderer;
}

void Renderer::destroy()
{
    auto &device = context.device;

    device.wait_idle();

    for (auto &receipt : rendering_done)
        device.destroy_receipt(receipt);

    for (auto &receipt : image_acquired)
        device.destroy_receipt(receipt);

    for (auto &receipt : transfer_done)
        device.destroy_receipt(receipt);

    for (auto &work_pool : work_pools)
    {
        device.destroy_work_pool(work_pool);
    }

    context.destroy();
}

void Renderer::on_resize()
{
    gfx::Device  &device  = context.device;
    gfx::Surface &surface = *context.surface;

    device.wait_idle();
    surface.destroy_swapchain(device);
    surface.create_swapchain(device);

    for (auto &receipt : rendering_done)
    {
        device.destroy_receipt(receipt);
        receipt = device.signaled_receipt();
    }

    for (auto &receipt : image_acquired)
    {
        device.destroy_receipt(receipt);
        receipt = device.signaled_receipt();
    }

    Vec<gfx::FramebufferAttachment> fb_attachments = {
        {.width = surface.extent.width, .height = surface.extent.height, .format = surface.format.format}
    };

    device.destroy_framebuffer(gui_framebuffer);
    gui_framebuffer = device.create_framebuffer({
            .width = surface.extent.width,
            .height = surface.extent.height,
            .attachments = fb_attachments,
        });

}

void Renderer::update()
{
    gfx::Device &device = context.device;
    auto current_frame = frame_count % FRAME_QUEUE_LENGTH;

    auto &io = ImGui::GetIO();
    io.DisplaySize.x = context.surface->extent.width;
    io.DisplaySize.y = context.surface->extent.height;

    // wait for fence, blocking
    auto &rendering_done = this->rendering_done[current_frame];
    device.wait_for(rendering_done);

    // reset the command buffers
    auto &work_pool = work_pools[current_frame];
    device.reset_work_pool(work_pool);

    ImGui::NewFrame();
    ImGui::Begin("Test");
    ImGui::End();
    ImGui::Render();

    // Transfer stuff
    ImDrawData *data = ImGui::GetDrawData();
    assert(sizeof(ImDrawVert) * static_cast<u32>(data->TotalVtxCount) < 1_MiB);
    assert(sizeof(ImDrawIdx)  * static_cast<u32>(data->TotalVtxCount) < 1_MiB);

    auto *vertices = device.map_buffer<ImDrawVert>(gui_vertices_staging);
    auto *indices  = device.map_buffer<ImDrawIdx>(gui_vertices_staging);

    for (int i = 0; i < data->CmdListsCount; i++)
    {
        const auto &cmd_list = *data->CmdLists[i];

        for (int i_vertex = 0; i_vertex < cmd_list.VtxBuffer.Size; i_vertex++)
        {
            vertices[i_vertex] = cmd_list.VtxBuffer.Data[i_vertex];
        }

        for (int i_index = 0; i_index < cmd_list.IdxBuffer.Size; i_index++)
        {
            indices[i_index] = cmd_list.IdxBuffer.Data[i_index];
        }

        vertices += cmd_list.VtxBuffer.Size;
        indices  += cmd_list.IdxBuffer.Size;
    }


    gfx::TransferWork transfer_cmd = device.get_transfer_work(work_pool);
    transfer_cmd.begin();
    transfer_cmd.barriers({}, {{gui_vertices_staging, gfx::BufferUsage::TransferSrc}, {gui_indices_staging, gfx::BufferUsage::TransferSrc}, {gui_vertices, gfx::BufferUsage::TransferDst}, {gui_indices, gfx::BufferUsage::TransferDst}});
    transfer_cmd.copy_buffer(gui_vertices_staging, gui_vertices);
    transfer_cmd.copy_buffer(gui_indices_staging, gui_indices);
    transfer_cmd.end();
    transfer_done[current_frame] = device.submit(transfer_cmd, &transfer_done[current_frame]);

    // receipt contains the image acquired semaphore
    auto &image_acquired = this->image_acquired[current_frame];
    bool out_of_date_swapchain = false;
    std::tie(image_acquired, out_of_date_swapchain) = device.acquire_next_swapchain(*context.surface, &image_acquired);
    if (out_of_date_swapchain)
    {
        on_resize();
        return;
    }

    gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);

    // wait_for previous work to complete, NOT BLOCKING!
    // previous wait_for was waiting because we are waiting on the device,
    // here the wait_for is in a command buffer, so on the gpu
    cmd.wait_for(image_acquired, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    cmd.wait_for(transfer_done[current_frame], VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

    // do random stuff
    {
        auto swapchain_image = context.surface->images[context.surface->current_image];

        cmd.begin();
        if (0)
        {
            cmd.barrier(swapchain_image, gfx::ImageUsage::TransferDst);
            cmd.clear_image(swapchain_image, {.float32 = {1.0f, 0.0f, 0.0f, 1.0f}});
        }
        else
        {
            cmd.barrier(swapchain_image, gfx::ImageUsage::ColorAttachment);
            cmd.begin_pass(gui_renderpass, gui_framebuffer, {swapchain_image}, {{{.float32 = {1.0f, 0.0f, 0.0f, 1.0f}}}});
            cmd.bind_pipeline(gui_program, 0);
            cmd.end_pass();
        }

        cmd.barrier(swapchain_image, gfx::ImageUsage::Present);
        cmd.end();
    }

    // submit signals a fence and semaphore
    rendering_done = device.submit(cmd, &rendering_done);

    // present will wait for semaphore
    out_of_date_swapchain = device.present(rendering_done, *context.surface, gfx::WorkPool::POOL_TYPE_GRAPHICS);
    if (out_of_date_swapchain)
    {
        on_resize();
        return;
    }

    frame_count += 1;
}
