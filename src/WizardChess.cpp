#include "WizardChess.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <set>
#include <array>
#include <fstream>
#include <algorithm>
#include <chrono>

#include "Types.h"
#include "Utils.h"
#include "VulkanHelper.h"
#include "VulkanDeviceManager.h"
#include "VulkanSurfaceManager.h"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))

#ifndef MODEL_PATH
#define MODEL_PATH "assets/models/"
#endif // MODEL_PATH

#ifndef TEXTURE_PATH
#define TEXTURE_PATH "assets/textures/"
#endif // TEXTURE_PATH

#ifndef COMPILED_SHADER_ROOT
#define COMPILED_SHADER_ROOT "compiled_shaders/"
#endif // COMPILED_SHADER_ROOT

enum EModel : unsigned int
{
    Cube   = 0,
    King   = 1,
    Queen  = 2,
    Bishop = 3,
    Knight = 4,
    Rook   = 5,
    Pawn   = 6,
};

enum ETexture : unsigned int
{
    ChessBoardWood = 0,
    Oak            = 1,
};

enum EShader : unsigned int
{
    VertRender = 0,
    FragRender = 1,
    VertShadow = 2,
    FragShadow = 3,
};

static inline std::string GetModelPaths(enum EModel index)
{
    static constexpr char* modelFileNames[] =
    {
        "Cube.obj",
        "simplify_King.obj",
        "simplify_Queen.obj",
        "simplify_Bishop.obj",
        "simplify_Knight.obj",
        "simplify_Rook.obj",
        "simplify_Pawn.obj",
    };

    return MODEL_PATH + std::string(modelFileNames[index]);
}

static inline std::string GetTexturePaths(enum ETexture index)
{
    static constexpr char* textureFileNames[] =
    {
        "ChessBoardWood.jpg",
        "oak.jpg",
    };

    return TEXTURE_PATH + std::string(textureFileNames[index]);
}

static inline std::string GetShaderPaths(enum EShader index)
{
    static constexpr char* shaderFileNames[] =
    {
        "vertRender.spv",
        "fragRender.spv",
        "vertShadow.spv",
        "fragShadow.spv",
    };

    return COMPILED_SHADER_ROOT + std::string(shaderFileNames[index]);
}

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> g_deviceExtensions =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const std::vector<const char*> g_validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

WizardChess::~WizardChess()
{
    delete g_pVk;
    g_pVk = nullptr;

    delete g_pMemoryTracker;
    g_pMemoryTracker = nullptr;
}

void WizardChess::run()
{
    InitVulkan();
    MainLoop();
    Cleanup();
}

static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<WizardChess*>(glfwGetWindowUserPointer(window));
    app->SetFramebufferResized();
    // printf("width: %d, height: %d\n", width, height);
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto app = reinterpret_cast<WizardChess*>(glfwGetWindowUserPointer(window));
    // printf("button: %d, action: %d, mods: %d\n", button, action, mods);
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        app->m_mousePressed = (action == GLFW_PRESS);
    }
}

static void MouseMoveCallback(GLFWwindow* window, double xpos, double ypos)
{
    float sensitivity = 0.25f;
    auto app = reinterpret_cast<WizardChess*>(glfwGetWindowUserPointer(window));
    // printf("xpos: %lf, ypos: %lf\n", xpos, ypos);
    if (app->m_mousePressed)
    {
        float dx          = xpos - app->m_lastMouseX;
        float dy          = ypos - app->m_lastMouseY;
        float sensitivity = 0.25f;

        app->m_mouseRotateAngleX += static_cast<float>(dx) * sensitivity;
        app->m_mouseRotateAngleY += static_cast<float>(dy) * sensitivity;

        while (app->m_mouseRotateAngleX > 360.0f) app->m_mouseRotateAngleX -= 360.0f;
        while (app->m_mouseRotateAngleX < 0.0f) app->m_mouseRotateAngleX += 360.0f;
        if (app->m_mouseRotateAngleY > 90.0f) app->m_mouseRotateAngleY = 90.0f;
        if (app->m_mouseRotateAngleY < -30.0f) app->m_mouseRotateAngleY = -30.0f;

        // printf("app->m_mouseRotateAngle (%lf, %lf)\n", app->m_mouseRotateAngleX, app->m_mouseRotateAngleY);

        app->m_mouseRotateMat = glm::mat4(1.0f);
        app->m_mouseRotateMat = glm::rotate(app->m_mouseRotateMat, glm::radians(app->m_mouseRotateAngleY), glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate around X-axis
        app->m_mouseRotateMat = glm::rotate(app->m_mouseRotateMat, glm::radians(app->m_mouseRotateAngleX), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around Y-axis
        // printf("deltaX: %lf, deltaY: %lf\n", deltaX, deltaY);
    }
    app->m_lastMouseX = xpos;
    app->m_lastMouseY = ypos;
}

void WizardChess::InitVulkan()
{
    // Create a VulkanDeviceManager object to manage Vulkan-specific operations.
    g_pVk = new VulkanDeviceManager();

    ///@note GLFW needs to be initialized before creating the Vulkan instance.
    ///      This ensures GLFW performs its internal setups, including platform-specific windowing
    ///      and registering Vulkan extensions required for rendering.
    VK.CreateGlfwWindow(m_width, m_height);

    auto window = VK.SurfaceManager()->Window();

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, MouseMoveCallback);

    // Enable validation layers for debugging and error checking (if enabled).
    // This registers the list of validation layers that will be used.
    VK.EnableValidationLayers(enableValidationLayers, &g_validationLayers);

    // Create the Vulkan instance, which acts as the foundation for all Vulkan operations.
    VK.CreateInstance();

    ///@note The surface must be created before selecting a physical device.
    ///      This ensures the selected device supports the swap chain, which is essential for rendering.
    VK.CreateSurface();

    // Enable device extensions (e.g., swap chain support) before picking the physical device.
    VK.EnableDeviceExtensions(&g_deviceExtensions);

    // Select an appropriate physical device (GPU) that meets the application's requirements.
    VK.PickPhysicalDevice();

    // Create a logical device to interface with the selected physical device.
    VK.CreateLogicalDevice();

    // Set up the swap chain, which handles the presentation of rendered images to the window.
    VK.CreateSwapChain();

    // Create a command pool, which manages the memory for command buffers.
    VK.CreateCommandPool();

    // Allocate command buffers from the command pool for recording rendering commands.
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT); // Resize to match the number of frames in flight.
    VK.CreateCommandBuffers(m_commandBuffers.data(), m_commandBuffers.size());

    // Create the render pass, defining how rendering operations interact with framebuffers.
    CreateRenderPass();

    // Create resources and pipeline for shadow mapping.
    CreateShadowPass();

    // Set up the descriptor set layout, which specifies how shaders access resources like uniforms and textures.
    CreateDescriptorSetLayout();

    // Create the graphics pipeline, which configures shaders, input assembly, viewport, and other rendering states.
    CreateGraphicsPipelines();

    // Create resources for depth buffering, allowing proper handling of 3D object occlusion.
    CreateDepthResourcesRender();
    CreateDepthResourcesShadow();

    // Create framebuffers, which represent the render targets for each swap chain image.
    CreateRenderFramebuffer();
    CreateShadowFramebuffer();

    // Load and create a texture image from file.
    CreateTextureImage();

    // Create a Vulkan image view for the texture, allowing shaders to sample it.
    CreateTextureImageView();

    // Create a sampler for the texture, which defines how the texture is sampled in shaders.
    CreateTextureSampler();

    // Load the 3D model data into memory.
    LoadModel();

    // Create uniform buffers to hold per-frame data like transformation matrices.
    CreateRenderUniformBuffers();
    CreateShadowUniformBuffers();

    // Create a descriptor pool, which allocates resources for descriptor sets.
    CreateDescriptorPool();

    // Allocate and configure descriptor sets, which link Render shaders to resources like textures and buffers.
    CreateDescriptorSetsRender();
    CreateDescriptorSetsShadow();

    // Create synchronization objects (semaphores and fences) to manage rendering and presentation.
    CreateSyncObjects();
}


void WizardChess::MainLoop()
{
    while (!glfwWindowShouldClose(VK.SurfaceManager()->Window()))
    {
        glfwPollEvents();
        DrawFrame();
    }

    vkDeviceWaitIdle(VK.Device());
}

void WizardChess::CleanupSwapChain()
{
    VkDevice device = VK.Device();
    vkDestroyImageView(device, m_renderDepthImageView, nullptr);
    vkDestroyImage(device, m_renderDepthImage, nullptr);
    vkFreeMemory(device, m_renderDepthImageMemory, nullptr);

    for (auto framebuffer : m_swapChainRenderFramebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyImageView(device, m_shadowDepthImageView, nullptr);
    vkDestroyImage(device, m_shadowDepthImage, nullptr);
    vkFreeMemory(device, m_shadowDepthImageMemory, nullptr);

    for (auto framebuffer : m_swapChainShadowFramebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    auto swapChainImageViews = VK.SurfaceManager()->SwapChainImageViews();
    for (auto imageView : swapChainImageViews)
    {
        vkDestroyImageView(device, imageView, nullptr);
    }

    VK.SurfaceManager()->DestroySwapChain();
}

void WizardChess::Cleanup()
{
    CleanupSwapChain();

    VkDevice device = VK.Device();
    vkDestroyPipeline(device, m_graphicsPipelineRender, nullptr);
    vkDestroyPipelineLayout(device, m_pipelineLayoutRender, nullptr);
    vkDestroyPipeline(device, m_graphicsPipelineShadow, nullptr);
    vkDestroyPipelineLayout(device, m_pipelineLayoutShadow, nullptr);
    vkDestroyRenderPass(device, m_renderPass, nullptr);
    vkDestroyRenderPass(device, m_shadowPass, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyBuffer(device, m_uniformBuffersVsRender[i], nullptr);
        vkFreeMemory(device, m_uniformBuffersVsRenderMemory[i], nullptr);

        vkDestroyBuffer(device, m_uniformBuffersFsRender[i], nullptr);
        vkFreeMemory(device, m_uniformBuffersFsRenderMemory[i], nullptr);

        vkDestroyBuffer(device, m_uniformBuffersVsShadow[i], nullptr);
        vkFreeMemory(device, m_uniformBuffersVsShadowMemory[i], nullptr);

        vkDestroyBuffer(device, m_uniformBuffersFsShadow[i], nullptr);
        vkFreeMemory(device, m_uniformBuffersFsShadowMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

    vkDestroySampler(device, m_textureSampler, nullptr);
    vkDestroyImageView(device, m_textureImageView, nullptr);

    vkDestroyImage(device, m_textureImage, nullptr);
    vkFreeMemory(device, m_textureImageMemory, nullptr);

    vkDestroyDescriptorSetLayout(device, m_descriptorSetLayoutRender, nullptr);
    vkDestroyDescriptorSetLayout(device, m_descriptorSetLayoutShadow, nullptr);

    for (Model*& pModel : m_models)
    {
        delete pModel;
        pModel = nullptr;
    }
    m_models.clear();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, m_inFlightFences[i], nullptr);
    }
}

void WizardChess::RecreateSwapChain()
{
    int width = 0, height = 0;
    VK.SurfaceManager()->GetGlfwFrameBufferSize(&width, &height);

    vkDeviceWaitIdle(VK.Device());

    CleanupSwapChain();

    VK.CreateSwapChain();
    CreateDepthResourcesRender();
    CreateRenderFramebuffer();
}

void WizardChess::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format          = VK.SurfaceManager()->SwapChainImageFormat();
    colorAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format          = FindDepthFormat();
    depthAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment   = 0;
    colorAttachmentRef.layout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment   = 1;
    depthAttachmentRef.layout       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass           = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass           = 0;
    dependency.srcStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask        = 0;
    dependency.dstStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount  = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments     = attachments.data();
    renderPassInfo.subpassCount     = 1;
    renderPassInfo.pSubpasses       = &subpass;
    renderPassInfo.dependencyCount  = 1;
    renderPassInfo.pDependencies    = &dependency;

    if (vkCreateRenderPass(VK.Device(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void WizardChess::CreateShadowPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format          = VK.SurfaceManager()->SwapChainImageFormat();
    colorAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format          = FindDepthFormat();
    depthAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment   = 0;
    colorAttachmentRef.layout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment   = 1;
    depthAttachmentRef.layout       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass           = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass           = 0;
    dependency.srcStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask        = 0;
    dependency.dstStageMask         = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount  = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments     = attachments.data();
    renderPassInfo.subpassCount     = 1;
    renderPassInfo.pSubpasses       = &subpass;
    renderPassInfo.dependencyCount  = 1;
    renderPassInfo.pDependencies    = &dependency;

    if (vkCreateRenderPass(VK.Device(), &renderPassInfo, nullptr, &m_shadowPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create shadow pass!");
    }
}

void WizardChess::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboVsLayoutBindingRender{};
    uboVsLayoutBindingRender.binding              = 0;
    uboVsLayoutBindingRender.descriptorCount      = 1;
    uboVsLayoutBindingRender.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboVsLayoutBindingRender.pImmutableSamplers   = nullptr;
    uboVsLayoutBindingRender.stageFlags           = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBindingRender{};
    samplerLayoutBindingRender.binding            = 1;
    samplerLayoutBindingRender.descriptorCount    = 1;
    samplerLayoutBindingRender.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBindingRender.pImmutableSamplers = nullptr;
    samplerLayoutBindingRender.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding uboFsLayoutBindingRender{};
    uboFsLayoutBindingRender.binding              = 2;
    uboFsLayoutBindingRender.descriptorCount      = 1;
    uboFsLayoutBindingRender.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboFsLayoutBindingRender.pImmutableSamplers   = nullptr;
    uboFsLayoutBindingRender.stageFlags           = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 3> renderBindings = { uboVsLayoutBindingRender, samplerLayoutBindingRender, uboFsLayoutBindingRender };

    VkDescriptorSetLayoutCreateInfo layoutInfoRender{};
    layoutInfoRender.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoRender.bindingCount = static_cast<uint32_t>(renderBindings.size());
    layoutInfoRender.pBindings = renderBindings.data();

    if (vkCreateDescriptorSetLayout(VK.Device(), &layoutInfoRender, nullptr, &m_descriptorSetLayoutRender) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    VkDescriptorSetLayoutBinding uboVsLayoutBindingShadow{};
    uboVsLayoutBindingShadow.binding              = 0;
    uboVsLayoutBindingShadow.descriptorCount      = 1;
    uboVsLayoutBindingShadow.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboVsLayoutBindingShadow.pImmutableSamplers   = nullptr;
    uboVsLayoutBindingShadow.stageFlags           = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBindingShadow{};
    samplerLayoutBindingShadow.binding            = 1;
    samplerLayoutBindingShadow.descriptorCount    = 1;
    samplerLayoutBindingShadow.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBindingShadow.pImmutableSamplers = nullptr;
    samplerLayoutBindingShadow.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding uboFsLayoutBindingShadow{};
    uboFsLayoutBindingShadow.binding              = 2;
    uboFsLayoutBindingShadow.descriptorCount      = 1;
    uboFsLayoutBindingShadow.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboFsLayoutBindingShadow.pImmutableSamplers   = nullptr;
    uboFsLayoutBindingShadow.stageFlags           = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 3> shadowBindings = { uboVsLayoutBindingShadow, samplerLayoutBindingShadow, uboFsLayoutBindingShadow };

    VkDescriptorSetLayoutCreateInfo layoutInfoShadow{};
    layoutInfoShadow.sType                        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoShadow.bindingCount                 = static_cast<uint32_t>(shadowBindings.size());
    layoutInfoShadow.pBindings                    = shadowBindings.data();

    if (vkCreateDescriptorSetLayout(VK.Device(), &layoutInfoShadow, nullptr, &m_descriptorSetLayoutShadow) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void WizardChess::CreateGraphicsPipelines()
{
    auto vertRenderCode = ReadFile(GetShaderPaths(EShader::VertRender));
    auto fragRenderCode = ReadFile(GetShaderPaths(EShader::FragRender));
    auto vertShadowCode = ReadFile(GetShaderPaths(EShader::VertShadow));
    auto fragShadowCode = ReadFile(GetShaderPaths(EShader::FragShadow));

    VkShaderModule vertRenderModule = CreateShaderModule(vertRenderCode);
    VkShaderModule fragRenderModule = CreateShaderModule(fragRenderCode);
    VkShaderModule vertShadowModule = CreateShaderModule(vertShadowCode);
    VkShaderModule fragShadowModule = CreateShaderModule(fragShadowCode);

    VkPipelineShaderStageCreateInfo vertRenderStageInfo{};
    vertRenderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertRenderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertRenderStageInfo.module = vertRenderModule;
    vertRenderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragRenderStageInfo{};
    fragRenderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragRenderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragRenderStageInfo.module = fragRenderModule;
    fragRenderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo vertShadowStageInfo{};
    vertShadowStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShadowStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShadowStageInfo.module = vertShadowModule;
    vertShadowStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShadowStageInfo{};
    fragShadowStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShadowStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShadowStageInfo.module = fragShadowModule;
    fragShadowStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo renderStages[] = { vertRenderStageInfo, fragRenderStageInfo };
    VkPipelineShaderStageCreateInfo shadowStages[] = { vertShadowStageInfo, fragShadowStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfoRender{};
    vertexInputInfoRender.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription    = Vertex::GetBindingDescription();
    auto renderAttributeDescriptions = Vertex::GetAttributeDescriptions();

    vertexInputInfoRender.vertexBindingDescriptionCount   = 1;
    vertexInputInfoRender.vertexAttributeDescriptionCount = static_cast<uint32_t>(renderAttributeDescriptions.size());
    vertexInputInfoRender.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfoRender.pVertexAttributeDescriptions    = renderAttributeDescriptions.data();

    VkPipelineVertexInputStateCreateInfo vertexInputInfoShadow{};
    vertexInputInfoShadow.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto shadowAttributeDescriptions = Vertex::GetAttributeDescriptions();

    vertexInputInfoShadow.vertexBindingDescriptionCount   = 1;
    vertexInputInfoShadow.vertexAttributeDescriptionCount = static_cast<uint32_t>(shadowAttributeDescriptions.size());
    vertexInputInfoShadow.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfoShadow.pVertexAttributeDescriptions    = shadowAttributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                    = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable         = VK_FALSE;
    rasterizer.rasterizerDiscardEnable  = VK_FALSE;
    rasterizer.polygonMode              = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth                = 1.0f;
    rasterizer.cullMode                 = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace                = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable          = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable   = VK_FALSE;
    multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable        = VK_TRUE;
    depthStencil.depthWriteEnable       = VK_TRUE;
    depthStencil.depthCompareOp         = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable  = VK_FALSE;
    depthStencil.stencilTestEnable      = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendingRender{};
    colorBlendingRender.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendingRender.logicOpEnable     = VK_FALSE;
    colorBlendingRender.logicOp           = VK_LOGIC_OP_COPY;
    colorBlendingRender.attachmentCount   = 1;
    colorBlendingRender.pAttachments      = &colorBlendAttachment;
    colorBlendingRender.blendConstants[0] = 0.0f;
    colorBlendingRender.blendConstants[1] = 0.0f;
    colorBlendingRender.blendConstants[2] = 0.0f;
    colorBlendingRender.blendConstants[3] = 0.0f;

    VkPipelineColorBlendStateCreateInfo colorBlendingShadow{};
    colorBlendingShadow.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendingShadow.logicOpEnable     = VK_FALSE;
    colorBlendingShadow.logicOp           = VK_LOGIC_OP_COPY;
    colorBlendingShadow.attachmentCount   = 1;
    colorBlendingShadow.pAttachments      = &colorBlendAttachment;
    colorBlendingShadow.blendConstants[0] = 0.0f;
    colorBlendingShadow.blendConstants[1] = 0.0f;
    colorBlendingShadow.blendConstants[2] = 0.0f;
    colorBlendingShadow.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    VkPushConstantRange pushConstantRangeRender{};
    pushConstantRangeRender.offset     = 0;
    pushConstantRangeRender.size       = sizeof(RenderPushConstants);
    pushConstantRangeRender.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPushConstantRange pushConstantRangeShadow{};
    pushConstantRangeShadow.offset     = 0;
    pushConstantRangeShadow.size       = sizeof(ShadowPushConstants);
    pushConstantRangeShadow.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfoRender{};
    pipelineLayoutInfoRender.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfoRender.setLayoutCount         = 1;
    pipelineLayoutInfoRender.pSetLayouts            = &m_descriptorSetLayoutRender;
    pipelineLayoutInfoRender.pushConstantRangeCount = 1;
    pipelineLayoutInfoRender.pPushConstantRanges    = &pushConstantRangeRender;

    VkPipelineLayoutCreateInfo pipelineLayoutInfoShadow{};
    pipelineLayoutInfoShadow.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfoShadow.setLayoutCount         = 1;
    pipelineLayoutInfoShadow.pSetLayouts            = &m_descriptorSetLayoutShadow;
    pipelineLayoutInfoShadow.pushConstantRangeCount = 1;
    pipelineLayoutInfoShadow.pPushConstantRanges    = &pushConstantRangeShadow;

    if (vkCreatePipelineLayout(VK.Device(), &pipelineLayoutInfoRender, nullptr, &m_pipelineLayoutRender) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    if (vkCreatePipelineLayout(VK.Device(), &pipelineLayoutInfoShadow, nullptr, &m_pipelineLayoutShadow) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineRenderInfo{};
    pipelineRenderInfo.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineRenderInfo.stageCount             = 2;
    pipelineRenderInfo.pStages                = renderStages;
    pipelineRenderInfo.pVertexInputState      = &vertexInputInfoRender;
    pipelineRenderInfo.pInputAssemblyState    = &inputAssembly;
    pipelineRenderInfo.pViewportState         = &viewportState;
    pipelineRenderInfo.pRasterizationState    = &rasterizer;
    pipelineRenderInfo.pMultisampleState      = &multisampling;
    pipelineRenderInfo.pDepthStencilState     = &depthStencil;
    pipelineRenderInfo.pColorBlendState       = &colorBlendingRender;
    pipelineRenderInfo.pDynamicState          = &dynamicState;
    pipelineRenderInfo.layout                 = m_pipelineLayoutRender;
    pipelineRenderInfo.renderPass             = m_renderPass;
    pipelineRenderInfo.subpass                = 0;
    pipelineRenderInfo.basePipelineHandle     = VK_NULL_HANDLE;

    VkGraphicsPipelineCreateInfo pipelineShadowInfo{};
    pipelineShadowInfo.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineShadowInfo.stageCount             = 2;
    pipelineShadowInfo.pStages                = shadowStages;
    pipelineShadowInfo.pVertexInputState      = &vertexInputInfoShadow;
    pipelineShadowInfo.pInputAssemblyState    = &inputAssembly;
    pipelineShadowInfo.pViewportState         = &viewportState;
    pipelineShadowInfo.pRasterizationState    = &rasterizer;
    pipelineShadowInfo.pMultisampleState      = &multisampling;
    pipelineShadowInfo.pDepthStencilState     = &depthStencil;
    pipelineShadowInfo.pColorBlendState       = &colorBlendingShadow;
    pipelineShadowInfo.pDynamicState          = &dynamicState;
    pipelineShadowInfo.layout                 = m_pipelineLayoutShadow;
    pipelineShadowInfo.renderPass             = m_shadowPass;
    pipelineShadowInfo.subpass                = 0;
    pipelineShadowInfo.basePipelineHandle     = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(VK.Device(), VK_NULL_HANDLE, 1, &pipelineRenderInfo, nullptr, &m_graphicsPipelineRender) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render graphics pipeline!");
    }

    if (vkCreateGraphicsPipelines(VK.Device(), VK_NULL_HANDLE, 1, &pipelineShadowInfo, nullptr, &m_graphicsPipelineShadow) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create shadow graphics pipeline!");
    }

    vkDestroyShaderModule(VK.Device(), fragRenderModule, nullptr);
    vkDestroyShaderModule(VK.Device(), vertRenderModule, nullptr);
    vkDestroyShaderModule(VK.Device(), fragShadowModule, nullptr);
    vkDestroyShaderModule(VK.Device(), vertShadowModule, nullptr);
}

void WizardChess::CreateRenderFramebuffer()
{
    auto swapChainImageViews = VK.SurfaceManager()->SwapChainImageViews();

    m_swapChainRenderFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments =
        {
            swapChainImageViews[i],
            m_renderDepthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments    = attachments.data();

        auto extent = VK.SurfaceManager()->SwapChainExtent();
        framebufferInfo.width   = extent.width;
        framebufferInfo.height  = extent.height;
        framebufferInfo.layers  = 1;

        if (vkCreateFramebuffer(VK.Device(), &framebufferInfo, nullptr, &m_swapChainRenderFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create Render framebuffer!");
        }
    }
}

void WizardChess::CreateShadowFramebuffer()
{
    auto swapChainImageViews = VK.SurfaceManager()->SwapChainImageViews();

    m_swapChainShadowFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments =
        {
            swapChainImageViews[i],
            m_shadowDepthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = m_shadowPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments    = attachments.data();

        auto extent = VK.SurfaceManager()->SwapChainExtent();
        framebufferInfo.width  = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(VK.Device(), &framebufferInfo, nullptr, &m_swapChainShadowFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create Shadow framebuffer!");
        }
    }
}

void WizardChess::CreateDepthResourcesRender()
{
    VkFormat depthFormat = FindDepthFormat();

    auto extent = VK.SurfaceManager()->SwapChainExtent();
    CreateImage(extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_renderDepthImage, m_renderDepthImageMemory);
    m_renderDepthImageView = VK.CreateImageView(m_renderDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void WizardChess::CreateDepthResourcesShadow()
{
    VkFormat depthFormat = FindDepthFormat();

    auto extent = VK.SurfaceManager()->SwapChainExtent();
    CreateImage(extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_shadowDepthImage, m_shadowDepthImageMemory);
    m_shadowDepthImageView = VK.CreateImageView(m_shadowDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkFormat WizardChess::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(VK.PhysicalDevice(), format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

VkFormat WizardChess::FindDepthFormat()
{
    return FindSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool WizardChess::HasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void WizardChess::CreateTextureImage()
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(GetTexturePaths(ETexture::ChessBoardWood).c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels)
    {
        assert(false);
        throw std::runtime_error("failed to load texture image!");
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    VkDevice device = VK.Device();
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pixels);

    CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_textureImage, m_textureImageMemory);

    TransitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer, m_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    TransitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void WizardChess::CreateTextureImageView()
{
    m_textureImageView = VK.CreateImageView(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void WizardChess::CreateTextureSampler()
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(VK.PhysicalDevice(), &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(VK.Device(), &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void WizardChess::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VkDevice device = VK.Device();

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(VK.PhysicalDevice(), memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

void WizardChess::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = VK.BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if ((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if ((oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) && (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    VK.EndSingleTimeCommands(commandBuffer);
}

void WizardChess::LoadModel()
{
    constexpr EModel firstModelIndex = EModel::Cube;
    constexpr EModel lastModelIndex  = EModel::Pawn;

    float            maxScalePiece   = 0.0f;
    float            maxScaleBoard   = 0.0f;
    for (int i = firstModelIndex; i <= lastModelIndex; i++)
    {
        Model* pModel = new Model(GetModelPaths(static_cast<EModel>(i)));

        if (i == EModel::Cube)
        {
            maxScaleBoard = std::max(maxScaleBoard, pModel->MaxScale());
        }
        else
        {
            maxScalePiece = std::max(maxScalePiece, pModel->MaxScale());
        }
        m_models.push_back(pModel);
    }

    for (int i = 0; i < m_models.size(); i++)
    {
        if (i == EModel::Cube)
        {
            m_models[i]->RescaleNormalizeMatrix(1.0f / maxScaleBoard);
        }
        else
        {
            m_models[i]->RescaleNormalizeMatrix(1.0f / maxScalePiece);
        }
    }

    // We define one cell in the chessboard as 1x1 unit.
    for (int i = 0; i < m_models.size(); i++)
    {
        Model* pModel = m_models[i];

        if (i == EModel::Cube)
        {
            constexpr float chessBoardWidth = 9.5f;
            pModel->Scale(glm::vec3(chessBoardWidth / 2.0f, 0.0001f, chessBoardWidth / 2.0f));
        }
        else
        {
            constexpr float chessPieceWidth = 2.0f;
            pModel->Scale(chessPieceWidth / 2.0f);

            // Move up 1 unit, so the pieces can be put on the board.
            pModel->Translate(glm::vec3(0.0f, 1.0f, 0.0f));

            ///@note Originally the model was along z-axis.
            ///      Rotate -90 degree along x-axis to make it point to the y-axis.
            pModel->Rotate(-90.0f, glm::vec3(1.0f, 0.0f, 0.0f));
        }
    }
}

void WizardChess::CreateRenderUniformBuffers()
{
    VkDeviceSize bufferSizeVs = sizeof(UniformBufferObjectVsRender);
    m_uniformBuffersVsRender.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersVsRenderMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersVsRenderMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateBuffer(bufferSizeVs, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffersVsRender[i], m_uniformBuffersVsRenderMemory[i]);

        vkMapMemory(VK.Device(), m_uniformBuffersVsRenderMemory[i], 0, bufferSizeVs, 0, &m_uniformBuffersVsRenderMapped[i]);
    }

    VkDeviceSize bufferSizeFs = sizeof(UniformBufferObjectFsRender);
    m_uniformBuffersFsRender.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersFsRenderMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersFsRenderMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateBuffer(bufferSizeFs, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffersFsRender[i], m_uniformBuffersFsRenderMemory[i]);

        vkMapMemory(VK.Device(), m_uniformBuffersFsRenderMemory[i], 0, bufferSizeFs, 0, &m_uniformBuffersFsRenderMapped[i]);
    }
}

void WizardChess::CreateShadowUniformBuffers()
{
    VkDeviceSize bufferSizeVs = sizeof(UniformBufferObjectVsShadow);
    m_uniformBuffersVsShadow.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersVsShadowMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersVsShadowMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateBuffer(bufferSizeVs, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffersVsShadow[i], m_uniformBuffersVsShadowMemory[i]);

        vkMapMemory(VK.Device(), m_uniformBuffersVsShadowMemory[i], 0, bufferSizeVs, 0, &m_uniformBuffersVsShadowMapped[i]);
    }

    VkDeviceSize bufferSizeFs = sizeof(UniformBufferObjectFsShadow);
    m_uniformBuffersFsShadow.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersFsShadowMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersFsShadowMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CreateBuffer(bufferSizeFs, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffersFsShadow[i], m_uniformBuffersFsShadowMemory[i]);

        vkMapMemory(VK.Device(), m_uniformBuffersFsShadowMemory[i], 0, bufferSizeFs, 0, &m_uniformBuffersFsShadowMapped[i]);
    }
}

void WizardChess::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2 * 2; // 2 for vs and fs, another 2 for render and shadow
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;     // 2 for render and shadow

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2;           // 2 for render and shadow

    if (vkCreateDescriptorPool(VK.Device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void WizardChess::CreateDescriptorSetsRender()
{
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayoutRender);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts        = layouts.data();

    m_descriptorSetsRender.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(VK.Device(), &allocInfo, m_descriptorSetsRender.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo bufferInfoUboVs{};
        bufferInfoUboVs.buffer = m_uniformBuffersVsRender[i];
        bufferInfoUboVs.offset = 0;
        bufferInfoUboVs.range  = sizeof(UniformBufferObjectVsRender);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = m_textureImageView;
        imageInfo.sampler     = m_textureSampler;

        VkDescriptorBufferInfo bufferInfoUboFs{};
        bufferInfoUboFs.buffer = m_uniformBuffersFsRender[i];
        bufferInfoUboFs.offset = 0;
        bufferInfoUboFs.range  = sizeof(UniformBufferObjectFsRender);

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = m_descriptorSetsRender[i];
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo     = &bufferInfoUboVs;

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = m_descriptorSetsRender[i];
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo      = &imageInfo;

        descriptorWrites[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet          = m_descriptorSetsRender[i];
        descriptorWrites[2].dstBinding      = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo     = &bufferInfoUboFs;
        vkUpdateDescriptorSets(VK.Device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void WizardChess::CreateDescriptorSetsShadow()
{
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayoutShadow);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts        = layouts.data();

    m_descriptorSetsShadow.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(VK.Device(), &allocInfo, m_descriptorSetsShadow.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo bufferInfoUboVs{};
        bufferInfoUboVs.buffer = m_uniformBuffersVsShadow[i];
        bufferInfoUboVs.offset = 0;
        bufferInfoUboVs.range  = sizeof(UniformBufferObjectVsShadow);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = m_textureImageView;
        imageInfo.sampler     = m_textureSampler;

        VkDescriptorBufferInfo bufferInfoUboFs{};
        bufferInfoUboFs.buffer = m_uniformBuffersFsShadow[i];
        bufferInfoUboFs.offset = 0;
        bufferInfoUboFs.range  = sizeof(UniformBufferObjectFsShadow);

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = m_descriptorSetsShadow[i];
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo     = &bufferInfoUboVs;

        descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet          = m_descriptorSetsShadow[i];
        descriptorWrites[1].dstBinding      = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo      = &imageInfo;

        descriptorWrites[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet          = m_descriptorSetsShadow[i];
        descriptorWrites[2].dstBinding      = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo     = &bufferInfoUboFs;
        vkUpdateDescriptorSets(VK.Device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void WizardChess::RecordRenderCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    // Begin recording commands into the command buffer.
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // Get the current swap chain extent for setting up the render area.
    auto swapChainExtent = VK.SurfaceManager()->SwapChainExtent();

    // Configure the render pass begin info.
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = m_renderPass; // The render pass to use.
    renderPassInfo.framebuffer       = m_swapChainRenderFramebuffers[imageIndex]; // Framebuffer for the current swap chain image.
    renderPassInfo.renderArea.offset = { 0, 0 }; // Render area starts at the top-left corner.
    renderPassInfo.renderArea.extent = swapChainExtent; // Render area size matches the swap chain extent.

    // Clear values for the color and depth buffer.
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = { {0.319f, 0.009f, 0.010f, 1.0f} }; // Clear color to a USC Cardinal red in SRGB.
    clearValues[1].depthStencil = { 1.0f, 0 }; // Clear depth to 1.0 and stencil to 0.

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Begin the render pass, specifying that commands will be submitted inline.
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline to the command buffer.
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineRender);

    // Set the viewport, defining the dimensions and depth range of the render area.
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    // Set the scissor rectangle to restrict drawing to the swap chain extent.
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Bind the descriptor set for the current frame, providing shader resources like textures and uniform buffers.
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayoutRender, 0, 1, &m_descriptorSetsRender[m_currentFrame], 0, nullptr);

    // Calculate elapsed time to create a dynamic rotation effect for models.
    // static auto startTime = std::chrono::high_resolution_clock::now();
    // auto currentTime = std::chrono::high_resolution_clock::now();
    // float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    // Create push constants for passing small amounts of dynamic data to shaders.
    RenderPushConstants constants{};

    struct ModelDrawInfo
    {
        EModel    modelIndex;
        glm::vec3 position;
        VkBool32  isWhite;
        VkBool32  useTexture;
    };

    std::vector<ModelDrawInfo> modelDrawInfos =
    {
        // Chessboard
        { EModel::Cube,   glm::vec3(0.0f, 0.0f, 0.0f), VK_FALSE, VK_TRUE},

        // White pieces
        { EModel::Rook,   glm::vec3(-3.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // A1
        { EModel::Knight, glm::vec3(-2.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // B1
        { EModel::Bishop, glm::vec3(-1.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // C1
        { EModel::Queen,  glm::vec3(-0.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // D1
        { EModel::King,   glm::vec3( 0.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // E1
        { EModel::Bishop, glm::vec3( 1.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // F1
        { EModel::Knight, glm::vec3( 2.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // G1
        { EModel::Rook,   glm::vec3( 3.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // H1
        { EModel::Pawn,   glm::vec3(-3.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // A2
        { EModel::Pawn,   glm::vec3(-2.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // B2
        { EModel::Pawn,   glm::vec3(-1.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // C2
        { EModel::Pawn,   glm::vec3(-0.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // D2
        { EModel::Pawn,   glm::vec3( 0.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // E2
        { EModel::Pawn,   glm::vec3( 1.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // F2
        { EModel::Pawn,   glm::vec3( 2.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // G2
        { EModel::Pawn,   glm::vec3( 3.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // H2

        // Black pieces
        { EModel::Rook,   glm::vec3(-3.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // A8
        { EModel::Knight, glm::vec3(-2.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // B8
        { EModel::Bishop, glm::vec3(-1.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // C8
        { EModel::Queen,  glm::vec3(-0.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // D8
        { EModel::King,   glm::vec3( 0.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // E8
        { EModel::Bishop, glm::vec3( 1.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // F8
        { EModel::Knight, glm::vec3( 2.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // G8
        { EModel::Rook,   glm::vec3( 3.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // H8
        { EModel::Pawn,   glm::vec3(-3.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // A7
        { EModel::Pawn,   glm::vec3(-2.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // B7
        { EModel::Pawn,   glm::vec3(-1.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // C7
        { EModel::Pawn,   glm::vec3(-0.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // D7
        { EModel::Pawn,   glm::vec3( 0.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // E7
        { EModel::Pawn,   glm::vec3( 1.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // F7
        { EModel::Pawn,   glm::vec3( 2.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // G7
        { EModel::Pawn,   glm::vec3( 3.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // H7
    };

    // Render each model in the scene.
    for (const auto& modelDrawInfo : modelDrawInfos)
    {
        auto model = m_models[static_cast<int>(modelDrawInfo.modelIndex)];
        // Initialize the model matrix and apply dynamic rotation.
        constants.model = glm::mat4(1.0);
        constants.model *= m_mouseRotateMat;
        constants.model = glm::translate(constants.model, modelDrawInfo.position);

        constants.model = constants.model * model->ModelMatrix();

        // Set the bool for white pieces.
        constants.isWhite = modelDrawInfo.isWhite;

        // Whether to use texture for this model.
        constants.useTexture = modelDrawInfo.useTexture;

        // Bind the vertex buffer for the current model.
        VkBuffer vertexBuffers[] = { model->VertexBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        // Bind the index buffer for the current model.
        vkCmdBindIndexBuffer(commandBuffer, model->IndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // Pass the normalization matrix to the shaders.
        constants.normailzeMatrix = model->NormalizeMatrix();
        vkCmdPushConstants(commandBuffer, m_pipelineLayoutRender, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderPushConstants), &constants);

        // Issue a draw command for the indexed geometry of the model.
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(model->Indices()), 1, 0, 0, 0);
    }

    // End the render pass.
    vkCmdEndRenderPass(commandBuffer);

    // Finalize recording the command buffer.
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void WizardChess::RecordShadowCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    // Begin recording commands into the command buffer.
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording shadow command buffer!");
    }

    // Get the current swap chain extent for setting up the render area.
    auto swapChainExtent = VK.SurfaceManager()->SwapChainExtent();

    // Configure the render pass begin info.
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = m_shadowPass; // The render pass to use.
    renderPassInfo.framebuffer       = m_swapChainShadowFramebuffers[imageIndex]; // Framebuffer for the current swap chain image.
    renderPassInfo.renderArea.offset = { 0, 0 }; // Render area starts at the top-left corner.
    renderPassInfo.renderArea.extent = swapChainExtent; // Render area size matches the swap chain extent.

    // Clear values for the color and depth buffer.
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = { {0.319f, 0.009f, 0.010f, 1.0f} }; // Clear color to a USC Cardinal red in SRGB.
    clearValues[1].depthStencil = { 1.0f, 0 }; // Clear depth to 1.0 and stencil to 0.

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    // Begin the render pass, specifying that commands will be submitted inline.
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline to the command buffer.
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineShadow);

    // Set the viewport, defining the dimensions and depth range of the render area.
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float)swapChainExtent.width;
    viewport.height   = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    // Set the scissor rectangle to restrict drawing to the swap chain extent.
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Bind the descriptor set for the current frame, providing shader resources like textures and uniform buffers.
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayoutShadow, 0, 1, &m_descriptorSetsShadow[m_currentFrame], 0, nullptr);

    // Calculate elapsed time to create a dynamic rotation effect for models.
    // static auto startTime = std::chrono::high_resolution_clock::now();
    // auto currentTime = std::chrono::high_resolution_clock::now();
    // float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    // Create push constants for passing small amounts of dynamic data to shaders.
    ShadowPushConstants constants{};

    struct ModelDrawInfo
    {
        EModel    modelIndex;
        glm::vec3 position;
        VkBool32  isWhite;
        VkBool32  useTexture;
    };

    std::vector<ModelDrawInfo> modelDrawInfos =
    {
        // Chessboard
        { EModel::Cube,   glm::vec3(0.0f, 0.0f, 0.0f), VK_FALSE, VK_TRUE},

        // White pieces
        { EModel::Rook,   glm::vec3(-3.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // A1
        { EModel::Knight, glm::vec3(-2.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // B1
        { EModel::Bishop, glm::vec3(-1.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // C1
        { EModel::Queen,  glm::vec3(-0.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // D1
        { EModel::King,   glm::vec3( 0.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // E1
        { EModel::Bishop, glm::vec3( 1.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // F1
        { EModel::Knight, glm::vec3( 2.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // G1
        { EModel::Rook,   glm::vec3( 3.5f, 0.0f, 3.5f), VK_TRUE, VK_FALSE }, // H1
        { EModel::Pawn,   glm::vec3(-3.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // A2
        { EModel::Pawn,   glm::vec3(-2.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // B2
        { EModel::Pawn,   glm::vec3(-1.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // C2
        { EModel::Pawn,   glm::vec3(-0.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // D2
        { EModel::Pawn,   glm::vec3( 0.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // E2
        { EModel::Pawn,   glm::vec3( 1.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // F2
        { EModel::Pawn,   glm::vec3( 2.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // G2
        { EModel::Pawn,   glm::vec3( 3.5f, 0.0f, 2.5f), VK_TRUE, VK_FALSE }, // H2

        // Black pieces
        { EModel::Rook,   glm::vec3(-3.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // A8
        { EModel::Knight, glm::vec3(-2.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // B8
        { EModel::Bishop, glm::vec3(-1.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // C8
        { EModel::Queen,  glm::vec3(-0.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // D8
        { EModel::King,   glm::vec3( 0.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // E8
        { EModel::Bishop, glm::vec3( 1.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // F8
        { EModel::Knight, glm::vec3( 2.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // G8
        { EModel::Rook,   glm::vec3( 3.5f, 0.0f,-3.5f), VK_FALSE, VK_FALSE }, // H8
        { EModel::Pawn,   glm::vec3(-3.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // A7
        { EModel::Pawn,   glm::vec3(-2.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // B7
        { EModel::Pawn,   glm::vec3(-1.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // C7
        { EModel::Pawn,   glm::vec3(-0.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // D7
        { EModel::Pawn,   glm::vec3( 0.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // E7
        { EModel::Pawn,   glm::vec3( 1.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // F7
        { EModel::Pawn,   glm::vec3( 2.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // G7
        { EModel::Pawn,   glm::vec3( 3.5f, 0.0f,-2.5f), VK_FALSE, VK_FALSE }, // H7
    };

    // Render each model in the scene.
    for (const auto& modelDrawInfo : modelDrawInfos)
    {
        auto model = m_models[static_cast<int>(modelDrawInfo.modelIndex)];
        // Initialize the model matrix and apply dynamic rotation.
        constants.model = glm::mat4(1.0);
        constants.model *= m_mouseRotateMat;
        constants.model = glm::translate(constants.model, modelDrawInfo.position);

        constants.model = constants.model * model->ModelMatrix();

        // Set the bool for white pieces.
        constants.isWhite = modelDrawInfo.isWhite;

        // Whether to use texture for this model.
        constants.useTexture = modelDrawInfo.useTexture;

        // Bind the vertex buffer for the current model.
        VkBuffer vertexBuffers[] = { model->VertexBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        // Bind the index buffer for the current model.
        vkCmdBindIndexBuffer(commandBuffer, model->IndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // Pass the normalization matrix to the shaders.
        constants.normailzeMatrix = model->NormalizeMatrix();
        vkCmdPushConstants(commandBuffer, m_pipelineLayoutShadow, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShadowPushConstants), &constants);

        // Issue a draw command for the indexed geometry of the model.
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(model->Indices()), 1, 0, 0, 0);
    }

    // End the render pass.
    vkCmdEndRenderPass(commandBuffer);

    // Finalize recording the command buffer.
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void WizardChess::CreateSyncObjects()
{
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(VK.Device(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(VK.Device(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(VK.Device(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void WizardChess::UpdateUniformBuffer(uint32_t currentImage, int modelIndex)
{
    auto swapChainExtent = VK.SurfaceManager()->SwapChainExtent();

    glm::vec3 eye = glm::vec3(0.0f, 8.0f, 10.0f);

    UniformBufferObjectVsRender uboVsRender{};
    uboVsRender.view = glm::lookAt(
        eye,                          // eye
        glm::vec3(0.0f, -0.5f, 0.0f), // target
        glm::vec3(0.0f, 1.0f, 0.0f)); // up vector
    uboVsRender.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 20.0f);

    // Vulkan's y-axis is pointing downwards.
    uboVsRender.proj[1][1] *= -1;

    memcpy(m_uniformBuffersVsRenderMapped[currentImage], &uboVsRender, sizeof(uboVsRender));

    UniformBufferObjectFsRender uboFsRender{};

    uboFsRender.lightPos   = glm::vec3(-5.0, 0.0, 0.0);
    uboFsRender.lightColor = glm::vec3( 1.0, 1.0, 1.0);
    uboFsRender.cameraPos  = eye;

    memcpy(m_uniformBuffersFsRenderMapped[currentImage], &uboFsRender, sizeof(uboFsRender));

    UniformBufferObjectVsShadow uboVsShadow{};
    uboVsShadow.view = glm::lookAt(
        eye,                          // eye
        glm::vec3(0.0f, -0.5f, 0.0f), // target
        glm::vec3(0.0f, 1.0f, 0.0f)); // up vector
    uboVsShadow.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 20.0f);

    // Vulkan's y-axis is pointing downwards.
    uboVsShadow.proj[1][1] *= -1;

    memcpy(m_uniformBuffersVsShadowMapped[currentImage], &uboVsShadow, sizeof(uboVsShadow));

    UniformBufferObjectFsShadow uboFsShadow{};

    uboFsShadow.lightPos = glm::vec3(-5.0, 0.0, 0.0);
    uboFsShadow.lightColor = glm::vec3(1.0, 1.0, 1.0);
    uboFsShadow.cameraPos = eye;

    memcpy(m_uniformBuffersFsShadowMapped[currentImage], &uboFsShadow, sizeof(uboFsShadow));
}

void WizardChess::DrawFrame()
{
    vkWaitForFences(VK.Device(), 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    VkSwapchainKHR swapChain = VK.SurfaceManager()->SwapChain();

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(VK.Device(), swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    UpdateUniformBuffer(m_currentFrame, 0);

    vkResetFences(VK.Device(), 1, &m_inFlightFences[m_currentFrame]);

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    RecordRenderCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);
    //RecordShadowCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);


    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

    VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(VK.GraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(VK.PresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized)
    {
        m_framebufferResized = false;
        RecreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
