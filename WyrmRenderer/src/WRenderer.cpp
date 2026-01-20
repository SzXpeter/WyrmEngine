//
// Created by pheen on 02/01/2026.
//

#include "WRenderer.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#include <fstream>
#include <iostream>
#include <sstream>

WRenderer& WRenderer::GetInstance()
{
    static WRenderer renderer;
    return renderer;
}

GLFWwindow* WRenderer::GetWindow() const
{
    return window;
}

void WRenderer::SetWindowSize(const int _width, const int _height)
{
    width = _width;
    height = _height;
}

void WRenderer::InitWindow()
{
#ifdef __linux__
    glfwWindowHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
#endif

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, frame_buffer_resize_callback);
}

void WRenderer::InitVulkan()
{
    create_vulkan_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    vma_init();
    create_swap_chain();
    create_image_views();
    create_graphics_pipeline();
    create_command_pool();
    create_command_buffer();
    create_vertex_buffer();
    create_index_buffer();
    create_sync_object();
}

void WRenderer::Cleanup()
{
    destroy_vulkan();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void WRenderer::DrawFrame()
{
    if (device.waitForFences(*in_flight_fences[frame_index], vk::True, UINT64_MAX) != vk::Result::eSuccess)
        throw std::runtime_error("failed to wait for fence(s)!");
    device.resetFences(*in_flight_fences[frame_index]);

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
    const vk::SubmitInfo submitI {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*present_complete_semaphores[frame_index],
        .pWaitDstStageMask = &waitDstStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_buffers[frame_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*render_finished_semaphores[imageIndex]
    };

    graphics_queue.submit(submitI, *in_flight_fences[frame_index]);

    const vk::PresentInfoKHR presentI {
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*render_finished_semaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &*swap_chain,
        .pImageIndices = &imageIndex
    };
    result = graphics_queue.presentKHR(presentI);
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

void WRenderer::create_vulkan_instance()
{
    constexpr vk::ApplicationInfo appI {
        .pApplicationName = "WRenderer",
        .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION( 1, 0, 0 ),
        .apiVersion  = vk::ApiVersion13
    };

    const auto requiredLayers = get_required_layers();
    const auto requiredExtensions = get_required_extensions();

    const vk::InstanceCreateInfo instanceCI {
        .pApplicationInfo = &appI,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()
    };
    instance = vk::raii::Instance(context, instanceCI);
}

std::vector<const char*> WRenderer::get_required_layers() const
{
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers)
        requiredLayers.assign(validationLayers.begin(), validationLayers.end());

    std::stringstream unsupportedLayers {};
    auto layerProperties = context.enumerateInstanceLayerProperties();
    for (const auto& requiredLayer : requiredLayers)
        if (std::ranges::none_of(layerProperties,
                                 [requiredLayer](auto const& layerProperty)
                                 { return strcmp(layerProperty.layerName, requiredLayer) == 0; }))
            unsupportedLayers << " - " << requiredLayer << "\n";

    if (!unsupportedLayers.str().empty())
        throw std::runtime_error("Unsupported vulkan layers:\n" + unsupportedLayers.str());

    return requiredLayers;
}

std::vector<const char*> WRenderer::get_required_extensions() const
{
    uint32_t glfwExtensionCount = 0;
    const auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers)
        requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);

    std::stringstream unsupportedExtensions {};
    auto extensionProperties = context.enumerateInstanceExtensionProperties();

    for (auto requiredExtension : requiredExtensions)
    {
        if (std::ranges::none_of(extensionProperties,
                                 [glfwExtension = requiredExtension](auto const& extensionProperty)
                                 { return strcmp(extensionProperty.extensionName, glfwExtension) == 0; }))
        {
            unsupportedExtensions << " - " << requiredExtension << "\n";
        }
    }
    if (!unsupportedExtensions.str().empty())
        throw std::runtime_error("Unsupported GLFW extensions:\n" + unsupportedExtensions.str());

    return requiredExtensions;
}

void WRenderer::setup_debug_messenger()
{
    // ReSharper disable CppDFAUnreachableCode
    if constexpr (!enableValidationLayers) return;
    // ReSharper restore CppDFAUnreachableCode

    constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
    );
    constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
    );

    constexpr vk::DebugUtilsMessengerCreateInfoEXT debugCI {
        .messageSeverity = severityFlags,
        .messageType = messageTypeFlags,
        .pfnUserCallback = &debugCallback
    };
    debug_messenger = instance.createDebugUtilsMessengerEXT(debugCI);
}

void WRenderer::create_surface()
{
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
        throw std::runtime_error("Failed to create window surface");

    surface = vk::raii::SurfaceKHR(instance, _surface);
}

void WRenderer::pick_physical_device()
{
    const auto physicalDevices = instance.enumeratePhysicalDevices();

    const auto device_it = std::ranges::find_if(physicalDevices,
                                                [&](const auto& physicalDevice) {
                                                    auto queueFamilies = physicalDevice.getQueueFamilyProperties();
                                                    bool isSuitable = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

                                                    const auto queueFamilyProperty_it = std::ranges::find_if(queueFamilies,
                                                        [](const auto& qfp) {
                                                            return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
                                                        });
                                                    isSuitable = isSuitable && (queueFamilyProperty_it != queueFamilies.end());

                                                    const auto extensions = physicalDevice.enumerateDeviceExtensionProperties();
                                                    bool found = true;
                                                    for (const auto& dExtension : device_extensions)
                                                    {
                                                        auto extension_it = std::ranges::find_if(extensions, [dExtension](const auto& ext){return strcmp(ext.extensionName, dExtension) == 0; });
                                                        found = found && (extension_it != extensions.end());
                                                    }
                                                    isSuitable = isSuitable && found;

                                                    if (isSuitable)
                                                        physical_device = physicalDevice;

                                                    return isSuitable;
                                                });

    if (device_it == physicalDevices.end())
        throw std::runtime_error("failed to find a suitable GPU");
}

void WRenderer::create_logical_device()
{
    const auto queueFamilyProperties = physical_device.getQueueFamilyProperties();

    const auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties,
                                                                  [](const auto& qfp) {
                                                                      return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
                                                                  }
    );
    uint32_t graphicsIndex = std::ranges::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty);
    const uint32_t presentationIndex = get_presentation_qfp_index(graphicsIndex);

    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures {
        .pNext = nullptr,
        .extendedDynamicState = true
    };
    vk::PhysicalDeviceVulkan13Features vulkan13Features{
        .pNext = &extendedDynamicStateFeatures,
        .synchronization2 = true,
        .dynamicRendering = true
    };
    vk::PhysicalDeviceVulkan12Features vulkan12Features {
        .pNext = &vulkan13Features,
        .bufferDeviceAddress = true
    };
    vk::PhysicalDeviceVulkan11Features vulkan11Features {
        .pNext = &vulkan12Features,
        .shaderDrawParameters = true
    };
    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2 {
        .pNext = &vulkan11Features
    };

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos {};
    constexpr float queuePriority = 1.f;
    const vk::DeviceQueueCreateInfo graphicsQueueCI {
        .queueFamilyIndex = queue_index = graphicsIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };
    queueCreateInfos.push_back(graphicsQueueCI);

    if (graphicsIndex != presentationIndex)
    {
        const vk::DeviceQueueCreateInfo presentQueueCI {
            .queueFamilyIndex = presentationIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.push_back(presentQueueCI);
    }

    const vk::DeviceCreateInfo deviceCI {
        .pNext = &physicalDeviceFeatures2,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data()
    };
    device = vk::raii::Device(physical_device, deviceCI);

    graphics_queue = device.getQueue(graphicsIndex, 0);
    present_queue = device.getQueue(presentationIndex, 0);
}

uint32_t WRenderer::get_presentation_qfp_index(uint32_t& graphicsIndex) const
{
    const auto queueFamilyProperties = physical_device.getQueueFamilyProperties();

    auto presentationIndex = physical_device.getSurfaceSupportKHR(graphicsIndex, *surface) ? graphicsIndex : static_cast<uint32_t>( queueFamilyProperties.size() );
    if ( presentationIndex == queueFamilyProperties.size() )
    {
        // the graphicsIndex doesn't support present -> look for another family index that supports both
        // graphics and present
        for ( size_t i = 0; i < queueFamilyProperties.size(); i++ )
        {
            if ( ( queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics ) &&
                physical_device.getSurfaceSupportKHR( static_cast<uint32_t>( i ), *surface ) )
            {
                graphicsIndex = static_cast<uint32_t>( i );
                presentationIndex  = graphicsIndex;
                break;
            }
        }
        if ( presentationIndex == queueFamilyProperties.size() )
        {
            // if there's not a single family index that supports both graphics and present -> look for another
            // family index that supports present
            for ( size_t i = 0; i < queueFamilyProperties.size(); i++ )
            {
                if ( physical_device.getSurfaceSupportKHR( static_cast<uint32_t>( i ), *surface ) )
                {
                    presentationIndex = static_cast<uint32_t>( i );
                    break;
                }
            }
        }
    }
    if ( graphicsIndex == queueFamilyProperties.size() || presentationIndex == queueFamilyProperties.size() )
    {
        throw std::runtime_error( "Could not find a queue for graphics or present -> terminating" );
    }

    return presentationIndex;
}

void WRenderer::vma_init()
{
    constexpr VmaVulkanFunctions vkFunctions {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkCreateImage = vkCreateImage
    };
    const VmaAllocatorCreateInfo allocatorCI {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = *physical_device,
        .device = *device,
        .pVulkanFunctions = &vkFunctions,
        .instance = *instance
    };
    vmaCreateAllocator(&allocatorCI, &allocator);
}

void WRenderer::create_swap_chain()
{
    const auto swapSurfaceCapabilities = physical_device.getSurfaceCapabilitiesKHR(*surface);
    const auto [format, colorSpace] = chooseSwapSurfaceFormat(physical_device.getSurfaceFormatsKHR(*surface));

    const vk::SwapchainCreateInfoKHR swapChainCI {
        .flags = vk::SwapchainCreateFlagsKHR{},
        .surface = *surface,
        .minImageCount = chooseSwapMinImageCount(swapSurfaceCapabilities),
        .imageFormat = swap_chain_image_format = format,
        .imageColorSpace = colorSpace,
        .imageExtent = swap_chain_extent = chooseSwapExtent(swapSurfaceCapabilities, window),
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = swapSurfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = chooseSwapPresentMode(physical_device.getSurfacePresentModesKHR(*surface)),
        .clipped = vk::True,
        .oldSwapchain = VK_NULL_HANDLE
    };

    swap_chain = device.createSwapchainKHR(swapChainCI);
    swap_chain_images = swap_chain.getImages();
}

void WRenderer::create_image_views()
{
    swap_chain_image_views.clear();

    vk::ImageViewCreateInfo imageViewCI {
        .viewType = vk::ImageViewType::e2D,
        .format = swap_chain_image_format,
        .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
    };

    for (const auto& image : swap_chain_images)
    {
        imageViewCI.image = image;
        swap_chain_image_views.emplace_back(device, imageViewCI);
    }
}

void WRenderer::create_graphics_pipeline()
{
    const auto shaderModule = create_shader_module(readShaderFile("src/shader.spv"));
    const vk::PipelineShaderStageCreateInfo vertexShaderCI {
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shaderModule,
        .pName = "vertMain"
    };
    const vk::PipelineShaderStageCreateInfo fragmentShaderCI {
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shaderModule,
        .pName = "fragMain"
    };
    const vk::PipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCI, fragmentShaderCI};

    constexpr auto vertexBindingDescription = Vertex::GetBindingDescription();
    constexpr auto vertexAttributeDescriptions = Vertex::GetAttributeDescriptions();
    // ReSharper disable once CppVariableCanBeMadeConstexpr <- you can't actually make this into a constexpr because of the pointers
    const vk::PipelineVertexInputStateCreateInfo vertexInputCI {
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBindingDescription,
        .vertexAttributeDescriptionCount = vertexAttributeDescriptions.size(),
        .pVertexAttributeDescriptions = vertexAttributeDescriptions.data(),
    };
    constexpr vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCI {
        .topology = vk::PrimitiveTopology::eTriangleList
    };

    const std::vector dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    const vk::PipelineDynamicStateCreateInfo dynamicStateCI {
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    constexpr vk::PipelineViewportStateCreateInfo viewPortCI {
        .viewportCount = 1,
        .scissorCount = 1
    };
    constexpr vk::PipelineRasterizationStateCreateInfo rasterizerCI {
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f
    };
    constexpr vk::PipelineMultisampleStateCreateInfo multisamplingCI {
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False
    };
    constexpr  vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };
    // ReSharper disable once CppVariableCanBeMadeConstexpr <- you can't actually make this into a constexpr because of the pointer
    const  vk::PipelineColorBlendStateCreateInfo colorBlendingCI {
        .logicOpEnable = vk::False,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    constexpr vk::PipelineLayoutCreateInfo pipelineLayoutCI {
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0
    };
    pipeline_layout = {device, pipelineLayoutCI};

    const vk::PipelineRenderingCreateInfo pipelineRenderingCI {
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swap_chain_image_format
    };
    const vk::GraphicsPipelineCreateInfo pipelineCI {
        .pNext = &pipelineRenderingCI,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputCI,
        .pInputAssemblyState = &inputAssemblyCI,
        .pViewportState = &viewPortCI,
        .pRasterizationState = &rasterizerCI,
        .pMultisampleState = &multisamplingCI,
        .pColorBlendState = &colorBlendingCI,
        .pDynamicState = &dynamicStateCI,
        .layout = pipeline_layout,
        .renderPass = nullptr
    };

    graphics_pipeline = {device, nullptr, pipelineCI};
}

vk::raii::ShaderModule WRenderer::create_shader_module(const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo shaderModuleCI {
        .codeSize = code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    return {device, shaderModuleCI};
}

void WRenderer::create_command_pool()
{
    const vk::CommandPoolCreateInfo poolCI {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queue_index
    };
    command_pool = {device, poolCI};
}

void WRenderer::create_command_buffer()
{
    const vk::CommandBufferAllocateInfo allocateI {
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };
    command_buffers = vk::raii::CommandBuffers(device, allocateI);
}

void create_buffer(const VmaAllocator& _allocator, vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage memoryUsage, vk::Buffer& buffer, VmaAllocation& allocation);
void WRenderer::create_vertex_buffer()
{
    const vk::DeviceSize bufferSize {sizeof(vertices[0]) * vertices.size()};
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAllocation;
    create_buffer(
        allocator,
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_CPU_ONLY,
        stagingBuffer,
        stagingAllocation
    );

    void* data = nullptr;
    vmaMapMemory(allocator, stagingAllocation, &data);
    memcpy(data, vertices.data(), bufferSize);
    vmaUnmapMemory(allocator, stagingAllocation);

    create_buffer(
        allocator,
        bufferSize,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_GPU_ONLY,
        vertex_buffer,
        vertex_buffer_alloc
    );

    copy_buffer(stagingBuffer, vertex_buffer, bufferSize);
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
}

void WRenderer::create_index_buffer()
{
    const vk::DeviceSize bufferSize {sizeof(indices[0]) * indices.size()};
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAllocation;
    create_buffer(
        allocator,
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_CPU_ONLY,
        stagingBuffer,
        stagingAllocation
    );

    void* data = nullptr;
    vmaMapMemory(allocator, stagingAllocation, &data);
    memcpy(data, indices.data(), bufferSize);
    vmaUnmapMemory(allocator, stagingAllocation);

    create_buffer(
        allocator,
        bufferSize,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_GPU_ONLY,
        index_buffer,
        index_buffer_alloc
    );

    copy_buffer(stagingBuffer, index_buffer, bufferSize);
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
}

void create_buffer(const VmaAllocator& _allocator, const vk::DeviceSize size, const vk::BufferUsageFlags usage, const VmaMemoryUsage memoryUsage, vk::Buffer& buffer, VmaAllocation& allocation)
{
    const vk::BufferCreateInfo bufferCI{
        .sType = vk::StructureType::eBufferCreateInfo,
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };

    const VmaAllocationCreateInfo memoryAllocationCI {
        .usage = memoryUsage,
    };
    vmaCreateBuffer(
        _allocator,
        &*bufferCI,
        &memoryAllocationCI,
        reinterpret_cast<VkBuffer*>(&buffer),
        &allocation,
        nullptr
    );
}

void WRenderer::copy_buffer(const vk::Buffer& srcBuffer, const vk::Buffer& dstBuffer, const vk::DeviceSize size) const
{
    const vk::CommandBufferAllocateInfo allocateI {
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };
    const vk::raii::CommandBuffer commandCopyBuffer = std::move(device.allocateCommandBuffers(allocateI).front());

    commandCopyBuffer.begin(vk::CommandBufferBeginInfo {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
    commandCopyBuffer.end();

    graphics_queue.submit(vk::SubmitInfo {.commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer}, nullptr);
    graphics_queue.waitIdle();
}

void WRenderer::create_sync_object()
{
    assert(present_complete_semaphores.empty() && render_finished_semaphores.empty() && in_flight_fences.empty());

    for (size_t i = 0; i < swap_chain_images.size(); i++)
        render_finished_semaphores.emplace_back(device, vk::SemaphoreCreateInfo());

    constexpr vk::FenceCreateInfo fenceCI {
        .flags = vk::FenceCreateFlagBits::eSignaled
    };
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        present_complete_semaphores.emplace_back(device, vk::SemaphoreCreateInfo());
        in_flight_fences.emplace_back(device, fenceCI);
    }
}

/** This is needed for correct destruction on wayland because the ownership works differently.
    The instance owns the window object, meaning the default destructor of WRenderer causes a segmentation error **/
void WRenderer::destroy_vulkan()
{
    cleanup_swap_chain();

    in_flight_fences.clear();
    render_finished_semaphores.clear();
    present_complete_semaphores.clear();

    command_buffers.clear();
    command_pool.clear();

    vmaDestroyBuffer(allocator, index_buffer, index_buffer_alloc);
    vmaDestroyBuffer(allocator, vertex_buffer, vertex_buffer_alloc);

    pipeline_layout.clear();
    graphics_pipeline.clear();

    graphics_queue.clear();
    present_queue.clear();

    vmaDestroyAllocator(allocator);

    device.clear();
    physical_device.clear();

    surface.clear();
    debug_messenger.clear();
    instance.clear();
}

void WRenderer::cleanup_swap_chain()
{
    device.waitIdle();

    swap_chain_image_views.clear();
    swap_chain = nullptr;
}

void WRenderer::recreate_swap_chain()
{
    int _width = 0, _height = 0;
    glfwGetWindowSize(window, &_width, &_height);
    while (_width == 0 || _height == 0)
    {
        glfwGetFramebufferSize(window, &_width, &_height);
        glfwWaitEvents();
    }

    cleanup_swap_chain();

    create_swap_chain();
    create_image_views();
}

void WRenderer::record_command_buffer(const uint32_t imageIndex) const
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

    constexpr vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    const vk::RenderingAttachmentInfo attachmentI {
        .imageView = swap_chain_image_views[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor
    };
    const vk::RenderingInfo renderingI {
        .renderArea = {.offset = {0, 0}, .extent = swap_chain_extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentI,
    };

    command_buffers[frame_index].beginRendering(renderingI);
    command_buffers[frame_index].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline);
    command_buffers[frame_index].bindVertexBuffers(0, vertex_buffer, {0});
    command_buffers[frame_index].bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint32);
    command_buffers[frame_index].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swap_chain_extent.width), static_cast<float>(swap_chain_extent.height), 0.0f, 1.0f));
    command_buffers[frame_index].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain_extent));

    command_buffers[frame_index].drawIndexed(indices.size(), 1, 0, 0, 0);

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

void WRenderer::transition_image_layout(const uint32_t imageIndex, const vk::ImageLayout oldLayout, const vk::ImageLayout newLayout, const vk::AccessFlags2 srcAccessMask, const vk::AccessFlags2 dstAccessMask, const vk::PipelineStageFlags2 srcStageMask, const vk::PipelineStageFlags2 dstStageMask) const
{
    constexpr vk::ImageSubresourceRange imgSubresourceRange {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    const vk::ImageMemoryBarrier2 barrier {
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = swap_chain_images[imageIndex],
        .subresourceRange = imgSubresourceRange
    };
    const vk::DependencyInfo dependencyI {
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };

    command_buffers[frame_index].pipelineBarrier2(dependencyI);
}

void WRenderer::frame_buffer_resize_callback(GLFWwindow* window, int width, int height)
{
    const auto app = static_cast<WRenderer*>(glfwGetWindowUserPointer(window));
    app->frame_buffer_resized = true;
}

vk::Bool32 WRenderer::debugCallback(const vk::DebugUtilsMessageSeverityFlagBitsEXT severity, const vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
{
    std::cerr << to_string(severity) << " validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            return availableFormat;

    return availableFormats.front();
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}

uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& swapSurfaceCapabilities)
{
    auto minImageCount = std::max(3u, swapSurfaceCapabilities.minImageCount);
    minImageCount = (swapSurfaceCapabilities.maxImageCount > 0 && minImageCount > swapSurfaceCapabilities.maxImageCount) ? swapSurfaceCapabilities.maxImageCount : minImageCount;

    return minImageCount;
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
            return availablePresentMode;

    return vk::PresentModeKHR::eFifo;
}

std::vector<char> readShaderFile(const std::string &filename)
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
