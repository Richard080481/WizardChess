#include "WizardChess.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <cmath>

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
#include "BmpUtils.h"
#include "Debug.h"
#include "VulkanHelper.h"
#include "VulkanDeviceManager.h"
#include "VulkanSurfaceManager.h"

#define ROTATE_WORLD false
#define ROTATE_LIGHT false

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

enum EPieceColor : unsigned int
{
    White = 0,
    Black = 1,
};

enum ETexture : unsigned int
{
    ChessBoardWood = 0,
    Oak            = 1,
};

enum EShader : unsigned int
{
    Vert       = 0,
    Frag       = 1,
    ShadowVert = 2,
    ShadowFrag = 3,
    SelectVert = 4,
    SelectFrag = 5,
};

static inline std::string GetModelPaths(enum EModelName index)
{
    static constexpr char* modelFileNames[] =
    {
        "Cube.obj",
        "simplify_Bishop.obj",
        "simplify_King.obj",
        "simplify_Knight.obj",
        "simplify_Rook.obj",
        "simplify_Pawn.obj",
        "simplify_Queen.obj",
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
        "vert.spv",
        "frag.spv",
        "shadow_vert.spv",
        "shadow_frag.spv",
        "select_vert.spv",
        "select_frag.spv",
    };

    return COMPILED_SHADER_ROOT + std::string(shaderFileNames[index]);
}

struct ChessPieceInfo
{
    char*       m_pieceName;
    char        m_file;
    char        m_rank;
    EPieceColor m_color;
    EModelName  m_modelName;

    std::string PositionStr() const
    {
        return std::to_string(m_file) + std::to_string(m_rank);
    }

    glm::vec3 PositionVec3() const
    {
        return glm::vec3(float(m_file - 'A'), 0.0f, -float(m_rank - '1'));
    }
};

static constexpr ChessPieceInfo chessPieces[] =
{
    {"A1 Rook",   'A', '1', EPieceColor::White, EModelName::Rook},
    {"B1 Knight", 'B', '1', EPieceColor::White, EModelName::Knight},
    {"C1 Bishop", 'C', '1', EPieceColor::White, EModelName::Bishop},
    {"D1 Queen",  'D', '1', EPieceColor::White, EModelName::Queen},
    {"E1 King",   'E', '1', EPieceColor::White, EModelName::King},
    {"F1 Bishop", 'F', '1', EPieceColor::White, EModelName::Bishop},
    {"G1 Knight", 'G', '1', EPieceColor::White, EModelName::Knight},
    {"H1 Rook",   'H', '1', EPieceColor::White, EModelName::Rook},
    {"A2 Pawn",   'A', '2', EPieceColor::White, EModelName::Pawn},
    {"B2 Pawn",   'B', '2', EPieceColor::White, EModelName::Pawn},
    {"C2 Pawn",   'C', '2', EPieceColor::White, EModelName::Pawn},
    {"D2 Pawn",   'D', '2', EPieceColor::White, EModelName::Pawn},
    {"E2 Pawn",   'E', '2', EPieceColor::White, EModelName::Pawn},
    {"F2 Pawn",   'F', '2', EPieceColor::White, EModelName::Pawn},
    {"G2 Pawn",   'G', '2', EPieceColor::White, EModelName::Pawn},
    {"H2 Pawn",   'H', '2', EPieceColor::White, EModelName::Pawn},

    {"A8 Rook",   'A', '8', EPieceColor::Black, EModelName::Rook},
    {"B8 Knight", 'B', '8', EPieceColor::Black, EModelName::Knight},
    {"C8 Bishop", 'C', '8', EPieceColor::Black, EModelName::Bishop},
    {"D8 Queen",  'D', '8', EPieceColor::Black, EModelName::Queen},
    {"E8 King",   'E', '8', EPieceColor::Black, EModelName::King},
    {"F8 Bishop", 'F', '8', EPieceColor::Black, EModelName::Bishop},
    {"G8 Knight", 'G', '8', EPieceColor::Black, EModelName::Knight},
    {"H8 Rook",   'H', '8', EPieceColor::Black, EModelName::Rook},
    {"A7 Pawn",   'A', '7', EPieceColor::Black, EModelName::Pawn},
    {"B7 Pawn",   'B', '7', EPieceColor::Black, EModelName::Pawn},
    {"C7 Pawn",   'C', '7', EPieceColor::Black, EModelName::Pawn},
    {"D7 Pawn",   'D', '7', EPieceColor::Black, EModelName::Pawn},
    {"E7 Pawn",   'E', '7', EPieceColor::Black, EModelName::Pawn},
    {"F7 Pawn",   'F', '7', EPieceColor::Black, EModelName::Pawn},
    {"G7 Pawn",   'G', '7', EPieceColor::Black, EModelName::Pawn},
    {"H7 Pawn",   'H', '7', EPieceColor::Black, EModelName::Pawn},
};

struct UniformBufferObjectVs
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 lightView;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 lightProj;
};

#pragma warning (push)
#pragma warning (disable: 4324)
struct UniformBufferObjectFs
{
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec3 lightColor;
    alignas(16) glm::vec3 cameraPos;
};
#pragma warning (pop)

struct UniformBufferObjectShadowVs
{
    alignas(16) glm::mat4 lightView;
    alignas(16) glm::mat4 lightProj;
};

struct UniformBufferObjectSelectionMapVs
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

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

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    WC_UNUSED_PARAMETER(width);
    WC_UNUSED_PARAMETER(height);
    auto app = reinterpret_cast<WizardChess*>(glfwGetWindowUserPointer(window));
    app->SetFramebufferResized();
}

// Callback function to handle mouse button events
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    WC_UNUSED_PARAMETER(mods);

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    auto app = reinterpret_cast<WizardChess*>(glfwGetWindowUserPointer(window));
    app->MouseButtonCallback(button, (float)mouseX, (float)mouseY, action);
}

void WizardChess::MouseButtonCallback(int button, float mouseX, float mouseY, int action)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            int file;
            int rank;
            ReadSelectionMap(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (uint32_t)mouseX, (uint32_t)mouseY, &file, &rank);
            ToggleSelectedFileRank(file, rank);

#if DEBUG
            if ((file != 0) && (rank != 0))
            {
                printf("%c%c\n", file - 1 + 'A', rank + '0');
            }
#endif // #if DEBUG
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            m_mouseDragging = true;
            m_mouseLastX    = mouseX;
            m_mouseLastY    = mouseY;
        }
        else if (action == GLFW_RELEASE)
        {
            m_mouseDragging = false;
        }
    }
}

static void cursorPosCallback(GLFWwindow* window, double mouseX, double mouseY)
{
    auto app = reinterpret_cast<WizardChess*>(glfwGetWindowUserPointer(window));

    app->CursorPosCallback((float)mouseX, (float)mouseY);
}

void WizardChess::CursorPosCallback(float mouseX, float mouseY)
{
    if (m_mouseDragging)
    {
        double dx = mouseX - m_mouseLastX;
        double dy = mouseY - m_mouseLastY;

        m_mouseLastX = mouseX;
        m_mouseLastY = mouseY;

        float sensitivity = 0.25f;
        m_mouseRotateAngleX += static_cast<float>(dx) * sensitivity;
        m_mouseRotateAngleY += static_cast<float>(dy) * sensitivity;

        while (m_mouseRotateAngleX > 360.0f) m_mouseRotateAngleX -= 360.0f;
        while (m_mouseRotateAngleX < 0.0f) m_mouseRotateAngleX += 360.0f;
        if (m_mouseRotateAngleY > 90.0f) m_mouseRotateAngleY = 90.0f;
        if (m_mouseRotateAngleY < 10.0f) m_mouseRotateAngleY = 10.0f;

        UpdateMouseRotateMat();
#if DEBUG
        printf("mouseX: %f, mouseY: %f, ", mouseX, mouseY);
        printf("m_mouseRotateAngleX: %f, m_mouseRotateAngleY: %f, ", m_mouseRotateAngleX, m_mouseRotateAngleY);
        printf("dx: %f, dy: %f\n", dx, dy);

        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                printf("%6.4f ", m_mouseRotateMat[i][j]);
            }
            printf("\n");
        }
#endif // #if DEBUG
    }
}

static void scrollCallback(GLFWwindow* window, double offsetX, double offsetY)
{
    auto app = reinterpret_cast<WizardChess*>(glfwGetWindowUserPointer(window));
    app->ScrollCallback(offsetX, offsetY);
}

void WizardChess::ScrollCallback(double offsetX, double offsetY)
{
    WC_UNUSED_PARAMETER(offsetX);
    m_mouseZoom *= pow(1.1f, static_cast<float>(offsetY)); // Adjust zoom speed
    m_mouseZoom = glm::clamp(m_mouseZoom, 0.9f, 1.3f); // Prevent extreme zoom
}

void WizardChess::UpdateMouseRotateMat()
{
    m_mouseRotateMat = glm::mat4(1.0f);
    m_mouseRotateMat = glm::rotate(m_mouseRotateMat, glm::radians(m_mouseRotateAngleY), glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate around X-axis
    m_mouseRotateMat = glm::rotate(m_mouseRotateMat, glm::radians(m_mouseRotateAngleX), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around Y-axis
}

void WizardChess::InitVulkan()
{
    // Create a VulkanDeviceManager object to manage Vulkan-specific operations.
    g_pVk = new VulkanDeviceManager();

    ///@note GLFW needs to be initialized before creating the Vulkan instance.
    ///      This ensures GLFW performs its internal setups, including platform-specific windowing
    ///      and registering Vulkan extensions required for rendering.
    VK.CreateGlfwWindow(m_width, m_height);

    // Set glfw window callback functions.
    VulkanSurfaceManager* pSurfaceManager = VK.SurfaceManager();
    pSurfaceManager->SetWindowUserPointer(this);
    pSurfaceManager->SetFramebufferSizeCallback(framebufferResizeCallback);
    pSurfaceManager->SetMouseButtonCallback(mouseButtonCallback);
    pSurfaceManager->SetCursorPosCallback(cursorPosCallback);
    pSurfaceManager->SetScrollCallback(scrollCallback);

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
    VK.CreateCommandBuffers(m_commandBuffers.data(), static_cast<uint32_t>(m_commandBuffers.size()));

    // Create the render pass, defining how rendering operations interact with framebuffers.
    CreateRenderPass();

    // Set up the descriptor set layout, which specifies how shaders access resources like uniforms and textures.
    CreateDescriptorSetLayout();

    // Create pipelines
    CreatePipelines();

    // Create resources
    CreateResources();

    // Create framebuffers
    CreateFramebuffers();

    // Load and create a texture image from file.
    CreateTextureImage();

    // Create a Vulkan image view for the texture, allowing shaders to sample it.
    CreateTextureImageView();

    // Create a sampler for the texture, which defines how the texture is sampled in shaders.
    CreateTextureSampler();

    // Load the 3D model data into memory.
    LoadModel();

    // Create uniform buffers to hold per-frame data like transformation matrices.
    CreateUniformBuffers();
    UpdateMouseRotateMat();

    // Create a descriptor pool, which allocates resources for descriptor sets.
    CreateDescriptorPool();

    // Allocate and configure descriptor sets, which link shaders to resources like textures and buffers.
    CreateDescriptorSets();

    // Create synchronization objects (semaphores and fences) to manage rendering and presentation.
    CreateSyncObjects();
}


void WizardChess::MainLoop()
{
    while (!VK.SurfaceManager()->WindowShouldClose())
    {
        glfwPollEvents();
        DrawFrame();
        //break;
    }

    vkDeviceWaitIdle(VK.Device());
}

void WizardChess::CleanupSwapChain()
{
    VkDevice device = VK.Device();
    vkDestroyImageView(device, m_depthImageView, nullptr);
    vkDestroyImage(device, m_depthImage, nullptr);
    vkFreeMemory(device, m_depthImageMemory, nullptr);

    for (auto framebuffer : m_swapChainFramebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyImageView(device, m_selectionMapImageView, nullptr);
    vkDestroyImage(device, m_selectionMapImage, nullptr);
    vkFreeMemory(device, m_selectionMapImageMemory, nullptr);
    vkDestroyFramebuffer(device, m_selectionMapFramebuffer, nullptr);

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
    vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(device, m_renderPass, nullptr);

    vkDestroyPipeline(device, m_shadowPassGraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, m_shadowPassPipelineLayout, nullptr);
    vkDestroyRenderPass(device, m_shadowRenderPass, nullptr);

    vkDestroyPipeline(device, m_selectionMapGraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, m_selectionMapPipelineLayout, nullptr);
    vkDestroyRenderPass(device, m_selectionMapRenderPass, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyBuffer(device, m_vsUniformBuffers[i], nullptr);
        vkUnmapMemory(device, m_vsUniformBuffersMemory[i]);
        vkFreeMemory(device, m_vsUniformBuffersMemory[i], nullptr);

        vkDestroyBuffer(device, m_fsUniformBuffers[i], nullptr);
        vkUnmapMemory(device, m_fsUniformBuffersMemory[i]);
        vkFreeMemory(device, m_fsUniformBuffersMemory[i], nullptr);

        vkDestroyBuffer(device, m_shadowVsUniformBuffers[i], nullptr);
        vkUnmapMemory(device, m_shadowVsUniformBuffersMemory[i]);
        vkFreeMemory(device, m_shadowVsUniformBuffersMemory[i], nullptr);
    }

    vkDestroyBuffer(device, m_selectionMapVsUniformBuffer, nullptr);
    vkUnmapMemory(device, m_selectionMapVsUniformBufferMemory);
    vkFreeMemory(device, m_selectionMapVsUniformBufferMemory, nullptr);

    vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

    vkDestroySampler(device, m_textureSampler, nullptr);
    vkDestroyImageView(device, m_textureImageView, nullptr);
    vkDestroyImage(device, m_textureImage, nullptr);
    vkFreeMemory(device, m_textureImageMemory, nullptr);

    vkDestroySampler(device, m_shadowImageSampler, nullptr);
    vkDestroyImageView(device, m_shadowImageView, nullptr);
    vkDestroyImage(device, m_shadowImage, nullptr);
    vkFreeMemory(device, m_shadowImageMemory, nullptr);
    vkDestroyFramebuffer(device, m_shadowPassFramebuffer, nullptr);

    vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_shadowPassDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_selectionMapDescriptorSetLayout, nullptr);

    for (auto& [modelName, pModel] : m_uniqueModels)
    {
        delete pModel;
    }
    m_uniqueModels.clear();

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

    m_width = width;
    m_height = height;
    VK.CreateSwapChain();
    CreateDepthResources();
    CreateSwapChainFramebuffers();
    CreateSelectionMapResources();
    CreateSelectionMapFramebuffers();
}

void WizardChess::CreateRenderPass()
{
    CreateShadowPassRenderPass();
    CreateScenePassRenderPass();
    CreateSelectionMapPassRenderPass();
}

void WizardChess::CreateShadowPassRenderPass()
{
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format          = m_shadowMapFormat;
    depthAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment   = 0;
    depthAttachmentRef.layout       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount  = 1;
    renderPassInfo.pAttachments     = &depthAttachment;
    renderPassInfo.subpassCount     = 1;
    renderPassInfo.pSubpasses       = &subpass;

    if (vkCreateRenderPass(VK.Device(), &renderPassInfo, nullptr, &m_shadowRenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void WizardChess::CreateScenePassRenderPass()
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
    depthAttachment.format          = DepthFormat();
    depthAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
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

    std::array<VkSubpassDependency, 2> dependency{};
    dependency[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
    dependency[0].dstSubpass        = 0;
    dependency[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency[0].srcAccessMask     = 0;
    dependency[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency[1].srcSubpass        = VK_SUBPASS_EXTERNAL;
    dependency[1].dstSubpass        = 0;
    dependency[1].srcStageMask      = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency[1].srcAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency[1].dstStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency[1].dstAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount  = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments     = attachments.data();
    renderPassInfo.subpassCount     = 1;
    renderPassInfo.pSubpasses       = &subpass;
    renderPassInfo.dependencyCount  = static_cast<uint32_t>(dependency.size());
    renderPassInfo.pDependencies    = dependency.data();

    if (vkCreateRenderPass(VK.Device(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void WizardChess::CreateSelectionMapPassRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format          = m_selectionMapFormat;
    colorAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format          = DepthFormat();
    depthAttachment.samples         = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
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

    std::array<VkSubpassDependency, 2> dependency{};
    dependency[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
    dependency[0].dstSubpass        = 0;
    dependency[0].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency[0].srcAccessMask     = 0;
    dependency[0].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency[0].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency[1].srcSubpass        = VK_SUBPASS_EXTERNAL;
    dependency[1].dstSubpass        = 0;
    dependency[1].srcStageMask      = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency[1].srcAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency[1].dstStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency[1].dstAccessMask     = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount  = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments     = attachments.data();
    renderPassInfo.subpassCount     = 1;
    renderPassInfo.pSubpasses       = &subpass;
    renderPassInfo.dependencyCount  = static_cast<uint32_t>(dependency.size());
    renderPassInfo.pDependencies    = dependency.data();

    if (vkCreateRenderPass(VK.Device(), &renderPassInfo, nullptr, &m_selectionMapRenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void WizardChess::CreateDescriptorSetLayout()
{
    // Create shadow pass descriptor set layout
    {
        std::vector<VkDescriptorSetLayoutBinding> uboLayoutBinding(1);
        uboLayoutBinding[0].binding                = 0;
        uboLayoutBinding[0].descriptorCount        = 1;
        uboLayoutBinding[0].descriptorType         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding[0].pImmutableSamplers     = nullptr;
        uboLayoutBinding[0].stageFlags             = VK_SHADER_STAGE_VERTEX_BIT;

        std::array<VkDescriptorSetLayoutBinding, 1> bindings = { uboLayoutBinding[0] };
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount                 = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings                    = bindings.data();

        if (vkCreateDescriptorSetLayout(VK.Device(), &layoutInfo, nullptr, &m_shadowPassDescriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    // Create render pass descriptor set layout
    {
        std::array<VkDescriptorSetLayoutBinding, 2> uboLayoutBinding{};
        // UBO for VS
        uboLayoutBinding[0].binding                = 0;
        uboLayoutBinding[0].descriptorCount        = 1;
        uboLayoutBinding[0].descriptorType         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding[0].pImmutableSamplers     = nullptr;
        uboLayoutBinding[0].stageFlags             = VK_SHADER_STAGE_VERTEX_BIT;
        // UBO for FS
        uboLayoutBinding[1].binding                = 1;
        uboLayoutBinding[1].descriptorCount        = 1;
        uboLayoutBinding[1].descriptorType         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding[1].pImmutableSamplers     = nullptr;
        uboLayoutBinding[1].stageFlags             = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> samplerLayoutBinding{};
        // Chess board texture
        samplerLayoutBinding[0].binding            = 2;
        samplerLayoutBinding[0].descriptorCount    = 1;
        samplerLayoutBinding[0].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding[0].pImmutableSamplers = nullptr;
        samplerLayoutBinding[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        // Shadow map
        samplerLayoutBinding[1].binding            = 3;
        samplerLayoutBinding[1].descriptorCount    = 1;
        samplerLayoutBinding[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding[1].pImmutableSamplers = nullptr;
        samplerLayoutBinding[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 4> bindings = { uboLayoutBinding[0], uboLayoutBinding[1], samplerLayoutBinding[0], samplerLayoutBinding[1] };
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount                 = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings                    = bindings.data();

        if (vkCreateDescriptorSetLayout(VK.Device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    // Create selection map descriptor set layout
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        // UBO for VS
        uboLayoutBinding.binding                = 0;
        uboLayoutBinding.descriptorCount        = 1;
        uboLayoutBinding.descriptorType         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers     = nullptr;
        uboLayoutBinding.stageFlags             = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount                 = 1;
        layoutInfo.pBindings                    = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(VK.Device(), &layoutInfo, nullptr, &m_selectionMapDescriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }
}

void WizardChess::CreatePipelines()
{
    CreateGraphicsPipeline();
    CreateShadowPassGraphicsPipeline();
    CreateSelectionMapGraphicsPipeline();
}

void WizardChess::CreateGraphicsPipeline()
{
    auto vertShaderCode = ReadFile(GetShaderPaths(EShader::Vert));
    auto fragShaderCode = ReadFile(GetShaderPaths(EShader::Frag));

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription    = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

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

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType                 = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable         = VK_FALSE;
    colorBlending.logicOp               = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount       = 1;
    colorBlending.pAttachments          = &colorBlendAttachment;
    colorBlending.blendConstants[0]     = 0.0f;
    colorBlending.blendConstants[1]     = 0.0f;
    colorBlending.blendConstants[2]     = 0.0f;
    colorBlending.blendConstants[3]     = 0.0f;

    std::vector<VkDynamicState> dynamicStates =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType              = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount  = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates     = dynamicStates.data();

    std::vector<VkPushConstantRange> pushConstantRange(2);
    pushConstantRange[0].offset        = offsetof(ModelPushConstants, vs);
    pushConstantRange[0].size          = sizeof(ModelPushConstants::ModelVsPushConstants);
    pushConstantRange[0].stageFlags    = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange[1].offset        = offsetof(ModelPushConstants, fs);
    pushConstantRange[1].size          = sizeof(ModelPushConstants::ModelFsPushConstants);
    pushConstantRange[1].stageFlags    = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount           = 1;
    pipelineLayoutInfo.pSetLayouts              = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount   = static_cast<uint32_t>(pushConstantRange.size());
    pipelineLayoutInfo.pPushConstantRanges      = pushConstantRange.data();

    if (vkCreatePipelineLayout(VK.Device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount             = 2;
    pipelineInfo.pStages                = shaderStages;
    pipelineInfo.pVertexInputState      = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState    = &inputAssembly;
    pipelineInfo.pViewportState         = &viewportState;
    pipelineInfo.pRasterizationState    = &rasterizer;
    pipelineInfo.pMultisampleState      = &multisampling;
    pipelineInfo.pDepthStencilState     = &depthStencil;
    pipelineInfo.pColorBlendState       = &colorBlending;
    pipelineInfo.pDynamicState          = &dynamicState;
    pipelineInfo.layout                 = m_pipelineLayout;
    pipelineInfo.renderPass             = m_renderPass;
    pipelineInfo.subpass                = 0;
    pipelineInfo.basePipelineHandle     = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(VK.Device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(VK.Device(), fragShaderModule, nullptr);
    vkDestroyShaderModule(VK.Device(), vertShaderModule, nullptr);
}

void WizardChess::CreateShadowPassGraphicsPipeline()
{
    auto vertShaderCode = ReadFile(GetShaderPaths(EShader::ShadowVert));
    auto fragShaderCode = ReadFile(GetShaderPaths(EShader::ShadowFrag));

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription   = Vertex::GetBindingDescription();
    auto attributeDescription = Vertex::GetPosOnlyAttributeDescription();

    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions    = &attributeDescription;

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
    depthStencil.depthCompareOp         = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable  = VK_FALSE;
    depthStencil.stencilTestEnable      = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType                 = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable         = VK_FALSE;

    std::vector<VkDynamicState> dynamicStates =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType              = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount  = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates     = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset        = offsetof(ModelPushConstants, vs);
    pushConstantRange.size          = sizeof(ModelPushConstants::ModelVsPushConstants);
    pushConstantRange.stageFlags    = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount           = 1;
    pipelineLayoutInfo.pSetLayouts              = &m_shadowPassDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount   = 1;
    pipelineLayoutInfo.pPushConstantRanges      = &pushConstantRange;

    if (vkCreatePipelineLayout(VK.Device(), &pipelineLayoutInfo, nullptr, &m_shadowPassPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount             = 2;
    pipelineInfo.pStages                = shaderStages;
    pipelineInfo.pVertexInputState      = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState    = &inputAssembly;
    pipelineInfo.pViewportState         = &viewportState;
    pipelineInfo.pRasterizationState    = &rasterizer;
    pipelineInfo.pMultisampleState      = &multisampling;
    pipelineInfo.pDepthStencilState     = &depthStencil;
    pipelineInfo.pColorBlendState       = &colorBlending;
    pipelineInfo.pDynamicState          = &dynamicState;
    pipelineInfo.layout                 = m_shadowPassPipelineLayout;
    pipelineInfo.renderPass             = m_shadowRenderPass;
    pipelineInfo.subpass                = 0;
    pipelineInfo.basePipelineHandle     = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(VK.Device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_shadowPassGraphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(VK.Device(), fragShaderModule, nullptr);
    vkDestroyShaderModule(VK.Device(), vertShaderModule, nullptr);
}

void WizardChess::CreateSelectionMapGraphicsPipeline()
{
    auto vertShaderCode = ReadFile(GetShaderPaths(EShader::SelectVert));
    auto fragShaderCode = ReadFile(GetShaderPaths(EShader::SelectFrag));

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto bindingDescription    = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

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

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType                 = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable         = VK_FALSE;
    colorBlending.logicOp               = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount       = 1;
    colorBlending.pAttachments          = &colorBlendAttachment;
    colorBlending.blendConstants[0]     = 0.0f;
    colorBlending.blendConstants[1]     = 0.0f;
    colorBlending.blendConstants[2]     = 0.0f;
    colorBlending.blendConstants[3]     = 0.0f;

    std::vector<VkDynamicState> dynamicStates =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType              = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount  = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates     = dynamicStates.data();

    std::vector<VkPushConstantRange> pushConstantRange(2);
    pushConstantRange[0].offset        = offsetof(ModelPushConstants, vs);
    pushConstantRange[0].size          = sizeof(ModelPushConstants::ModelVsPushConstants);
    pushConstantRange[0].stageFlags    = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange[1].offset        = offsetof(ModelPushConstants, fs);
    pushConstantRange[1].size          = sizeof(ModelPushConstants::ModelFsPushConstants);
    pushConstantRange[1].stageFlags    = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount           = 1;
    pipelineLayoutInfo.pSetLayouts              = &m_selectionMapDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount   = static_cast<uint32_t>(pushConstantRange.size());
    pipelineLayoutInfo.pPushConstantRanges      = pushConstantRange.data();

    if (vkCreatePipelineLayout(VK.Device(), &pipelineLayoutInfo, nullptr, &m_selectionMapPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount             = 2;
    pipelineInfo.pStages                = shaderStages;
    pipelineInfo.pVertexInputState      = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState    = &inputAssembly;
    pipelineInfo.pViewportState         = &viewportState;
    pipelineInfo.pRasterizationState    = &rasterizer;
    pipelineInfo.pMultisampleState      = &multisampling;
    pipelineInfo.pDepthStencilState     = &depthStencil;
    pipelineInfo.pColorBlendState       = &colorBlending;
    pipelineInfo.pDynamicState          = &dynamicState;
    pipelineInfo.layout                 = m_selectionMapPipelineLayout;
    pipelineInfo.renderPass             = m_selectionMapRenderPass;
    pipelineInfo.subpass                = 0;
    pipelineInfo.basePipelineHandle     = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(VK.Device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_selectionMapGraphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(VK.Device(), fragShaderModule, nullptr);
    vkDestroyShaderModule(VK.Device(), vertShaderModule, nullptr);
}

void WizardChess::CreateFramebuffers()
{
    CreateSwapChainFramebuffers();
    CreateShadowPassFramebuffers();
    CreateSelectionMapFramebuffers();
}

void WizardChess::CreateSwapChainFramebuffers()
{
    auto swapChainImageViews = VK.SurfaceManager()->SwapChainImageViews();

    m_swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments =
        {
            swapChainImageViews[i],
            m_depthImageView
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

        if (vkCreateFramebuffer(VK.Device(), &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void WizardChess::CreateShadowPassFramebuffers()
{
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = m_shadowRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments    = &m_shadowImageView;
    framebufferInfo.width           = m_shadowMapExtent.width;
    framebufferInfo.height          = m_shadowMapExtent.height;
    framebufferInfo.layers          = 1;

    if (vkCreateFramebuffer(VK.Device(), &framebufferInfo, nullptr, &m_shadowPassFramebuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create framebuffer!");
    }
}

void WizardChess::CreateSelectionMapFramebuffers()
{
    auto extent = VK.SurfaceManager()->SwapChainExtent();

    std::array<VkImageView, 2> attachments =
    {
        m_selectionMapImageView,
        m_depthImageView
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = m_selectionMapRenderPass;
    framebufferInfo.attachmentCount = 2;
    framebufferInfo.pAttachments    = attachments.data();
    framebufferInfo.width           = extent.width;
    framebufferInfo.height          = extent.height;
    framebufferInfo.layers          = 1;

    if (vkCreateFramebuffer(VK.Device(), &framebufferInfo, nullptr, &m_selectionMapFramebuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create framebuffer!");
    }
}

void WizardChess::CreateResources()
{
    CreateDepthResources();
    CreateShadowMapResources();
    CreateSelectionMapResources();
}

void WizardChess::CreateDepthResources()
{
    const VkExtent2D swapChainExtent = VK.SurfaceManager()->SwapChainExtent();
    const VkFormat   depthFormat     = DepthFormat();

    CreateImage(swapChainExtent.width,
                swapChainExtent.height,
                depthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_depthImage,
                m_depthImageMemory);
    m_depthImageView = VK.CreateImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    TransitionImageLayout(VK_NULL_HANDLE, m_depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void WizardChess::CreateShadowMapResources()
{
    CreateImage(m_shadowMapExtent.width,
                m_shadowMapExtent.height,
                m_shadowMapFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_shadowImage,
                m_shadowImageMemory);
    m_shadowImageView = VK.CreateImageView(m_shadowImage, m_shadowMapFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    TransitionImageLayout(VK_NULL_HANDLE, m_shadowImage, m_shadowMapFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void WizardChess::CreateSelectionMapResources()
{
    const VkExtent2D swapChainExtent = VK.SurfaceManager()->SwapChainExtent();

    CreateImage(swapChainExtent.width,
                swapChainExtent.height,
                m_selectionMapFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_selectionMapImage,
                m_selectionMapImageMemory);
    m_selectionMapImageView = VK.CreateImageView(m_selectionMapImage, m_selectionMapFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    TransitionImageLayout(VK_NULL_HANDLE, m_selectionMapImage, m_selectionMapFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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

VkFormat WizardChess::DepthFormat()
{
    if (m_depthFormat == VK_FORMAT_UNDEFINED)
    {
        m_depthFormat = FindSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }
    return m_depthFormat;
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

    TransitionImageLayout(VK_NULL_HANDLE, m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer, m_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    TransitionImageLayout(VK_NULL_HANDLE, m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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

    // Shadow map image sampler
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = VK_FILTER_LINEAR;
        samplerInfo.minFilter               = VK_FILTER_LINEAR;
        samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        // Enable depth comparison (unused)
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_NEVER;

        // Optional: Enable anisotropy if supported
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;

        if (vkCreateSampler(VK.Device(), &samplerInfo, nullptr, &m_shadowImageSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    // Chess board texture sampler
    {
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

void WizardChess::TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) const
{
    WC_UNUSED_PARAMETER(format);

    const bool singleTimeCommand = (commandBuffer == VK_NULL_HANDLE);
    if (singleTimeCommand)
    {
        commandBuffer = VK.BeginSingleTimeCommands();
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = (format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    auto GetAccessMaskAndStageFromLayout = [](const VkImageLayout layout, VkAccessFlags* pAccessMask, VkPipelineStageFlags* pStage)
    {
        switch (layout)
        {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                *pAccessMask = 0;
                *pStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                *pAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                *pStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                *pAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                *pStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                *pAccessMask = VK_ACCESS_SHADER_READ_BIT;
                *pStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                *pAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                *pStage      = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: // intentionally fallthrough
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                *pAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                *pStage      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;

            default:
                assert(false);
                throw std::invalid_argument("unsupported layout transition!");
                break;
        }
    };

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    GetAccessMaskAndStageFromLayout(oldLayout, &barrier.srcAccessMask, &srcStage);
    GetAccessMaskAndStageFromLayout(newLayout, &barrier.dstAccessMask, &dstStage);

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStage, dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    if (singleTimeCommand)
    {
        VK.EndSingleTimeCommands(commandBuffer);
    }
}

void WizardChess::LoadModel()
{
    // Load pieces
    {
        constexpr EModelName firstModelIndex = EModelName::Bishop;
        constexpr EModelName lastModelIndex  = EModelName::Queen;
        float                maxScale        = 0.0f;

        for (int i = firstModelIndex; i <= lastModelIndex; i++)
        {
            EModelName modelName = static_cast<EModelName>(i);
            Model* pModel = new Model(GetModelPaths(modelName), EModelType::ChessPiece);

            // Move up to place on the board.
            pModel->Translate(glm::vec3(0.0f, 1.0f, 0.0f));

            ///@note Originally the model was along z-axis.
            ///      Rotate -90 degree along x-axis to make it point to the y-axis.
            pModel->Rotate(-90.0f, glm::vec3(1.0f, 0.0f, 0.0f));

            maxScale = std::max(maxScale, pModel->MaxScale());
            m_uniqueModels[modelName] = pModel;
        }

        for (auto& [modelName, pModel] : m_uniqueModels)
        {
            pModel->RescaleNormalizeMatrix(1.0f / maxScale);
        }
    }

    // Load board
    {
        Model* pModel = new Model(GetModelPaths(EModelName::Cube), EModelType::ChessBoard);

        pModel->Scale(glm::vec3(1.0f, 0.0001f, 1.0f));

        m_uniqueModels[EModelName::Cube] = pModel;
    }
}

void WizardChess::CreateUniformBuffers()
{
    // For shadow pass vertex shader
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObjectShadowVs);

        m_shadowVsUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_shadowVsUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_shadowVsUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_shadowVsUniformBuffers[i], m_shadowVsUniformBuffersMemory[i]);

            vkMapMemory(VK.Device(), m_shadowVsUniformBuffersMemory[i], 0, bufferSize, 0, &m_shadowVsUniformBuffersMapped[i]);
        }
    }

    // For vertex shader
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObjectVs);

        m_vsUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_vsUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_vsUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_vsUniformBuffers[i], m_vsUniformBuffersMemory[i]);

            vkMapMemory(VK.Device(), m_vsUniformBuffersMemory[i], 0, bufferSize, 0, &m_vsUniformBuffersMapped[i]);
        }
    }

    // For fragment shader
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObjectFs);

        m_fsUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_fsUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        m_fsUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_fsUniformBuffers[i], m_fsUniformBuffersMemory[i]);

            vkMapMemory(VK.Device(), m_fsUniformBuffersMemory[i], 0, bufferSize, 0, &m_fsUniformBuffersMapped[i]);
        }
    }

    // For selection map vertex shader
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObjectSelectionMapVs);
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_selectionMapVsUniformBuffer, m_selectionMapVsUniformBufferMemory);
        vkMapMemory(VK.Device(), m_selectionMapVsUniformBufferMemory, 0, bufferSize, 0, &m_selectionMapVsUniformBufferMapped);
    }
}

void WizardChess::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 4 * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);  // one for VS, one for FS, one for shadow pass VS, one for selection map VS
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 2 * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);  // one for FS, one for shadow pass FS

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = 2 * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) + 1;

    if (vkCreateDescriptorPool(VK.Device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void WizardChess::CreateDescriptorSets()
{
    // Shadow map pass
    {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_shadowPassDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts        = layouts.data();

        m_shadowPassDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(VK.Device(), &allocInfo, m_shadowPassDescriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_shadowVsUniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range  = sizeof(UniformBufferObjectShadowVs);

            VkWriteDescriptorSet descriptorWrites{};

            descriptorWrites.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites.dstSet          = m_shadowPassDescriptorSets[i];
            descriptorWrites.dstBinding      = 0;
            descriptorWrites.dstArrayElement = 0;
            descriptorWrites.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites.descriptorCount = 1;
            descriptorWrites.pBufferInfo     = &bufferInfo;

            vkUpdateDescriptorSets(VK.Device(), 1, &descriptorWrites, 0, nullptr);
        }
    }

    // Render pass
    {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts        = layouts.data();

        m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(VK.Device(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            std::array<VkDescriptorBufferInfo, 2> bufferInfo{};
            bufferInfo[0].buffer = m_vsUniformBuffers[i];
            bufferInfo[0].offset = 0;
            bufferInfo[0].range  = sizeof(UniformBufferObjectVs);
            bufferInfo[1].buffer = m_fsUniformBuffers[i];
            bufferInfo[1].offset = 0;
            bufferInfo[1].range  = sizeof(UniformBufferObjectFs);

            std::array<VkDescriptorImageInfo, 2> imageInfo{};
            imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo[0].imageView   = m_textureImageView;
            imageInfo[0].sampler     = m_textureSampler;
            imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo[1].imageView   = m_shadowImageView;
            imageInfo[1].sampler     = m_shadowImageSampler;

            std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

            descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet          = m_descriptorSets[i];
            descriptorWrites[0].dstBinding      = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo     = &bufferInfo[0];

            descriptorWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet          = m_descriptorSets[i];
            descriptorWrites[1].dstBinding      = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo     = &bufferInfo[1];

            descriptorWrites[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet          = m_descriptorSets[i];
            descriptorWrites[2].dstBinding      = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo      = &imageInfo[0];

            descriptorWrites[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet          = m_descriptorSets[i];
            descriptorWrites[3].dstBinding      = 3;
            descriptorWrites[3].dstArrayElement = 0;
            descriptorWrites[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pImageInfo      = &imageInfo[1];

            vkUpdateDescriptorSets(VK.Device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    // Selection map
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_selectionMapDescriptorSetLayout;

        if (vkAllocateDescriptorSets(VK.Device(), &allocInfo, &m_selectionMapDescriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_selectionMapVsUniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(UniformBufferObjectSelectionMapVs);

        std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

        descriptorWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet          = m_selectionMapDescriptorSet;
        descriptorWrites[0].dstBinding      = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(VK.Device(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void WizardChess::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
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

    // Create push constants for passing small amounts of dynamic data to shaders.
    ModelPushConstants pushConstants{};
    pushConstants.vs.world = m_mouseRotateMat;
    float worldScale = 3.75f;
    pushConstants.vs.world = glm::scale(pushConstants.vs.world, glm::vec3(worldScale, worldScale, worldScale));

#if ROTATE_WORLD
    // Calculate elapsed time to create a dynamic rotation effect for models.
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    pushConstants.vs.world = glm::rotate(pushConstants.vs.world, time * glm::radians(-10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
#endif // #if ROTATE_WORLD

    auto RecordDrawModel = [commandBuffer=commandBuffer](
        const ModelPushConstants* pPushConstants,
        Model*                    pModel,
        const VkPipelineLayout    pipelineLayout,
        bool                      needVsPushConstants,
        bool                      needFsPushConstants)
    {
        // Bind the vertex buffer for the current model.
        VkBuffer vertexBuffers[] = { pModel->VertexBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        // Bind the index buffer for the current model.
        vkCmdBindIndexBuffer(commandBuffer, pModel->IndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        if (needVsPushConstants)
        {
            // Pass the VS push constant to the vertex shader.
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, offsetof(ModelPushConstants, vs),
                sizeof(ModelPushConstants::ModelVsPushConstants), &pPushConstants->vs);
        }

        if (needFsPushConstants)
        {
            // Pass the FS push constant to the fragment shader.
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(ModelPushConstants, fs),
                sizeof(ModelPushConstants::ModelFsPushConstants), &pPushConstants->fs);
        }

        // Issue a draw command for the indexed geometry of the model.
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(pModel->Indices()), 1, 0, 0, 0);
    };

    auto RenderChessBoard = [&](
        ModelPushConstants&       pushConstants,
        const VkPipelineLayout    pipelineLayout,
        bool                      needVsPushConstants,
        bool                      needFsPushConstants,
        bool                      isSelectionMapPass = false)
    {
        Model* pModel = m_uniqueModels[EModelName::Cube];

        pushConstants.vs.flag            = 0;
        pushConstants.vs.model           = pModel->ModelMatrix();
        pushConstants.vs.normailzeMatrix = pModel->NormalizeMatrix();
        if (isSelectionMapPass)
        {
            pushConstants.fs.fileRank    = {};
        }
        else
        {
            pushConstants.fs.renderMode  = pModel->RenderMode();
        }

        RecordDrawModel(&pushConstants, pModel, pipelineLayout, needVsPushConstants, needFsPushConstants);
    };

    auto RenderChessPieces = [&](
        ModelPushConstants&       pushConstants,
        const VkPipelineLayout    pipelineLayout,
        bool                      needVsPushConstants,
        bool                      needFsPushConstants,
        bool                      isSelectionMapPass = false)
    {
        // Render each chess piece in the scene.
        for (const auto& chessPiece : chessPieces)
        {
            Model* pModel = m_uniqueModels[chessPiece.m_modelName];

            pushConstants.vs.model           = glm::mat4(1.0);
            float modelScale = 1.0f / 4.75f;
            pushConstants.vs.model           = glm::scale(pushConstants.vs.model, glm::vec3(modelScale, modelScale, modelScale));
            pushConstants.vs.model           = glm::translate(pushConstants.vs.model, glm::vec3(-3.5f, 0.0f, 3.5f));
            pushConstants.vs.model           = glm::translate(pushConstants.vs.model, chessPiece.PositionVec3());
            pushConstants.vs.model           = pushConstants.vs.model * pModel->ModelMatrix();
            pushConstants.vs.normailzeMatrix = pModel->NormalizeMatrix();
            pushConstants.vs.flag = 0;
            pushConstants.vs.flag |= (chessPiece.m_color == EPieceColor::Black) ? ModelPushConstants::PUSH_CONSTANT_FLAG_IS_BLACK : 0;
            pushConstants.vs.flag |= ((chessPiece.m_file - 'A' + 1 == m_selectedFile) &&
                                      (chessPiece.m_rank - '1' + 1 == m_selectedRank)) ? ModelPushConstants::PUSH_CONSTANT_FLAG_IS_SELECTED : 0;

            if (isSelectionMapPass)
            {
                pushConstants.fs.fileRank.file = chessPiece.m_file - 'A' + 1;
                pushConstants.fs.fileRank.rank = chessPiece.m_rank - '1' + 1;
            }
            else
            {
                pushConstants.fs.renderMode    = pModel->RenderMode();
            }

            RecordDrawModel(&pushConstants, pModel, pipelineLayout, needVsPushConstants, needFsPushConstants);
        }
    };

    // First shadow map pass
    {
        // Configure the render pass begin info.
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_shadowRenderPass;
        renderPassInfo.framebuffer       = m_shadowPassFramebuffer;
        renderPassInfo.renderArea.offset = { 0, 0 }; // Render area starts at the top-left corner.
        renderPassInfo.renderArea.extent = m_shadowMapExtent;

        // Clear values for the color and depth buffer.
        std::array<VkClearValue, 1> clearValues{};
        clearValues[0].depthStencil = { 1.0f, 0 }; // Clear depth to 1.0 and stencil to 0.

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        // Begin the render pass, specifying that commands will be submitted inline.
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind the graphics pipeline to the command buffer.
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPassGraphicsPipeline);

        // Set the viewport, defining the dimensions and depth range of the render area.
        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = (float)m_shadowMapExtent.width;
        viewport.height   = (float)m_shadowMapExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        // Set the scissor rectangle to restrict drawing to the swap chain extent.
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_shadowMapExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Bind the descriptor set for the current frame, providing shader resources like textures and uniform buffers.
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPassPipelineLayout, 0, 1, &m_shadowPassDescriptorSets[m_currentImage], 0, nullptr);

        RenderChessPieces(pushConstants, m_shadowPassPipelineLayout, true, false);

        // End the render pass.
        vkCmdEndRenderPass(commandBuffer);
    }

    TransitionImageLayout(commandBuffer, m_shadowImage, m_shadowMapFormat, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Second render pass
    {
        // Configure the render pass begin info.
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_renderPass; // The render pass to use.
        renderPassInfo.framebuffer       = m_swapChainFramebuffers[imageIndex]; // Framebuffer for the current swap chain image.
        renderPassInfo.renderArea.offset = { 0, 0 }; // Render area starts at the top-left corner.
        renderPassInfo.renderArea.extent = swapChainExtent; // Render area size matches the swap chain extent.

        // Clear values for the color and depth buffer.
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.604f, 0.000f, 0.033f, 1.0f} }; // Clear color to a Rutgers red.
        clearValues[1].depthStencil = { 1.0f, 0 }; // Clear depth to 1.0 and stencil to 0.

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        // Begin the render pass, specifying that commands will be submitted inline.
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind the graphics pipeline to the command buffer.
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

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
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentImage], 0, nullptr);

        RenderChessPieces(pushConstants, m_pipelineLayout, true, true);
        RenderChessBoard(pushConstants, m_pipelineLayout, true, true);

        // End the render pass.
        vkCmdEndRenderPass(commandBuffer);
    }

    // Third selection map pass
    {
        // Configure the render pass begin info.
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_selectionMapRenderPass;
        renderPassInfo.framebuffer       = m_selectionMapFramebuffer;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = swapChainExtent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues    = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_selectionMapGraphicsPipeline);

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = (float)swapChainExtent.width;
        viewport.height   = (float)swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Bind the descriptor set for the current frame, providing shader resources like textures and uniform buffers.
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_selectionMapPipelineLayout, 0, 1, &m_selectionMapDescriptorSet, 0, nullptr);

        RenderChessPieces(pushConstants, m_selectionMapPipelineLayout, true, true, true);
        RenderChessBoard(pushConstants, m_selectionMapPipelineLayout, true, true, true);

        // End the render pass.
        vkCmdEndRenderPass(commandBuffer);
    }

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

void WizardChess::UpdateUniformBuffer(uint32_t currentImage)
{
#if ROTATE_LIGHT
    // Calculate elapsed time to create a dynamic rotation effect for models.
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
#else
    float time = 0.0f;
#endif // #if ROTATE_LIGHT

    auto GetCameraPosVecAndViewMatrix = [](
        const float      longitude,
        const float      latitude,
        const float      radius,
        const glm::vec3& targetPos,
        const glm::vec3& upVector,
        glm::vec3&       cameraPos,
        glm::mat4&       cameraView)
    {
        // Convert spherical coordinates to Cartesian
        cameraPos = targetPos + glm::vec3(
            radius * cos(latitude) * sin(longitude),  // X
            radius * sin(latitude),                   // Y
            radius * cos(latitude) * cos(longitude)   // Z
        );

        // Define camera view matrix
        cameraView = glm::lookAt(cameraPos, targetPos, upVector);
    };

    const glm::vec3 targetPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3       cameraPos;
    glm::mat4       cameraView;
    glm::vec3       lightPos;
    glm::mat4       lightView;

    GetCameraPosVecAndViewMatrix(
        glm::radians(0.0f),     // theta, horizontal rotation
        glm::radians(0.0f),     // phi, vertical rotation
        10.0f / m_mouseZoom,    // Distance from center
        targetPos,
        glm::vec3(0.0f, 1.0f, 0.0f), // upVector
        cameraPos,
        cameraView);

    GetCameraPosVecAndViewMatrix(
        glm::radians(-45.0f + (15.0f * time)),
        glm::radians(45.0f),
        25.0f,
        targetPos,
        glm::vec3(0.0f, 1.0f, 0.0f), // upVector
        lightPos,
        lightView);

    VkExtent2D swapChainExtent = VK.SurfaceManager()->SwapChainExtent();
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 1.0f, 80.0f);

    // Define orthographic projection
    float orthoSize = 5.0f;
    float aspectRatio = swapChainExtent.width / (float)swapChainExtent.height;

    glm::mat4 orthoProj = glm::ortho(-orthoSize * aspectRatio, orthoSize * aspectRatio,
                                     -orthoSize, orthoSize,
                                      0.1f, 100.0f);

    // Vulkan's y-axis is pointing downwards.
    proj[1][1] *= -1;
    orthoProj[1][1] *= -1;

    // Shadow pass vertex shader ubo
    {
        UniformBufferObjectShadowVs ubo{};
        ubo.lightView = lightView;
        ubo.lightProj = orthoProj;
        memcpy(m_shadowVsUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    // Render pass vertex shader ubo
    {
        UniformBufferObjectVs ubo{};
        ubo.view      = cameraView;
        ubo.lightView = lightView;
        ubo.proj      = proj;
        ubo.lightProj = orthoProj;
        memcpy(m_vsUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    // Render pass fragment shader ubo
    {
        UniformBufferObjectFs ubo{};
        ubo.lightPos   = lightPos;
        ubo.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        ubo.cameraPos  = cameraPos;
        memcpy(m_fsUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    // Selection map vertex shader ubo
    {
        UniformBufferObjectSelectionMapVs ubo{};
        ubo.view = cameraView;
        ubo.proj = proj;
        memcpy(m_selectionMapVsUniformBufferMapped, &ubo, sizeof(ubo));
    }
}

void WizardChess::ReadSelectionMap(VkImageLayout oldLayout, uint32_t fboX, uint32_t fboY, int* pFile, int* pRank) const
{
    VkDevice device = VK.Device();
    VkFormat format = m_selectionMapFormat;

    vkDeviceWaitIdle(device);

    // 1. Create a staging buffer
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VkExtent2D     extent = VK.SurfaceManager()->SwapChainExtent();
    const uint32_t width  = extent.width;
    const uint32_t height = extent.height;

    CreateBuffer(width * height * sizeof(float),
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingBufferMemory);

    // 2. Transition image to transfer source layout
    TransitionImageLayout(VK_NULL_HANDLE, m_selectionMapImage, format, oldLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // 3. Copy buffer to staging buffer
    CopyImageToBuffer(m_selectionMapImage,
                      stagingBuffer,
                      (format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
                      width,
                      height);

    // 4. Map memory and normalize depth values
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);

    std::vector<uint8_t> bmpData;

    if (format == VK_FORMAT_B8G8R8A8_UNORM)
    {
        bmpData.resize(width * height * sizeof(float));
        memcpy(bmpData.data(), data, width * height * sizeof(float));
    }
    else
    {
        assert(false);
    }

    vkUnmapMemory(device, stagingBufferMemory);

    // 5. Read here
    uint8_t* pPixelColor = &bmpData[(fboY * width + fboX) * sizeof(float)];
    float rColor = pPixelColor[2] / 255.0f * 8.0f;
    float gColor = pPixelColor[1] / 255.0f * 8.0f;
    int file = (int)roundf(rColor);
    int rank = (int)roundf(gColor);
    *pFile = file;
    *pRank = rank;

    // Cleanup
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    // 6. Transition image from transfer source layout back to old layout
    TransitionImageLayout(VK_NULL_HANDLE, m_selectionMapImage, format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, oldLayout);

    vkDeviceWaitIdle(VK.Device());
}

void WizardChess::SaveImageAsBMP(VkDevice device, VkImage image, VkFormat format, VkImageLayout oldLayout, int width, int height, std::string fileName)
{
    vkDeviceWaitIdle(VK.Device());

    VkImage stagingImage = image;
    VkDeviceMemory stagingImageMemory = VK_NULL_HANDLE;

    // 1. Create a staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(width * height * sizeof(float),
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingBufferMemory);

    // 2. Transition image to transfer source layout
    TransitionImageLayout(VK_NULL_HANDLE, image, format, oldLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // 3. Copy buffer to staging buffer
    CopyImageToBuffer(stagingImage,
                      stagingBuffer,
                      (format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
                      width,
                      height);

    // 4. Map memory and normalize depth values
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);

    std::vector<uint8_t> bmpData;

    bool isGrayscale = false;

    if (format == VK_FORMAT_D32_SFLOAT)
    {
        bmpData.resize(width * height);
        isGrayscale = true;
        float* depthData = (float*)data;

        for (int i = 0; i < width * height; i++)
        {
            bmpData[i] = static_cast<uint8_t>(depthData[i] * 255.0f); // Normalize
        }
    }
    else if (format == VK_FORMAT_B8G8R8A8_SRGB)
    {
        bmpData.resize(width * height * 4);
        memcpy(bmpData.data(), data, width * height * 4);
    }
    else if (format == VK_FORMAT_B8G8R8A8_UNORM)
    {
        bmpData.resize(width * height * 4);
        memcpy(bmpData.data(), data, width * height * 4);
    }
    else
    {
        assert(false);
    }

    vkUnmapMemory(device, stagingBufferMemory);

    // 5. Save as BMP
    SaveBMP(fileName, bmpData, width, height, isGrayscale);

    // Cleanup
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    if (stagingImage != image)
    {
        vkDestroyImage(device, stagingImage, nullptr);
        vkFreeMemory(device, stagingImageMemory, nullptr);
    }

    // 6. Transition image from transfer source layout back to old layout
    TransitionImageLayout(VK_NULL_HANDLE, image, format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, oldLayout);

    vkDeviceWaitIdle(VK.Device());
}

void WizardChess::DrawFrame()
{
    vkWaitForFences(VK.Device(), 1, &m_inFlightFences[m_currentImage], VK_TRUE, UINT64_MAX);

    VkSwapchainKHR swapChain = VK.SurfaceManager()->SwapChain();

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(VK.Device(), swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentImage], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    UpdateUniformBuffer(m_currentImage);

    vkResetFences(VK.Device(), 1, &m_inFlightFences[m_currentImage]);

    vkResetCommandBuffer(m_commandBuffers[m_currentImage], /*VkCommandBufferResetFlagBits*/ 0);
    RecordCommandBuffer(m_commandBuffers[m_currentImage], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentImage] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentImage];

    VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentImage] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    LimitFPS(30);

    if (vkQueueSubmit(VK.GraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentImage]) != VK_SUCCESS)
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

    // Dump first frame
    if (m_currentFrame == 0)
    {
        auto extent = VK.SurfaceManager()->SwapChainExtent();
        VkImage image = VK.SurfaceManager()->SwapChainImages()[m_currentImage];
        VkFormat format = VK.SurfaceManager()->SwapChainImageFormat();
        SaveImageAsBMP(VK.Device(), image, format, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, extent.width, extent.height, "present.bmp");

        SaveImageAsBMP(VK.Device(), m_selectionMapImage, m_selectionMapFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, extent.width, extent.height, "select.bmp");
    }

    m_currentImage = (m_currentImage + 1) % MAX_FRAMES_IN_FLIGHT;
    m_currentFrame = m_currentFrame + 1;
}
