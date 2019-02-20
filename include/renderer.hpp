#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <vector>

#include "vulkan_context.hpp"
#include "model.hpp"
#include "image.hpp"
#include "buffer.hpp"

struct GLFWwindow;


namespace my_app
{
    constexpr int WIDTH = 800;
    constexpr int HEIGHT = 600;
    constexpr int NUM_FRAME_DATA = 2;

    struct UniformBufferObject
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    class Renderer
    {
    public:
        Renderer(GLFWwindow *window);
        Renderer(Renderer& other) = delete;
        ~Renderer();

        void CreateSwapchain();
        void CreateCommandPoolAndBuffers();
        void CreateSemaphores();
        void CreateDepthBuffer();
        void CreateUniformBuffer();
        void CreateDescriptors();
        void CreateRenderPass();
        void CreateFrameBuffers();
        void CreateIndexBuffer();
        void CreateVertexBuffer();
        void LoadShaders();
        void CreateGraphicsPipeline();
        void FillCommandBuffers();

        void DrawFrame();
        void WaitIdle();

    private:

        VulkanContext ctx_;

        Model model_;
        UniformBufferObject ubo_;

        vk::UniqueSwapchainKHR swapchain;
        std::vector<vk::Image> swapchain_images;
        std::vector<vk::ImageView> swapchain_image_views;
        vk::Format swapchain_format;
        vk::PresentModeKHR swapchain_present_mode;
        vk::Extent2D swapchain_extent;

        vk::CommandPool command_pool;
        std::vector<vk::CommandBuffer> command_buffers;
        std::vector<vk::Fence> command_buffers_fences;
        std::vector<vk::Semaphore> acquire_semaphores;
        std::vector<vk::Semaphore> render_complete_semaphores;

        Image depth_image;
        vk::ImageView depth_image_view;
        vk::Format depth_format;

        Buffer uniform_buffer;
        Buffer index_buffer;
        Buffer vertex_buffer;

        vk::ShaderModule vert_module;
        vk::ShaderModule frag_module;

        vk::DescriptorPool desc_pool;
        std::vector<vk::DescriptorSetLayout> desc_set_layouts;
        std::vector<vk::DescriptorSet> desc_sets;

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;

        vk::Pipeline pipeline;
        vk::PipelineCache pipeline_cache;
        vk::PipelineLayout pipeline_layout;
        vk::RenderPass render_pass;
        std::vector<vk::Framebuffer> frame_buffers;
    };
}
