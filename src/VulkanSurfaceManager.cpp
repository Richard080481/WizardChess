#include "VulkanSurfaceManager.h"
#include "VulkanHelper.h"

#include <stdexcept>
#include <cassert>
#include <algorithm>

void VulkanSurfaceManager::InitWindow(int width, int height)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
}

void VulkanSurfaceManager::CreateSurface()
{
    assert(m_window != nullptr);
    assert(m_pDeviceManager != nullptr);

    VkResult result = glfwCreateWindowSurface(m_pDeviceManager->Instance(), m_window, nullptr, &m_surface);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}

void VulkanSurfaceManager::DestroySurface()
{
    if (m_surface != VK_NULL_HANDLE)
    {
        assert(m_pDeviceManager != nullptr);
        vkDestroySurfaceKHR(m_pDeviceManager->Instance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

SwapChainSupportDetails VulkanSurfaceManager::QuerySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details;

    VkSurfaceKHR surface = m_surface;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanSurfaceManager::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR VulkanSurfaceManager::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSurfaceManager::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);

        VkExtent2D actualExtent =
        {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void VulkanSurfaceManager::CreateSwapChain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_pDeviceManager->PhysicalDevice());

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = m_pDeviceManager->FindQueueFamilies(m_pDeviceManager->PhysicalDevice());
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    VkDevice device = m_pDeviceManager->Device();

    assert(m_swapChain == VK_NULL_HANDLE);
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    // Image
    vkGetSwapchainImagesKHR(device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;

    // Image view
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
    {
        m_swapChainImageViews[i] = m_pDeviceManager->CreateImageView(m_swapChainImages[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanSurfaceManager::DestroySwapChain()
{
    VkDevice device = m_pDeviceManager->Device();

    vkDestroySwapchainKHR(device, m_swapChain, nullptr);
    m_swapChain = VK_NULL_HANDLE;
}

void VulkanSurfaceManager::GetGlfwFrameBufferSize(int* pWidth, int* pHeight)
{
    assert(pWidth != nullptr);
    assert(pHeight != nullptr);

    glfwGetFramebufferSize(m_window, pWidth, pHeight);

    while ((*pWidth) == 0 || (*pHeight == 0))
    {
        glfwGetFramebufferSize(m_window, pWidth, pHeight);
        glfwWaitEvents();
    }
}
