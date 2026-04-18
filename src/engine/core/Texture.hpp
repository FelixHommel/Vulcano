#ifndef VULCANO_SRC_ENGINE_CORE_TEXTURE_HPP
#define VULCANO_SRC_ENGINE_CORE_TEXTURE_HPP

#include "core/CommandPool.hpp"
#include "core/Device.hpp"

#include <volk.h>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <filesystem>

namespace vulc
{

class Texture
{
public:
    // FIXME: Remove the commandPool dependency by separating texture resource handling (image, image view, sampler) from the memory allocation
    Texture(const Device& device, const CommandPool& commandPool, const std::filesystem::path& filepath);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;

    [[nodiscard]] VkDescriptorImageInfo descriptorInfo() const noexcept
    {
        return {.sampler = m_sampler, .imageView = m_view, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    }

private:
    static constexpr auto MAX_ANISOTROPY{8.f};

    const Device& m_device;

    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VkImage m_image{VK_NULL_HANDLE};
    VkImageView m_view{VK_NULL_HANDLE};
    VkSampler m_sampler{VK_NULL_HANDLE};
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_TEXTURE_HPP
