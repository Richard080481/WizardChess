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
    Vert = 0,
    Frag = 1,
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
    alignas(16) glm::mat4 proj;
};

struct UniformBufferObjectFs
{
    glm::vec3 lightPos;
    alignas(16) glm::vec3 lightColor;
    alignas(16) glm::vec3 viewPos;
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
    auto app = reinterpret_cast<WizardChess*>(glfwGetWindowUserPointer(window));
    app->SetFramebufferResized();
}

void WizardChess::InitVulkan()
{
    // Create a VulkanDeviceManager object to manage Vulkan-specific operations.
    g_pVk = new VulkanDeviceManager();

    ///@note GLFW needs to be initialized before creating the Vulkan instance.
    ///      This ensures GLFW performs its internal setups, including platform-specific windowing
    ///      and registering Vulkan extensions required for rendering.
    VK.CreateGlfwWindow(m_width, m_height);

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

    // Set up the descriptor set layout, which specifies how shaders access resources like uniforms and textures.
    CreateDescriptorSetLayout();

    // Create the graphics pipeline, which configures shaders, input assembly, viewport, and other rendering states.
    CreateGraphicsPipeline();

    // Create resources for depth buffering, allowing proper handling of 3D object occlusion.
    CreateDepthResources();

    // Create framebuffers, which represent the render targets for each swap chain image.
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

    // Create a descriptor pool, which allocates resources for descriptor sets.
    CreateDescriptorPool();

    // Allocate and configure descriptor sets, which link shaders to resources like textures and buffers.
    CreateDescriptorSets();

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
    vkDestroyImageView(device, m_depthImageView, nullptr);
    vkDestroyImage(device, m_depthImage, nullptr);
    vkFreeMemory(device, m_depthImageMemory, nullptr);

    for (auto framebuffer : m_swapChainFramebuffers)
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
    vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(device, m_renderPass, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyBuffer(device, m_vsUniformBuffers[i], nullptr);
        vkUnmapMemory(device, m_vsUniformBuffersMemory[i]);
        vkFreeMemory(device, m_vsUniformBuffersMemory[i], nullptr);

        vkDestroyBuffer(device, m_fsUniformBuffers[i], nullptr);
        vkUnmapMemory(device, m_fsUniformBuffersMemory[i]);
        vkFreeMemory(device, m_fsUniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

    vkDestroySampler(device, m_textureSampler, nullptr);
    vkDestroyImageView(device, m_textureImageView, nullptr);

    vkDestroyImage(device, m_textureImage, nullptr);
    vkFreeMemory(device, m_textureImageMemory, nullptr);

    vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);

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

    VK.CreateSwapChain();
    CreateDepthResources();
    CreateFramebuffers();
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

void WizardChess::CreateDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> uboLayoutBinding(2);
    uboLayoutBinding[0].binding                = 0;
    uboLayoutBinding[0].descriptorCount        = 1;
    uboLayoutBinding[0].descriptorType         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding[0].pImmutableSamplers     = nullptr;
    uboLayoutBinding[0].stageFlags             = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding[1].binding                = 1;
    uboLayoutBinding[1].descriptorCount        = 1;
    uboLayoutBinding[1].descriptorType         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding[1].pImmutableSamplers     = nullptr;
    uboLayoutBinding[1].stageFlags             = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding            = 2;
    samplerLayoutBinding.descriptorCount    = 1;
    samplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = { uboLayoutBinding[0], uboLayoutBinding[1], samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount                 = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings                    = bindings.data();

    if (vkCreateDescriptorSetLayout(VK.Device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
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

void WizardChess::CreateFramebuffers()
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

void WizardChess::CreateDepthResources()
{
    VkFormat depthFormat = FindDepthFormat();

    auto extent = VK.SurfaceManager()->SwapChainExtent();
    CreateImage(extent.width, extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);
    m_depthImageView = VK.CreateImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
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
    // Load pieces
    {
        constexpr EModelName firstModelIndex = EModelName::Bishop;
        constexpr EModelName lastModelIndex  = EModelName::Queen;
        constexpr int        numModels       = lastModelIndex - firstModelIndex + 1;
        constexpr float      x_offset        = 1.2f;
        constexpr float      theta           = 360.0f / numModels;
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

        constexpr float cellCenterOffset = 0.5f;
        constexpr float borderOffset     = 0.75f;
        pModel->Translate(glm::vec3(-(borderOffset + cellCenterOffset), 0.0f, (borderOffset + cellCenterOffset)));

        constexpr float scale = 4.75f;
        pModel->Scale(glm::vec3(scale, 0.0f, scale));
        pModel->Translate(glm::vec3(1.0f, 0.0f, -1.0f));

        ///@note Originally the model was along z-axis.
        ///      Rotate -90 degree along x-axis to make it point to the y-axis.
        pModel->Rotate(-90.0f, glm::vec3(1.0f, 0.0f, 0.0f));

        m_uniqueModels[EModelName::Cube] = pModel;
    }
}

void WizardChess::CreateUniformBuffers()
{
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
}

void WizardChess::CreateDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 2 * static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);  // one for VS, one for FS
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(VK.Device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void WizardChess::CreateDescriptorSets()
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
        std::vector<VkDescriptorBufferInfo> bufferInfo(2);
        bufferInfo[0].buffer = m_vsUniformBuffers[i];
        bufferInfo[0].offset = 0;
        bufferInfo[0].range  = sizeof(UniformBufferObjectVs);
        bufferInfo[1].buffer = m_fsUniformBuffers[i];
        bufferInfo[1].offset = 0;
        bufferInfo[1].range  = sizeof(UniformBufferObjectFs);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = m_textureImageView;
        imageInfo.sampler     = m_textureSampler;

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

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
        descriptorWrites[2].pImageInfo      = &imageInfo;

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

    // Configure the render pass begin info.
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass; // The render pass to use.
    renderPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex]; // Framebuffer for the current swap chain image.
    renderPassInfo.renderArea.offset = { 0, 0 }; // Render area starts at the top-left corner.
    renderPassInfo.renderArea.extent = swapChainExtent; // Render area size matches the swap chain extent.

    // Clear values for the color and depth buffer.
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { {0.25f, 0.0f, 0.0f, 1.0f} }; // Clear color to a dark red.
    clearValues[1].depthStencil = { 1.0f, 0 }; // Clear depth to 1.0 and stencil to 0.

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // Begin the render pass, specifying that commands will be submitted inline.
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline to the command buffer.
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

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
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentFrame], 0, nullptr);

    // Calculate elapsed time to create a dynamic rotation effect for models.
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    // Create push constants for passing small amounts of dynamic data to shaders.
    ModelPushConstants pushConstants{};
    pushConstants.vs.world = glm::mat4(1.0);

    // Rotate to display the world
    pushConstants.vs.world = glm::rotate(pushConstants.vs.world, time * glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Add back offset to center the world
    pushConstants.vs.world = glm::translate(pushConstants.vs.world, glm::vec3(-3.5, 0, 3.5));

    auto RecordDrawModel = [commandBuffer=commandBuffer, pipelineLayout=m_pipelineLayout](const ModelPushConstants* pPushConstants, Model* pModel)
    {
        // Bind the vertex buffer for the current model.
        VkBuffer vertexBuffers[] = { pModel->VertexBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        // Bind the index buffer for the current model.
        vkCmdBindIndexBuffer(commandBuffer, pModel->IndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // Pass the VS push constant to the vertex shader.
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, offsetof(ModelPushConstants, vs), sizeof(ModelPushConstants::ModelVsPushConstants), &pPushConstants->vs);

        // Pass the FS push constant to the fragment shader.
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(ModelPushConstants, fs), sizeof(ModelPushConstants::ModelFsPushConstants), &pPushConstants->fs);

        // Issue a draw command for the indexed geometry of the model.
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(pModel->Indices()), 1, 0, 0, 0);
    };

    // Render each chess piece in the scene.
    for (const auto& chessPiece : chessPieces)
    {
        Model* pModel = m_uniqueModels[chessPiece.m_modelName];

        // Populate the push constants.
        pushConstants.vs.model           = glm::translate(glm::mat4(1.0), chessPiece.PositionVec3());
        pushConstants.vs.model           = pushConstants.vs.model * pModel->ModelMatrix();
        pushConstants.vs.normailzeMatrix = pModel->NormalizeMatrix();
        pushConstants.vs.isBlack         = chessPiece.m_color == EPieceColor::Black;
        pushConstants.fs.renderMode      = pModel->RenderMode();

        RecordDrawModel(&pushConstants, pModel);
    }

    // Render the chess board.
    {
        Model* pModel = m_uniqueModels[EModelName::Cube];

        // Populate the push constants.
        pushConstants.vs.model           = pModel->ModelMatrix();
        pushConstants.vs.normailzeMatrix = pModel->NormalizeMatrix();
        pushConstants.fs.renderMode      = pModel->RenderMode();

        RecordDrawModel(&pushConstants, pModel);
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
    glm::vec3 cameraPos = glm::vec3(0.0f, 10.0f, 10.0f);

    {
        auto swapChainExtent = VK.SurfaceManager()->SwapChainExtent();

        UniformBufferObjectVs ubo{};

        ubo.view = glm::lookAt(cameraPos,
                               glm::vec3(0.0f,  0.0f,  0.0f),
                               glm::vec3(0.0f,  1.0f, -1.0f));

        ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 20.0f);

        // Vulkan's y-axis is pointing downwards.
        ubo.proj[1][1] *= -1;

        memcpy(m_vsUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    {
        UniformBufferObjectFs ubo{};
        ubo.lightPos   = glm::vec3(2.0f, 4.0f, -2.0f);
        ubo.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        ubo.viewPos    = cameraPos;

        memcpy(m_fsUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }
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
    RecordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

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
