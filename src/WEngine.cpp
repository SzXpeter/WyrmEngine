//
// Created by pheen on 02/01/2026.
//

#include "WEngine.h"

#include <fstream>
#include <iostream>

void WEngine::Run()
{
    init_window();
    init_vulkan();
    main_loop();
    cleanup();
}

std::pair<uint32_t, uint32_t> WEngine::GetWindowSize()
{
    return {width, height};
}

void WEngine::SetWindowSize(uint32_t _width, uint32_t _height)
{
    width = _width;
    height = _height;
}

std::vector<char> WEngine::ReadShaderFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) throw std::runtime_error("failed to open file!");

    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    std::cout << buffer.size() << std::endl;

    file.close();
    return buffer;
}

vk::raii::ShaderModule WEngine::CreateShaderModule(const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo shaderModuleCreateInfo {};
    shaderModuleCreateInfo.codeSize = code.size() * sizeof(char);
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    return {logical_device, shaderModuleCreateInfo};
}

void WEngine::init_window()
{
#ifdef __linux__
    glfwWindowHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
#endif

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), "Vulkan", nullptr, nullptr);
}



void WEngine::main_loop() const
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        draw_frame();
    }

    logical_device.waitIdle();
}

void WEngine::draw_frame() const
{
    graphics_queue.waitIdle();

    auto [result, imageIndex] = swap_chain.acquireNextImage(UINT64_MAX, *present_complete_semaphore, nullptr);

    record_command_buffer(imageIndex);
    logical_device.resetFences(*draw_fence);

    constexpr vk::PipelineStageFlags waitDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submitInfo {};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &*present_complete_semaphore;
    submitInfo.pWaitDstStageMask = &waitDstStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*command_buffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &*render_finished_semaphore;

    graphics_queue.submit(submitInfo, *draw_fence);

    vk::PresentInfoKHR presentInfo {};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &*render_finished_semaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &*swap_chain;
    presentInfo.pImageIndices = &imageIndex;

    result = graphics_queue.presentKHR(presentInfo);
}

void WEngine::record_command_buffer(uint32_t imageIndex) const
{
    command_buffer.begin({});
    transition_image_layout(
        imageIndex,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput
    );

    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::RenderingAttachmentInfo attachmentInfo {};
    attachmentInfo.imageView = swap_chain_image_views[imageIndex];
    attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
    attachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
    attachmentInfo.clearValue = clearColor;

    vk::RenderingInfo renderingInfo {};
    renderingInfo.renderArea = {.offset = {0, 0}, .extent = swap_chain_extent};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &attachmentInfo;

    command_buffer.beginRendering(renderingInfo);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
    command_buffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swap_chain_extent.width), static_cast<float>(swap_chain_extent.height), 0.0f, 1.0f));
    command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain_extent));

    command_buffer.draw(3, 1, 0, 0);

    command_buffer.endRendering();
    transition_image_layout(
        imageIndex,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe
    );
    command_buffer.end();
}

void WEngine::cleanup() const
{
    glfwDestroyWindow(window);

    glfwTerminate();
}

vk::Bool32 WEngine::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
{
    std::cerr << to_string(severity) << " validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
}
