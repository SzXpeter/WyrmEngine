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

struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T*;

struct VmaAllocation_T;
using VmaAllocation = VmaAllocation_T*;

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

    static void WThrowException(const std::string& message, int line = __LINE__);

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

    VmaAllocator allocator = nullptr;

    uint32_t queue_index = ~0;
    vk::raii::Queue graphics_queue = nullptr;
    vk::raii::Queue present_queue = nullptr;

    vk::raii::SwapchainKHR swap_chain = nullptr;
    std::vector<vk::Image> swap_chain_images;
    vk::Format swap_chain_image_format = vk::Format::eUndefined;
    vk::Extent2D swap_chain_extent;
    std::vector<vk::raii::ImageView> swap_chain_image_views;

    vk::raii::DescriptorSetLayout descriptor_set_layout = nullptr;
    vk::raii::PipelineLayout pipeline_layout = nullptr;
    vk::raii::Pipeline graphics_pipeline = nullptr;

    vk::Buffer vertex_buffer = nullptr;
    VmaAllocation vertex_buffer_alloc = nullptr;
    vk::Buffer index_buffer = nullptr;
    VmaAllocation index_buffer_alloc = nullptr;

    std::vector<vk::Buffer> uniform_buffers;
    std::vector<VmaAllocation> uniform_buffer_allocs;
    std::vector<void*> uniform_buffers_mapped;

    vk::raii::DescriptorPool descriptor_pool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptor_sets;

    vk::raii::CommandPool command_pool = nullptr;
    std::vector<vk::raii::CommandBuffer> command_buffers;

    std::vector<vk::raii::Semaphore> present_complete_semaphores;
    std::vector<vk::raii::Semaphore> render_finished_semaphores;
    std::vector<vk::raii::Fence> in_flight_fences;

    uint32_t frame_index = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    void create_vulkan_instance();
    [[nodiscard]] std::vector<const char*> get_required_layers() const;
    [[nodiscard]] std::vector<const char*> get_required_extensions() const;

    void setup_debug_messenger();
    void create_surface();

    void pick_physical_device();
    void create_logical_device();
    uint32_t get_presentation_qfp_index(uint32_t& graphicsIndex) const;
    void vma_init();

    void create_swap_chain();
    void create_image_views();

    void create_descriptor_set_layout();
    void create_graphics_pipeline();
    [[nodiscard]] vk::raii::ShaderModule create_shader_module(const std::vector<char>& code);

    void create_command_pool();
    void create_command_buffers();

    void create_vertex_buffer();
    void create_index_buffer();
    void copy_buffer(const vk::Buffer& srcBuffer, const vk::Buffer& dstBuffer, vk::DeviceSize size) const;
    void create_uniform_buffers();

    void create_descriptor_pool();
    void create_descriptor_sets();

    void create_sync_object();

    void update_uniform_buffers(uint32_t currentImage) const;

    void cleanup_swap_chain();
    void recreate_swap_chain();

    void record_command_buffer(uint32_t imageIndex) const;
    void transition_image_layout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask) const;

    void destroy_vulkan();

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

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
};

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& swapSurfaceCapabilities);
vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
std::vector<char> readShaderFile(const std::string& filename);

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}}
};

const std::vector<uint32_t> indices = {
    0, 1, 2, 2, 3, 0
};