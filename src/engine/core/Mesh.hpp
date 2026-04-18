#ifndef VULCANO_SRC_ENGINE_CORE_MESH_HPP
#define VULCANO_SRC_ENGINE_CORE_MESH_HPP

#include "core/Device.hpp"

#include <volk.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <filesystem>

namespace vulc
{

class Mesh
{
public:
    Mesh(const Device& device, const std::filesystem::path& filepath);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = delete;
    Mesh& operator=(Mesh&&) = delete;

    [[nodiscard]] VkDeviceSize indexCount() const noexcept { return m_indexCount; }
    [[nodiscard]] VkDeviceSize vertexBufferSize() const noexcept { return m_vertexBufferSize; }
    [[nodiscard]] VkBuffer buffer() const noexcept { return m_buffer; }

private:
    const Device& m_device;

    VkDeviceSize m_indexCount{0};
    VkDeviceSize m_vertexBufferSize{0};
    VkBuffer m_buffer{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_MESH_HPP
