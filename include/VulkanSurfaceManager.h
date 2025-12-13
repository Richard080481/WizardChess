#ifndef __VULKAN_SURFACE_MANAGER_H__
#define __VULKAN_SURFACE_MANAGER_H__

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VulkanDeviceManager.h"

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanDeviceManager;

class VulkanSurfaceManager
{
public:
    VulkanSurfaceManager(VulkanDeviceManager* pDeviceManager) : m_pDeviceManager(pDeviceManager) {}
    ~VulkanSurfaceManager()
    {
        DestroySurface();
    }

    void InitWindow(int width, int height);
    void CreateSurface();
    void DestroySurface();

    SwapChainSupportDetails         QuerySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR              ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR                ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D                      ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void                            CreateSwapChain();
    void                            DestroySwapChain();

    GLFWwindow*                     Window()  const { return m_window; }
    VkSurfaceKHR                    Surface() const { return m_surface; }

    VkSwapchainKHR                  SwapChain()            const { return m_swapChain; }
    const VkExtent2D                SwapChainExtent()      const { return m_swapChainExtent; }
    const std::vector<VkImage>&     SwapChainImages()      const { return m_swapChainImages; }
    VkFormat                        SwapChainImageFormat() const { return m_swapChainImageFormat; }
    const std::vector<VkImageView>& SwapChainImageViews()  const { return m_swapChainImageViews; }

    constexpr VkExtent2D            ShadowMapExtent()      const { return VkExtent2D{ 1024, 1024 }; }

    void GetGlfwFrameBufferSize(int* pWidth, int* pHeight);
private:
    VulkanDeviceManager*     m_pDeviceManager = nullptr;

    VkSurfaceKHR             m_surface = VK_NULL_HANDLE;
    GLFWwindow*              m_window  = nullptr;

    VkSwapchainKHR           m_swapChain            = VK_NULL_HANDLE;
    VkExtent2D               m_swapChainExtent      = {};
    std::vector<VkImage>     m_swapChainImages;
    VkFormat                 m_swapChainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImageView> m_swapChainImageViews;
};

#endif // __VULKAN_SURFACE_MANAGER_H__
