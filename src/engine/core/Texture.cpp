#include "Texture.hpp"

#include "core/Device.hpp"
#include "utility/Common.hpp"

#include <ktx.h>
#include <ktxvulkan.h>
#include <vk_mem_alloc.h>
#include <volk.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

namespace vulc
{

Texture::Texture(const Device* device, Texture::Description description, const Texture::UploadInfo& upload)
    : m_device{ device }, m_description(std::move(description))
{
    VULCANO_ASSERT(device != nullptr, "Device needs to be a valid pointer to a vulc::Device");

    // NOTE: Step 3 - copy image data from CPU into staging buffer
    VkBuffer imgStagingBuffer{ VK_NULL_HANDLE };
    VmaAllocation imgStagingAllocation{ VK_NULL_HANDLE };
    const VkBufferCreateInfo imgStagingBufferCI{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                 .size = static_cast<std::uint32_t>(upload.data.size()),
                                                 .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE };
    const VmaAllocationCreateInfo imgStagingAllocCI{ .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                            | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                                     .usage = VMA_MEMORY_USAGE_AUTO };
    VmaAllocationInfo imgStagingAllocInfo{};
    chk(vmaCreateBuffer(
        m_device->allocator(),
        &imgStagingBufferCI,
        &imgStagingAllocCI,
        &imgStagingBuffer,
        &imgStagingAllocation,
        &imgStagingAllocInfo
    ));

    std::memcpy(imgStagingAllocInfo.pMappedData, upload.data.data(), upload.data.size());

    const VkImageCreateInfo texImgCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = m_description.format,
        .extent = { .width = m_description.width, .height = m_description.height, .depth = 1 },
        .mipLevels = m_description.mipLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    const VmaAllocationCreateInfo texImageAllocCI{ .usage = VMA_MEMORY_USAGE_AUTO };
    chk(vmaCreateImage(m_device->allocator(), &texImgCI, &texImageAllocCI, &m_image, &m_allocation, nullptr));

    // NOTE: Step 5 - record image transition command buffer
    VkCommandBuffer oneTimeCmdBuffer{ m_device->createCommandBuffer() };

    const VkImageMemoryBarrier2 barrierTexImage{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = m_image,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = m_description.mipLevels,
                             .layerCount = 1 }
    };
    VkDependencyInfo barrierTexInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                     .imageMemoryBarrierCount = 1,
                                     .pImageMemoryBarriers = &barrierTexImage };
    vkCmdPipelineBarrier2(oneTimeCmdBuffer, &barrierTexInfo);

    vkCmdCopyBufferToImage(
        oneTimeCmdBuffer,
        imgStagingBuffer,
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<std::uint32_t>(upload.copyRegions.size()),
        upload.copyRegions.data()
    );

    const VkImageMemoryBarrier2 barrierTexRead{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = m_image,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = m_description.mipLevels,
                             .layerCount = 1 }
    };
    barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
    vkCmdPipelineBarrier2(oneTimeCmdBuffer, &barrierTexInfo);

    // NOTE: Step 6 - finish command buffer recording and submit to queue
    m_device->submitCommandBuffer(oneTimeCmdBuffer);

    vmaDestroyBuffer(m_device->allocator(), imgStagingBuffer, imgStagingAllocation);

    // NOTE: Step 7 - create texture utility
    const VkSamplerCreateInfo samplerCI{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                         .magFilter = VK_FILTER_LINEAR,
                                         .minFilter = VK_FILTER_LINEAR,
                                         .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                         .anisotropyEnable = VK_TRUE,
                                         .maxAnisotropy = MAX_ANISOTROPY,
                                         .maxLod = static_cast<float>(m_description.mipLevels) };
    chk(vkCreateSampler(m_device->handle(), &samplerCI, nullptr, &m_sampler));

    const VkImageViewCreateInfo texViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = texImgCI.format,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = m_description.mipLevels,
                             .layerCount = 1 }
    };
    chk(vkCreateImageView(m_device->handle(), &texViewCI, nullptr, &m_view));
}

Texture::~Texture()
{
    vkDestroyImageView(m_device->handle(), m_view, nullptr);
    vkDestroySampler(m_device->handle(), m_sampler, nullptr);
    vmaDestroyImage(m_device->allocator(), m_image, m_allocation);
}

} // namespace vulc

