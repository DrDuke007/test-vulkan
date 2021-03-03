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

    gfx::RenderState state = {.alpha_blending = true};
    uint gui_default = device.compile(renderer.gui_program, state);
    UNUSED(gui_default);

    auto &io = ImGui::GetIO();
    uchar *pixels = nullptr;
    int width  = 0;
    int height = 0;
    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    usize font_atlas_size = width * height * sizeof(u32);

    renderer.gui_font_atlas = device.create_image({
            .name = "Font Atlas",
            .size = {static_cast<u32>(width), static_cast<u32>(height), 1},
            .format = VK_FORMAT_R8G8B8A8_UNORM,
        });

    renderer.gui_font_atlas_staging = device.create_buffer({
            .name = "Imgui font atlas staging",
            .size = font_atlas_size,
            .usage = gfx::source_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
        });

    uchar *p_font_atlas = device.map_buffer<uchar>(renderer.gui_font_atlas_staging);
    for (uint i = 0; i < font_atlas_size; i++)
    {
        p_font_atlas[i] = pixels[i];
    }
    device.flush_buffer(renderer.gui_font_atlas_staging);

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
            .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

    renderer.gui_indices = device.create_buffer({
            .name = "Imgui indices",
            .size = 1_MiB,
            .usage = gfx::index_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

    renderer.gui_options = device.create_buffer({
            .name = "Imgui options",
            .size = 1_KiB,
            .usage = gfx::storage_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });

    renderer.fence = device.create_fence();
    renderer.transfer_done = device.create_fence(renderer.transfer_fence_value);

    return renderer;
}

void Renderer::destroy()
{
    auto &device = context.device;

    device.wait_idle();

    device.destroy_fence(fence);
    device.destroy_fence(transfer_done);

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

    // wait for fence, blocking: dont wait for the first QUEUE_LENGTH frames
    u64 wait_value = frame_count < FRAME_QUEUE_LENGTH ? 0 : frame_count-FRAME_QUEUE_LENGTH+1;
    device.wait_for(fence, wait_value);

    // reset the command buffers
    auto &work_pool = work_pools[current_frame];
    device.reset_work_pool(work_pool);

    ImGui::Render();

    // Transfer stuff

    ImDrawData *data = ImGui::GetDrawData();
    assert(sizeof(ImDrawVert) * static_cast<u32>(data->TotalVtxCount) < 1_MiB);
    assert(sizeof(ImDrawIdx)  * static_cast<u32>(data->TotalVtxCount) < 1_MiB);

    auto *vertices = device.map_buffer<ImDrawVert>(gui_vertices);
    auto *indices  = device.map_buffer<ImDrawIdx>(gui_indices);

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

    auto *options = device.map_buffer<float>(gui_options);
    std::memset(vertices, 0, 1_KiB);
    options[0] = 2.0f / data->DisplaySize.x; // X Scale
    options[1] = 2.0f / data->DisplaySize.y; // Y Scale
    options[2] = -1.0f - data->DisplayPos.x * options[0]; // X Translation
    options[3] = -1.0f - data->DisplayPos.y * options[1]; // Y Translation

    if (frame_count == 0)
    {
        gfx::TransferWork transfer_cmd = device.get_transfer_work(work_pool);
        transfer_cmd.begin();
        transfer_cmd.clear_barrier(gui_font_atlas, gfx::ImageUsage::TransferDst);
        transfer_cmd.copy_buffer_to_image(gui_font_atlas_staging, gui_font_atlas);
        transfer_cmd.end();
        device.submit(transfer_cmd, {transfer_done}, {transfer_fence_value+1});
    }

    // receipt contains the image acquired semaphore
    bool out_of_date_swapchain = device.acquire_next_swapchain(*context.surface);
    if (out_of_date_swapchain)
    {
        on_resize();
        return;
    }

    gfx::GraphicsWork cmd = device.get_graphics_work(work_pool);

    // vulkan hack: this command buffer will wait for the image acquire semaphore
    cmd.wait_for_acquired(*context.surface, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    if (frame_count == 0)
    {
        cmd.wait_for(transfer_done, transfer_fence_value+1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        transfer_fence_value += 1;
    }

    // do random stuff
    {
        auto swapchain_image = context.surface->images[context.surface->current_image];

        cmd.begin();

        cmd.barrier(swapchain_image, gfx::ImageUsage::ColorAttachment);
        cmd.barrier(gui_font_atlas, gfx::ImageUsage::GraphicsShaderRead);

        cmd.begin_pass(gui_renderpass, gui_framebuffer, {swapchain_image}, {{{.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}}});

        cmd.bind_buffer(gui_program, 0, gui_vertices);
        cmd.bind_buffer(gui_program, 1, gui_options);
        cmd.bind_image(gui_program, 2, gui_font_atlas);
        cmd.bind_pipeline(gui_program, 0);
        cmd.bind_index_buffer(gui_indices);


        ImVec2 clip_off   = data->DisplayPos;       // (0,0) unless using multi-viewports
        ImVec2 clip_scale = data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

        VkViewport viewport{};
        viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
        viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
        viewport.minDepth = 1.0f;
        viewport.maxDepth = 1.0f;
        cmd.set_viewport(viewport);

        i32 vertex_offset = 0;
        u32 index_offset  = 0;
        for (int list = 0; list < data->CmdListsCount; list++)
        {
            const auto &cmd_list = *data->CmdLists[list];

            for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++)
            {
                const auto &draw_command = cmd_list.CmdBuffer[command_index];

                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = (draw_command.ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (draw_command.ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (draw_command.ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (draw_command.ClipRect.w - clip_off.y) * clip_scale.y;

                // Apply scissor/clipping rectangle
                // FIXME: We could clamp width/height based on clamped min/max values.
                VkRect2D scissor;
                scissor.offset.x      = (static_cast<i32>(clip_rect.x) > 0) ? static_cast<i32>(clip_rect.x) : 0;
                scissor.offset.y      = (static_cast<i32>(clip_rect.y) > 0) ? static_cast<i32>(clip_rect.y) : 0;
                scissor.extent.width  = static_cast<u32>(clip_rect.z - clip_rect.x);
                scissor.extent.height = static_cast<u32>(clip_rect.w - clip_rect.y + 1); // FIXME: Why +1 here?

                cmd.set_scissor(scissor);

                cmd.draw_indexed({.vertex_count = draw_command.ElemCount, .index_offset = index_offset, .vertex_offset = vertex_offset});

                index_offset += draw_command.ElemCount;
            }
            vertex_offset += cmd_list.VtxBuffer.Size;
        }

        cmd.end_pass();
        cmd.barrier(swapchain_image, gfx::ImageUsage::Present);

        cmd.end();
    }

    // vulkan hack: hint the device to submit a semaphore to wait on before presenting
    cmd.prepare_present(*context.surface);

    device.submit(cmd, {fence}, {frame_count+1});

    // present will wait for semaphore
    out_of_date_swapchain = device.present(*context.surface, gfx::WorkPool::POOL_TYPE_GRAPHICS);
    if (out_of_date_swapchain)
    {
        on_resize();
        return;
    }

    frame_count += 1;
}
