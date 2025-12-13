#include "VulkanDeviceManager.h"
#include "MemoryTracker.h"

#include <set>

VulkanDeviceManager* g_pVk = nullptr;

VulkanDeviceManager::~VulkanDeviceManager()
{
    Destroy();
}

void VulkanDeviceManager::DestroyValidationLayerNames()
{
    for (int i = 0; i < m_validationLayers.size(); i++)
    {
        FREE(const_cast<char*>(m_validationLayers[i]));
    }
    m_validationLayers.clear();
}

void VulkanDeviceManager::EnableValidationLayers(
    bool enableValidationLayers,
    const std::vector<const char*>* pValidationLayers)
{
    assert(m_instance == VK_NULL_HANDLE);

    m_enableValidationLayers = enableValidationLayers;

    if (enableValidationLayers)
    {
        DestroyValidationLayerNames();

        assert(pValidationLayers != nullptr);
        for (const auto& validationLayerStr : *pValidationLayers)
        {
            int size = strlen(validationLayerStr) + 1; // null-terminated char array
            char* layerName = static_cast<char*>(MALLOC(sizeof(char) * size));
            assert(layerName != nullptr);
            if (layerName != nullptr)
            {
                memcpy(layerName, validationLayerStr, size);
                m_validationLayers.push_back(layerName);
            }
        }
    }
}

void VulkanDeviceManager::DestroyDeviceExtensionNames()
{
    for (int i = 0; i < m_deviceExtensions.size(); i++)
    {
        FREE(const_cast<char*>(m_deviceExtensions[i]));
    }
    m_deviceExtensions.clear();
}

void VulkanDeviceManager::EnableDeviceExtensions(const std::vector<const char*>* pDeviceExtensions)
{
    assert(m_device == VK_NULL_HANDLE);

    DestroyDeviceExtensionNames();

    if (pDeviceExtensions != nullptr)
    {
        for (const auto& deviceExtensionStr : *pDeviceExtensions)
        {
            int size = strlen(deviceExtensionStr) + 1; // null-terminated char array
            char* extensionName = static_cast<char*>(MALLOC(sizeof(char) * size));
            assert(extensionName != nullptr);
            if (extensionName != nullptr)
            {
                memcpy(extensionName, deviceExtensionStr, size);
                m_deviceExtensions.push_back(extensionName);
            }
        }
    }
}

void VulkanDeviceManager::CreateGlfwWindow(int width, int height)
{
    m_pSurfaceManager = new VulkanSurfaceManager(this);
    m_pSurfaceManager->InitWindow(width, height);
}

void VulkanDeviceManager::CreateSurface()
{
    m_pSurfaceManager->CreateSurface();
}

QueueFamilyIndices VulkanDeviceManager::FindQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_pSurfaceManager->Surface(), &presentSupport);

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

bool VulkanDeviceManager::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool VulkanDeviceManager::IsDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = FindQueueFamilies(device);

    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = m_pSurfaceManager->QuerySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(device, &properties);

    uint32_t major = VK_VERSION_MAJOR(properties.apiVersion);
    uint32_t minor = VK_VERSION_MINOR(properties.apiVersion);

    bool vk13Supported = (major > 1 || (major == 1 && minor >= 3));

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy && vk13Supported;
}

void VulkanDeviceManager::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto& device : devices)
    {
        if (IsDeviceSuitable(device))
        {
            m_physicalDevice = device;
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void VulkanDeviceManager::CreateLogicalDevice()
{
    QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

    if (m_enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    VkPhysicalDeviceSynchronization2Features sync2{};
    sync2.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &sync2;
    features2.features.samplerAnisotropy = VK_TRUE;

    createInfo.pNext = &features2;

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    assert(m_graphicsQueue == VK_NULL_HANDLE);
    assert(m_presentQueue == VK_NULL_HANDLE);
    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

void VulkanDeviceManager::DestroyLogicalDevice()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
}

void VulkanDeviceManager::CreateSwapChain()
{
    m_pSurfaceManager->CreateSwapChain();
}

void VulkanDeviceManager::DestroySwapChain()
{
    m_pSurfaceManager->DestroySwapChain();
}

bool VulkanDeviceManager::CheckValidationLayerSupport(const std::vector<const char*>& validationLayers)
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers)
    {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanDeviceManager::GetRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (m_enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void VulkanDeviceManager::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = VulkanDeviceManager::debugCallback;
}

void VulkanDeviceManager::CreateInstance()
{
    assert(m_instance == VK_NULL_HANDLE);

    if (m_enableValidationLayers && !CheckValidationLayerSupport(m_validationLayers))
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();

        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create instance!");
    }

    // Set up debug messenger for validation Layer for the instance.
    SetupDebugMessenger();
}

void VulkanDeviceManager::DestroyInstance()
{
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDeviceManager::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::cerr << "[Validation layer] " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

VkResult VulkanDeviceManager::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VulkanDeviceManager::SetupDebugMessenger()
{
    if (!m_enableValidationLayers) return;

    assert(m_instance != VK_NULL_HANDLE);

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    PopulateDebugMessengerCreateInfo(createInfo);

    if (this->CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void VulkanDeviceManager::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

void VulkanDeviceManager::Destroy()
{
    DestroyCommandPool();

    DestroyLogicalDevice();

    DestroyDeviceExtensionNames();

    glfwDestroyWindow(m_pSurfaceManager->Window());

    delete m_pSurfaceManager;
    m_pSurfaceManager = nullptr;

    if (m_enableValidationLayers)
    {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }
    DestroyInstance();

    DestroyValidationLayerNames();
    glfwTerminate();
}

VkImageView VulkanDeviceManager::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image    = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = format;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VkImageView imageView;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void VulkanDeviceManager::CreateCommandPool()
{
    assert(m_commandPool == VK_NULL_HANDLE);

    QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics command pool!");
    }
}

void VulkanDeviceManager::DestroyCommandPool()
{
    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
}

void VulkanDeviceManager::CreateCommandBuffers(
    VkCommandBuffer*     pCommandBuffers,
    uint32_t             count,
    VkCommandBufferLevel level)
{
    assert(pCommandBuffers != nullptr);
    assert(m_commandPool != VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;

    if (vkAllocateCommandBuffers(m_device, &allocInfo, pCommandBuffers) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}
