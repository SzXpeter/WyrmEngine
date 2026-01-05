//
// Created by pheen on 02/01/2026.
//

#ifndef WYRMENGINE_WENGINE_H
#define WYRMENGINE_WENGINE_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

class WEngine
{
public:
    void Run();

    std::pair<uint32_t, uint32_t> GetWindowSize();
    void SetWindowSize(uint32_t width, uint32_t height);

    static std::vector<char> ReadShaderFile(const std::string& filename);
    [[nodiscard]] vk::raii::ShaderModule CreateShaderModule(const std::vector<char>& code);

private:
    uint32_t width = 1280;
    uint32_t height = 720;
    GLFWwindow* window = nullptr;

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::SurfaceKHR surface = nullptr;

    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device logical_device = nullptr;

    vk::raii::Queue graphics_queue = nullptr;
    vk::raii::Queue present_queue = nullptr;

    vk::raii::SwapchainKHR swap_chain = nullptr;
    vk::Extent2D swap_chain_extent;
    vk::Format swap_chain_image_format = vk::Format::eUndefined;
    std::vector<vk::Image> swap_chain_images;
    std::vector<vk::raii::ImageView> swap_chain_image_views;

    vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;

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

    void main_loop();
    void cleanup();

    std::vector<const char*> deviceExtensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName
    };

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);
};

#endif //WYRMENGINE_WENGINE_H