#include <iostream>

#include "WEngine.h"

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
/* The instance is the connection between the application and the Vulkan library */
void WEngine::create_vulkan_instance()
{
    vk::ApplicationInfo appI {};
    appI.pApplicationName = "WEngine";
    appI.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appI.pEngineName = "No Engine";
    appI.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appI.apiVersion  = vk::ApiVersion14;

    const auto requiredLayers = getRequiredLayers(context);
    const auto requiredExtensions = getRequiredExtensions(context);

    vk::InstanceCreateInfo instanceCI {};
    instanceCI.pApplicationInfo = &appI;
    instanceCI.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
    instanceCI.ppEnabledLayerNames = requiredLayers.data();
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    instanceCI.ppEnabledExtensionNames = requiredExtensions.data();

    instance = vk::raii::Instance(context, instanceCI);
}

/* Layers are optional components that augment the Vulkan system
 * They can intercept, evaluate and modify existing Vulkan functions */
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

/* Extensions have the ability to add new functionality
 * They may define new Vulkan functions, enums, structs, or feature bits */
std::vector<const char*> getRequiredExtensions(const vk::raii::Context& context)
{
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

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

    vk::DebugUtilsMessengerCreateInfoEXT debugCI {};
    debugCI.messageSeverity = severityFlags;
    debugCI.messageType = messageTypeFlags;
    debugCI.pfnUserCallback = &debugCallback;

    debug_messenger = instance.createDebugUtilsMessengerEXT(debugCI);
}

/* VkSurfaceKHR represents an abstract type of surface to present rendered images to
 * This surface is backed by glfw in this program */
void WEngine::create_surface()
{
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
        throw std::runtime_error("Failed to create window surface");

    surface = vk::raii::SurfaceKHR(instance, _surface);
}

/* A “Queue Family” just describes a set of VkQueues that have common properties and support the same functionality,
 * as advertised in VkQueueFamilyProperties */
void WEngine::pick_physical_device()
{
    auto physicalDevices = instance.enumeratePhysicalDevices();

    const auto device_it = std::ranges::find_if(physicalDevices,
        [&](const auto& physicalDevice) {
            auto queueFamilies = physicalDevice.getQueueFamilyProperties();
            bool isSuitable = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_4;

            const auto queueFamilyProperty_it = std::ranges::find_if(queueFamilies,
                [](const auto& qfp) {
                    return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
            });
            isSuitable = isSuitable && (queueFamilyProperty_it != queueFamilies.end());

            auto extensions = physicalDevice.enumerateDeviceExtensionProperties();
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
/* A logical device is used to interface the physical device or smtn idk
 * The creation process describes the features we want to use.
 * We also need to specify which queues to create now that we’ve queried which queue families are available */
void WEngine::create_logical_device()
{
    auto queueFamilyProperties = physical_device.getQueueFamilyProperties();

    auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties,
        [](const auto& qfp) {
            return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
        });
    uint32_t graphicsIndex = std::ranges::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty);
    uint32_t presentationIndex = getPresentationQFPIndex(physical_device, surface, graphicsIndex);

    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2 {};

    vk::PhysicalDeviceVulkan11Features vulkan11Features {};
    vulkan11Features.shaderDrawParameters = true;

    vk::PhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.dynamicRendering = true;
    vulkan13Features.synchronization2 = true;

    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures {};
    extendedDynamicStateFeatures.extendedDynamicState = true;

    physicalDeviceFeatures2.pNext = &vulkan11Features;
    vulkan11Features.pNext = &vulkan13Features;
    vulkan13Features.pNext = &extendedDynamicStateFeatures;
    extendedDynamicStateFeatures.pNext = nullptr;

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos {};
    float queuePriority = .5f;

    vk::DeviceQueueCreateInfo graphicsQueueCI {};
    graphicsQueueCI.queueFamilyIndex = queue_index = graphicsIndex;
    graphicsQueueCI.queueCount = 1;
    graphicsQueueCI.pQueuePriorities = &queuePriority;

    queueCreateInfos.push_back(graphicsQueueCI);

    if (graphicsIndex != presentationIndex)
    {
        vk::DeviceQueueCreateInfo presentQueueCI {};
        presentQueueCI.queueFamilyIndex = presentationIndex;
        presentQueueCI.queueCount = 1;
        presentQueueCI.pQueuePriorities = &queuePriority;

        queueCreateInfos.push_back(presentQueueCI);
    }

    vk::DeviceCreateInfo deviceCI {};
    deviceCI.pNext = &physicalDeviceFeatures2;
    deviceCI.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCI.pQueueCreateInfos = queueCreateInfos.data();
    deviceCI.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    deviceCI.ppEnabledExtensionNames = device_extensions.data();

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
/* The swap chain is essentially a queue of images that are waiting to be presented to the screen
 * The general purpose of the swap chain is to synchronize the presentation of images with the refresh rate of the screen */
void WEngine::create_swap_chain()
{
    auto swapSurfaceCapabilities = physical_device.getSurfaceCapabilitiesKHR(*surface);
    auto swapSurfaceFormat = chooseSwapSurfaceFormat(physical_device.getSurfaceFormatsKHR(*surface));

    vk::SwapchainCreateInfoKHR swapChainCI {};
    swapChainCI.flags = vk::SwapchainCreateFlagsKHR{};
    swapChainCI.surface = *surface;
    swapChainCI.minImageCount = chooseSwapMinImageCount(swapSurfaceCapabilities);
    swapChainCI.imageFormat = swap_chain_image_format = swapSurfaceFormat.format;
    swapChainCI.imageColorSpace = swapSurfaceFormat.colorSpace;
    swapChainCI.imageExtent = swap_chain_extent = chooseSwapExtent(swapSurfaceCapabilities, window);
    swapChainCI.imageArrayLayers = 1;
    swapChainCI.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCI.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCI.preTransform = swapSurfaceCapabilities.currentTransform;
    swapChainCI.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCI.presentMode = chooseSwapPresentMode(physical_device.getSurfacePresentModesKHR(*surface));
    swapChainCI.clipped = vk::True;
    swapChainCI.oldSwapchain = VK_NULL_HANDLE;

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

/* An image view discribes how to access the image and which part of the image to access */
void WEngine::create_image_views()
{
    swap_chain_image_views.clear();

    vk::ImageViewCreateInfo imageViewCI {};
    imageViewCI.viewType = vk::ImageViewType::e2D;
    imageViewCI.format = swap_chain_image_format;
    imageViewCI.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    for (auto image : swap_chain_images)
    {
        imageViewCI.image = image;
        swap_chain_image_views.emplace_back(logical_device, imageViewCI);
    }
}

/* The graphics pipeline is the sequence of operations that take the vertices and textures
 * of your meshes all the way to the pixels in the render targets */
void WEngine::create_graphics_pipeline()
{
    auto shaderModule = CreateShaderModule(ReadShaderFile("src/shaders/shader.spv"));

    vk::PipelineShaderStageCreateInfo vertexShaderCI {};
    vertexShaderCI.stage = vk::ShaderStageFlagBits::eVertex;
    vertexShaderCI.module = shaderModule;
    vertexShaderCI.pName = "vertMain";

    vk::PipelineShaderStageCreateInfo fragmentShaderCI {};
    fragmentShaderCI.stage = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderCI.module = shaderModule;
    fragmentShaderCI.pName = "fragMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCI, fragmentShaderCI};
    vk::PipelineVertexInputStateCreateInfo vertexInputCI {};

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCI {};
    inputAssemblyCI.topology = vk::PrimitiveTopology::eTriangleList;

    std::vector dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo dynamicStateCI{};
    dynamicStateCI.dynamicStateCount = dynamicStates.size();
    dynamicStateCI.pDynamicStates = dynamicStates.data();

    vk::PipelineViewportStateCreateInfo viewPortCI {};
    viewPortCI.viewportCount = 1;
    viewPortCI.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizerCI {};
    rasterizerCI.depthClampEnable = vk::False;
    rasterizerCI.rasterizerDiscardEnable = vk::False;
    rasterizerCI.polygonMode = vk::PolygonMode::eFill;
    rasterizerCI.cullMode = vk::CullModeFlagBits::eBack;
    rasterizerCI.frontFace = vk::FrontFace::eClockwise;
    rasterizerCI.depthBiasEnable = vk::False;
    rasterizerCI.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisamplingCI {};
    multisamplingCI.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisamplingCI.sampleShadingEnable = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = vk::False;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlendingCI {};
    colorBlendingCI.logicOpEnable = vk::False;
    colorBlendingCI.attachmentCount = 1;
    colorBlendingCI.pAttachments = &colorBlendAttachment;

    vk::PipelineLayoutCreateInfo pipelineLayoutCI {};
    pipelineLayoutCI.setLayoutCount = 0;
    pipelineLayoutCI.pushConstantRangeCount = 0;
    pipeline_layout = {logical_device, pipelineLayoutCI};

    vk::PipelineRenderingCreateInfo pipelineRenderingCI {};
    pipelineRenderingCI.colorAttachmentCount = 1;
    pipelineRenderingCI.pColorAttachmentFormats = &swap_chain_image_format;

    vk::GraphicsPipelineCreateInfo pipelineCI {};
    pipelineCI.pNext = &pipelineRenderingCI;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages;
    pipelineCI.pVertexInputState = &vertexInputCI;
    pipelineCI.pInputAssemblyState = &inputAssemblyCI;
    pipelineCI.pViewportState = &viewPortCI;
    pipelineCI.pRasterizationState = &rasterizerCI;
    pipelineCI.pMultisampleState = &multisamplingCI;
    pipelineCI.pColorBlendState = &colorBlendingCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.layout = pipeline_layout;
    pipelineCI.renderPass = nullptr;

    graphics_pipeline = {logical_device, nullptr, pipelineCI};
}

/* Command pools manage the memory that is used to store the buffers and command buffers are allocated from them */
void WEngine::create_command_pool()
{
    vk::CommandPoolCreateInfo poolCI {};
    poolCI.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolCI.queueFamilyIndex = queue_index;

    command_pool = {logical_device, poolCI};
}

/* You have to record all the operations you want to perform in command buffer objects
 * it is more efficient to process the commands all together */
void WEngine::create_command_buffer()
{
    vk::CommandBufferAllocateInfo allocateI {};
    allocateI.commandPool = command_pool;
    allocateI.level = vk::CommandBufferLevel::ePrimary;
    allocateI.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    command_buffers = vk::raii::CommandBuffers(logical_device, allocateI);
}

/* These are used to keep the gpu in sync with itself and with the cpu */
void WEngine::create_sync_object()
{
    assert(present_complete_semaphores.empty() && render_finished_semaphores.empty() && in_flight_fences.empty());

    for (size_t i = 0; i < swap_chain_images.size(); i++)
        render_finished_semaphores.emplace_back(logical_device, vk::SemaphoreCreateInfo());

    vk::FenceCreateInfo fenceCI {};
    fenceCI.flags = vk::FenceCreateFlagBits::eSignaled;

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

/* This is triggered if the window is resized or minimized */
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

/* This is needed for correct destruction on wayland because the ownership works differently (the instance owns the window object) */
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
