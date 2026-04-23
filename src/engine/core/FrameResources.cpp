#include "FrameResources.hpp"

#include "core/CommandPool.hpp"
#include "core/Device.hpp"
#include "utility/Common.hpp"

#include <volk.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

namespace vulc
{

FrameResources::FrameResources(const Device& device, const CommandPool& commandPool) : m_device{ device }
{
    createShaderDataBuffer();
    createCommandBuffer(commandPool);
    createFence();
    createSemaphore();
}

FrameResources::~FrameResources()
{
    vmaDestroyBuffer(m_device.allocator(), shaderDataBuffer.buffer, shaderDataBuffer.allocation);
    vkDestroyFence(m_device.handle(), fence, nullptr);
    vkDestroySemaphore(m_device.handle(), acquireSemaphore, nullptr);
}

void FrameResources::createShaderDataBuffer()
{
    const VkBufferCreateInfo bufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                       .size = sizeof(ShaderData),
                                       .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT };

    const VmaAllocationCreateInfo bufferAllocCI{ .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                        | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                                                        | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                                 .usage = VMA_MEMORY_USAGE_AUTO };

    chk(vmaCreateBuffer(
        m_device.allocator(),
        &bufferCI,
        &bufferAllocCI,
        &shaderDataBuffer.buffer,
        &shaderDataBuffer.allocation,
        &shaderDataBuffer.allocationInfo
    ));

    const VkBufferDeviceAddressInfo bufferDeviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                             .buffer = shaderDataBuffer.buffer };
    shaderDataBuffer.deviceAddress = vkGetBufferDeviceAddress(m_device.handle(), &bufferDeviceAddressInfo);
}

void FrameResources::createCommandBuffer(const CommandPool& commandPool)
{
    const VkCommandBufferAllocateInfo commandBufferAI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                       .commandPool = commandPool.handle(),
                                                       .commandBufferCount = 1 };
    chk(vkAllocateCommandBuffers(m_device.handle(), &commandBufferAI, &commandBuffer));
}

void FrameResources::createFence()
{
    const VkFenceCreateInfo fenceCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                     .flags = VK_FENCE_CREATE_SIGNALED_BIT };

    chk(vkCreateFence(m_device.handle(), &fenceCI, nullptr, &fence));
}

void FrameResources::createSemaphore()
{
    const VkSemaphoreCreateInfo semaphoreCI{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    chk(vkCreateSemaphore(m_device.handle(), &semaphoreCI, nullptr, &acquireSemaphore));
}

} // namespace vulc
