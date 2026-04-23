#ifndef VULCANO_SRC_ENGINE_CORE_TEXTURE_HPP
#define VULCANO_SRC_ENGINE_CORE_TEXTURE_HPP

#include "core/Device.hpp"

#include <cstddef>
#include <cstdint>
#include <volk.h>

#include <ktx.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <expected>
#include <filesystem>
#include <span>

namespace vulc
{

class Texture
{
public:
    // TODO: Add constructor that takes generic std::span<std::byte> and creates/initializes the Texture class from that.
    // Then only static/global functions are needed as factory functions to make it easier.
    Texture() = default;
    Texture(const Device* device, std::span<const std::byte> data, std::uint32_t width, std::uint32_t height, VkFormat format);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;

    [[nodiscard]] VkDescriptorImageInfo descriptorInfo() const noexcept
    {
        return {.sampler = m_sampler, .imageView = m_view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }

    void fromFile(Device* device, const std::filesystem::path& filepath);
    void fromBuffer(Device* device, std::span<const std::byte> buffer, VkFormat format, std::uint32_t texWidth, std::uint32_t texHeight);

private:
    struct KtxTextureDeleter
    {
        void operator()(ktxTexture* texture) const noexcept
        {
            if(texture != nullptr)
                ktxTexture_Destroy(texture);
        }
    };
    using KtxTexturePtr = std::unique_ptr<ktxTexture, KtxTextureDeleter>;

    static constexpr auto MAX_ANISOTROPY{8.f};

    Device const* m_device{nullptr};
    std::uint32_t m_width{0};
    std::uint32_t m_height{0};
    VkFormat m_format{VK_FORMAT_UNDEFINED};

    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VkImage m_image{VK_NULL_HANDLE};
    VkImageView m_view{VK_NULL_HANDLE};
    VkSampler m_sampler{VK_NULL_HANDLE};

    static std::expected<Texture::KtxTexturePtr, ktxResult> loadKTXFile(const std::filesystem::path& filepath) noexcept;
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_TEXTURE_HPP
