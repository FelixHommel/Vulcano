#include "Device.hpp"

#include "utility/Common.hpp"

#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <vector>

namespace
{

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/
)
{
    switch(severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        spdlog::info("{}", pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        spdlog::warn("{}", pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        spdlog::error("{}", pCallbackData->pMessage);
        break;
    default:
        spdlog::critical("{}", pCallbackData->pMessage);
        break;
    }

    return VK_FALSE;
}


} // namespace

namespace vulc
{

Device::Device()
{
    Device::prepareSDL();
    createInstance();
    createDebugMessenger();
    pickPhysicalDevice();
    findQueueFamilies();
    createDevice();
    createAllocator();
    createCommandPool();
}

Device::~Device()
{
    vkDestroyCommandPool(m_device, m_internalCommandPool, nullptr);
    vmaDestroyAllocator(m_allocator);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    vkDestroyDevice(m_device, nullptr);

    if constexpr(ENABLE_VALIDATION_LAYERS)
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);

    vkDestroyInstance(m_instance, nullptr);
}

VkCommandBuffer Device::createCommandBuffer(VkCommandBufferLevel level, bool begin) const
{
    VkCommandBuffer cmdBuffer{ VK_NULL_HANDLE };

    const VkCommandBufferAllocateInfo commandBufferAI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                       .commandPool = m_internalCommandPool,
                                                       .level = level,
                                                       .commandBufferCount = 1 };
    chk(vkAllocateCommandBuffers(m_device, &commandBufferAI, &cmdBuffer));

    if(begin)
    {
        const VkCommandBufferBeginInfo cbBeginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
        chk(vkBeginCommandBuffer(cmdBuffer, &cbBeginInfo));
    }

    return cmdBuffer;
}

void Device::submitCommandBuffer(VkCommandBuffer cmdBuffer, bool free) const
{
    if(cmdBuffer == VK_NULL_HANDLE)
        return;

    chk(vkEndCommandBuffer(cmdBuffer));

    const VkSubmitInfo submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &cmdBuffer };

    VkFence submitFence{ VK_NULL_HANDLE };
    const VkFenceCreateInfo fenceCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    chk(vkCreateFence(m_device, &fenceCI, nullptr, &submitFence));

    chk(vkQueueSubmit(m_transferQueue.queue, 1, &submitInfo, submitFence));
    chk(vkWaitForFences(m_device, 1, &submitFence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(m_device, submitFence, nullptr);

    if(free)
        vkFreeCommandBuffers(m_device, m_internalCommandPool, 1, &cmdBuffer);
}

void Device::prepareSDL()
{
    chk(SDLResult{ SDL_Init(SDL_INIT_VIDEO) });
    chk(SDLResult{ SDL_Vulkan_LoadLibrary(nullptr) });
}

void Device::createInstance()
{
    volkInitialize();

    const VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                     .pApplicationName = "VulkanRenderer",
                                     .apiVersion = VULKAN_API_VERSION };

    std::uint32_t instanceExtensionSDLCount{ 0 };
    const auto* instanceExtensionsSDL{ SDL_Vulkan_GetInstanceExtensions(&instanceExtensionSDLCount) };
    const std::span<const char* const> sdlExtensions{ instanceExtensionsSDL, instanceExtensionSDLCount };

    std::vector<const char*> extensions(
        std::move_iterator(sdlExtensions.cbegin()), std::move_iterator(sdlExtensions.cend())
    );
    if constexpr(ENABLE_VALIDATION_LAYERS)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo instanceCI{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                     .pApplicationInfo = &appInfo,
                                     .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
                                     .ppEnabledExtensionNames = extensions.data() };

    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI{};
    if constexpr(ENABLE_VALIDATION_LAYERS)
    {
        debugMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugMessengerCI.messageSeverity
            = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                     | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                     | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugMessengerCI.pfnUserCallback = ::debugMessageCallback;

        instanceCI.enabledLayerCount = static_cast<std::uint32_t>(DEVICE_LAYERS.size());
        instanceCI.ppEnabledLayerNames = DEVICE_LAYERS.data();
        instanceCI.pNext = &debugMessengerCI;
    }

    chk(vkCreateInstance(&instanceCI, nullptr, &m_instance));
    volkLoadInstance(m_instance);
}

void Device::createDebugMessenger()
{
    if constexpr(!ENABLE_VALIDATION_LAYERS)
        return;

    const VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity
        = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = ::debugMessageCallback
    };
    chk(vkCreateDebugUtilsMessengerEXT(m_instance, &debugMessengerCI, nullptr, &m_debugMessenger));
}

void Device::pickPhysicalDevice()
{
    // NOTE: Retrieve all physical devices
    std::uint32_t deviceCount{ 0 };
    chk(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr));
    std::vector<VkPhysicalDevice> devices(deviceCount);
    chk(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()));

    // TODO: Implement a robuster device selection algorithm
    m_physicalDevice = devices.at(0);

    VkPhysicalDeviceProperties2 deviceProperties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &deviceProperties);
    spdlog::info("Selected device: {}", deviceProperties.properties.deviceName);
}

void Device::findQueueFamilies()
{
    std::uint32_t queueFamilyCount{ 0 };
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    for(std::size_t i{ 0 }; i < queueFamilies.size(); ++i)
    {
        // NOTE: Not the best queue selection heuristic (the first queue that fits the flag is chosen) but it works for now

        if((queueFamilies.at(i).queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u)
            m_graphicsQueue.queueFamilyIndex = i;

        if((queueFamilies.at(i).queueFlags & VK_QUEUE_TRANSFER_BIT) != 0u)
            m_transferQueue.queueFamilyIndex = i;

        if(m_graphicsQueue.queueFamilyIndex != ::INVALID_QUEUE_INDEX
           && m_transferQueue.queueFamilyIndex != ::INVALID_QUEUE_INDEX)
        {
            spdlog::info(
                "Found queues for graphics and transfer. Graphics index: {} | Transfer index: {}",
                m_graphicsQueue.queueFamilyIndex,
                m_transferQueue.queueFamilyIndex
            );
            break;
        }
    }

    if(m_transferQueue.queueFamilyIndex == ::INVALID_QUEUE_INDEX)
    {
        spdlog::info("Could not find a dedicated transfer queue, using same as graphics.");
        m_transferQueue.queueFamilyIndex = m_graphicsQueue.queueFamilyIndex;
    }

    chk(SDLResult{ SDL_Vulkan_GetPresentationSupport(m_instance, m_physicalDevice, m_graphicsQueue.queueFamilyIndex) });
}

void Device::createDevice()
{
    VkPhysicalDeviceVulkan12Features enabledVk12Features{ .sType
                                                          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                                                          .descriptorIndexing = VK_TRUE,
                                                          .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
                                                          .descriptorBindingVariableDescriptorCount = VK_TRUE,
                                                          .runtimeDescriptorArray = VK_TRUE,
                                                          .bufferDeviceAddress = VK_TRUE };

    const VkPhysicalDeviceVulkan13Features enabledVk13Features{ .sType
                                                                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                                                                .pNext = &enabledVk12Features,
                                                                .synchronization2 = VK_TRUE,
                                                                .dynamicRendering = VK_TRUE };

    const VkPhysicalDeviceFeatures enabledVk10Features{ .samplerAnisotropy = VK_TRUE };

    const VkDeviceQueueCreateInfo queueCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                           .queueFamilyIndex = m_graphicsQueue.queueFamilyIndex,
                                           .queueCount = 1,
                                           .pQueuePriorities = &QUEUE_FAMILY_PRIOS };

    const VkDeviceCreateInfo deviceCI{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                       .pNext = &enabledVk13Features,
                                       .queueCreateInfoCount = 1,
                                       .pQueueCreateInfos = &queueCI,
                                       .enabledExtensionCount = static_cast<std::uint32_t>(DEVICE_EXTENSIONS.size()),
                                       .ppEnabledExtensionNames = DEVICE_EXTENSIONS.data(),
                                       .pEnabledFeatures = &enabledVk10Features };
    chk(vkCreateDevice(m_physicalDevice, &deviceCI, nullptr, &m_device));

    volkLoadDevice(m_device);

    vkGetDeviceQueue(m_device, m_graphicsQueue.queueFamilyIndex, 0, &m_graphicsQueue.queue);
    vkGetDeviceQueue(m_device, m_transferQueue.queueFamilyIndex, 0, &m_transferQueue.queue);
}

void Device::createAllocator()
{
    const VmaVulkanFunctions vkFunctions{ .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                                          .vkGetDeviceProcAddr = vkGetDeviceProcAddr };

    const VmaAllocatorCreateInfo allocatorCI{ .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                                              .physicalDevice = m_physicalDevice,
                                              .device = m_device,
                                              .pVulkanFunctions = &vkFunctions,
                                              .instance = m_instance,
                                              .vulkanApiVersion = VULKAN_API_VERSION };
    chk(vmaCreateAllocator(&allocatorCI, &m_allocator));
}

void Device::createCommandPool()
{
    const VkCommandPoolCreateInfo commandPoolCI{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                 .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                                                        | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                 .queueFamilyIndex = m_transferQueue.queueFamilyIndex };
    chk(vkCreateCommandPool(m_device, &commandPoolCI, nullptr, &m_internalCommandPool));
}

} // namespace vulc

