#include "CommandPool.hpp"

#include "core/Device.hpp"
#include "utility/Common.hpp"

#include <volk.h>

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <span>

namespace vulc
{

CommandPool::CommandPool(const Device& device) : m_device{device}
{
    const VkCommandPoolCreateInfo commandPoolCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_device.graphicsQueue().queueFamilyIndex
    };
    chk(vkCreateCommandPool(m_device.handle(), &commandPoolCI, nullptr, &m_commandPool));
}

CommandPool::~CommandPool()
{
    vkDestroyCommandPool(m_device.handle(), m_commandPool, nullptr);
}

void CommandPool::allocateCommandBuffers(std::span<VkCommandBuffer> commandBuffers)
{
    const VkCommandBufferAllocateInfo cbAllocCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_commandPool,
        .commandBufferCount = static_cast<std::uint32_t>(commandBuffers.size())
    };
    chk(vkAllocateCommandBuffers(m_device.handle(), &cbAllocCI, commandBuffers.data()));
}

} // namespace vulc

