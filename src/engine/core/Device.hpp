#ifndef VULCANO_SRC_ENGINE_CORE_DEVICE_HPP
#define VULCANO_SRC_ENGINE_CORE_DEVICE_HPP

#include <limits>
#include <volk.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#include <cstdint>

namespace vulc
{

struct Queue
{
    static constexpr auto INVALID_QUEUE_INDEX{std::numeric_limits<std::uint32_t>::max()};

    VkQueue queue{VK_NULL_HANDLE};
    // NOTE: Use very high number to initialize index with since 0 is a valid queue index
    std::uint32_t queueFamilyIndex{INVALID_QUEUE_INDEX};
};

class Device
{
public:
    Device();
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] VkDevice handle() const noexcept { return m_device; }
    [[nodiscard]] VkDevice handle() noexcept { return m_device; }

    [[nodiscard]] VkInstance instance() const noexcept { return m_instance; }
    [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept { return m_physicalDevice; }
    [[nodiscard]] VmaAllocator allocator() const noexcept { return m_allocator; }

    [[nodiscard]] const Queue& graphicsQueue() const noexcept { return m_graphicsQueue; }

private:
    static constexpr auto VULKAN_API_VERSION{VK_API_VERSION_1_3};
    static constexpr auto ENABLE_VALIDATION_LAYERS{
#if VULCANO_DEBUG
        true
#else
        false
#endif
    };
    static constexpr auto QUEUE_FAMILY_PRIOS{1.f};

    const std::vector<const char*> DEVICE_EXTENSIONS{VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_dynamic_rendering"};
    const std::vector<const char*> VALIDATION_LAYERS{"VK_LAYER_KHRONOS_validation"};

    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    Queue m_graphicsQueue;
    Queue m_transferQueue;
    VmaAllocator m_allocator{VK_NULL_HANDLE};

    static void prepareSDL();
    void createInstance();
    void createDebugMessenger();
    void pickPhysicalDevice();
    void findQueueFamilies();
    void createDevice();
    void createAllocator();
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_DEVICE_HPP

