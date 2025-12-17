#ifndef __WIZARD_CHESS_H__
#define __WIZARD_CHESS_H__

#define DUMP_DEPTH_BUFFER 0

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

    static constexpr float RENDER_Z_NEAR = 0.1f;
    static constexpr float RENDER_Z_FAR  = 20.0f;

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
    void     CreateShadowPass();
    void     CreateDescriptorSetLayout();
    void     CreateGraphicsPipelines();
    void     CreateRenderFramebuffer();
    void     CreateShadowFramebuffer();
    void     CreateDepthResourcesRender();
    void     CreateDepthResourcesShadow();
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat FindDepthFormat();
    bool     HasStencilComponent(VkFormat format);
    void     CreateTextureImage();
    void     CreateTextureImageView();
    void     CreateTextureSampler();
	void     CreateShadowMapSampler();
    void     CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void     TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void     LoadModel();
    void     CreateRenderUniformBuffers();
    void     CreateShadowUniformBuffers();
    void     CreateDescriptorPool();
    void     CreateDescriptorSetsRender();
    void     CreateDescriptorSetsShadow();
    void     RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void     RecordRenderCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void     RecordShadowCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void     CreateSyncObjects();
    void     UpdateUniformBuffer(uint32_t currentImage, int modelIndex);
    void     DrawFrame();

    int m_width;
    int m_height;

    std::vector<VkFramebuffer> m_swapChainRenderFramebuffers;
    std::vector<VkFramebuffer> m_swapChainShadowFramebuffers;

    VkRenderPass            m_renderPass = VK_NULL_HANDLE;
    VkRenderPass            m_shadowPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout   m_descriptorSetLayoutRender;
    VkDescriptorSetLayout   m_descriptorSetLayoutShadow;
    VkPipelineLayout        m_pipelineLayoutRender;
    VkPipelineLayout        m_pipelineLayoutShadow;
    VkPipeline              m_graphicsPipelineRender;
    VkPipeline              m_graphicsPipelineShadow;

    VkImage                 m_renderDepthImage;
    VkDeviceMemory          m_renderDepthImageMemory;
    VkImageView             m_renderDepthImageView;

    VkImage                 m_shadowDepthImage;
    VkDeviceMemory          m_shadowDepthImageMemory;
    VkImageView             m_shadowDepthImageView;
    VkSampler               m_shadowDepthSampler;

    VkImage                 m_textureImage;
    VkDeviceMemory          m_textureImageMemory;
    VkImageView             m_textureImageView;
    VkSampler               m_textureSampler;

    std::vector<Model*>     m_models;

    std::vector<VkBuffer>       m_uniformBuffersVsRender;
    std::vector<VkDeviceMemory> m_uniformBuffersVsRenderMemory;
    std::vector<void*>          m_uniformBuffersVsRenderMapped;

    std::vector<VkBuffer>       m_uniformBuffersFsRender;
    std::vector<VkDeviceMemory> m_uniformBuffersFsRenderMemory;
    std::vector<void*>          m_uniformBuffersFsRenderMapped;

    std::vector<VkBuffer>       m_uniformBuffersVsShadow;
    std::vector<VkDeviceMemory> m_uniformBuffersVsShadowMemory;
    std::vector<void*>          m_uniformBuffersVsShadowMapped;

    std::vector<VkBuffer>       m_uniformBuffersFsShadow;
    std::vector<VkDeviceMemory> m_uniformBuffersFsShadowMemory;
    std::vector<void*>          m_uniformBuffersFsShadowMapped;

    VkDescriptorPool             m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSetsRender;
    std::vector<VkDescriptorSet> m_descriptorSetsShadow;

    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;
    uint32_t                 m_currentFrame = 0;

    bool m_framebufferResized = false;
};

#endif // __WIZARD_CHESS_H__