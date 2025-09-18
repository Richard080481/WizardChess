#ifndef __WIZARD_CHESS_H__
#define __WIZARD_CHESS_H__

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <optional>

#include "Model.h"

#include "VulkanSurfaceManager.h"
#include "MemoryTracker.h"

class WizardChess {
public:
    WizardChess(int width, int height) : m_width(width), m_height(height) {}
    ~WizardChess();

    void run();
    void SetFramebufferResized()
    {
        m_framebufferResized = true;
    }

    bool      m_mousePressed      = false;
    float     m_lastMouseX        = 0.0f;
    float     m_lastMouseY        = 0.0f;
    float     m_mouseRotateAngleX = 0.0f;
    float     m_mouseRotateAngleY = 0.0f;
    glm::mat4 m_mouseRotateMat    = glm::mat4(1.0f);

private:
    void     InitVulkan();
    void     MainLoop();
    void     CleanupSwapChain();
    void     Cleanup();
    void     RecreateSwapChain();
    void     CreateRenderPass();
    void     CreateDescriptorSetLayout();
    void     CreateGraphicsPipeline();
    void     CreateFramebuffers();
    void     CreateDepthResources();
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat FindDepthFormat();
    bool     HasStencilComponent(VkFormat format);
    void     CreateTextureImage();
    void     CreateTextureImageView();
    void     CreateTextureSampler();
    void     CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void     TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void     LoadModel();
    void     CreateUniformBuffers();
    void     CreateDescriptorPool();
    void     CreateDescriptorSets();
    void     RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void     CreateSyncObjects();
    void     UpdateUniformBuffer(uint32_t currentImage, int modelIndex);
    void     DrawFrame();

    int m_width;
    int m_height;

    std::vector<VkFramebuffer> m_swapChainFramebuffers;

    VkRenderPass            m_renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout   m_descriptorSetLayout;
    VkPipelineLayout        m_pipelineLayout;
    VkPipeline              m_graphicsPipeline;

    VkImage                 m_depthImage;
    VkDeviceMemory          m_depthImageMemory;
    VkImageView             m_depthImageView;

    VkImage                 m_textureImage;
    VkDeviceMemory          m_textureImageMemory;
    VkImageView             m_textureImageView;
    VkSampler               m_textureSampler;

    std::vector<Model*>     m_models;

    std::vector<VkBuffer>       m_uniformBuffersVs;
    std::vector<VkDeviceMemory> m_uniformBuffersVsMemory;
    std::vector<void*>          m_uniformBuffersVsMapped;

    std::vector<VkBuffer>       m_uniformBuffersFs;
    std::vector<VkDeviceMemory> m_uniformBuffersFsMemory;
    std::vector<void*>          m_uniformBuffersFsMapped;

    VkDescriptorPool             m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets;

    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;
    uint32_t                 m_currentFrame = 0;

    bool m_framebufferResized = false;
};

#endif // __WIZARD_CHESS_H__