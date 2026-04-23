#include "GLTFModel.hpp"

#include "core/Device.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <tiny_gltf.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace vulc
{

GLTFModel::GLTFModel(Device& device)
    : m_device{device}
{
}

GLTFModel::~GLTFModel()
{
    vkDestroyBuffer(m_device.handle(), vertices.buffer, nullptr);
    vkFreeMemory(m_device.handle(), vertices.memory, nullptr);
    vkDestroyBuffer(m_device.handle(), indices.buffer, nullptr);
    vkFreeMemory(m_device.handle(), indices.memory, nullptr);
}

void GLTFModel::loadImages(tinygltf::Model& input)
{
    m_images.resize(input.images.size());
    for(std::size_t i{0}; i < m_images.size(); ++i)
    {
        auto& glTFImage{ input.images[i] };

        // NOTE: If Image resource is in RGB, then convert it to RGBA
        if(glTFImage.component == GLTF_RGB_COMPONENT_COUNT)
        {
            const auto pixelCount{static_cast<std::size_t>(glTFImage.width) * static_cast<std::size_t>(glTFImage.height)};
            const auto& src{ glTFImage.image };

            std::vector<unsigned char> rgba(pixelCount * GLTF_RGBA_COMPONENT_COUNT);
            for(std::size_t j{0}; j < pixelCount; ++j)
            {
                const auto srcIdx{j * GLTF_RGB_COMPONENT_COUNT};
                const auto dstIdx{j * GLTF_RGBA_COMPONENT_COUNT};
                
                rgba[dstIdx + 0] = src[srcIdx + 0];
                rgba[dstIdx + 1] = src[srcIdx + 1];
                rgba[dstIdx + 2] = src[srcIdx + 2];
                rgba[dstIdx + 3] = 255; // NOLINT(readability-magic-numbers)
            }

            m_images[i].texture->fromBuffer(&m_device, std::as_bytes(std::span{rgba}), VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height);
        }
        else
            m_images[i].texture->fromBuffer(&m_device, std::as_bytes(std::span{glTFImage.image}), VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height);
    }
}

void GLTFModel::loadTextures(tinygltf::Model& input)
{
    m_textures.resize(input.textures.size());
    for (std::size_t i{0}; i < m_textures.size(); ++i)
        m_textures[i].imageIndex = input.textures[i].source;
}

void GLTFModel::loadMaterials(tinygltf::Model& input)
{
    m_materials.resize(input.materials.size());
    for(std::size_t i{0}; i < m_materials.size(); ++i)
    {
        auto& glTFMaterial{input.materials[i]};

        if(glTFMaterial.values.contains(::BASE_COLOR_FACTOR))
            m_materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values[::BASE_COLOR_FACTOR].ColorFactor().data());

        if(glTFMaterial.values.contains(::BASE_COLOR_TEXTURE))
            m_materials[i].baseColorTextureIndex = glTFMaterial.values[::BASE_COLOR_TEXTURE].TextureIndex();
    }
}

void GLTFModel::loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, GLTFModel::Node* parent, std::vector<std::uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer)
{
    auto* node{new GLTFModel::Node()};
    node->matrix = glm::mat4(1.f);
    node->parent = parent;

    // NOTE: Step 1 - get the local node transform
    if(inputNode.translation.size() == ::TRANSLATION_SIZE)
        node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));

    if(inputNode.rotation.size() == ::ROTATION_SIZE)
    {
        glm::quat q{glm::make_quat(inputNode.rotation.data())};
        node->matrix *= glm::mat4(q);
    }

    if(inputNode.scale.size() == ::SCALE_SIZE)
        node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));

    if(inputNode.matrix.size() == ::MATRIX_SIZE)
        node->matrix = glm::make_mat4x4(inputNode.matrix.data());

    if(!inputNode.children.empty())
        for(std::size_t i{0}; i < inputNode.children.size(); ++i)
            loadNode(input.nodes[inputNode.children[i]], input, node, indexBuffer, vertexBuffer);

    // NOTE: Step 2 - load mesh data if the node contains mesh data
    if(inputNode.mesh > -1)
    {
        const auto& mesh{input.meshes[inputNode.mesh]};

        for(std::size_t i{0}; i < mesh.primitives.size(); ++i)
        {
            const auto& glTFPrimitive{mesh.primitives[i]};
            std::uint32_t firstIndex{static_cast<std::uint32_t>(indexBuffer.size())};
            std::uint32_t vertexStart{static_cast<std::uint32_t>(vertexBuffer.size())};
            std::uint32_t indexCount{0};

            {
                const float* positionBuffer{nullptr};
                const float* normalsBuffer{nullptr};
                const float* texCoordsBuffer{nullptr};
                std::size_t vertexCount{0};

                // NOTE: Get data for vertex position
                if(glTFPrimitive.attributes.contains(::GLTF_PRIMITIVE_ATTRIBUTE_POSITION))
                {
                    const auto& accessor{input.accessors[glTFPrimitive.attributes.find(::GLTF_PRIMITIVE_ATTRIBUTE_POSITION)->second]};
                    const auto& view{input.bufferViews[accessor.bufferView]};

                    positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    vertexCount = accessor.count;
                }

                // NOTE: Get data for vertex normals
                if(glTFPrimitive.attributes.contains(::GLTF_PRIMITIVE_ATTRIBUTE_NORMAL))
                {
                    const auto& accessor{input.accessors[glTFPrimitive.attributes.find(::GLTF_PRIMITIVE_ATTRIBUTE_NORMAL)->second]};
                    const auto& view{input.bufferViews[accessor.bufferView]};

                    normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                // NOTE: Get data for vertex texture coordinates (glTF supports multiple sets, just load the first one)
                if(glTFPrimitive.attributes.contains(::GLTF_PRIMITIVE_ATTRIBUTE_TEX_COORD))
                {
                    const auto& accessor{input.accessors[glTFPrimitive.attributes.find(::GLTF_PRIMITIVE_ATTRIBUTE_TEX_COORD)->second]};
                    const auto& view{input.bufferViews[accessor.bufferView]};
                    
                    texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                for(std::size_t v{0}; v < vertexCount; ++v)
                {
                    vertexBuffer.emplace_back(
                        glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.f),
                        glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.f))),
                        texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.f),
                        glm::vec3(1.f)
                    );
                }
            }
            {
                const auto& accessor{input.accessors[glTFPrimitive.indices]};
                const auto& bufferView{input.bufferViews[accessor.bufferView]};
                const auto& buffer{input.buffers[bufferView.buffer]};

                indexCount += static_cast<std::uint32_t>(accessor.count);

                switch(accessor.componentType)
                {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        {
                            const auto* buf{reinterpret_cast<const std::uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset])};

                            for(std::size_t index{0}; index < accessor.count; ++index)
                                indexBuffer.push_back(buf[index] + vertexStart);

                            break;
                        }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                        {
                            const auto* buf{reinterpret_cast<const std::uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset])};

                            for(std::size_t index{0}; index < accessor.count; ++index)
                                indexBuffer.push_back(buf[index] + vertexStart);

                            break;
                        }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        {
                            const auto* buf{reinterpret_cast<const std::uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset])};

                            for(std::size_t index{0}; index < accessor.count; ++index)
                                indexBuffer.push_back(buf[index] + vertexStart);

                            break;
                        }
                    default:
                        {
                            spdlog::warn("Index component type {} not supported!", accessor.componentType);
                            return;
                        }
                }
            }

            node->mesh.primitives.emplace_back(firstIndex, indexCount, glTFPrimitive.material);
        }
    }

    if(parent != nullptr)
        parent->children.push_back(std::unique_ptr<GLTFModel::Node>(node));
    else
        m_nodes.push_back(std::unique_ptr<GLTFModel::Node>(node));
}

void GLTFModel::draw(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout)
{
    const VkDeviceSize offset{0};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertices.buffer, &offset);
    vkCmdBindIndexBuffer(cmdBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);

    for(const auto& node : m_nodes)
        drawNode(cmdBuffer, pipelineLayout, *node);
}

void GLTFModel::drawNode(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout, const GLTFModel::Node& node)
{
    if(!node.mesh.primitives.empty())
    {
        auto nodeMatrix{ node.matrix };
        const auto* currentParent{ node.parent };

        while(currentParent != nullptr)
        {
            nodeMatrix = currentParent->matrix * nodeMatrix;
            currentParent = currentParent->parent;
        }

        vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
        for(const auto& primitive : node.mesh.primitives)
        {
            if(primitive.indexCount > 0)
            {
                auto texture{ m_textures[m_materials[primitive.materialIndex].baseColorTextureIndex] };

                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &m_images[texture.imageIndex].descriptorSet, 0, nullptr);
                vkCmdDrawIndexed(cmdBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
            }
        }
    }

    for(const auto& child : node.children)
        drawNode(cmdBuffer, pipelineLayout, *child);
}

} // namespace vulc

