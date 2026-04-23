#include "Swapchain.hpp"

#include "core/Device.hpp"
#include "core/Window.hpp"
#include "utility/Common.hpp"

#include <SDL3/SDL_video.h>
#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <vector>

namespace vulc
{

Swapchain::Swapchain(const Device& device, Window& window) : m_device{ device }
{
    createSurface(window);
    createSwapchain();
    createColorAttachment();
    createDepthAttachment();
}

Swapchain::~Swapchain()
{
    destroySwapchainImages();

    vkDestroySwapchainKHR(m_device.handle(), m_swapchain, nullptr);
    vkDestroySurfaceKHR(m_device.instance(), m_surface, nullptr);
}

void Swapchain::recreate(Window& window)
{
    VkSwapchainKHR oldSwapchain{ m_swapchain };
    destroySwapchainImages();

    createSurface(window, true);
    createSwapchain(oldSwapchain);
    vkDestroySwapchainKHR(m_device.handle(), oldSwapchain, nullptr);

    createColorAttachment();
    createDepthAttachment();
}

void Swapchain::createSurface(Window& window, bool recreate)
{
    if(!recreate)
        chk(SDLResult{ SDL_Vulkan_CreateSurface(window.handle(), m_device.instance(), nullptr, &m_surface) });

    int windowWidth{ 0 };
    int windowHeight{ 0 };
    chk(SDLResult{ SDL_GetWindowSize(window.handle(), &windowWidth, &windowHeight) });

    chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.physicalDevice(), m_surface, &m_surfaceCapabilities));

    m_swapchainExtent = m_surfaceCapabilities.currentExtent;
    if(m_surfaceCapabilities.currentExtent.width == WAYLAND_SURFACE_FULLSCREEN_SPECIAL_VALUE)
        m_swapchainExtent
            = { .width = static_cast<std::uint32_t>(windowWidth), .height = static_cast<std::uint32_t>(windowHeight) };
}

void Swapchain::createSwapchain(VkSwapchainKHR oldSwapchain)
{
    const VkSwapchainCreateInfoKHR swapchainCI{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_surface,
        .minImageCount = m_surfaceCapabilities.minImageCount,
        .imageFormat = SWAPCHAIN_IMAGE_FORMAT,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = { .width = m_swapchainExtent.width, .height = m_swapchainExtent.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR, // TODO: Implement better algorithm to determine present mode
        .oldSwapchain = oldSwapchain
    };
    chk(vkCreateSwapchainKHR(m_device.handle(), &swapchainCI, nullptr, &m_swapchain));
}

void Swapchain::createColorAttachment()
{
    chk(vkGetSwapchainImagesKHR(m_device.handle(), m_swapchain, &m_swapchainImageCount, nullptr));
    m_swapchainImages.resize(m_swapchainImageCount);
    m_swapchainImageViews.resize(m_swapchainImageCount);
    chk(vkGetSwapchainImagesKHR(m_device.handle(), m_swapchain, &m_swapchainImageCount, m_swapchainImages.data()));

    VkImageViewCreateInfo viewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = SWAPCHAIN_IMAGE_FORMAT,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };
    for(auto i{ 0 }; i < m_swapchainImageCount; ++i)
    {
        viewCI.image = m_swapchainImages[i];
        chk(vkCreateImageView(m_device.handle(), &viewCI, nullptr, &m_swapchainImageViews[i]));
    }
}

void Swapchain::createDepthAttachment()
{
    chooseDepthFormat();

    const VkImageCreateInfo depthImageCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = m_depthFormat,
        .extent = { .width = m_swapchainExtent.width, .height = m_swapchainExtent.height, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    const VmaAllocationCreateInfo allocCI{ .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                           .usage = VMA_MEMORY_USAGE_AUTO };
    chk(vmaCreateImage(m_device.allocator(), &depthImageCI, &allocCI, &m_depthImage, &m_depthImageAllocation, nullptr));

    const VkImageViewCreateInfo depthImageViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_depthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_depthFormat,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
    };
    chk(vkCreateImageView(m_device.handle(), &depthImageViewCI, nullptr, &m_depthImageView));
}

void Swapchain::chooseDepthFormat()
{
    const std::vector<VkFormat> depthFormatList{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    for(const auto& format : depthFormatList)
    {
        VkFormatProperties2 formatProperties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
        vkGetPhysicalDeviceFormatProperties2(m_device.physicalDevice(), format, &formatProperties);

        if((formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
           != 0u)
        {
            m_depthFormat = format;
            break;
        }
    }
    VULCANO_ASSERT(m_depthFormat != VK_FORMAT_UNDEFINED, "Could not find a compatible swapchain depth image format");
}

void Swapchain::destroySwapchainImages()
{
    if(m_depthImage != VK_NULL_HANDLE)
    {
        vmaDestroyImage(m_device.allocator(), m_depthImage, m_depthImageAllocation);
        m_depthImage = VK_NULL_HANDLE;
        m_depthImageAllocation = VK_NULL_HANDLE;
    }
    if(m_depthImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_device.handle(), m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }

    for(auto i{ 0 }; i < m_swapchainImageViews.size(); ++i)
        vkDestroyImageView(m_device.handle(), m_swapchainImageViews[i], nullptr);

    m_swapchainImageViews.clear();
    m_swapchainImages.clear();
}

} // namespace vulc

