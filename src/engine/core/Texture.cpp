#include "Texture.hpp"

#include "core/CommandPool.hpp"
#include "core/Device.hpp"
#include "utility/Common.hpp"

#include <volk.h>

#include <ktx.h>
#include <ktxvulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <span>
#include <utility>
#include <vector>

namespace vulc
{

Texture::~Texture()
{
    vkDestroyImageView(m_device->handle(), m_view, nullptr);
    vkDestroySampler(m_device->handle(), m_sampler, nullptr);
    vmaDestroyImage(m_device->allocator(), m_image, m_allocation);
}

void Texture::fromFile(Device* device, const std::filesystem::path& filepath)
{
    // NOTE: Step 1 - init internal Texture state
    VULCANO_ASSERT(device != nullptr, "Device needs to be a valid pointer to a vulc::Device");

    m_device = device;

    // NOTE: Step 2 - load the ktx file from disk
    auto ktxLoadResult{ Texture::loadKTXFile(filepath) };
    chk(ktxLoadResult.has_value(), std::format("KTX failed to load texture(from: '{}'): {}", filepath.string(), ktxLoadResult.error())); // FIXME: calling .error() on std::expected that contains an expected value is UB
    auto* texture{ ktxLoadResult->get() };

    auto ktxTextureFormat{ ktxTexture_GetVkFormat(texture) };

    // NOTE: NOT USED. Investigate removal
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(m_device->physicalDevice(), ktxTextureFormat, &formatProperties);

    // NOTE: Step 3 - copy image data from CPU into staging buffer
    VkBuffer imgStagingBuffer{VK_NULL_HANDLE};
    VmaAllocation imgStagingAllocation{VK_NULL_HANDLE};
    const VkBufferCreateInfo imgStagingBufferCI{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = static_cast<std::uint32_t>(texture->dataSize),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    const VmaAllocationCreateInfo imgStagingAllocCI{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    VmaAllocationInfo imgStagingAllocInfo{};
    chk(vmaCreateBuffer(
        m_device->allocator(), &imgStagingBufferCI, &imgStagingAllocCI, &imgStagingBuffer, &imgStagingAllocation, &imgStagingAllocInfo
    ));

    std::memcpy(imgStagingAllocInfo.pMappedData, texture->pData, texture->dataSize);

    std::vector<VkBufferImageCopy> copyRegions{};
    for(auto i{0}; i < texture->numLevels; ++i)
    {
        ktx_size_t mipOffset{0};
        const auto ret{ktxTexture_GetImageOffset(texture, i, 0, 0, &mipOffset)};
        chk(ret == KTX_SUCCESS, "KTX failed to get image offset");

        VkBufferImageCopy bufferCopyRegion{
            .bufferOffset = mipOffset,
            .imageSubresource
            = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = static_cast<std::uint32_t>(i), .baseArrayLayer =0, .layerCount = 1},
            .imageExtent = {.width = std::max(1u, texture->baseWidth >> i),        .height = std::max(1u, texture->baseHeight >> i),        .depth = 1     }
        };
        copyRegions.push_back(std::move(bufferCopyRegion));
    }

    // NOTE: Step 4 - prepare the image to receive the data
    const VkImageCreateInfo texImgCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = ktxTextureFormat,
        .extent = {.width = texture->baseWidth, .height = texture->baseHeight, .depth = 1},
        .mipLevels = texture->numLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    const VmaAllocationCreateInfo texImageAllocCI{.usage = VMA_MEMORY_USAGE_AUTO};
    chk(vmaCreateImage(m_device->allocator(), &texImgCI, &texImageAllocCI, &m_image, &m_allocation, nullptr));

    // NOTE: Step 5 - record image transition command buffer
    VkCommandBuffer oneTimeCmdBuffer{m_device->createCommandBuffer()};

    const VkImageMemoryBarrier2 barrierTexImage{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = m_image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = texture->numLevels, .layerCount = 1}
    };
    VkDependencyInfo barrierTexInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrierTexImage
    };
    vkCmdPipelineBarrier2(oneTimeCmdBuffer, &barrierTexInfo);

    vkCmdCopyBufferToImage(
        oneTimeCmdBuffer,
        imgStagingBuffer,
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
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = m_image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = texture->numLevels, .layerCount = 1}
    };
    barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
    vkCmdPipelineBarrier2(oneTimeCmdBuffer, &barrierTexInfo);

    // NOTE: Step 6 - finish command buffer recording and submit to queue
    m_device->submitCommandBuffer(oneTimeCmdBuffer);

    vmaDestroyBuffer(m_device->allocator(), imgStagingBuffer, imgStagingAllocation);

    // NOTE: Step 7 - create texture utility
    const VkSamplerCreateInfo samplerCI{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = MAX_ANISOTROPY,
        .maxLod = static_cast<float>(texture->numLevels)
    };
    chk(vkCreateSampler(m_device->handle(), &samplerCI, nullptr, &m_sampler));

    const VkImageViewCreateInfo texViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = texImgCI.format,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = texture->numLevels, .layerCount = 1}
    };
    chk(vkCreateImageView(m_device->handle(), &texViewCI, nullptr, &m_view));
}

void Texture::fromBuffer(Device* device, const CommandPool& commandPool, std::span<const std::byte> buffer)
{
    VULCANO_ASSERT(false, "NOT IMPLEMENTED YET");
}

std::expected<Texture::KtxTexturePtr, ktxResult> Texture::loadKTXFile(const std::filesystem::path& filepath) noexcept
{
    if(!std::filesystem::exists(filepath))
        return std::unexpected(KTX_FILE_OPEN_FAILED);

    ktxTexture* texture{nullptr};
    ktxResult result{ktxTexture_CreateFromNamedFile(filepath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture)};

    if(result != KTX_SUCCESS)
        return std::unexpected(result);

    return Texture::KtxTexturePtr{texture, {}};
}

} // namespace vulc

