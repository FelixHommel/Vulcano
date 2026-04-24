#include "Renderer.hpp"

#include "core/CommandPool.hpp"
#include "core/Device.hpp"
#include "core/FrameResources.hpp"
#include "core/ObjModel.hpp"
#include "core/Pipeline.hpp"
#include "core/Swapchain.hpp"
#include "core/Texture.hpp"
#include "core/Window.hpp"
#include "utility/Common.hpp"
#include "utility/Globals.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <volk.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace vulc
{

Renderer::Renderer(
    const Device& device,
    Window& window,
    Swapchain& swapchain,
    const Pipeline& pipeline,
    std::vector<std::unique_ptr<Texture>> textures,
    const CommandPool& commandPool
)
    : m_device{ device }
    , m_window{ window }
    , m_swapchain{ swapchain }
    , m_pipeline{ pipeline }
    , m_textures(std::move(textures))
{
    createFrameResources(commandPool);
    createRenderSemaphores();
    createDescriptors();
}

Renderer::~Renderer()
{
    chk(vkDeviceWaitIdle(m_device.handle()));

    for(auto& s : m_renderSemaphores)
        vkDestroySemaphore(m_device.handle(), s, nullptr);

    vkDestroyDescriptorPool(m_device.handle(), m_descriptorPool, nullptr);
}

void Renderer::draw(const ObjModel& mesh, const ShaderData& shaderData)
{
    if(m_recreateSwapchain)
    {
        recreateSwapchain();
        return;
    }

    syncSwapchainImages();
    if(m_recreateSwapchain)
    {
        recreateSwapchain();
        return;
    }

    updateShaderdataBuffers(shaderData);
    recordCommandBuffers(mesh);
    submitToGraphicsQueue();

    m_frameIndex = (m_frameIndex + 1) % globals::MAX_FRMES_IN_FLIGHT;
    presentImage();

    if(m_recreateSwapchain)
        recreateSwapchain();
}

void Renderer::createFrameResources(const CommandPool& commandPool)
{
    m_frameResources.reserve(globals::MAX_FRMES_IN_FLIGHT);
    for(auto i{ 0 }; i < globals::MAX_FRMES_IN_FLIGHT; ++i)
        m_frameResources.emplace_back(std::make_unique<FrameResources>(m_device, commandPool));
}

void Renderer::createRenderSemaphores()
{
    const VkSemaphoreCreateInfo semaphoreCI{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    m_renderSemaphores.resize(m_swapchain.images().size());
    for(auto& s : m_renderSemaphores)
        chk(vkCreateSemaphore(m_device.handle(), &semaphoreCI, nullptr, &s));
}

void Renderer::createDescriptors()
{
    const VkDescriptorPoolSize poolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         .descriptorCount = static_cast<std::uint32_t>(m_textures.size()) };
    const VkDescriptorPoolCreateInfo descriptorPoolCI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                       .maxSets = 1,
                                                       .poolSizeCount = 1,
                                                       .pPoolSizes = &poolSize };
    chk(vkCreateDescriptorPool(m_device.handle(), &descriptorPoolCI, nullptr, &m_descriptorPool));

    std::uint32_t variableDescriptorCount{ static_cast<std::uint32_t>(m_textures.size()) };
    const VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &variableDescriptorCount
    };
    VkDescriptorSetLayout descriptorSetLayout{ m_pipeline.textureDescriptorSetLayout() };
    const VkDescriptorSetAllocateInfo descriptorSetAI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                       .pNext = &variableDescCountCI,
                                                       .descriptorPool = m_descriptorPool,
                                                       .descriptorSetCount = 1,
                                                       .pSetLayouts = &descriptorSetLayout };
    chk(vkAllocateDescriptorSets(m_device.handle(), &descriptorSetAI, &m_descriptorSet));

    std::vector<VkDescriptorImageInfo> textureDescriptors{};
    textureDescriptors.reserve(m_textures.size());
    for(const auto& t : m_textures)
        textureDescriptors.push_back(t->descriptorInfo());

    const VkWriteDescriptorSet writeDescSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                             .dstSet = m_descriptorSet,
                                             .dstBinding = 0,
                                             .descriptorCount = static_cast<std::uint32_t>(textureDescriptors.size()),
                                             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                             .pImageInfo = textureDescriptors.data() };
    vkUpdateDescriptorSets(m_device.handle(), 1, &writeDescSet, 0, nullptr);
}

void Renderer::syncSwapchainImages()
{
    auto& frameResources{ m_frameResources.at(m_frameIndex) };

    chk(vkWaitForFences(m_device.handle(), 1, &frameResources->fence, VK_TRUE, UINT64_MAX));

    chk(vkAcquireNextImageKHR(
            m_device.handle(),
            m_swapchain.handle(),
            UINT64_MAX,
            frameResources->acquireSemaphore,
            VK_NULL_HANDLE,
            &m_imageIndex
        ),
        m_recreateSwapchain);
}

void Renderer::updateShaderdataBuffers(const ShaderData& shaderData)
{
    std::memcpy(
        m_frameResources.at(m_frameIndex)->shaderDataBuffer.allocationInfo.pMappedData, &shaderData, sizeof(ShaderData)
    );
}

void Renderer::recordCommandBuffers(const ObjModel& mesh)
{
    auto& frameResources{ m_frameResources.at(m_frameIndex) };
    auto& commandBuffer{ frameResources->commandBuffer };

    chk(vkResetCommandBuffer(commandBuffer, 0));

    const VkCommandBufferBeginInfo cbBI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    chk(vkBeginCommandBuffer(commandBuffer, &cbBI));

    const std::array<VkImageMemoryBarrier2, 2> outputBarriers{
        VkImageMemoryBarrier2{
                              .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                              .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              .srcAccessMask = 0,
                              .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                              .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                              .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                              .image = m_swapchain.images().at(m_imageIndex),
                              .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } },
        VkImageMemoryBarrier2{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                              .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                              .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                              .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                              .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                              .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                              .image = m_swapchain.depthImage(),
                              .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                                  .levelCount = 1,
                                                  .layerCount = 1 }                                                          }
    };
    const VkDependencyInfo barrierDependencyInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                  .imageMemoryBarrierCount
                                                  = static_cast<std::uint32_t>(outputBarriers.size()),
                                                  .pImageMemoryBarriers = outputBarriers.data() };
    vkCmdPipelineBarrier2(commandBuffer, &barrierDependencyInfo);

    const VkRenderingAttachmentInfo colorAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                         .imageView = m_swapchain.imageViews().at(m_imageIndex),
                                                         .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                         .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                         .clearValue = { .color = { 0.f, 0.f, 0.f, 1.f } } };
    const VkRenderingAttachmentInfo depthAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                         .imageView = m_swapchain.depthImageView(),
                                                         .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                         .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                         .clearValue
                                                         = { .depthStencil = { .depth = 1.f, .stencil = 0 } } };
    const VkRenderingInfo renderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                         .renderArea = { .extent = m_swapchain.extent() },
                                         .layerCount = 1,
                                         .colorAttachmentCount = 1,
                                         .pColorAttachments = &colorAttachmentInfo,
                                         .pDepthAttachment = &depthAttachmentInfo };
    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    const VkViewport viewport{
        .width = static_cast<float>(m_swapchain.extent().width),
        .height = static_cast<float>(m_swapchain.extent().height),
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    const VkRect2D scissor{ .extent = m_swapchain.extent() };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.handle());
    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.layout(), 0, 1, &m_descriptorSet, 0, nullptr
    );

    const VkDeviceSize vOffset{ 0 };
    VkBuffer meshBuffer{ mesh.buffer() };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &meshBuffer, &vOffset);
    vkCmdBindIndexBuffer(commandBuffer, meshBuffer, mesh.vertexBufferSize(), VK_INDEX_TYPE_UINT16);

    const VkDeviceAddress shaderDataAddress{ frameResources->shaderDataBuffer.deviceAddress };

    vkCmdPushConstants(
        commandBuffer, m_pipeline.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &shaderDataAddress
    );

    vkCmdDrawIndexed(commandBuffer, mesh.indexCount(), VERTEX_INDICES, 0, 0, 0);

    vkCmdEndRendering(commandBuffer);

    const VkImageMemoryBarrier2 barrierPresent{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = m_swapchain.images().at(m_imageIndex),
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };
    const VkDependencyInfo barrierPresentDependencyInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                         .imageMemoryBarrierCount = 1,
                                                         .pImageMemoryBarriers = &barrierPresent };
    vkCmdPipelineBarrier2(commandBuffer, &barrierPresentDependencyInfo);

    chk(vkEndCommandBuffer(commandBuffer));
}

void Renderer::submitToGraphicsQueue()
{
    auto& frameResources{ m_frameResources.at(m_frameIndex) };

    const VkPipelineStageFlags waitStages{ VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT };
    const VkSubmitInfo submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   .waitSemaphoreCount = 1,
                                   .pWaitSemaphores = &frameResources->acquireSemaphore,
                                   .pWaitDstStageMask = &waitStages,
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &frameResources->commandBuffer,
                                   .signalSemaphoreCount = 1,
                                   .pSignalSemaphores = &m_renderSemaphores.at(m_imageIndex) };

    chk(vkResetFences(m_device.handle(), 1, &frameResources->fence));
    chk(vkQueueSubmit(m_device.graphicsQueue().queue, 1, &submitInfo, frameResources->fence));
}

void Renderer::presentImage()
{
    VkSwapchainKHR swapchain{ m_swapchain.handle() };
    const VkPresentInfoKHR presentInfo{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                        .waitSemaphoreCount = 1,
                                        .pWaitSemaphores = &m_renderSemaphores.at(m_imageIndex),
                                        .swapchainCount = 1,
                                        .pSwapchains = &swapchain,
                                        .pImageIndices = &m_imageIndex };
    chk(vkQueuePresentKHR(m_device.graphicsQueue().queue, &presentInfo), m_recreateSwapchain);
}

void Renderer::recreateSwapchain()
{
    vkDeviceWaitIdle(m_device.handle());

    m_swapchain.recreate(m_window);

    for(auto& s : m_renderSemaphores)
        vkDestroySemaphore(m_device.handle(), s, nullptr);

    createRenderSemaphores();

    m_recreateSwapchain = false;
}

} // namespace vulc

