#ifndef VULCANO_SRC_ENGINE_CORE_TEXTURE_HPP
#define VULCANO_SRC_ENGINE_CORE_TEXTURE_HPP

#include "core/Device.hpp"
#include "utility/Common.hpp"

#include <ktx.h>
#include <ktxvulkan.h>
#include <vk_mem_alloc.h>
#include <volk.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace vulc
{

class Texture
{
public:
    struct Description
    {
        std::uint32_t width{ 0 };
        std::uint32_t height{ 0 };
        std::uint32_t mipLevels{ 0 };
        VkFormat format{ VK_FORMAT_UNDEFINED };
    };

    struct UploadInfo
    {
        std::span<const std::byte> data;
        std::span<const VkBufferImageCopy> copyRegions;
    };

    Texture(const Device* device, Texture::Description description, const Texture::UploadInfo& upload);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;

    [[nodiscard]] VkDescriptorImageInfo descriptorInfo() const noexcept
    {
        return { .sampler = m_sampler, .imageView = m_view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    }

private:
    // TODO: Replace with a regular Buffer instance as soon as a Buffer class was added
    struct StagingBuffer
    {
        VkBuffer buffer{ VK_NULL_HANDLE };
        VmaAllocation allocation{ VK_NULL_HANDLE };
        VmaAllocationInfo allocInfo{};
    };

    static constexpr auto MAX_ANISOTROPY{ 8.f };

    const Device* m_device{ nullptr };
    Texture::Description m_description;

    VmaAllocation m_allocation{ VK_NULL_HANDLE };
    VkImage m_image{ VK_NULL_HANDLE };
    VkImageView m_view{ VK_NULL_HANDLE };
    VkSampler m_sampler{ VK_NULL_HANDLE };

    StagingBuffer createStagingBuffer(std::span<const std::byte> data);
    void createTextureImage();
    void transitionImage(const StagingBuffer& stagingBuffer, const Texture::UploadInfo& uploadInfo);
    void createTextureResources();
};

namespace texture
{

namespace details
{

struct KtxTextureDeleter
{
    void operator()(ktxTexture* texture) const noexcept
    {
        if(texture != nullptr)
            ktxTexture_Destroy(texture);
    }
};
using KtxTexturePtr = std::unique_ptr<ktxTexture, KtxTextureDeleter>;


static inline std::expected<KtxTexturePtr, ktxResult> loadKTXFile(const std::filesystem::path& filepath) noexcept
{
    if(!std::filesystem::exists(filepath))
        return std::unexpected(KTX_FILE_OPEN_FAILED);

    ktxTexture* texture{ nullptr };
    ktxResult result{
        ktxTexture_CreateFromNamedFile(filepath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture)
    };

    if(result != KTX_SUCCESS)
        return std::unexpected(result);

    return KtxTexturePtr{ texture, {} };
}

} // namespace details

static inline std::unique_ptr<Texture> fromFile(Device* device, const std::filesystem::path& filepath)
{
    auto ktxLoadResult{ details::loadKTXFile(filepath) };
    // chk(ktxLoadResult.has_value(), std::format("KTX failed to load texture(from: '{}'): {}", filepath.string(), ktxLoadResult.error())); // FIXME: calling .error() on std::expected that contains an expected value is UB
    chk(
        ktxLoadResult.has_value(), std::format("KTX failed to load texture(from: '{}'): {}", filepath.string(), "")
    ); // FIXME: calling .error() on std::expected that contains an expected value is UB
    auto* texture{ ktxLoadResult->get() };

    std::vector<VkBufferImageCopy> copyRegions{};
    for(auto i{ 0 }; i < texture->numLevels; ++i)
    {
        ktx_size_t mipOffset{ 0 };
        const auto ret{ ktxTexture_GetImageOffset(texture, i, 0, 0, &mipOffset) };
        chk(ret == KTX_SUCCESS, "KTX failed to get image offset");

        VkBufferImageCopy bufferCopyRegion{
            .bufferOffset = mipOffset,
            .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .mipLevel = static_cast<std::uint32_t>(i),
                                 .baseArrayLayer = 0,
                                 .layerCount = 1 },
            .imageExtent = { .width = std::max(1u, texture->baseWidth >> i),
                                 .height = std::max(1u, texture->baseHeight >> i),
                                 .depth = 1 }
        };
        copyRegions.push_back(std::move(bufferCopyRegion));
    }

    Texture::UploadInfo uploadInfo{
        .data = std::span<const std::byte>(reinterpret_cast<const std::byte*>(texture->pData), texture->dataSize),
        .copyRegions = std::span<const VkBufferImageCopy>(copyRegions)
    };

    Texture::Description desc{ .width = texture->baseWidth,
                               .height = texture->baseHeight,
                               .mipLevels = texture->numLevels,
                               .format = ktxTexture_GetVkFormat(texture) };

    return std::make_unique<Texture>(device, std::move(desc), uploadInfo);
}

static inline std::unique_ptr<Texture> fromBuffer(
    Device* device, std::span<const std::byte> buffer, VkFormat format, std::uint32_t texWidth, std::uint32_t texHeight
)
{
    // NOTE: Assumption for buffer image mipmap is that there is just one mipmap layer that is as big as the image
    const std::array<VkBufferImageCopy, 1> bufferCopyRegion{
        { { .bufferOffset = 0,
            .imageSubresource
            = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
            .imageExtent = { .width = texWidth, .height = texHeight, .depth = 1 } } }
    };

    Texture::UploadInfo uploadInfo{ .data = buffer,
                                    .copyRegions = std::span<const VkBufferImageCopy>(bufferCopyRegion) };

    Texture::Description desc{ .width = texWidth, .height = texHeight, .mipLevels = 1, .format = format };

    return std::make_unique<Texture>(device, std::move(desc), uploadInfo);
}

} // namespace texture

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_TEXTURE_HPP
