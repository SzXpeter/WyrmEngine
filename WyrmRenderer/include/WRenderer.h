//
// Created by pheen on 02/01/2026.
//
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <glm/glm.hpp>

#ifdef NDEBUG
static constexpr bool enableValidationLayers = false;
#else
static constexpr bool enableValidationLayers = true;
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#endif

class WRenderer
{
public:
    WRenderer(const WRenderer&) = delete;
    WRenderer(WRenderer&&) = delete;
    WRenderer& operator=(const WRenderer&) = delete;
    WRenderer& operator=(WRenderer&&) = delete;

    static WRenderer& GetInstance();
    [[nodiscard]] GLFWwindow* GetWindow() const;
    void SetWindowSize(int _width, int _height);

    void InitWindow();
    void InitVulkan();
    void Cleanup();

    void DrawFrame();

private:
    WRenderer() = default;

    uint32_t width = 1280;
    uint32_t height = 720;
    GLFWwindow* window = nullptr;

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;
    vk::raii::SurfaceKHR surface = nullptr;

    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device device = nullptr;

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

    vk::raii::Buffer vertex_buffer = nullptr;
    vk::raii::DeviceMemory vertex_buffer_memory = nullptr;

    vk::raii::CommandPool command_pool = nullptr;
    std::vector<vk::raii::CommandBuffer> command_buffers;

    std::vector<vk::raii::Semaphore> present_complete_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> in_flight_fences;

    uint32_t frame_index = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    void create_vulkan_instance();
    void setup_debug_messenger();
    void create_surface();
    void pick_physical_device();
    void create_logical_device();
    void create_swap_chain();
    void create_image_views();
    void create_graphics_pipeline();
    void create_vertex_buffer();
    void create_command_pool();
    void create_command_buffer();
    void create_sync_object();

    void destroy_vulkan();

    void cleanup_swap_chain();
    void recreate_swap_chain();

    void record_command_buffer(uint32_t imageIndex) const;
    void transition_image_layout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask) const;

    std::vector<const char*> device_extensions = {
        vk::KHRSwapchainExtensionName
    };

    bool frame_buffer_resized = false;
    static void frame_buffer_resize_callback(GLFWwindow* window, int width, int height);

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);
};

struct Vertex
{
    glm::vec2 position;
    glm::vec3 color;

    static constexpr vk::VertexInputBindingDescription GetBindingDescription()
    {
        return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }

    static constexpr std::array<vk::VertexInputAttributeDescription, 2> GetAttributeDescriptions()
    {
        return {
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))
        };
    }
};

std::vector<const char*> getRequiredLayers(const vk::raii::Context& context);
std::vector<const char*> getRequiredExtensions(const vk::raii::Context& context);
uint32_t getPresentationQFPIndex(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface, uint32_t& graphicsIndex);
vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& swapSurfaceCapabilities);
vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
std::vector<char> read_shader_file(const std::string& filename);
[[nodiscard]] vk::raii::ShaderModule create_shader_module(const vk::raii::Device& device, const std::vector<char>& code);
uint32_t findMemoryType(const vk::PhysicalDevice& physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};
