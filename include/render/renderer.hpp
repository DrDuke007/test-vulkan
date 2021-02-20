#pragma once

#include "render/vulkan/context.hpp"

namespace gfx = vulkan;

inline constexpr uint FRAME_QUEUE_LENGTH = 2;

struct Renderer
{
    gfx::Context context;
    uint frame_count;

    // ImGuiPass
    Handle<gfx::GraphicsProgram> gui_program;
    Handle<gfx::RenderPass> gui_renderpass;
    Handle<gfx::Framebuffer> gui_framebuffer;
    Handle<gfx::Image> gui_font_atlas;

    Handle<gfx::Buffer> gui_vertices;
    Handle<gfx::Buffer> gui_vertices_staging;
    Handle<gfx::Buffer> gui_indices;
    Handle<gfx::Buffer> gui_indices_staging;

    // Command submission
    std::array<gfx::WorkPool, FRAME_QUEUE_LENGTH> work_pools;
    std::array<gfx::Receipt,  FRAME_QUEUE_LENGTH> rendering_done;
    std::array<gfx::Receipt,  FRAME_QUEUE_LENGTH> image_acquired;

    std::array<gfx::Receipt,  FRAME_QUEUE_LENGTH> transfer_done;

    /// ---

    static Renderer create(const platform::Window *window);
    void destroy();

    void on_resize();
    void update();

};
