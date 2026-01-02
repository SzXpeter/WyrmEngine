//
// Created by pheen on 02/01/2026.
//

#include "WEngine.h"

#include <sstream>


void WEngine::Run()
{
    init_window();
    init_vulkan();
    main_loop();
    cleanup();
}

std::pair<uint32_t, uint32_t> WEngine::GetWindowSize()
{
    return window_size;
}

void WEngine::SetWindowSize(uint32_t width, uint32_t height)
{
    window_size = { width, height };
}

void WEngine::SetWindowSize(const std::pair<uint32_t, uint32_t>& windowSize)
{
    window_size = windowSize;
}

void WEngine::init_window()
{
#ifdef __linux__
    glfwWindowHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
#endif

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(window_size.first, window_size.second, "Vulkan", nullptr, nullptr);
}

void WEngine::init_vulkan()
{
}


void WEngine::main_loop()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
}

void WEngine::cleanup()
{
    glfwDestroyWindow(window);

    glfwTerminate();
}
