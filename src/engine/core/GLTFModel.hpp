#ifndef VULCANO_SRC_ENGINE_CORE_GLTF_MODEL_HPP
#define VULCANO_SRC_ENGINE_CORE_GLTF_MODEL_HPP

#include "core/Device.hpp"
#include "core/Texture.hpp"
#include <memory>

#include <glm/glm.hpp>

#include <tiny_gltf.h>
#include <volk.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <vector>

namespace vulc
{

struct
{
    VkBuffer buffer;
    VkDeviceMemory memory;
} vertices;

struct
{
    std::int32_t count;
    VkBuffer buffer;
    VkDeviceMemory memory;
} indices;

struct Primitive
{
    std::uint32_t firstIndex;
    std::uint32_t indexCount;
    std::int32_t materialIndex;
};

struct Mesh
{
    std::vector<Primitive> primitives;
};

struct Node
{
    Node* parent;
    std::vector<Node*> children;
    Mesh mesh;
    glm::mat4 matrix;

    ~Node()
    {
        for(auto& c : children)
            delete c;
    }
};

struct Material
{
    glm::vec4 baseColor{ glm::vec4(1.f) };
    std::uint32_t baseColorTextureIndex;
};

struct Image
{
    std::unique_ptr<Texture> texture;
    VkDescriptorSet descriptorSet;
};

struct GLTFTexture
{
    std::int32_t imageIndex;
};

class GLTFModel
{
public:
    GLTFModel(const Device& device);
    ~GLTFModel();

    GLTFModel(const GLTFModel&) = delete;
    GLTFModel& operator=(const GLTFModel&) = delete;
    GLTFModel(GLTFModel&&) = delete;
    GLTFModel& operator=(GLTFModel&&) = delete;

    void loadImages(tinygltf::Model& input);

private:
    static constexpr auto GLTF_RGB_COMPONENT_COUNT{ 3 };
    static constexpr auto GLTF_RGBA_COMPONENT_COUNT{ 4 };

    const Device& m_device;

    std::vector<Image> m_images;
    std::vector<Texture> m_textures;
    std::vector<Material> m_materials;
    std::vector<Node*> m_nodes;
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_GLTF_MODEL_HPP

