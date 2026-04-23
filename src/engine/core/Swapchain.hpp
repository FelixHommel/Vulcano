#ifndef VULCANO_SRC_ENGINE_CORE_SWAPCHAIN_HPP
#define VULCANO_SRC_ENGINE_CORE_SWAPCHAIN_HPP

#include "core/Device.hpp"
#include "core/Window.hpp"

#include <volk.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <vector>

namespace vulc
{

struct SwapchainImageFormats
{
    VkFormat colorAttachmentFormat{ VK_FORMAT_UNDEFINED };
    VkFormat depthAttachmentFormat{ VK_FORMAT_UNDEFINED };
};

class Swapchain
{
public:
    Swapchain(const Device& device, Window& window);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    [[nodiscard]] VkSwapchainKHR handle() const noexcept { return m_swapchain; }
    [[nodiscard]] VkSwapchainKHR handle() noexcept { return m_swapchain; }

    [[nodiscard]] SwapchainImageFormats imageFormats() const noexcept
    {
        return { .colorAttachmentFormat = SWAPCHAIN_IMAGE_FORMAT, .depthAttachmentFormat = m_depthFormat };
    }
    [[nodiscard]] VkExtent2D extent() const noexcept { return m_swapchainExtent; }
    [[nodiscard]] const std::vector<VkImage>& images() const noexcept { return m_swapchainImages; }
    [[nodiscard]] const std::vector<VkImageView>& imageViews() const noexcept { return m_swapchainImageViews; }
    [[nodiscard]] VkImage depthImage() const noexcept { return m_depthImage; }
    [[nodiscard]] VkImageView depthImageView() const noexcept { return m_depthImageView; }

    void recreate(Window& window);

private:
    static constexpr auto WAYLAND_SURFACE_FULLSCREEN_SPECIAL_VALUE{ 0xFFFFFFFF };
    static constexpr auto SWAPCHAIN_IMAGE_FORMAT{ VK_FORMAT_B8G8R8A8_SRGB };

    const Device& m_device;

    VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
    VkSurfaceCapabilitiesKHR m_surfaceCapabilities{};
    VkExtent2D m_swapchainExtent{};

    VkSwapchainKHR m_swapchain{ VK_NULL_HANDLE };

    std::uint32_t m_swapchainImageCount{ 0 };
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;

    VkFormat m_depthFormat{ VK_FORMAT_UNDEFINED };
    VkImage m_depthImage{ VK_NULL_HANDLE };
    VmaAllocation m_depthImageAllocation{ VK_NULL_HANDLE };
    VkImageView m_depthImageView{ VK_NULL_HANDLE };

    void createSurface(Window& window, bool recreate = false);
    void createSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void createColorAttachment();
    void createDepthAttachment();

    void chooseDepthFormat();
    void destroySwapchainImages();
};

} // namespace vulc

#endif // !VULCANO_SRC_ENGINE_CORE_SWAPCHAIN_HPP
