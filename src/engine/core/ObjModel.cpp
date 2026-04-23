#include "ObjModel.hpp"

#include "core/Device.hpp"
#include "utility/Common.hpp"
#include "utility/Vertex.hpp"

#include <volk.h>

#include <tiny_obj_loader.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <utility>
#include <vector>

namespace vulc
{

ObjModel::ObjModel(const Device& device, const std::filesystem::path& filepath) : m_device{ device }
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    chk(
        tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, nullptr, filepath.c_str())
    ); // TODO: Look into the nullptr arguments for debug messages

    m_indexCount = shapes[0].mesh.indices.size();
    std::vector<Vertex> vertices{};
    std::vector<std::uint16_t> indices{};
    for(const auto& index : shapes[0].mesh.indices)
    {
        const std::size_t indexBaseOffset{ static_cast<std::size_t>(index.vertex_index) * 3 };
        const std::size_t normalBaseOffset{ static_cast<std::size_t>(index.normal_index) * 3 };
        const std::size_t uvBaseOffset{ static_cast<std::size_t>(index.texcoord_index) * 2 };

        Vertex v{
            .pos = { attrib.vertices[indexBaseOffset],
                    -attrib.vertices[indexBaseOffset + 1],
                    attrib.vertices[indexBaseOffset + 2] },
            .normal = { attrib.normals[normalBaseOffset],
                    -attrib.normals[normalBaseOffset + 1],
                    attrib.normals[normalBaseOffset + 2] },
            .uv = { attrib.texcoords[uvBaseOffset], 1.0 - attrib.texcoords[uvBaseOffset + 1] }
        };

        vertices.push_back(std::move(v));
        indices.push_back(indices.size());
    }

    m_vertexBufferSize = sizeof(Vertex) * vertices.size();
    const VkDeviceSize iBufSize{ sizeof(std::uint16_t) * indices.size() };
    const VkBufferCreateInfo bufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                       .size = m_vertexBufferSize + iBufSize,
                                       .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT };
    const VmaAllocationCreateInfo vBufferAllocCI{ .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                         | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                                                         | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                                  .usage = VMA_MEMORY_USAGE_AUTO };
    VmaAllocationInfo allocationInfo{};
    chk(vmaCreateBuffer(m_device.allocator(), &bufferCI, &vBufferAllocCI, &m_buffer, &m_allocation, &allocationInfo));

    std::memcpy(allocationInfo.pMappedData, vertices.data(), m_vertexBufferSize);
    std::memcpy(
        std::next(static_cast<char*>(allocationInfo.pMappedData), static_cast<std::ptrdiff_t>(m_vertexBufferSize)),
        indices.data(),
        iBufSize
    );
}

ObjModel::~ObjModel()
{
    vmaDestroyBuffer(m_device.allocator(), m_buffer, m_allocation);
}

} // namespace vulc

