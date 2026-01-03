//
// Created by pheen on 02/01/2026.
//

#include "WEngine.h"

#include "json.hpp"

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

vk::raii::Instance createVulkanInstance(const vk::raii::Context& context);
void WEngine::init_vulkan()
{
    instance = createVulkanInstance(context);
    setup_debug_messenger();
    pick_physical_device();
}

std::vector<const char*> getRequiredLayers(const vk::raii::Context& context);
std::vector<const char*> getRequiredExtensions(const vk::raii::Context& context);
vk::raii::Instance createVulkanInstance(const vk::raii::Context& context)
{
    constexpr vk::ApplicationInfo app_info {
        .pApplicationName   = "WEngine",
        .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
        .apiVersion         = vk::ApiVersion14
    };

    auto requiredLayers = getRequiredLayers(context);
    auto requiredExtensions = getRequiredExtensions(context);

    vk::InstanceCreateInfo createInfo {
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()
    };
    return {context, createInfo};
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
    if constexpr (!enableValidationLayers) return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
    );
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
    );

    vk::DebugUtilsMessengerCreateInfoEXT createInfo {
        .messageSeverity = severityFlags,
        .messageType = messageTypeFlags,
        .pfnUserCallback = &debugCallback
    };
    debug_messenger = instance.createDebugUtilsMessengerEXT(createInfo);
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

uint32_t findQueueFamilies(const vk::raii::PhysicalDevice& physicalDevice, vk::QueueFlagBits requestedProperty);
void WEngine::create_logical_device()
{
    auto graphicsIndex = findQueueFamilies(physical_device, vk::QueueFlagBits::eGraphics);

    float queuePriority = .5f;
    vk::DeviceQueueCreateInfo queueCreateInfo {
        .queueFamilyIndex = graphicsIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain (
        {},
        {.dynamicRendering = true},
        {.extendedDynamicState = true}
    );

    vk::DeviceCreateInfo deviceCreateInfo {
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount =  static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()
    };

    device = vk::raii::Device(physical_device, deviceCreateInfo);

    graphics_queue = vk::raii::Queue(device, graphicsIndex, 0);
}

uint32_t findQueueFamilies(const vk::raii::PhysicalDevice& physicalDevice, vk::QueueFlagBits requestedProperty)
{
    auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    auto requestedQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties,
        [requestedProperty](const auto& qfp) {
            return (qfp.queueFlags & requestedProperty) != static_cast<vk::QueueFlags>(0);
        });

    return std::ranges::distance(queueFamilyProperties.begin(), requestedQueueFamilyProperty);
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

vk::Bool32 WEngine::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
{
    std::cerr << to_string(severity) << " validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
}
