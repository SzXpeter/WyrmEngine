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
    void SetWindowSize(const std::pair<uint32_t, uint32_t>& windowSize);

private:
    std::pair<uint32_t, uint32_t> window_size;
    GLFWwindow* window = nullptr;

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;

    vk::raii::PhysicalDevice physical_device = nullptr;
    vk::raii::Device device = nullptr;
    vk::raii::Queue graphics_queue = nullptr;

    vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;

    void init_window();

    void init_vulkan();
    void setup_debug_messenger();
    void pick_physical_device();
    void create_logical_device();

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