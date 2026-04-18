#include "Texture.hpp"

#include "core/CommandPool.hpp"
#include "core/Device.hpp"
#include "utility/Common.hpp"

#include <cstdint>
#include <cstring>
#include <volk.h>

#include <ktx.h>
#include <ktxvulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <filesystem>
#include <vector>

namespace vulc
{

Texture::Texture(const Device& device, const CommandPool& commandPool, const std::filesystem::path& filepath)
    : m_device(device)
{
    ktxTexture* texture{nullptr};
    ktxTexture_CreateFromNamedFile(filepath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

    const VkImageCreateInfo texImgCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = ktxTexture_GetVkFormat(texture),
        .extent = {.width = texture->baseWidth, .height = texture->baseHeight, .depth = 1},
        .mipLevels = texture->numLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    const VmaAllocationCreateInfo texImageAllocCI{.usage = VMA_MEMORY_USAGE_AUTO};
    chk(vmaCreateImage(m_device.allocator(), &texImgCI, &texImageAllocCI, &m_image, &m_allocation, nullptr));

    const VkImageViewCreateInfo texViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = texImgCI.format,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = texture->numLevels, .layerCount = 1}
    };
    chk(vkCreateImageView(m_device.handle(), &texViewCI, nullptr, &m_view));

    VkBuffer imgSrcBuffer{VK_NULL_HANDLE};
    VmaAllocation imgSrcAllocation{VK_NULL_HANDLE};
    const VkBufferCreateInfo imgSrcBufferCI{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = static_cast<std::uint32_t>(texture->dataSize),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };
    const VmaAllocationCreateInfo imgSrcAllocCI{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    VmaAllocationInfo imgSrcAllocInfo{};
    chk(vmaCreateBuffer(
        m_device.allocator(), &imgSrcBufferCI, &imgSrcAllocCI, &imgSrcBuffer, &imgSrcAllocation, &imgSrcAllocInfo
    ));

    std::memcpy(imgSrcAllocInfo.pMappedData, texture->pData, texture->dataSize);

    const VkFenceCreateInfo oneTimeFenceCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence oneTimeFence{VK_NULL_HANDLE};
    chk(vkCreateFence(m_device.handle(), &oneTimeFenceCI, nullptr, &oneTimeFence));

    VkCommandBuffer oneTimeCB{VK_NULL_HANDLE};
    const VkCommandBufferAllocateInfo oneTimeCBAI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool.handle(),
        .commandBufferCount = 1
    };
    chk(vkAllocateCommandBuffers(m_device.handle(), &oneTimeCBAI, &oneTimeCB));

    const VkCommandBufferBeginInfo oneTimeCBBI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    chk(vkBeginCommandBuffer(oneTimeCB, &oneTimeCBBI));

    const VkImageMemoryBarrier2 barrierTexImage{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = m_image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = texture->numLevels, .layerCount = 1}
    };
    VkDependencyInfo barrierTexInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrierTexImage
    };
    vkCmdPipelineBarrier2(oneTimeCB, &barrierTexInfo);

    std::vector<VkBufferImageCopy> copyRegions{};
    for(auto j{0}; j < texture->numLevels; ++j)
    {
        ktx_size_t mipOffset{0};
        [[maybe_unused]] auto ret{ktxTexture_GetImageOffset(texture, j, 0, 0, &mipOffset)};

        copyRegions.push_back({
            .bufferOffset = mipOffset,
            .imageSubresource
            = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = static_cast<std::uint32_t>(j), .layerCount = 1},
            .imageExtent = {.width = texture->baseWidth >> j,        .height = texture->baseHeight >> j,        .depth = 1     }
        });
    }

    vkCmdCopyBufferToImage(
        oneTimeCB,
        imgSrcBuffer,
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<std::uint32_t>(copyRegions.size()),
        copyRegions.data()
    );

    const VkImageMemoryBarrier2 barrierTexRead{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .image = m_image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = texture->numLevels, .layerCount = 1}
    };
    barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
    vkCmdPipelineBarrier2(oneTimeCB, &barrierTexInfo);

    chk(vkEndCommandBuffer(oneTimeCB));

    const VkSubmitInfo oneTimeSI{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &oneTimeCB
    };
    chk(vkQueueSubmit(m_device.graphicsQueue().queue, 1, &oneTimeSI, oneTimeFence));
    chk(vkWaitForFences(m_device.handle(), 1, &oneTimeFence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(m_device.handle(), oneTimeFence, nullptr);
    vmaDestroyBuffer(m_device.allocator(), imgSrcBuffer, imgSrcAllocation);

    const VkSamplerCreateInfo samplerCI{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = MAX_ANISOTROPY,
        .maxLod = static_cast<float>(texture->numLevels)
    };
    chk(vkCreateSampler(m_device.handle(), &samplerCI, nullptr, &m_sampler));

    ktxTexture_Destroy(texture);
}

Texture::~Texture()
{
    vkDestroyImageView(m_device.handle(), m_view, nullptr);
    vkDestroySampler(m_device.handle(), m_sampler, nullptr);
    vmaDestroyImage(m_device.allocator(), m_image, m_allocation);
}

} // namespace vulc

