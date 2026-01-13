//
// Created by pheen on 02/01/2026.
//
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#ifdef NDEBUG
static constexpr bool enableValidationLayers = false;
#else
static constexpr bool enableValidationLayers = true;
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#endif

class WEngine
{
public:
    void Run();

    std::pair<uint32_t, uint32_t> GetWindowSize();
    void SetWindowSize(uint32_t _width, uint32_t _height);

    static std::vector<char> ReadShaderFile(const std::string& filename);
    [[nodiscard]] vk::raii::ShaderModule CreateShaderModule(const std::vector<char>& code);

private:
    uint32_t width = 1280;
    uint32_t height = 720;
    GLFWwindow* window = nullptr;

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;
    vk::raii::SurfaceKHR surface = nullptr;

    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device logical_device = nullptr;

    uint32_t queue_index = ~0;
    vk::raii::Queue graphics_queue = nullptr;
    vk::raii::Queue present_queue = nullptr;

    vk::raii::SwapchainKHR swap_chain = nullptr;
    std::vector<vk::Image> swap_chain_images;
    vk::Format swap_chain_image_format = vk::Format::eUndefined;
    vk::Extent2D swap_chain_extent;
    std::vector<vk::raii::ImageView> swap_chain_image_views;

    vk::raii::PipelineLayout pipeline_layout = nullptr;
    vk::raii::Pipeline graphics_pipeline = nullptr;

    vk::raii::CommandPool command_pool = nullptr;
    std::vector<vk::raii::CommandBuffer> command_buffers;

    std::vector<vk::raii::Semaphore> present_complete_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> in_flight_fences;

    uint32_t frame_index = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    void init_window();

    void init_vulkan();
    void create_vulkan_instance();
    void setup_debug_messenger();
    void create_surface();
    void pick_physical_device();
    void create_logical_device();
    void create_swap_chain();
    void create_image_views();
    void create_graphics_pipeline();
    void create_command_pool();
    void create_command_buffer();
    void create_sync_object();

    void cleanup_swap_chain();
    void recreate_swap_chain();

    void destroy_vulkan();

    void main_loop();
    void draw_frame();
    void record_command_buffer(uint32_t imageIndex) const;
    void transition_image_layout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask) const;

    void cleanup();

    std::vector<const char*> device_extensions = {
        vk::KHRSwapchainExtensionName
    };

    bool frame_buffer_resized = false;
    static void frame_buffer_resized_callback(GLFWwindow* window, int width, int height);

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);
};