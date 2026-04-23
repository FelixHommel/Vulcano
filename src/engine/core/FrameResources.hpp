#ifndef VULCANO_SRC_ENGINE_CORE_FRAME_RESOURCES_HPP
#define VULCANO_SRC_ENGINE_CORE_FRAME_RESOURCES_HPP

#include "core/CommandPool.hpp"
#include "core/Device.hpp"
#include "utility/Globals.hpp"

#include <glm/glm.hpp>

#include <volk.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>

namespace
{

constexpr auto DEFAULT_LIGHT_POS{
    glm::vec4{ 0.f, -10.f, 10.f, 0.f }
};

} // namespace

namespace vulc
{

struct ShaderDataBuffer
{
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VmaAllocationInfo allocationInfo{};
    VkBuffer buffer{ VK_NULL_HANDLE };
    VkDeviceAddress deviceAddress{};
};

struct ShaderData
{
    glm::mat4 projection;
    glm::mat4 view;
    std::array<glm::mat4, globals::NUM_MODELS> model;
    glm::vec4 lightPos{ ::DEFAULT_LIGHT_POS };
    std::uint32_t selected{ 1 };
};

class FrameResources
{
public:
    FrameResources(const Device& device, const CommandPool& commandPool);
    ~FrameResources();

    FrameResources(const FrameResources&) = delete;
    FrameResources& operator=(const FrameResources&) = delete;
    FrameResources(FrameResources&&) = delete;
    FrameResources& operator=(FrameResources&&) = delete;

    ShaderDataBuffer shaderDataBuffer{};
    VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
    VkFence fence{ VK_NULL_HANDLE };
    VkSemaphore acquireSemaphore{ VK_NULL_HANDLE };

    const Device& m_device;

    void createShaderDataBuffer();
    void createCommandBuffer(const CommandPool& commandPool);
    void createFence();
    void createSemaphore();
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_FRAME_RESOURCES_HPP

