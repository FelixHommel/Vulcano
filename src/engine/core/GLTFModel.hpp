#ifndef VULCANO_SRC_ENGINE_CORE_GLTF_MODEL_HPP
#define VULCANO_SRC_ENGINE_CORE_GLTF_MODEL_HPP

#include "core/Device.hpp"
#include "core/Texture.hpp"
#include "utility/Vertex.hpp"

#include <glm/glm.hpp>
#include <tiny_gltf.h>
#include <volk.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace
{

constexpr auto BASE_COLOR_FACTOR{"baseColorFactor"};
constexpr auto BASE_COLOR_TEXTURE{"baseColorTexture"};

constexpr std::size_t TRANSLATION_SIZE{3};
constexpr std::size_t ROTATION_SIZE{4};
constexpr std::size_t SCALE_SIZE{3};
constexpr std::size_t MATRIX_SIZE{16};

constexpr auto GLTF_PRIMITIVE_ATTRIBUTE_POSITION{"POSITION"};
constexpr auto GLTF_PRIMITIVE_ATTRIBUTE_NORMAL{"NORMAL"};
constexpr auto GLTF_PRIMITIVE_ATTRIBUTE_TEX_COORD{"TEXCOORD_0"};

} // namespace

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

class GLTFModel
{
public:
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
        glm::vec4 baseColorFactor{ glm::vec4(1.f) };
        std::uint32_t baseColorTextureIndex;
    };

    struct Image
    {
        std::unique_ptr<Texture> texture;
        VkDescriptorSet descriptorSet;
    };

    struct Texture
    {
        std::int32_t imageIndex;
    };

    GLTFModel(const Device& device);
    ~GLTFModel();

    GLTFModel(const GLTFModel&) = delete;
    GLTFModel& operator=(const GLTFModel&) = delete;
    GLTFModel(GLTFModel&&) = delete;
    GLTFModel& operator=(GLTFModel&&) = delete;

    /// \brief Load the images that are specified by the glTF file
    ///
    /// Handles the difference between RGB and RGBA images by converting RGB images into RGBA format to allow for
    /// unified handling moving forward.
    ///
    /// \param input the tinygltf::Model that contains all the glTF data
    void loadImages(tinygltf::Model& input);
    void loadTextures(tinygltf::Model& input);
    void loadMaterials(tinygltf::Model& input);
    void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, GLTFModel::Node* parent, std::vector<std::uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer);

    void draw(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);

private:
    static constexpr auto GLTF_RGB_COMPONENT_COUNT{ 3u };
    static constexpr auto GLTF_RGBA_COMPONENT_COUNT{ 4u };

    const Device& m_device;

    std::vector<GLTFModel::Image> m_images;
    std::vector<GLTFModel::Texture> m_textures;
    std::vector<GLTFModel::Material> m_materials;
    std::vector<GLTFModel::Node*> m_nodes;

    void drawNode(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout, GLTFModel::Node* node);
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_GLTF_MODEL_HPP

