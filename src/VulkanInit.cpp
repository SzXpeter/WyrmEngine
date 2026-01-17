//
// Created by pheen on 09/01/2026.
//

#include "WEngine.h"

#include <iostream>
#include <sstream>

void WEngine::init_vulkan()
{
    create_vulkan_instance();
    setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swap_chain();
    create_image_views();
    create_graphics_pipeline();
    create_command_pool();
    create_command_buffer();
    create_sync_object();
}

std::vector<const char*> getRequiredLayers(const vk::raii::Context& context);
std::vector<const char*> getRequiredExtensions(const vk::raii::Context& context);
void WEngine::create_vulkan_instance()
{
    constexpr vk::ApplicationInfo appI {
        .pApplicationName = "WEngine",
        .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION( 1, 0, 0 ),
        .apiVersion  = vk::ApiVersion14
    };

    const auto requiredLayers = getRequiredLayers(context);
    const auto requiredExtensions = getRequiredExtensions(context);

    const vk::InstanceCreateInfo instanceCI {
        .pApplicationInfo = &appI,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()
    };
    instance = vk::raii::Instance(context, instanceCI);
}

std::vector<const char*> getRequiredLayers(const vk::raii::Context& context)
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

std::vector<const char*> getRequiredExtensions(const vk::raii::Context& context)
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

void WEngine::setup_debug_messenger()
{
    // ReSharper disable CppDFAUnreachableCode
    if constexpr (!enableValidationLayers) return;
    // ReSharper restore CppDFAUnreachableCode

    constexpr vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
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

void WEngine::create_surface()
{
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
        throw std::runtime_error("Failed to create window surface");

    surface = vk::raii::SurfaceKHR(instance, _surface);
}

void WEngine::pick_physical_device()
{
    const auto physicalDevices = instance.enumeratePhysicalDevices();

    const auto device_it = std::ranges::find_if(physicalDevices,
        [&](const auto& physicalDevice) {
            auto queueFamilies = physicalDevice.getQueueFamilyProperties();
            bool isSuitable = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_4;

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

uint32_t getPresentationQFPIndex(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface, uint32_t& graphicsIndex);
void WEngine::create_logical_device()
{
    const auto queueFamilyProperties = physical_device.getQueueFamilyProperties();

    const auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties,
        [](const auto& qfp) {
            return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
        }
    );
    uint32_t graphicsIndex = std::ranges::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty);
    const uint32_t presentationIndex = getPresentationQFPIndex(physical_device, surface, graphicsIndex);

    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures {
        .pNext = nullptr,
        .extendedDynamicState = true
    };
    vk::PhysicalDeviceVulkan13Features vulkan13Features{
        .pNext = &extendedDynamicStateFeatures,
        .synchronization2 = true,
        .dynamicRendering = true
    };
    vk::PhysicalDeviceVulkan11Features vulkan11Features {
        .pNext = &vulkan13Features,
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
    logical_device = vk::raii::Device(physical_device, deviceCI);

    graphics_queue = logical_device.getQueue(graphicsIndex, 0);
    present_queue = logical_device.getQueue(presentationIndex, 0);
}

uint32_t getPresentationQFPIndex(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface, uint32_t& graphicsIndex)
{
    const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    auto presentationIndex = physicalDevice.getSurfaceSupportKHR(graphicsIndex, *surface) ? graphicsIndex : static_cast<uint32_t>( queueFamilyProperties.size() );
    if ( presentationIndex == queueFamilyProperties.size() )
    {
        // the graphicsIndex doesn't support present -> look for another family index that supports both
        // graphics and present
        for ( size_t i = 0; i < queueFamilyProperties.size(); i++ )
        {
            if ( ( queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics ) &&
                 physicalDevice.getSurfaceSupportKHR( static_cast<uint32_t>( i ), *surface ) )
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
                if ( physicalDevice.getSurfaceSupportKHR( static_cast<uint32_t>( i ), *surface ) )
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

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& swapSurfaceCapabilities);
vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
void WEngine::create_swap_chain()
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

    swap_chain = logical_device.createSwapchainKHR(swapChainCI);
    swap_chain_images = swap_chain.getImages();
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

void WEngine::create_image_views()
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
        swap_chain_image_views.emplace_back(logical_device, imageViewCI);
    }
}

void WEngine::create_graphics_pipeline()
{
    const auto shaderModule = CreateShaderModule(ReadShaderFile("src/shaders/shader.spv"));
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

    constexpr vk::PipelineVertexInputStateCreateInfo vertexInputCI {};
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
    // ReSharper disable once CppVariableCanBeMadeConstexpr
    // you can't actually make this into a constexpr because of the pointer
    const  vk::PipelineColorBlendStateCreateInfo colorBlendingCI {
        .logicOpEnable = vk::False,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    constexpr vk::PipelineLayoutCreateInfo pipelineLayoutCI {
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0
    };
    pipeline_layout = {logical_device, pipelineLayoutCI};

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

    graphics_pipeline = {logical_device, nullptr, pipelineCI};
}

void WEngine::create_command_pool()
{
    const vk::CommandPoolCreateInfo poolCI {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queue_index
    };
    command_pool = {logical_device, poolCI};
}

void WEngine::create_command_buffer()
{
    const vk::CommandBufferAllocateInfo allocateI {
        .commandPool = command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };
    command_buffers = vk::raii::CommandBuffers(logical_device, allocateI);
}

void WEngine::create_sync_object()
{
    assert(present_complete_semaphores.empty() && render_finished_semaphores.empty() && in_flight_fences.empty());

    for (size_t i = 0; i < swap_chain_images.size(); i++)
        render_finished_semaphores.emplace_back(logical_device, vk::SemaphoreCreateInfo());

    constexpr vk::FenceCreateInfo fenceCI {
        .flags = vk::FenceCreateFlagBits::eSignaled
    };
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        present_complete_semaphores.emplace_back(logical_device, vk::SemaphoreCreateInfo());
        in_flight_fences.emplace_back(logical_device, fenceCI);
    }
}

void WEngine::cleanup_swap_chain()
{
    logical_device.waitIdle();

    swap_chain_image_views.clear();
    swap_chain = nullptr;
}

void WEngine::recreate_swap_chain()
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

/** This is needed for correct destruction on wayland because the ownership works differently.
    The instance owns the window object, meaning the default destructor of WEngine causes a segmentation error **/
void WEngine::destroy_vulkan()
{
    cleanup_swap_chain();

    in_flight_fences.clear();
    render_finished_semaphores.clear();
    present_complete_semaphores.clear();

    command_buffers.clear();
    command_pool = nullptr;

    pipeline_layout = nullptr;
    graphics_pipeline = nullptr;

    graphics_queue = nullptr;
    present_queue = nullptr;

    logical_device = nullptr;
    physical_device = nullptr;

    surface = nullptr;
    debug_messenger = nullptr;
    instance = nullptr;
}
