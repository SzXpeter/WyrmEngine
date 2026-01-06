//
// Created by pheen on 02/01/2026.
//

#include "WEngine.h"

#include <fstream>
#include <iostream>
#include <sstream>

#ifdef NDEBUG
static constexpr bool enableValidationLayers = false;
#else
static constexpr bool enableValidationLayers = true;
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#endif

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

    window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
}

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
}

std::vector<const char*> getRequiredLayers(const vk::raii::Context& context);
std::vector<const char*> getRequiredExtensions(const vk::raii::Context& context);
void WEngine::create_vulkan_instance()
{
    vk::ApplicationInfo appInfo {};
    appInfo.pApplicationName = "WEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.apiVersion  = vk::ApiVersion14;

    const auto requiredLayers = getRequiredLayers(context);
    const auto requiredExtensions = getRequiredExtensions(context);

    vk::InstanceCreateInfo instanceCreateInfo {};
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = requiredLayers.data();
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();

    instance = vk::raii::Instance(context, instanceCreateInfo);
}

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

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
    debugCreateInfo.messageSeverity = severityFlags;
    debugCreateInfo.messageType = messageTypeFlags;
    debugCreateInfo.pfnUserCallback = &debugCallback;

    debug_messenger = instance.createDebugUtilsMessengerEXT(debugCreateInfo);
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
    auto physicalDevices = instance.enumeratePhysicalDevices();

    const auto device_it = std::ranges::find_if(physicalDevices,
        [&](const auto& physicalDevice) {
            auto queueFamilies = physicalDevice.getQueueFamilyProperties();
            bool isSuitable = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

            const auto queueFamilyProperty_it = std::ranges::find_if(queueFamilies,
                [](const auto& qfp) {
                    return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
            });
            isSuitable = isSuitable && (queueFamilyProperty_it != queueFamilies.end());

            auto extensions = physicalDevice.enumerateDeviceExtensionProperties();
            bool found = true;
            for (const auto& dExtension : deviceExtensions)
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
    auto queueFamilyProperties = physical_device.getQueueFamilyProperties();

    auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties,
        [](const auto& qfp) {
            return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
        });

    uint32_t graphicsIndex = std::ranges::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty);
    uint32_t presentationIndex = getPresentationQFPIndex(physical_device, surface, graphicsIndex);
    bool sameQFP = graphicsIndex == presentationIndex;

    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2 {};

    vk::PhysicalDeviceVulkan11Features vulkan11Features {};
    vulkan11Features.pNext = &physicalDeviceFeatures2;
    vulkan11Features.shaderDrawParameters = true;

    vk::PhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.pNext = &vulkan11Features;
    vulkan13Features.dynamicRendering = true;

    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures {};
    extendedDynamicStateFeatures.pNext = &vulkan13Features;
    extendedDynamicStateFeatures.extendedDynamicState = true;

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = .5f;

    vk::DeviceQueueCreateInfo graphicsQueueCreateInfo {};
    graphicsQueueCreateInfo.queueFamilyIndex = graphicsIndex;
    graphicsQueueCreateInfo.queueCount = 1;
    graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;

    queueCreateInfos.push_back(graphicsQueueCreateInfo);

    if (!sameQFP)
    {
        vk::DeviceQueueCreateInfo presentQueueCreateInfo {};
        presentQueueCreateInfo.queueFamilyIndex = presentationIndex;
        presentQueueCreateInfo.queueCount = 1;
        presentQueueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.push_back(presentQueueCreateInfo);
    }

    vk::DeviceCreateInfo deviceCreateInfo {};
    deviceCreateInfo.pNext = &extendedDynamicStateFeatures;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount =  static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    logical_device = vk::raii::Device(physical_device, deviceCreateInfo);

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
            // there's nothing like a single family index that supports both graphics and present -> look for another
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
    if ( ( graphicsIndex == queueFamilyProperties.size() ) || ( presentationIndex == queueFamilyProperties.size() ) )
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
    auto swapSurfaceCapabilities = physical_device.getSurfaceCapabilitiesKHR(*surface);
    auto swapSurfaceFormat = chooseSwapSurfaceFormat(physical_device.getSurfaceFormatsKHR(*surface));

    vk::SwapchainCreateInfoKHR swapChainCreateInfo {};
    swapChainCreateInfo.flags = vk::SwapchainCreateFlagsKHR{};
    swapChainCreateInfo.surface = *surface;
    swapChainCreateInfo.minImageCount = chooseSwapMinImageCount(swapSurfaceCapabilities);
    swapChainCreateInfo.imageFormat = swap_chain_image_format = swapSurfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = swapSurfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = swap_chain_extent = chooseSwapExtent(swapSurfaceCapabilities, window);
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapChainCreateInfo.preTransform = swapSurfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapChainCreateInfo.presentMode = chooseSwapPresentMode(physical_device.getSurfacePresentModesKHR(*surface));
    swapChainCreateInfo.clipped = vk::True;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    swap_chain = logical_device.createSwapchainKHR(swapChainCreateInfo);
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

    vk::ImageViewCreateInfo imageViewCreateInfo {};
    imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imageViewCreateInfo.format = swap_chain_image_format;
    imageViewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

    for (auto image : swap_chain_images)
    {
        imageViewCreateInfo.image = image;
        swap_chain_image_views.emplace_back(logical_device, imageViewCreateInfo);
    }
}

void WEngine::create_graphics_pipeline()
{
    auto shaderModule = CreateShaderModule(ReadShaderFile("src/shaders/shader.spv"));

    vk::PipelineShaderStageCreateInfo vertexShaderInfo {};
    vertexShaderInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertexShaderInfo.module = shaderModule;
    vertexShaderInfo.pName = "VertexMain";

    vk::PipelineShaderStageCreateInfo fragmentShaderInfo {};
    fragmentShaderInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragmentShaderInfo.module = shaderModule;
    fragmentShaderInfo.pName = "FragmentMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertexShaderInfo, fragmentShaderInfo};
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo {};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly {};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

    std::vector dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.dynamicStateCount = dynamicStates.size();
    dynamicState.pDynamicStates = dynamicStates.data();

    vk::PipelineViewportStateCreateInfo viewPort {};
    viewPort.viewportCount = 1;
    viewPort.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.depthClampEnable = vk::False;
    rasterizer.rasterizerDiscardEnable = vk::False;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = vk::False;
    rasterizer.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling {};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = vk::False;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = vk::True;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo colorBlending {};
    colorBlending.logicOpEnable = vk::False;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo {};
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipeline_layout = {logical_device, pipelineLayoutInfo};

    vk::PipelineRenderingCreateInfo pipelineRenderingInfo {};
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &swap_chain_image_format;

    vk::GraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.pNext = &pipelineRenderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewPort;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipeline_layout;
    pipelineInfo.renderPass = nullptr;

    graphics_pipeline = {logical_device, nullptr, pipelineInfo};
}

void WEngine::main_loop() const
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
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
