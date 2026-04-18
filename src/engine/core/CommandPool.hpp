#ifndef VULCANO_SRC_ENGINE_CORE_COMMAND_POOL_HPP
#define VULCANO_SRC_ENGINE_CORE_COMMAND_POOL_HPP

#include "core/Device.hpp"

#include <volk.h>

#include <vulkan/vulkan_core.h>

#include <span>

namespace vulc
{

class CommandPool
{
public:
    CommandPool(const Device& device);
    ~CommandPool();

    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;
    CommandPool(CommandPool&&) = delete;
    CommandPool& operator=(CommandPool&&) = delete;

    [[nodiscard]] VkCommandPool handle() const noexcept { return m_commandPool; }
    [[nodiscard]] VkCommandPool handle() noexcept { return m_commandPool; }

    void allocateCommandBuffers(std::span<VkCommandBuffer> commandBuffers);

private:
    const Device& m_device;

    VkCommandPool m_commandPool{VK_NULL_HANDLE};
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_COMMAND_POOL_HPP

