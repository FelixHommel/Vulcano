#ifndef VULCANO_SRC_ENGINE_CORE_PIPELINE_HPP
#define VULCANO_SRC_ENGINE_CORE_PIPELINE_HPP

#include "core/Device.hpp"
#include "core/Shader.hpp"
#include "core/Swapchain.hpp"

#include <volk.h>

#include <vulkan/vulkan_core.h>

#include <cstdint>

namespace
{

constexpr std::uint32_t DEFAULT_MAX_DESCRIPTOR_COUNT{16};

} // namespace

namespace vulc
{

class Pipeline
{
public:
    Pipeline(
        const Device& device,
        const Shader& shader,
        const SwapchainImageFormats& imageFormats,
        std::uint32_t maxDescriptorCount = ::DEFAULT_MAX_DESCRIPTOR_COUNT
    );
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;

    [[nodiscard]] VkPipeline handle() const noexcept { return m_pipeline; }
    [[nodiscard]] VkPipeline handle() noexcept { return m_pipeline; }

    [[nodiscard]] VkPipelineLayout layout() const noexcept { return m_pipelineLayout; }
    [[nodiscard]] VkDescriptorSetLayout textureDescriptorSetLayout() const noexcept
    {
        return m_textureDescriptorSetLayout;
    }

private:
    static constexpr auto BLEND_COLOR_WRITE_MASK{0xF};

    const Device& m_device;
    std::uint32_t m_maxDescriptorCount{0};

    VkDescriptorSetLayout m_textureDescriptorSetLayout{VK_NULL_HANDLE};

    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};

    void createDescriptorSetLayout();
    void createPipelineLayout(const Shader& shader, const SwapchainImageFormats& imageFormats);
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_PIPELINE_HPP
