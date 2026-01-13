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

    window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, frame_buffer_resized_callback);
}

void WEngine::main_loop()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        draw_frame();
    }

    logical_device.waitIdle();
}

void WEngine::draw_frame()
{
    if (logical_device.waitForFences(*in_flight_fences[frame_index], vk::True, UINT64_MAX) != vk::Result::eSuccess)
        throw std::runtime_error("failed to wait for fence(s)!");
    logical_device.resetFences(*in_flight_fences[frame_index]);

    auto [result, imageIndex] = swap_chain.acquireNextImage(UINT64_MAX, *present_complete_semaphores[frame_index], nullptr);

    switch (result)
    {
        case vk::Result::eSuccess:
        case vk::Result::eSuboptimalKHR:
            break;

        case vk::Result::eErrorOutOfDateKHR:
            recreate_swap_chain();
            break;

        default:
            throw std::runtime_error("failed to acquire swap chain image");
    }

    command_buffers[frame_index].reset();
    record_command_buffer(imageIndex);

    constexpr vk::PipelineStageFlags waitDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submitInfo {};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &*present_complete_semaphores[frame_index];
    submitInfo.pWaitDstStageMask = &waitDstStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*command_buffers[frame_index];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &*render_finished_semaphores[imageIndex];

    graphics_queue.submit(submitInfo, *in_flight_fences[frame_index]);

    vk::PresentInfoKHR presentInfo {};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &*render_finished_semaphores[imageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &*swap_chain;
    presentInfo.pImageIndices = &imageIndex;

    result = graphics_queue.presentKHR(presentInfo);
    if (frame_buffer_resized)
    {
        frame_buffer_resized = false;
        recreate_swap_chain();
    }
    switch (result)
    {
        case vk::Result::eErrorOutOfDateKHR:
        case vk::Result::eSuboptimalKHR:
            recreate_swap_chain();
            break;

        case vk::Result::eSuccess:
            break;

        default:
            throw std::runtime_error("failed to present swap chain image");
    }

    frame_index = (frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

void WEngine::record_command_buffer(uint32_t imageIndex) const
{
    command_buffers[frame_index].begin({});
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

    command_buffers[frame_index].beginRendering(renderingInfo);
    command_buffers[frame_index].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
    command_buffers[frame_index].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swap_chain_extent.width), static_cast<float>(swap_chain_extent.height), 0.0f, 1.0f));
    command_buffers[frame_index].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain_extent));

    command_buffers[frame_index].draw(3, 1, 0, 0);

    command_buffers[frame_index].endRendering();
    transition_image_layout(
        imageIndex,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe
    );
    command_buffers[frame_index].end();
}

void WEngine::transition_image_layout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask) const
{
    vk::ImageSubresourceRange imgSubresourceRange {};
    imgSubresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    imgSubresourceRange.baseMipLevel = 0;
    imgSubresourceRange.levelCount = 1;
    imgSubresourceRange.baseArrayLayer = 0;
    imgSubresourceRange.layerCount = 1;

    vk::ImageMemoryBarrier2 barrier {};
    barrier.srcStageMask = srcStageMask;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstStageMask = dstStageMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image = swap_chain_images[imageIndex];
    barrier.subresourceRange = imgSubresourceRange;

    vk::DependencyInfo dependencyInfo {};
    dependencyInfo.dependencyFlags = {};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    command_buffers[frame_index].pipelineBarrier2(dependencyInfo);
}

void WEngine::cleanup()
{
    destroy_vulkan();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void WEngine::frame_buffer_resized_callback(GLFWwindow* window, int width, int height)
{
    const auto app = static_cast<WEngine*>(glfwGetWindowUserPointer(window));
    app->frame_buffer_resized = true;
}

vk::Bool32 WEngine::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                  vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
{
    std::cerr << to_string(severity) << " validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
}
