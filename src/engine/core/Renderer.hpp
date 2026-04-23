#ifndef VULCANO_SRC_ENGINE_CORE_RENDERER_HPP
#define VULCANO_SRC_ENGINE_CORE_RENDERER_HPP

#include "core/CommandPool.hpp"
#include "core/Device.hpp"
#include "core/FrameResources.hpp"
#include "core/ObjModel.hpp"
#include "core/Pipeline.hpp"
#include "core/Swapchain.hpp"
#include "core/Texture.hpp"
#include "core/Window.hpp"
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include <volk.h>
#include <vulkan/vulkan_core.h>

#include <memory>

namespace vulc
{

class Renderer
{
public:
    Renderer(
        const Device& device,
        Window& window,
        Swapchain& swapchain,
        const Pipeline& pipeline,
        std::vector<std::unique_ptr<Texture>> textures,
        const CommandPool& commandPool
    );
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    void draw(const ObjModel& mesh, const ShaderData& shaderData);
    void requestSwapchainRecreate() noexcept { m_recreateSwapchain = true; }

private:
    static constexpr auto VERTEX_INDICES{ 3 };

    const Device& m_device;
    Window& m_window;
    Swapchain& m_swapchain;
    const Pipeline& m_pipeline;
    std::vector<std::unique_ptr<Texture>> m_textures;

    std::vector<std::unique_ptr<FrameResources>> m_frameResources;
    std::vector<VkSemaphore> m_renderSemaphores;

    VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
    VkDescriptorSet m_descriptorSet{ VK_NULL_HANDLE };

    std::uint32_t m_frameIndex{ 0 };
    std::uint32_t m_imageIndex{ 0 };
    bool m_recreateSwapchain{ false };

    void createFrameResources(const CommandPool& commandPool);
    void createRenderSemaphores();
    void createDescriptors();

    void syncSwapchainImages();
    void updateShaderdataBuffers(const ShaderData& shaderData);
    void recordCommandBuffers(const ObjModel& mesh);
    void submitToGraphicsQueue();
    void presentImage();

    void recreateSwapchain();
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_RENDERER_HPP

