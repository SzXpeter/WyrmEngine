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


    void init_window();

    void init_vulkan();

    void main_loop();
    void cleanup();
};

#endif //WYRMENGINE_WENGINE_H