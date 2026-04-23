#ifndef VULCANO_SRC_ENGINE_CORE_GLTF_MODEL_HPP
#define VULCANO_SRC_ENGINE_CORE_GLTF_MODEL_HPP

#include "core/Device.hpp"
#include "core/Texture.hpp"
#include "utility/Common.hpp"
#include "utility/Vertex.hpp"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
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
        Node* parent{nullptr};
        std::vector<std::unique_ptr<Node>> children;
        Mesh mesh;
        glm::mat4 matrix;
    };

    struct Material
    {
        glm::vec4 baseColorFactor{ glm::vec4(1.f) };
        std::uint32_t baseColorTextureIndex;
    };

    struct Image
    {
        std::unique_ptr<vulc::Texture> texture{std::make_unique<vulc::Texture>()};
        VkDescriptorSet descriptorSet;
    };

    struct Texture
    {
        std::int32_t imageIndex;
    };

    GLTFModel(Device& device);
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

    Device& m_device;

    std::vector<GLTFModel::Image> m_images;
    std::vector<GLTFModel::Texture> m_textures;
    std::vector<GLTFModel::Material> m_materials;
    std::vector<std::unique_ptr<GLTFModel::Node>> m_nodes;

    void drawNode(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout, const GLTFModel::Node& node);
};

namespace gltf
{

static inline GLTFModel* loadGLTF(Device& device, const std::filesystem::path& filepath)
{
    tinygltf::Model gltfInput;
    tinygltf::TinyGLTF gltfContext;
    std::string error{};
    std::string warning{};

    bool fileLoaded{gltfContext.LoadASCIIFromFile(&gltfInput, &error, &warning, filepath.string())};

    auto* model{new GLTFModel(device)};

    VULCANO_ASSERT(model != nullptr, "The model must be initialized");

    std::vector<std::uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;

    if(fileLoaded)
    {
        model->loadImages(gltfInput);
        model->loadMaterials(gltfInput);
        model->loadTextures(gltfInput);

        const tinygltf::Scene& scene{gltfInput.scenes[0]};
        for(std::size_t i{0}; i < scene.nodes.size(); ++i)
        {
            const auto& node{ gltfInput.nodes[scene.nodes[i]] };
            model->loadNode(node, gltfInput, nullptr, indexBuffer, vertexBuffer);
        }
    }
    else
    {
        spdlog::error("Could not open gltf file at: {}", filepath.string());
        return nullptr;
    }

    return model;
}

} // namespace gltf

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_GLTF_MODEL_HPP

