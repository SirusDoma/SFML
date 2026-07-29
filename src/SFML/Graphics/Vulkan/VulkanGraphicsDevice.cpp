////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2026 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>
#include <SFML/Graphics/Vulkan/VulkanRenderTargetImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanRenderTextureImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanRenderWindowImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanShaderImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanTextureImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanVertexBufferImpl.hpp>

#include <SFML/Window/Vulkan.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <memory>
#include <mutex>
#include <ostream>
#include <string_view>
#include <vector>
#include <vma/vk_mem_alloc.h>

#include <cstring>

// The built-in shaders live in DefaultShader.hlsl, compiled to SPIR-V at build
// time when a compiler is available, with committed bytecode as the fallback
#ifdef SFML_VULKAN_PRECOMPILED_SHADERS
#include <VulkanDefaultFragmentShader.hpp>
#include <VulkanDefaultVertexShader.hpp>
#else
#include <SFML/Graphics/Vulkan/VulkanDefaultShaderSpirv.hpp>
#endif


namespace
{
// Convert an sf::BlendMode::Factor to the corresponding Vulkan blend factor.
// Unlike Direct3D, Vulkan accepts color factors in the alpha slots, where the
// alpha channel of the factor is used, so a single conversion covers both.
VkBlendFactor factorToVulkan(sf::BlendMode::Factor blendFactor)
{
    // clang-format off
    switch (blendFactor)
    {
        case sf::BlendMode::Factor::Zero:             return VK_BLEND_FACTOR_ZERO;
        case sf::BlendMode::Factor::One:              return VK_BLEND_FACTOR_ONE;
        case sf::BlendMode::Factor::SrcColor:         return VK_BLEND_FACTOR_SRC_COLOR;
        case sf::BlendMode::Factor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case sf::BlendMode::Factor::DstColor:         return VK_BLEND_FACTOR_DST_COLOR;
        case sf::BlendMode::Factor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case sf::BlendMode::Factor::SrcAlpha:         return VK_BLEND_FACTOR_SRC_ALPHA;
        case sf::BlendMode::Factor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case sf::BlendMode::Factor::DstAlpha:         return VK_BLEND_FACTOR_DST_ALPHA;
        case sf::BlendMode::Factor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    }
    // clang-format on

    return VK_BLEND_FACTOR_ZERO;
}

// Convert an sf::BlendMode::Equation to the corresponding blend operation
VkBlendOp equationToVulkan(sf::BlendMode::Equation blendEquation)
{
    // clang-format off
    switch (blendEquation)
    {
        case sf::BlendMode::Equation::Add:             return VK_BLEND_OP_ADD;
        case sf::BlendMode::Equation::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case sf::BlendMode::Equation::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case sf::BlendMode::Equation::Min:             return VK_BLEND_OP_MIN;
        case sf::BlendMode::Equation::Max:             return VK_BLEND_OP_MAX;
    }
    // clang-format on

    return VK_BLEND_OP_ADD;
}

// Convert an sf::StencilComparison to the corresponding comparison operation
VkCompareOp stencilFunctionToVulkan(sf::StencilComparison comparison)
{
    // clang-format off
    switch (comparison)
    {
        case sf::StencilComparison::Never:        return VK_COMPARE_OP_NEVER;
        case sf::StencilComparison::Less:         return VK_COMPARE_OP_LESS;
        case sf::StencilComparison::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case sf::StencilComparison::Greater:      return VK_COMPARE_OP_GREATER;
        case sf::StencilComparison::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case sf::StencilComparison::Equal:        return VK_COMPARE_OP_EQUAL;
        case sf::StencilComparison::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case sf::StencilComparison::Always:       return VK_COMPARE_OP_ALWAYS;
    }
    // clang-format on

    return VK_COMPARE_OP_ALWAYS;
}

// Convert an sf::StencilUpdateOperation to the corresponding stencil operation.
// The OpenGL backend uses the clamping GL_INCR/GL_DECR, so the clamping variants match.
VkStencilOp stencilOperationToVulkan(sf::StencilUpdateOperation operation)
{
    // clang-format off
    switch (operation)
    {
        case sf::StencilUpdateOperation::Keep:      return VK_STENCIL_OP_KEEP;
        case sf::StencilUpdateOperation::Zero:      return VK_STENCIL_OP_ZERO;
        case sf::StencilUpdateOperation::Replace:   return VK_STENCIL_OP_REPLACE;
        case sf::StencilUpdateOperation::Increment: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case sf::StencilUpdateOperation::Decrement: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case sf::StencilUpdateOperation::Invert:    return VK_STENCIL_OP_INVERT;
    }
    // clang-format on

    return VK_STENCIL_OP_KEEP;
}

// Pack a blend mode and color write flag into a cache key
std::uint32_t packBlendMode(const sf::BlendMode& mode, bool colorWrite)
{
    return static_cast<std::uint32_t>(mode.colorSrcFactor) | (static_cast<std::uint32_t>(mode.colorDstFactor) << 4) |
           (static_cast<std::uint32_t>(mode.colorEquation) << 8) |
           (static_cast<std::uint32_t>(mode.alphaSrcFactor) << 12) |
           (static_cast<std::uint32_t>(mode.alphaDstFactor) << 16) |
           (static_cast<std::uint32_t>(mode.alphaEquation) << 20) | (colorWrite ? 1u << 24 : 0u);
}

// Pack a stencil mode into a cache key (the reference value is dynamic state, not part of the key)
// Pack the parts of a stencil mode a pipeline is built from. The mask and the
// reference are dynamic state, so they are deliberately left out: including
// them would create a distinct pipeline per value for identical bytecode.
std::uint32_t packStencilMode(const sf::StencilMode& mode)
{
    const bool enabled = !(mode == sf::StencilMode());
    return (static_cast<std::uint32_t>(mode.stencilComparison) << 8) |
           (static_cast<std::uint32_t>(mode.stencilUpdateOperation) << 12) | (enabled ? 1u << 16 : 0u);
}

// Combine a value into a hash
void hashCombine(std::size_t& seed, std::size_t value)
{
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}

// Byte capacity newly created transient chunks have at least
constexpr std::size_t transientChunkSize = 1024 * 1024;

// Result of the one-time run-time availability probe. The probe is reachable
// from any thread through sf::isRendererAvailable and sf::getAvailableRenderers,
// so it is guarded rather than left to whichever caller arrives first.
enum class ProbeResult
{
    Unknown,
    Available,
    Unavailable
};

ProbeResult probeResult = ProbeResult::Unknown;
std::mutex  probeMutex;
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
std::size_t VulkanGraphicsDevice::RenderPassKeyHasher::operator()(const RenderPassKey& key) const
{
    std::size_t seed = 0;
    hashCombine(seed, static_cast<std::size_t>(key.colorFormat));
    hashCombine(seed, static_cast<std::size_t>(key.depthStencilFormat));
    hashCombine(seed, key.sampleCount);
    hashCombine(seed, key.hasResolve ? 1u : 0u);
    hashCombine(seed, key.generalLayout ? 1u : 0u);
    hashCombine(seed, static_cast<std::size_t>(key.colorLoad));
    hashCombine(seed, static_cast<std::size_t>(key.stencilLoad));
    hashCombine(seed, static_cast<std::size_t>(key.colorInitialLayout));
    hashCombine(seed, static_cast<std::size_t>(key.dsInitialLayout));
    return seed;
}


////////////////////////////////////////////////////////////
std::size_t VulkanGraphicsDevice::FramebufferKeyHasher::operator()(const FramebufferKey& key) const
{
    std::size_t seed = 0;
    hashCombine(seed, vulkanHandleHash(key.renderPass));
    hashCombine(seed, vulkanHandleHash(key.colorView));
    hashCombine(seed, vulkanHandleHash(key.resolveView));
    hashCombine(seed, vulkanHandleHash(key.depthStencilView));
    hashCombine(seed, key.width);
    hashCombine(seed, key.height);
    return seed;
}


////////////////////////////////////////////////////////////
std::size_t VulkanGraphicsDevice::PipelineKeyHasher::operator()(const PipelineKey& key) const
{
    std::size_t seed = 0;
    hashCombine(seed, key.shaderId);
    hashCombine(seed, key.blendKey);
    hashCombine(seed, key.stencilKey);
    hashCombine(seed, static_cast<std::size_t>(key.topology));
    hashCombine(seed, static_cast<std::size_t>(key.colorFormat));
    hashCombine(seed, static_cast<std::size_t>(key.depthStencilFormat));
    hashCombine(seed, key.sampleCount);
    hashCombine(seed, key.generalLayout ? 1u : 0u);
    return seed;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::isAvailable()
{
    const std::lock_guard lock(probeMutex);

    if (probeResult != ProbeResult::Unknown)
        return probeResult == ProbeResult::Available;

    probeResult = ProbeResult::Unavailable;

    // The window module checks the loader and the surface extensions
    if (!sf::Vulkan::isAvailable(true))
        return false;

    VulkanFunctions functions;
    if (!functions.loadLoaderEntryPoints())
        return false;

    // The loader itself has to speak Vulkan 1.2
    std::uint32_t instanceVersion = 0;
    if ((functions.vkEnumerateInstanceVersion(&instanceVersion) != VK_SUCCESS) || (instanceVersion < VK_API_VERSION_1_2))
        return false;

    // Look for a physical device fit for the backend
    const auto& extensions = sf::Vulkan::getGraphicsRequiredInstanceExtensions();

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo        = &applicationInfo;
    instanceInfo.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.data();

    VkInstance instance = VK_NULL_HANDLE;
    if (functions.vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS)
        return false;

    bool deviceFound = false;
    if (functions.loadInstanceEntryPoints(instance))
    {
        std::uint32_t deviceCount = 0;
        functions.vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        std::vector<VkPhysicalDevice> devices(deviceCount);
        functions.vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties deviceProperties{};
            functions.vkGetPhysicalDeviceProperties(device, &deviceProperties);
            if (deviceProperties.apiVersion < VK_API_VERSION_1_2)
                continue;

            std::uint32_t familyCount = 0;
            functions.vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);

            std::vector<VkQueueFamilyProperties> families(familyCount);
            functions.vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

            const bool hasGraphics = std::any_of(families.begin(),
                                                 families.end(),
                                                 [](const VkQueueFamilyProperties& family)
                                                 { return (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0; });
            if (!hasGraphics)
                continue;

            std::uint32_t extensionCount = 0;
            functions.vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> extensionProperties(extensionCount);
            functions.vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensionProperties.data());

            const bool hasSwapchain = std::any_of(extensionProperties.begin(),
                                                  extensionProperties.end(),
                                                  [](const VkExtensionProperties& properties) {
                                                      return std::string_view(properties.extensionName) ==
                                                             VK_KHR_SWAPCHAIN_EXTENSION_NAME;
                                                  });
            if (!hasSwapchain)
                continue;

            deviceFound = true;
            break;
        }
    }

    // The probe owns the instance whether or not the entry points resolved;
    // the destructor itself is one of them, so it has to be checked
    if (functions.vkDestroyInstance)
        functions.vkDestroyInstance(instance, nullptr);

    if (deviceFound)
        probeResult = ProbeResult::Available;

    return deviceFound;
}


////////////////////////////////////////////////////////////
VulkanGraphicsDevice::VulkanGraphicsDevice()
{
    if (!createDevice())
    {
        err() << "Failed to create the Vulkan device" << std::endl;
        return;
    }

    createPipeline();
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::createDevice()
{
    if (!m_fn.loadLoaderEntryPoints())
        return false;

    // Create the instance with the surface extensions of the platform
    const auto& extensions = sf::Vulkan::getGraphicsRequiredInstanceExtensions();

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "SFML";
    applicationInfo.pEngineName      = "SFML";
    applicationInfo.apiVersion       = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo        = &applicationInfo;
    instanceInfo.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.data();

#ifdef SFML_DEBUG
    // Enable the validation layer when it is installed, mirroring the Direct3D debug layer
    constexpr std::array validationLayers = {"VK_LAYER_KHRONOS_validation"};
    instanceInfo.enabledLayerCount        = static_cast<std::uint32_t>(validationLayers.size());
    instanceInfo.ppEnabledLayerNames      = validationLayers.data();

    if (m_fn.vkCreateInstance(&instanceInfo, nullptr, &m_instance) != VK_SUCCESS)
    {
        instanceInfo.enabledLayerCount   = 0;
        instanceInfo.ppEnabledLayerNames = nullptr;
        m_instance                       = VK_NULL_HANDLE;
    }
#endif

    if (!m_instance && !vkCheck(m_fn.vkCreateInstance(&instanceInfo, nullptr, &m_instance)))
        return false;

    if (!m_fn.loadInstanceEntryPoints(m_instance))
        return false;

    // Pick the most capable physical device fit for the backend
    std::uint32_t deviceCount = 0;
    m_fn.vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    m_fn.vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    int bestScore = -1;
    for (VkPhysicalDevice candidate : devices)
    {
        VkPhysicalDeviceProperties candidateProperties{};
        m_fn.vkGetPhysicalDeviceProperties(candidate, &candidateProperties);
        if (candidateProperties.apiVersion < VK_API_VERSION_1_2)
            continue;

        std::uint32_t familyCount = 0;
        m_fn.vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);

        std::vector<VkQueueFamilyProperties> families(familyCount);
        m_fn.vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

        std::uint32_t graphicsFamily = familyCount;
        for (std::uint32_t i = 0; i < familyCount; ++i)
        {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphicsFamily = i;
                break;
            }
        }

        if (graphicsFamily == familyCount)
            continue;

        std::uint32_t extensionCount = 0;
        m_fn.vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> extensionProperties(extensionCount);
        m_fn.vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, extensionProperties.data());

        const bool hasSwapchain = std::any_of(extensionProperties.begin(),
                                              extensionProperties.end(),
                                              [](const VkExtensionProperties& properties) {
                                                  return std::string_view(properties.extensionName) ==
                                                         VK_KHR_SWAPCHAIN_EXTENSION_NAME;
                                              });
        if (!hasSwapchain)
            continue;

        // clang-format off
        int score = 0;
        switch (candidateProperties.deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   score = 4; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 3; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    score = 2; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            score = 1; break;
            default:                                     score = 0; break;
        }
        // clang-format on

        if (score > bestScore)
        {
            bestScore          = score;
            m_physicalDevice   = candidate;
            m_queueFamilyIndex = graphicsFamily;
        }
    }

    if (!m_physicalDevice)
    {
        err() << "No Vulkan 1.2 device with a graphics queue and swapchain support was found" << std::endl;
        return false;
    }

    m_fn.vkGetPhysicalDeviceProperties(m_physicalDevice, &m_properties);

    // Enable the optional features the backend can make use of
    VkPhysicalDeviceFeatures supportedFeatures{};
    m_fn.vkGetPhysicalDeviceFeatures(m_physicalDevice, &supportedFeatures);
    m_features.geometryShader = supportedFeatures.geometryShader;

    const float queuePriority = 1.f;

    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Present-wait lets low-latency windows pace to the moment their frame
    // reaches the screen, the Vulkan analog of the DXGI waitable object
    {
        std::uint32_t extensionCount = 0;
        m_fn.vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> extensionProperties(extensionCount);
        m_fn.vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extensionCount, extensionProperties.data());

        const auto hasExtension = [&](const char* name)
        {
            return std::any_of(extensionProperties.begin(),
                               extensionProperties.end(),
                               [&](const VkExtensionProperties& properties)
                               { return std::string_view(properties.extensionName) == name; });
        };

        bool wantPresentWait = hasExtension(VK_KHR_PRESENT_ID_EXTENSION_NAME) &&
                               hasExtension(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
        bool wantMaintenance5 = hasExtension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME) &&
                                hasExtension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

        // An advertised extension does not by itself guarantee its feature
        // bits, only enable what the device actually reports
        if (wantPresentWait || wantMaintenance5)
        {
            VkPhysicalDevicePresentIdFeaturesKHR presentIdQuery{};
            presentIdQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;

            VkPhysicalDevicePresentWaitFeaturesKHR presentWaitQuery{};
            presentWaitQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
            presentWaitQuery.pNext = &presentIdQuery;

            VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingQuery{};
            dynamicRenderingQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
            dynamicRenderingQuery.pNext = &presentWaitQuery;

            VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5Query{};
            maintenance5Query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
            maintenance5Query.pNext = &dynamicRenderingQuery;

            VkPhysicalDeviceFeatures2 featuresQuery{};
            featuresQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            featuresQuery.pNext = &maintenance5Query;

            m_fn.vkGetPhysicalDeviceFeatures2(m_physicalDevice, &featuresQuery);

            wantPresentWait = wantPresentWait && presentIdQuery.presentId && presentWaitQuery.presentWait;
            wantMaintenance5 = wantMaintenance5 && maintenance5Query.maintenance5 && dynamicRenderingQuery.dynamicRendering;
        }

        if (wantPresentWait)
        {
            deviceExtensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
            deviceExtensions.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
            m_hasPresentWait = true;
        }

        // maintenance5 gives points a default size of 1.0 when the shader does
        // not write one, matching Direct3D semantics for the shared HLSL user
        // shaders; dynamic rendering is a dependency of the extension
        if (wantMaintenance5)
        {
            deviceExtensions.push_back(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
            deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
            m_hasMaintenance5 = true;
        }
    }

    void* featureChain = nullptr;

    VkPhysicalDevicePresentIdFeaturesKHR presentIdFeatures{};
    presentIdFeatures.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
    presentIdFeatures.presentId = VK_TRUE;

    VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeatures{};
    presentWaitFeatures.sType       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
    presentWaitFeatures.presentWait = VK_TRUE;

    if (m_hasPresentWait)
    {
        presentIdFeatures.pNext   = featureChain;
        presentWaitFeatures.pNext = &presentIdFeatures;
        featureChain              = &presentWaitFeatures;
    }

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5Features{};
    maintenance5Features.sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
    maintenance5Features.maintenance5 = VK_TRUE;

    if (m_hasMaintenance5)
    {
        dynamicRenderingFeatures.pNext = featureChain;
        maintenance5Features.pNext     = &dynamicRenderingFeatures;
        featureChain                   = &maintenance5Features;
    }

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext                   = featureChain;
    deviceInfo.queueCreateInfoCount    = 1;
    deviceInfo.pQueueCreateInfos       = &queueInfo;
    deviceInfo.enabledExtensionCount   = static_cast<std::uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceInfo.pEnabledFeatures        = &m_features;

    if (!vkCheck(m_fn.vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device)))
        return false;

    if (!m_fn.loadDeviceEntryPoints(m_device))
        return false;

    m_fn.vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);

    // Create the memory allocator on top of the dynamically loaded entry points
    VmaVulkanFunctions vmaFunctions{};
    vmaFunctions.vkGetInstanceProcAddr                   = m_fn.vkGetInstanceProcAddr;
    vmaFunctions.vkGetDeviceProcAddr                     = m_fn.vkGetDeviceProcAddr;
    vmaFunctions.vkGetPhysicalDeviceProperties           = m_fn.vkGetPhysicalDeviceProperties;
    vmaFunctions.vkGetPhysicalDeviceMemoryProperties     = m_fn.vkGetPhysicalDeviceMemoryProperties;
    vmaFunctions.vkAllocateMemory                        = m_fn.vkAllocateMemory;
    vmaFunctions.vkFreeMemory                            = m_fn.vkFreeMemory;
    vmaFunctions.vkMapMemory                             = m_fn.vkMapMemory;
    vmaFunctions.vkUnmapMemory                           = m_fn.vkUnmapMemory;
    vmaFunctions.vkFlushMappedMemoryRanges               = m_fn.vkFlushMappedMemoryRanges;
    vmaFunctions.vkInvalidateMappedMemoryRanges          = m_fn.vkInvalidateMappedMemoryRanges;
    vmaFunctions.vkBindBufferMemory                      = m_fn.vkBindBufferMemory;
    vmaFunctions.vkBindImageMemory                       = m_fn.vkBindImageMemory;
    vmaFunctions.vkGetBufferMemoryRequirements           = m_fn.vkGetBufferMemoryRequirements;
    vmaFunctions.vkGetImageMemoryRequirements            = m_fn.vkGetImageMemoryRequirements;
    vmaFunctions.vkCreateBuffer                          = m_fn.vkCreateBuffer;
    vmaFunctions.vkDestroyBuffer                         = m_fn.vkDestroyBuffer;
    vmaFunctions.vkCreateImage                           = m_fn.vkCreateImage;
    vmaFunctions.vkDestroyImage                          = m_fn.vkDestroyImage;
    vmaFunctions.vkCmdCopyBuffer                         = m_fn.vkCmdCopyBuffer;
    vmaFunctions.vkGetBufferMemoryRequirements2KHR       = m_fn.vkGetBufferMemoryRequirements2;
    vmaFunctions.vkGetImageMemoryRequirements2KHR        = m_fn.vkGetImageMemoryRequirements2;
    vmaFunctions.vkBindBufferMemory2KHR                  = m_fn.vkBindBufferMemory2;
    vmaFunctions.vkBindImageMemory2KHR                   = m_fn.vkBindImageMemory2;
    vmaFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = m_fn.vkGetPhysicalDeviceMemoryProperties2;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice   = m_physicalDevice;
    allocatorInfo.device           = m_device;
    allocatorInfo.instance         = m_instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.pVulkanFunctions = &vmaFunctions;

    if (!vkCheck(vmaCreateAllocator(&allocatorInfo, &m_allocator)))
        return false;

    // Create the per-frame recording resources
    for (FrameSlot& slot : m_frameSlots)
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = m_queueFamilyIndex;

        if (!vkCheck(m_fn.vkCreateCommandPool(m_device, &poolInfo, nullptr, &slot.commandPool)))
            return false;

        VkCommandBufferAllocateInfo bufferInfo{};
        bufferInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.commandPool        = slot.commandPool;
        bufferInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        bufferInfo.commandBufferCount = 1;

        if (!vkCheck(m_fn.vkAllocateCommandBuffers(m_device, &bufferInfo, &slot.commandBuffer)))
            return false;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        if (!vkCheck(m_fn.vkCreateFence(m_device, &fenceInfo, nullptr, &slot.fence)))
            return false;
    }

    return true;
}


////////////////////////////////////////////////////////////
VulkanGraphicsDevice::~VulkanGraphicsDevice()
{
    const ContextLock lock(*this);

    if (m_device)
        m_fn.vkDeviceWaitIdle(m_device);

    for (FrameSlot& slot : m_frameSlots)
    {
        slot.submitted = false;
        recycleFrameSlot(slot);

        for (FrameSlot::TransientChunk& chunk : slot.transientChunks)
            vmaDestroyBuffer(m_allocator, chunk.buffer, chunk.allocation);
        slot.transientChunks.clear();

        for (VkDescriptorPool pool : slot.descriptorPools)
            m_fn.vkDestroyDescriptorPool(m_device, pool, nullptr);
        slot.descriptorPools.clear();

        if (slot.fence)
            m_fn.vkDestroyFence(m_device, slot.fence, nullptr);
        if (slot.commandPool)
            m_fn.vkDestroyCommandPool(m_device, slot.commandPool, nullptr);
    }

    for (const auto& [key, pipeline] : m_pipelines)
        m_fn.vkDestroyPipeline(m_device, pipeline, nullptr);

    for (const auto& [key, framebuffer] : m_framebuffers)
        m_fn.vkDestroyFramebuffer(m_device, framebuffer, nullptr);

    for (const auto& [key, renderPass] : m_renderPasses)
        m_fn.vkDestroyRenderPass(m_device, renderPass, nullptr);

    for (VkSampler sampler : m_samplerStates)
    {
        if (sampler)
            m_fn.vkDestroySampler(m_device, sampler, nullptr);
    }

    if (m_fanIndexBuffer)
        vmaDestroyBuffer(m_allocator, m_fanIndexBuffer, m_fanIndexBufferAllocation);

    if (m_whiteImageView)
        m_fn.vkDestroyImageView(m_device, m_whiteImageView, nullptr);
    if (m_whiteImage)
        vmaDestroyImage(m_allocator, m_whiteImage, m_whiteImageAllocation);

    for (VkDescriptorPool pool : m_builtinSetPools)
        m_fn.vkDestroyDescriptorPool(m_device, pool, nullptr);
    m_builtinSetPools.clear();
    m_builtinSets.clear();

    if (m_builtinPipelineLayout)
        m_fn.vkDestroyPipelineLayout(m_device, m_builtinPipelineLayout, nullptr);
    if (m_builtinSetLayout)
        m_fn.vkDestroyDescriptorSetLayout(m_device, m_builtinSetLayout, nullptr);
    if (m_defaultVertexShader)
        m_fn.vkDestroyShaderModule(m_device, m_defaultVertexShader, nullptr);
    if (m_defaultFragmentShader)
        m_fn.vkDestroyShaderModule(m_device, m_defaultFragmentShader, nullptr);

    if (m_allocator)
        vmaDestroyAllocator(m_allocator);
    if (m_device)
        m_fn.vkDestroyDevice(m_device, nullptr);
    if (m_instance)
        m_fn.vkDestroyInstance(m_instance, nullptr);
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::createPipeline()
{
    // Both the build-time-compiled headers and the committed fallback declare
    // the same arrays, only the included file differs
    const std::uint32_t* vertexShaderCode   = defaultVertexShaderSpirv;
    const std::size_t    vertexShaderSize   = sizeof(defaultVertexShaderSpirv);
    const std::uint32_t* fragmentShaderCode = defaultFragmentShaderSpirv;
    const std::size_t    fragmentShaderSize = sizeof(defaultFragmentShaderSpirv);

    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = vertexShaderSize;
    moduleInfo.pCode    = vertexShaderCode;

    if (!vkCheck(m_fn.vkCreateShaderModule(m_device, &moduleInfo, nullptr, &m_defaultVertexShader)))
        return;

    moduleInfo.codeSize = fragmentShaderSize;
    moduleInfo.pCode    = fragmentShaderCode;

    if (!vkCheck(m_fn.vkCreateShaderModule(m_device, &moduleInfo, nullptr, &m_defaultFragmentShader)))
        return;

    // Descriptor set layout of the built-in pipeline: the draw's texture at
    // binding 16 and its sampler at binding 32, matching the register-to-binding
    // convention of the backend. The matrices arrive as push constants, so the
    // set only changes when the texture does
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding         = 16;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding         = 32;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (!vkCheck(m_fn.vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_builtinSetLayout)))
        return;

    const VkPushConstantRange matricesRange = builtinMatricesRange();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &m_builtinSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &matricesRange;

    if (!vkCheck(m_fn.vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_builtinPipelineLayout)))
        return;

    // 1x1 white texture for untextured draws
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent        = {1, 1, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (!vkCheck(vmaCreateImage(m_allocator, &imageInfo, &allocationInfo, &m_whiteImage, &m_whiteImageAllocation, nullptr)))
        return;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                       = m_whiteImage;
    viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                      = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    if (!vkCheck(m_fn.vkCreateImageView(m_device, &viewInfo, nullptr, &m_whiteImageView)))
        return;

    // Upload the white pixel through a transient staging allocation
    constexpr std::uint32_t whitePixel = 0xFFFFFFFF;

    VulkanTransientAllocation staging;
    VkCommandBuffer           commandBuffer = currentCommandBuffer();
    if (!commandBuffer || !allocateTransient(&whitePixel, sizeof(whitePixel), 4, staging))
        return;

    vulkanImageBarrier(m_fn,
                       commandBuffer,
                       m_whiteImage,
                       VK_IMAGE_ASPECT_COLOR_BIT,
                       VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset                = staging.offset;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent                 = {1, 1, 1};

    m_fn.vkCmdCopyBufferToImage(commandBuffer, staging.buffer, m_whiteImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    vulkanImageBarrier(m_fn,
                       commandBuffer,
                       m_whiteImage,
                       VK_IMAGE_ASPECT_COLOR_BIT,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    commitCommandBuffer(true);
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderTargetImpl> VulkanGraphicsDevice::createRenderTargetImpl()
{
    return std::make_unique<VulkanRenderTargetImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderTextureImpl> VulkanGraphicsDevice::createRenderTextureImpl()
{
    return std::make_unique<VulkanRenderTextureImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderWindowImpl> VulkanGraphicsDevice::createRenderWindowImpl(WindowHandle           handle,
                                                                               const ContextSettings& settings,
                                                                               unsigned int           bitsPerPixel)
{
    return std::make_unique<VulkanRenderWindowImpl>(*this, handle, settings, bitsPerPixel);
}


////////////////////////////////////////////////////////////
std::unique_ptr<ShaderImpl> VulkanGraphicsDevice::createShaderImpl()
{
    return std::make_unique<VulkanShaderImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<TextureImpl> VulkanGraphicsDevice::createTextureImpl()
{
    return std::make_unique<VulkanTextureImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<VertexBufferImpl> VulkanGraphicsDevice::createVertexBufferImpl()
{
    return std::make_unique<VulkanVertexBufferImpl>(*this);
}


////////////////////////////////////////////////////////////
unsigned int VulkanGraphicsDevice::getMaximumAntiAliasingLevel()
{
    return clampAntiAliasingLevel(64);
}


////////////////////////////////////////////////////////////
unsigned int VulkanGraphicsDevice::getMaximumTextureSize()
{
    return m_device ? m_properties.limits.maxImageDimension2D : 0;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::isShaderAvailable()
{
    return m_device != VK_NULL_HANDLE;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::isGeometryShaderAvailable()
{
    return m_device && m_features.geometryShader;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::isVertexBufferAvailable()
{
    return m_device != VK_NULL_HANDLE;
}


////////////////////////////////////////////////////////////
Renderer VulkanGraphicsDevice::getRenderer() const
{
    return Renderer::Vulkan;
}


////////////////////////////////////////////////////////////
ShadingLanguage VulkanGraphicsDevice::getShadingLanguage() const
{
#ifdef SFML_VULKAN_RUNTIME_SHADER_COMPILER
    return ShadingLanguage::Hlsl;
#else
    return ShadingLanguage::SpirV;
#endif
}


////////////////////////////////////////////////////////////
const VulkanFunctions& VulkanGraphicsDevice::fn() const
{
    return m_fn;
}


////////////////////////////////////////////////////////////
VkInstance VulkanGraphicsDevice::getInstance() const
{
    return m_instance;
}


////////////////////////////////////////////////////////////
VkPhysicalDevice VulkanGraphicsDevice::getPhysicalDevice() const
{
    return m_physicalDevice;
}


////////////////////////////////////////////////////////////
VkDevice VulkanGraphicsDevice::getDevice() const
{
    return m_device;
}


////////////////////////////////////////////////////////////
VkQueue VulkanGraphicsDevice::getQueue() const
{
    return m_queue;
}


////////////////////////////////////////////////////////////
std::uint32_t VulkanGraphicsDevice::getQueueFamilyIndex() const
{
    return m_queueFamilyIndex;
}


////////////////////////////////////////////////////////////
VmaAllocator VulkanGraphicsDevice::getAllocator() const
{
    return m_allocator;
}


////////////////////////////////////////////////////////////
std::size_t VulkanGraphicsDevice::getUniformBufferAlignment() const
{
    return std::max<std::size_t>(static_cast<std::size_t>(m_properties.limits.minUniformBufferOffsetAlignment), 16);
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::hasPresentWait() const
{
    return m_hasPresentWait;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::hasMaintenance5() const
{
    return m_hasMaintenance5;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::hasStencilAttachment() const
{
    return m_attachments.depthStencilFormat != VK_FORMAT_UNDEFINED;
}


////////////////////////////////////////////////////////////
Vector2u VulkanGraphicsDevice::getAttachmentSize() const
{
    return m_attachments.size;
}


////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
VkPushConstantRange VulkanGraphicsDevice::builtinMatricesRange()
{
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.offset     = 0;
    range.size       = 32 * sizeof(float); // model-view projection and texture matrix

    return range;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::pushBuiltinMatrices(VkPipelineLayout layout)
{
    // The shader consumes one combined matrix, which keeps the payload within
    // the 128 bytes of push constants every implementation guarantees
    std::array<float, 32> matrices{};

    const float* modelView  = m_pending.constants.data();
    const float* projection = m_pending.constants.data() + 16;

    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.f;
            for (int i = 0; i < 4; ++i)
                sum += projection[i * 4 + row] * modelView[column * 4 + i];

            matrices[static_cast<std::size_t>(column * 4 + row)] = sum;
        }
    }

    std::memcpy(matrices.data() + 16, m_pending.constants.data() + 32, 16 * sizeof(float));

    if (m_pushedValid && (m_pushedLayout == layout) && (matrices == m_pushedMatrices))
        return;

    m_fn.vkCmdPushConstants(m_frameSlots[m_currentSlot].commandBuffer,
                            layout,
                            VK_SHADER_STAGE_VERTEX_BIT,
                            0,
                            static_cast<std::uint32_t>(matrices.size() * sizeof(float)),
                            matrices.data());

    m_pushedMatrices = matrices;
    m_pushedLayout   = layout;
    m_pushedValid    = true;
}


////////////////////////////////////////////////////////////
unsigned int VulkanGraphicsDevice::clampAntiAliasingLevel(unsigned int level) const
{
    if (!m_device)
        return 1;

    const VkSampleCountFlags supported = m_properties.limits.framebufferColorSampleCounts &
                                         m_properties.limits.framebufferStencilSampleCounts;

    unsigned int samples = std::min(level, 64u);

    // Sample counts are powers of two, round down to one
    while (samples > 1)
    {
        const auto bit = static_cast<VkSampleCountFlags>(samples);
        if (((samples & (samples - 1)) == 0) && ((supported & bit) != 0))
            return samples;

        --samples;
    }

    return 1;
}


////////////////////////////////////////////////////////////
VkImageLayout VulkanGraphicsDevice::getTrackedImageLayout(VkImage image) const
{
    const ContextLock lock(*this);

    const auto it = m_imageLayouts.find(image);
    return (it != m_imageLayouts.end()) ? it->second : VK_IMAGE_LAYOUT_UNDEFINED;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setTrackedImageLayout(VkImage image, VkImageLayout layout)
{
    const ContextLock lock(*this);

    m_imageLayouts[image] = layout;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::forgetImage(VkImage image)
{
    const ContextLock lock(*this);

    m_imageLayouts.erase(image);
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::purgeFramebuffers(VkImageView view)
{
    const ContextLock lock(*this);

    if (!view)
        return;

    for (auto it = m_framebuffers.begin(); it != m_framebuffers.end();)
    {
        if ((it->first.colorView == view) || (it->first.resolveView == view) || (it->first.depthStencilView == view))
        {
            // The framebuffer may still be referenced by the recording command
            // buffer or in-flight commands, destroy it once the frame recycles
            deferSlot().deadFramebuffers.push_back(it->second);
            it = m_framebuffers.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // State applied without a draw, like a scissored clear, feeds the pending
    // texture straight into a fresh set: leaving the dying view pending would
    // re-populate the cache with it right after this purge emptied it
    if (m_pending.textureView == view)
    {
        m_pending.textureView   = m_whiteImageView;
        m_pending.textureLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        m_appliedTextureValid   = false;
    }

    // The built-in sets outlive frames, so a dying view has to drop the sets
    // sampling it; they too can still be referenced by in-flight commands
    for (auto it = m_builtinSets.begin(); it != m_builtinSets.end();)
    {
        if (it->first.view == view)
        {
            if (m_builtinDescriptorSet == it->second.second)
            {
                m_builtinDescriptorSet   = VK_NULL_HANDLE;
                m_appliedDescriptorValid = false;
                m_appliedTextureValid    = false;
            }

            deferSlot().deadBuiltinSets.push_back(it->second);
            it = m_builtinSets.erase(it);
        }
        else
        {
            ++it;
        }
    }
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::bindSurface(VulkanRenderSurface* surface)
{
    const ContextLock lock(*this);

    if (m_currentSurface == surface)
        return;

    // Pending work belongs to the previous surface
    flushPendingDraws();
    materializePendingClears();
    endEncoding();

    m_currentSurface = surface;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::unbindSurface(VulkanRenderSurface* surface)
{
    const ContextLock lock(*this);

    if (m_currentSurface != surface)
        return;

    flushPendingDraws();
    materializePendingClears();
    endEncoding();

    m_currentSurface        = nullptr;
    m_currentRenderTargetId = 0;
}


////////////////////////////////////////////////////////////
VulkanRenderSurface* VulkanGraphicsDevice::getCurrentSurface() const
{
    return m_currentSurface;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setCurrentRenderTargetId(std::uint64_t id)
{
    const ContextLock lock(*this);

    m_currentRenderTargetId = id;
}


////////////////////////////////////////////////////////////
std::uint64_t VulkanGraphicsDevice::getCurrentRenderTargetId() const
{
    const ContextLock lock(*this);

    return m_currentRenderTargetId;
}


////////////////////////////////////////////////////////////
VkImageView VulkanGraphicsDevice::getCurrentTextureView() const
{
    const ContextLock lock(*this);

    return m_pending.textureView;
}


////////////////////////////////////////////////////////////
VkSampler VulkanGraphicsDevice::getCurrentTextureSampler() const
{
    const ContextLock lock(*this);

    return m_pending.textureSampler;
}


////////////////////////////////////////////////////////////
VkImageLayout VulkanGraphicsDevice::getCurrentTextureLayout() const
{
    const ContextLock lock(*this);

    return m_pending.textureLayout;
}


////////////////////////////////////////////////////////////
VkSampler VulkanGraphicsDevice::getSamplerState(bool smooth, bool repeated, bool mipmapped)
{
    const ContextLock lock(*this);

    auto& sampler = m_samplerStates[(smooth ? 1u : 0u) | (repeated ? 2u : 0u) | (mipmapped ? 4u : 0u)];
    if (!sampler && m_device)
    {
        const VkSamplerAddressMode addressMode = repeated ? VK_SAMPLER_ADDRESS_MODE_REPEAT
                                                          : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        const VkFilter             filter      = smooth ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = filter;
        samplerInfo.minFilter    = filter;
        samplerInfo.addressModeU = addressMode;
        samplerInfo.addressModeV = addressMode;
        samplerInfo.addressModeW = addressMode;

        if (mipmapped)
        {
            // Matches the GL_LINEAR_MIPMAP_LINEAR / GL_NEAREST_MIPMAP_LINEAR filters of the OpenGL backend
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.maxLod     = VK_LOD_CLAMP_NONE;
        }
        else
        {
            // A zero maxLod hides the extra levels of textures whose mipmap was invalidated
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.maxLod     = 0.f;
        }

        vkCheck(m_fn.vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler));
    }

    return sampler;
}


////////////////////////////////////////////////////////////
VkImageView VulkanGraphicsDevice::getWhiteTextureView() const
{
    return m_whiteImageView;
}


////////////////////////////////////////////////////////////
VkBuffer VulkanGraphicsDevice::getTriangleFanIndexBuffer(std::size_t vertexCount, std::size_t& indexCount)
{
    const ContextLock lock(*this);

    if (!m_device || (vertexCount < 3))
        return VK_NULL_HANDLE;

    indexCount = (vertexCount - 2) * 3;

    // Grow the shared pattern, fans of any size draw a prefix of the same index list
    if (vertexCount > m_fanIndexBufferVertices)
    {
        const std::size_t newVertices = std::max<std::size_t>({vertexCount, m_fanIndexBufferVertices * 2, 1024});

        std::vector<std::uint32_t> indices((newVertices - 2) * 3);
        for (std::size_t i = 0; i < newVertices - 2; ++i)
        {
            indices[i * 3 + 0] = 0;
            indices[i * 3 + 1] = static_cast<std::uint32_t>(i + 1);
            indices[i * 3 + 2] = static_cast<std::uint32_t>(i + 2);
        }

        // The old buffer may still be referenced by recorded commands
        if (m_fanIndexBuffer)
            deferDestroyBuffer(m_fanIndexBuffer, m_fanIndexBufferAllocation);
        m_fanIndexBuffer           = VK_NULL_HANDLE;
        m_fanIndexBufferAllocation = nullptr;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size  = sizeof(std::uint32_t) * indices.size();
        bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo mappedInfo{};
        if (!vkCheck(
                vmaCreateBuffer(m_allocator, &bufferInfo, &allocationInfo, &m_fanIndexBuffer, &m_fanIndexBufferAllocation, &mappedInfo)))
        {
            m_fanIndexBuffer         = VK_NULL_HANDLE;
            m_fanIndexBufferVertices = 0;
            return VK_NULL_HANDLE;
        }

        std::memcpy(mappedInfo.pMappedData, indices.data(), sizeof(std::uint32_t) * indices.size());
        vmaFlushAllocation(m_allocator, m_fanIndexBufferAllocation, 0, VK_WHOLE_SIZE);

        m_fanIndexBufferVertices = newVertices;
    }

    return m_fanIndexBuffer;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingBlendMode(const BlendMode& mode, bool colorWrite)
{
    const ContextLock lock(*this);

    const std::uint32_t key = packBlendMode(mode, colorWrite);
    if (m_pending.blendKey == key)
        return;

    flushPendingDraws();
    m_pending.blendMode  = mode;
    m_pending.colorWrite = colorWrite;
    m_pending.blendKey   = key;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingStencilMode(const StencilMode& mode)
{
    const ContextLock lock(*this);

    // Compared in full: the mask and the reference are not part of the key
    if (m_pending.stencilMode == mode)
        return;

    flushPendingDraws();
    m_pending.stencilMode = mode;
    m_pending.stencilKey  = packStencilMode(mode);
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingTexture(VkImageView view, VkSampler sampler, VkImageLayout layout)
{
    const ContextLock lock(*this);

    if ((m_pending.textureView == view) && (m_pending.textureSampler == sampler))
        return;

    flushPendingDraws();
    m_pending.textureView    = view;
    m_pending.textureSampler = sampler;
    m_pending.textureLayout  = layout;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingViewport(const VkViewport& viewport)
{
    const ContextLock lock(*this);

    if (std::memcmp(&m_pending.viewport, &viewport, sizeof(viewport)) == 0)
        return;

    flushPendingDraws();
    m_pending.viewport = viewport;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingScissor(bool enabled, const VkRect2D& rect)
{
    const ContextLock lock(*this);

    if ((m_pending.scissorEnabled == enabled) && (std::memcmp(&m_pending.scissorRect, &rect, sizeof(rect)) == 0))
        return;

    flushPendingDraws();
    m_pending.scissorEnabled = enabled;
    m_pending.scissorRect    = rect;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingConstants(const std::array<float, 48>& constants)
{
    const ContextLock lock(*this);

    if (std::memcmp(m_pending.constants.data(), constants.data(), sizeof(constants)) == 0)
        return;

    flushPendingDraws();
    m_pending.constants = constants;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingUserShader(const void* shader, std::uint64_t stateId)
{
    // Set on every draw, and draws only happen on the rendering thread, which
    // always observes its own writes, so the unchanged case can skip the lock
    if ((m_pending.userShader == shader) && (m_pending.userShaderState == stateId))
        return;

    const ContextLock lock(*this);

    flushPendingDraws();
    m_pending.userShader      = shader;
    m_pending.userShaderState = stateId;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingUserPipelineSource(
    VkShaderModule   vertexModule,
    VkShaderModule   pointVertexModule,
    VkShaderModule   geometryModule,
    VkShaderModule   fragmentModule,
    VkPipelineLayout layout,
    std::uint32_t    shaderId)
{
    const ContextLock lock(*this);

    m_userVertexModule      = vertexModule;
    m_userPointVertexModule = pointVertexModule;
    m_userGeometryModule    = geometryModule;
    m_userFragmentModule    = fragmentModule;
    m_userPipelineLayout    = layout;
    m_userShaderId          = shaderId;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::clearShaderPipelines(std::uint32_t shaderId)
{
    const ContextLock lock(*this);

    if (!m_device)
        return;

    for (auto it = m_pipelines.begin(); it != m_pipelines.end();)
    {
        if (it->first.shaderId == shaderId)
        {
            // The pipeline may still be referenced by the recording command
            // buffer or in-flight commands, destroy it once the frame recycles
            deferSlot().deadPipelines.push_back(it->second);
            it = m_pipelines.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (m_appliedPipelineValid && (m_appliedPipelineKey.shaderId == shaderId))
        m_appliedPipelineValid = false;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingClearColor(Color color)
{
    const ContextLock lock(*this);

    // Already-drawn content is overwritten by the clear, the open pass ends
    // and the clear becomes the load operation of the next one
    flushPendingDraws();
    endEncoding();

    m_pendingColorClear = true;
    m_pendingClearColor = color;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::setPendingClearStencil(std::uint8_t value)
{
    const ContextLock lock(*this);

    flushPendingDraws();
    endEncoding();

    m_pendingStencilClear      = true;
    m_pendingStencilClearValue = value;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::ensureRenderPass()
{
    if (m_passOpen)
        return true;

    if (!m_device || !m_currentSurface)
        return false;

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    if (!commandBuffer)
        return false;

    if (!m_currentSurface->prepareAttachments(m_attachments))
        return false;

    // The load operations carry out the pending clears; content is loaded
    // when the attachment has been rendered to before, and dropped when the
    // image is fresh and its content undefined anyway
    const VkImageLayout colorLayout = getTrackedImageLayout(m_attachments.colorImage);

    VkAttachmentLoadOp colorLoad    = VK_ATTACHMENT_LOAD_OP_LOAD;
    VkImageLayout      colorInitial = colorLayout;
    if (m_pendingColorClear)
    {
        colorLoad    = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorInitial = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    else if (colorLayout == VK_IMAGE_LAYOUT_UNDEFINED)
    {
        colorLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    VkAttachmentLoadOp stencilLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkImageLayout      dsInitial   = VK_IMAGE_LAYOUT_UNDEFINED;
    if (m_attachments.depthStencilImage)
    {
        dsInitial = getTrackedImageLayout(m_attachments.depthStencilImage);

        if (m_pendingStencilClear)
            stencilLoad = VK_ATTACHMENT_LOAD_OP_CLEAR;
        else if (dsInitial != VK_IMAGE_LAYOUT_UNDEFINED)
            stencilLoad = VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    VkRenderPass renderPass = getRenderPass(m_attachments.colorFormat,
                                            m_attachments.depthStencilFormat,
                                            m_attachments.sampleCount,
                                            false,
                                            m_attachments.colorGeneralLayout,
                                            colorLoad,
                                            stencilLoad,
                                            colorInitial,
                                            dsInitial);
    if (!renderPass)
        return false;

    VkFramebuffer framebuffer = getFramebuffer(renderPass, m_attachments);
    if (!framebuffer)
        return false;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{m_pendingClearColor.r / 255.f,
                                    m_pendingClearColor.g / 255.f,
                                    m_pendingClearColor.b / 255.f,
                                    m_pendingClearColor.a / 255.f}};
    clearValues[1].depthStencil = {1.f, m_pendingStencilClearValue};

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass        = renderPass;
    beginInfo.framebuffer       = framebuffer;
    beginInfo.renderArea.extent = {m_attachments.size.x, m_attachments.size.y};
    beginInfo.clearValueCount   = static_cast<std::uint32_t>(clearValues.size());
    beginInfo.pClearValues      = clearValues.data();

    m_fn.vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Track where the pass leaves the attachments
    const VkImageLayout colorFinal = m_attachments.colorGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL
                                                                      : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    setTrackedImageLayout(m_attachments.colorImage, colorFinal);
    if (m_attachments.depthStencilImage)
        setTrackedImageLayout(m_attachments.depthStencilImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    m_pendingColorClear   = false;
    m_pendingStencilClear = false;
    m_passOpen            = true;
    m_openRenderPass      = renderPass;

    // The dynamic state and bindings of the command buffer survive pass
    // boundaries, only the pipeline's render pass compatibility is per-pass
    return true;
}


////////////////////////////////////////////////////////////
VkRenderPass VulkanGraphicsDevice::getRenderPass(
    VkFormat           colorFormat,
    VkFormat           depthStencilFormat,
    unsigned int       sampleCount,
    bool               hasResolve,
    bool               generalLayout,
    VkAttachmentLoadOp colorLoad,
    VkAttachmentLoadOp stencilLoad,
    VkImageLayout      colorInitialLayout,
    VkImageLayout      dsInitialLayout)
{
    const ContextLock lock(*this);

    if (!m_device)
        return VK_NULL_HANDLE;

    const RenderPassKey key{colorFormat,
                            depthStencilFormat,
                            sampleCount,
                            hasResolve,
                            generalLayout,
                            colorLoad,
                            stencilLoad,
                            colorInitialLayout,
                            dsInitialLayout};

    const auto it = m_renderPasses.find(key);
    if (it != m_renderPasses.end())
        return it->second;

    const VkImageLayout colorPassLayout = generalLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentDescription, 3> attachments{};
    std::uint32_t                          attachmentCount = 1;

    attachments[0].format         = colorFormat;
    attachments[0].samples        = static_cast<VkSampleCountFlagBits>(sampleCount);
    attachments[0].loadOp         = colorLoad;
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout  = colorInitialLayout;
    attachments[0].finalLayout    = colorPassLayout;

    VkAttachmentReference colorReference{0, colorPassLayout};

    VkAttachmentReference depthStencilReference{};
    if (depthStencilFormat != VK_FORMAT_UNDEFINED)
    {
        auto& depthStencil = attachments[attachmentCount];

        depthStencil.format  = depthStencilFormat;
        depthStencil.samples = static_cast<VkSampleCountFlagBits>(sampleCount);
        // Depth is only used by raw user code, keep whatever it holds once initialized
        depthStencil.loadOp         = (dsInitialLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                                                                                     : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthStencil.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        depthStencil.stencilLoadOp  = stencilLoad;
        depthStencil.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthStencil.initialLayout  = dsInitialLayout;
        depthStencil.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthStencilReference = {attachmentCount, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        ++attachmentCount;
    }

    VkAttachmentReference resolveReference{};
    if (hasResolve)
    {
        auto& resolve = attachments[attachmentCount];

        resolve.format         = colorFormat;
        resolve.samples        = VK_SAMPLE_COUNT_1_BIT;
        resolve.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        resolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        resolve.finalLayout    = colorPassLayout;

        resolveReference = {attachmentCount, colorPassLayout};
        ++attachmentCount;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorReference;
    subpass.pResolveAttachments     = hasResolve ? &resolveReference : nullptr;
    subpass.pDepthStencilAttachment = (depthStencilFormat != VK_FORMAT_UNDEFINED) ? &depthStencilReference : nullptr;

    // Order attachment access against surrounding passes and transfers
    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass   = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass   = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |
                                    VK_ACCESS_TRANSFER_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachmentCount;
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    renderPassInfo.pDependencies   = dependencies.data();

    VkRenderPass renderPass = VK_NULL_HANDLE;
    if (!vkCheck(m_fn.vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &renderPass)))
        return VK_NULL_HANDLE; // Not cached: a later attempt may well succeed

    return m_renderPasses.emplace(key, renderPass).first->second;
}


////////////////////////////////////////////////////////////
VkFramebuffer VulkanGraphicsDevice::getFramebuffer(VkRenderPass renderPass, const VulkanSurfaceAttachments& attachments)
{
    const FramebufferKey key{renderPass,
                             attachments.colorView,
                             attachments.resolveView,
                             attachments.depthStencilView,
                             attachments.size.x,
                             attachments.size.y};

    const auto it = m_framebuffers.find(key);
    if (it != m_framebuffers.end())
        return it->second;

    std::array<VkImageView, 3> views{};
    std::uint32_t              viewCount = 0;

    views[viewCount++] = attachments.colorView;
    if (attachments.depthStencilView)
        views[viewCount++] = attachments.depthStencilView;
    if (attachments.resolveView)
        views[viewCount++] = attachments.resolveView;

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = renderPass;
    framebufferInfo.attachmentCount = viewCount;
    framebufferInfo.pAttachments    = views.data();
    framebufferInfo.width           = attachments.size.x;
    framebufferInfo.height          = attachments.size.y;
    framebufferInfo.layers          = 1;

    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    if (!vkCheck(m_fn.vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &framebuffer)))
        return VK_NULL_HANDLE;

    return m_framebuffers.emplace(key, framebuffer).first->second;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::applyPendingState()
{
    const ContextLock lock(*this);

    if (!ensureRenderPass())
        return false;

    VkCommandBuffer commandBuffer = m_frameSlots[m_currentSlot].commandBuffer;

    // Decide whether the draw that made the user shader pending has to bind it.
    // A user shader's descriptor set replaces the built-in one at set 0, so the
    // built-in set has to be re-bound after any user shader was in use.
    //
    // The draw's texture is written into that set when it is bound, unlike the
    // Direct3D 11 backend where it stays a separate binding, so a texture
    // change has to re-bind the shader even when the shader itself is unchanged
    const bool userTextureChanged = (m_pending.userShader != nullptr) &&
                                    (!m_appliedTextureValid || (m_applied.textureView != m_pending.textureView) ||
                                     (m_applied.textureSampler != m_pending.textureSampler));

    if (!m_appliedValid || (m_applied.userShader != m_pending.userShader) ||
        (m_applied.userShaderState != m_pending.userShaderState) || userTextureChanged)
    {
        m_userShaderBindPending = (m_pending.userShader != nullptr);
        if (!m_pending.userShader)
            m_userShaderId = 0;

        if (!m_appliedValid || (m_applied.userShader != m_pending.userShader))
            m_appliedDescriptorValid = false;
    }

    // The viewport is expressed in top-left coordinates and flipped here so the
    // clip space Y axis points up like it does on the other backends, letting
    // every backend share the same projection matrices
    if (!m_appliedValid || (std::memcmp(&m_applied.viewport, &m_pending.viewport, sizeof(m_pending.viewport)) != 0))
    {
        VkViewport flipped = m_pending.viewport;
        flipped.y          = m_pending.viewport.y + m_pending.viewport.height;
        flipped.height     = -m_pending.viewport.height;

        // Zero-sized viewports are invalid in Vulkan but reachable through
        // SFML: a minimized window reports a zero size and views accept empty
        // viewport rectangles. Substitute the smallest legal viewport, the
        // equally empty scissor rectangle already rasterizes nothing
        if (flipped.width <= 0.f)
            flipped.width = 1.f;
        if (flipped.height == 0.f)
            flipped.height = -1.f;

        m_fn.vkCmdSetViewport(commandBuffer, 0, 1, &flipped);
    }

    // Scissor testing cannot be disabled on Vulkan, the whole attachment is the
    // effective rectangle while it is off
    VkRect2D effectiveScissor{};
    if (m_pending.scissorEnabled)
    {
        effectiveScissor = m_pending.scissorRect;
    }
    else
    {
        effectiveScissor.extent = {m_attachments.size.x, m_attachments.size.y};
    }

    if (!m_appliedValid || (std::memcmp(&m_applied.scissorRect, &effectiveScissor, sizeof(effectiveScissor)) != 0))
        m_fn.vkCmdSetScissor(commandBuffer, 0, 1, &effectiveScissor);

    // Stencil reference and masks are dynamic state
    if (!m_appliedValid || (m_applied.stencilMode.stencilReference.value != m_pending.stencilMode.stencilReference.value))
        m_fn.vkCmdSetStencilReference(commandBuffer,
                                      VK_STENCIL_FACE_FRONT_AND_BACK,
                                      m_pending.stencilMode.stencilReference.value);

    if (!m_appliedValid || (m_applied.stencilMode.stencilMask.value != m_pending.stencilMode.stencilMask.value))
    {
        m_fn.vkCmdSetStencilCompareMask(commandBuffer,
                                        VK_STENCIL_FACE_FRONT_AND_BACK,
                                        m_pending.stencilMode.stencilMask.value & 0xFFu);
        m_fn.vkCmdSetStencilWriteMask(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0xFF);
    }

    // The built-in descriptors only fit the built-in pipeline layout, a user
    // shader binds its own descriptor set covering all its resources
    if (!m_pending.userShader)
    {
        if (!bindBuiltinDescriptors())
            return false;
    }

    m_applied             = m_pending;
    m_applied.scissorRect = effectiveScissor;
    m_appliedValid        = true;
    m_appliedTextureValid = true;

    return true;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::uploadPendingConstants(VulkanTransientAllocation& allocation)
{
    const ContextLock lock(*this);

    if (!m_builtinConstantsValid || !m_constantsBuffer ||
        (std::memcmp(m_uploadedConstants.data(), m_pending.constants.data(), sizeof(m_pending.constants)) != 0))
    {
        VulkanTransientAllocation fresh;
        if (!allocateTransient(m_pending.constants.data(), sizeof(m_pending.constants), getUniformBufferAlignment(), fresh))
            return false;

        m_constantsBuffer       = fresh.buffer;
        m_constantsOffset       = fresh.offset;
        m_uploadedConstants     = m_pending.constants;
        m_builtinConstantsValid = true;
    }

    allocation.buffer = m_constantsBuffer;
    allocation.offset = m_constantsOffset;

    return true;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::bindBuiltinDescriptors()
{
    VkCommandBuffer commandBuffer = m_frameSlots[m_currentSlot].commandBuffer;

    // State can be applied before any draw set a texture, like by a clear or
    // by raw interop code; descriptors cannot hold null handles
    if (!m_pending.textureView)
    {
        m_pending.textureView   = m_whiteImageView;
        m_pending.textureLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (!m_pending.textureSampler)
        m_pending.textureSampler = getSamplerState(false, false);

    // The set only covers the texture and its sampler, the matrices are pushed
    // as constants; sets survive their frame, so a texture is only ever written
    // into a set once and alternating textures cost a re-bind and nothing else
    if (!m_builtinDescriptorSet || !m_appliedTextureValid || (m_builtinSetView != m_pending.textureView) ||
        (m_builtinSetSampler != m_pending.textureSampler))
    {
        VkDescriptorSet set = getBuiltinDescriptorSet(m_pending.textureView, m_pending.textureLayout, m_pending.textureSampler);
        if (!set)
            return false;

        m_builtinDescriptorSet   = set;
        m_builtinSetView         = m_pending.textureView;
        m_builtinSetSampler      = m_pending.textureSampler;
        m_appliedDescriptorValid = false;
    }

    if (!m_appliedDescriptorValid)
    {
        m_fn.vkCmdBindDescriptorSets(commandBuffer,
                                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     m_builtinPipelineLayout,
                                     0,
                                     1,
                                     &m_builtinDescriptorSet,
                                     0,
                                     nullptr);
        m_appliedDescriptorValid = true;
    }

    return true;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::takeUserShaderBindPending()
{
    const ContextLock lock(*this);

    const bool pending      = m_userShaderBindPending;
    m_userShaderBindPending = false;

    return pending;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::bindDrawPipeline(VkPrimitiveTopology topology)
{
    const ContextLock lock(*this);

    if (!m_passOpen)
        return false;

    const bool userShader = (m_pending.userShader != nullptr) && (m_userShaderId != 0);

    // Stencil testing needs a stencil attachment, the mode is dropped without one
    const std::uint32_t stencilKey = (m_attachments.depthStencilFormat != VK_FORMAT_UNDEFINED)
                                         ? m_pending.stencilKey
                                         : packStencilMode(StencilMode());

    const PipelineKey key{userShader ? m_userShaderId : 0,
                          m_pending.blendKey,
                          stencilKey,
                          topology,
                          m_attachments.colorFormat,
                          m_attachments.depthStencilFormat,
                          m_attachments.sampleCount,
                          m_attachments.colorGeneralLayout};

    // Point pipelines use the vertex module variant that writes the PointSize
    // builtin when the shader has one; with a geometry stage that stage is the
    // one before rasterization and carries the size instead (see VulkanShaderImpl)
    VkShaderModule userVertexModule = m_userVertexModule;
    if ((topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST) && m_userPointVertexModule && !m_userGeometryModule)
        userVertexModule = m_userPointVertexModule;

    VkPipelineLayout layout = userShader ? m_userPipelineLayout : m_builtinPipelineLayout;

    if (!m_appliedPipelineValid || !(key == m_appliedPipelineKey))
    {
        VkPipeline pipeline = getPipeline(key,
                                          userShader ? userVertexModule : VK_NULL_HANDLE,
                                          userShader ? m_userGeometryModule : VK_NULL_HANDLE,
                                          userShader ? m_userFragmentModule : VK_NULL_HANDLE,
                                          layout);
        if (!pipeline)
            return false;

        m_fn.vkCmdBindPipeline(m_frameSlots[m_currentSlot].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        m_appliedPipelineKey   = key;
        m_appliedPipelineValid = true;
    }

    // The built-in vertex stage reads its matrices from push constants; it also
    // serves a user shader that only replaces the fragment stage, whose layout
    // declares the same range
    if (!userShader || !userVertexModule)
        pushBuiltinMatrices(layout);

    return true;
}


////////////////////////////////////////////////////////////
VkPipeline VulkanGraphicsDevice::getPipeline(
    const PipelineKey& key,
    VkShaderModule     vertexModule,
    VkShaderModule     geometryModule,
    VkShaderModule     fragmentModule,
    VkPipelineLayout   layout)
{
    const auto it = m_pipelines.find(key);
    if (it != m_pipelines.end())
        return it->second;

    if (!vertexModule)
        vertexModule = m_defaultVertexShader;
    if (!fragmentModule)
        fragmentModule = m_defaultFragmentShader;

    if (!m_device || !vertexModule || !fragmentModule || !layout)
        return VK_NULL_HANDLE;

    // User shaders are compiled with the `main` entry point, the built-in
    // modules keep their HLSL entry names
    std::array<VkPipelineShaderStageCreateInfo, 3> stages{};
    std::uint32_t                                  stageCount = 0;

    stages[stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[stageCount].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[stageCount].module = vertexModule;
    stages[stageCount].pName  = (vertexModule == m_defaultVertexShader) ? "VSMain" : "main";
    ++stageCount;

    if (geometryModule)
    {
        stages[stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[stageCount].stage  = VK_SHADER_STAGE_GEOMETRY_BIT;
        stages[stageCount].module = geometryModule;
        stages[stageCount].pName  = "main";
        ++stageCount;
    }

    stages[stageCount].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[stageCount].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[stageCount].module = fragmentModule;
    stages[stageCount].pName  = (fragmentModule == m_defaultFragmentShader) ? "PSMain" : "main";
    ++stageCount;

    // Vertex layout matching sf::Vertex
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    attributes[1] = {1, 0, VK_FORMAT_R8G8B8A8_UNORM, 8};
    attributes[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, 12};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions    = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = key.topology;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode    = VK_CULL_MODE_NONE;
    rasterization.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth   = 1.f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = static_cast<VkSampleCountFlagBits>(key.sampleCount);

    // Depth is never tested by SFML, the stencil test replicates the packed mode
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    const bool stencilEnabled = (key.stencilKey & (1u << 16)) != 0;
    if (stencilEnabled && (key.depthStencilFormat != VK_FORMAT_UNDEFINED))
    {
        const auto comparison = static_cast<StencilComparison>((key.stencilKey >> 8) & 0xFu);
        const auto operation  = static_cast<StencilUpdateOperation>((key.stencilKey >> 12) & 0xFu);

        depthStencil.stencilTestEnable = VK_TRUE;

        VkStencilOpState stencilOp{};
        stencilOp.failOp      = VK_STENCIL_OP_KEEP;
        stencilOp.passOp      = stencilOperationToVulkan(operation);
        stencilOp.depthFailOp = stencilOperationToVulkan(operation);
        stencilOp.compareOp   = stencilFunctionToVulkan(comparison);

        depthStencil.front = stencilOp;
        depthStencil.back  = stencilOp;
    }

    // Unpack the blend mode baked into the key
    const auto colorSrc   = static_cast<BlendMode::Factor>(key.blendKey & 0xFu);
    const auto colorDst   = static_cast<BlendMode::Factor>((key.blendKey >> 4) & 0xFu);
    const auto colorEq    = static_cast<BlendMode::Equation>((key.blendKey >> 8) & 0xFu);
    const auto alphaSrc   = static_cast<BlendMode::Factor>((key.blendKey >> 12) & 0xFu);
    const auto alphaDst   = static_cast<BlendMode::Factor>((key.blendKey >> 16) & 0xFu);
    const auto alphaEq    = static_cast<BlendMode::Equation>((key.blendKey >> 20) & 0xFu);
    const bool colorWrite = (key.blendKey & (1u << 24)) != 0;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable         = VK_TRUE;
    blendAttachment.srcColorBlendFactor = factorToVulkan(colorSrc);
    blendAttachment.dstColorBlendFactor = factorToVulkan(colorDst);
    blendAttachment.colorBlendOp        = equationToVulkan(colorEq);
    blendAttachment.srcAlphaBlendFactor = factorToVulkan(alphaSrc);
    blendAttachment.dstAlphaBlendFactor = factorToVulkan(alphaDst);
    blendAttachment.alphaBlendOp        = equationToVulkan(alphaEq);
    blendAttachment.colorWriteMask      = colorWrite ? (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
                                                     : 0;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments    = &blendAttachment;

    constexpr std::array dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                          VK_DYNAMIC_STATE_SCISSOR,
                                          VK_DYNAMIC_STATE_STENCIL_REFERENCE,
                                          VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
                                          VK_DYNAMIC_STATE_STENCIL_WRITE_MASK};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    // Any load-op variant of the pass is compatible, pipelines are created
    // against the canonical one
    VkRenderPass renderPass = getRenderPass(key.colorFormat,
                                            key.depthStencilFormat,
                                            key.sampleCount,
                                            false,
                                            key.generalLayout,
                                            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                            VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_UNDEFINED);
    if (!renderPass)
        return VK_NULL_HANDLE;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = stageCount;
    pipelineInfo.pStages             = stages.data();
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState   = &multisample;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlend;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = layout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (!vkCheck(m_fn.vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline)))
        return VK_NULL_HANDLE;

    return m_pipelines.emplace(key, pipeline).first->second;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::invalidatePipeline()
{
    const ContextLock lock(*this);

    m_appliedValid              = false;
    m_appliedTextureValid       = false;
    m_appliedPipelineValid      = false;
    m_appliedDescriptorValid    = false;
    m_builtinConstantsValid     = false;
    m_pushedValid               = false;
    m_builtinDescriptorSet      = VK_NULL_HANDLE;
    m_constantsBuffer           = VK_NULL_HANDLE;
    m_userShaderBindPending     = false;
    m_currentVertexBuffer       = VK_NULL_HANDLE;
    m_currentVertexBufferOffset = 0;
    m_currentIndexBuffer        = VK_NULL_HANDLE;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::invalidateTextureBinding()
{
    const ContextLock lock(*this);

    m_appliedTextureValid = false;
}


////////////////////////////////////////////////////////////
VkCommandBuffer VulkanGraphicsDevice::getRenderCommandBuffer() const
{
    return (m_recording && m_passOpen) ? m_frameSlots[m_currentSlot].commandBuffer : VK_NULL_HANDLE;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::uploadVertices(const void* vertices, std::size_t vertexCount, std::size_t& firstVertex)
{
    const ContextLock lock(*this);

    if (!m_recording)
        return false;

    // Aligning to the vertex stride lets the draw address the data by vertex
    // index with the chunk bound at offset zero, so consecutive uploads into
    // the same chunk skip re-binding the vertex buffer
    VulkanTransientAllocation allocation;
    if (!allocateTransient(vertices, sizeof(Vertex) * vertexCount, sizeof(Vertex), allocation))
        return false;

    bindVertexBuffer(allocation.buffer, 0);
    firstVertex = allocation.offset / sizeof(Vertex);

    return true;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::bindVertexBuffer(VkBuffer buffer)
{
    bindVertexBuffer(buffer, 0);
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::bindVertexBuffer(VkBuffer buffer, std::size_t offset)
{
    const ContextLock lock(*this);

    if (!m_recording || ((m_currentVertexBuffer == buffer) && (m_currentVertexBufferOffset == offset)))
        return;

    const VkDeviceSize bufferOffset = offset;
    m_fn.vkCmdBindVertexBuffers(m_frameSlots[m_currentSlot].commandBuffer, 0, 1, &buffer, &bufferOffset);

    m_currentVertexBuffer       = buffer;
    m_currentVertexBufferOffset = offset;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::bindIndexBuffer(VkBuffer buffer)
{
    const ContextLock lock(*this);

    if (!m_recording || (m_currentIndexBuffer == buffer))
        return;

    m_fn.vkCmdBindIndexBuffer(m_frameSlots[m_currentSlot].commandBuffer, buffer, 0, VK_INDEX_TYPE_UINT32);

    m_currentIndexBuffer = buffer;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::allocateTransient(const void*                data,
                                             std::size_t                size,
                                             std::size_t                alignment,
                                             VulkanTransientAllocation& allocation)
{
    const ContextLock lock(*this);

    if (!m_device || !m_recording)
        return false;

    FrameSlot& slot = m_frameSlots[m_currentSlot];

    FrameSlot::TransientChunk* chunk = slot.transientChunks.empty() ? nullptr : &slot.transientChunks.back();

    // Alignments are not restricted to powers of two, vertex data aligns to
    // its stride so draws can address it by vertex index
    std::size_t alignedCursor = 0;
    if (chunk)
    {
        alignedCursor = chunk->cursor;
        if (const std::size_t misalignment = alignedCursor % alignment)
            alignedCursor += alignment - misalignment;
        if (alignedCursor + size > chunk->capacity)
            chunk = nullptr;
    }

    if (!chunk)
    {
        const std::size_t capacity = std::max(size, transientChunkSize);

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size  = capacity;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        FrameSlot::TransientChunk newChunk;
        VmaAllocationInfo         allocationInfo{};
        if (!vkCheck(
                vmaCreateBuffer(m_allocator, &bufferInfo, &allocationCreateInfo, &newChunk.buffer, &newChunk.allocation, &allocationInfo)))
            return false;

        // Host-coherent memory needs no flushing after writes, which every
        // desktop device provides; the flush call stays for the rest
        VkMemoryPropertyFlags memoryFlags = 0;
        vmaGetAllocationMemoryProperties(m_allocator, newChunk.allocation, &memoryFlags);

        newChunk.mapped   = allocationInfo.pMappedData;
        newChunk.capacity = capacity;
        newChunk.coherent = (memoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

        slot.transientChunks.push_back(newChunk);
        chunk         = &slot.transientChunks.back();
        alignedCursor = 0;
    }

    std::memcpy(static_cast<std::byte*>(chunk->mapped) + alignedCursor, data, size);
    if (!chunk->coherent)
        vmaFlushAllocation(m_allocator, chunk->allocation, alignedCursor, size);

    allocation.buffer = chunk->buffer;
    allocation.offset = alignedCursor;
    chunk->cursor     = alignedCursor + size;

    return true;
}


////////////////////////////////////////////////////////////
VkDescriptorSet VulkanGraphicsDevice::getBuiltinDescriptorSet(VkImageView view, VkImageLayout layout, VkSampler sampler)
{
    const ContextLock lock(*this);

    const FrameSlot::BuiltinSetKey key{view, sampler};

    const auto cachedItem = m_builtinSets.find(key);
    if (cachedItem != m_builtinSets.end())
        return cachedItem->second.second;

    if (!m_device)
        return VK_NULL_HANDLE;

    // The sets are freed individually when the texture they reference dies,
    // and the pools they come from are never reset for a frame
    VkDescriptorSet set = VK_NULL_HANDLE;

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (!m_builtinSetPools.empty())
        {
            VkDescriptorSetAllocateInfo allocateInfo{};
            allocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocateInfo.descriptorPool     = m_builtinSetPools.back();
            allocateInfo.descriptorSetCount = 1;
            allocateInfo.pSetLayouts        = &m_builtinSetLayout;

            const VkResult result = m_fn.vkAllocateDescriptorSets(m_device, &allocateInfo, &set);
            if (result == VK_SUCCESS)
                break;

            if ((result != VK_ERROR_OUT_OF_POOL_MEMORY) && (result != VK_ERROR_FRAGMENTED_POOL))
            {
                vkCheck(result);
                return VK_NULL_HANDLE;
            }
        }

        constexpr std::array<VkDescriptorPoolSize, 2> poolSizes = {
            {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256}, {VK_DESCRIPTOR_TYPE_SAMPLER, 256}}};

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = 256;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (!vkCheck(m_fn.vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &pool)))
            return VK_NULL_HANDLE;

        m_builtinSetPools.push_back(pool);
    }

    if (!set)
        return VK_NULL_HANDLE;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView   = view;
    imageInfo.imageLayout = layout;

    VkDescriptorImageInfo samplerInfo{};
    samplerInfo.sampler = sampler;

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = set;
    writes[0].dstBinding      = 16;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[0].pImageInfo      = &imageInfo;
    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = set;
    writes[1].dstBinding      = 32;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[1].pImageInfo      = &samplerInfo;

    m_fn.vkUpdateDescriptorSets(m_device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);

    m_builtinSets.emplace(key, std::make_pair(m_builtinSetPools.back(), set));

    return set;
}


////////////////////////////////////////////////////////////
VkDescriptorSet VulkanGraphicsDevice::allocateDescriptorSet(VkDescriptorSetLayout layout)
{
    const ContextLock lock(*this);

    if (!m_device || !m_recording)
        return VK_NULL_HANDLE;

    FrameSlot& slot = m_frameSlots[m_currentSlot];

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (!slot.descriptorPools.empty())
        {
            VkDescriptorSetAllocateInfo allocateInfo{};
            allocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocateInfo.descriptorPool     = slot.descriptorPools.back();
            allocateInfo.descriptorSetCount = 1;
            allocateInfo.pSetLayouts        = &layout;

            VkDescriptorSet set    = VK_NULL_HANDLE;
            const VkResult  result = m_fn.vkAllocateDescriptorSets(m_device, &allocateInfo, &set);
            if (result == VK_SUCCESS)
                return set;

            if ((result != VK_ERROR_OUT_OF_POOL_MEMORY) && (result != VK_ERROR_FRAGMENTED_POOL))
            {
                vkCheck(result);
                return VK_NULL_HANDLE;
            }
        }

        // The pool ran out, chain a new one
        constexpr std::array<VkDescriptorPoolSize, 5> poolSizes = {
            {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 512},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 512},
             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024},
             {VK_DESCRIPTOR_TYPE_SAMPLER, 1024},
             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256}}};

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets       = 1024;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (!vkCheck(m_fn.vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &pool)))
            return VK_NULL_HANDLE;

        slot.descriptorPools.push_back(pool);
    }

    return VK_NULL_HANDLE;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::appendPendingVertices(const Vertex* vertices, std::size_t vertexCount, VkPrimitiveTopology topology)
{
    const ContextLock lock(*this);

    // Bound the staging memory and the latency of a single merged draw
    constexpr std::size_t maxPendingVertices = 16384;

    if (!m_pendingVertices.empty() &&
        ((topology != m_pendingTopology) || (m_pendingVertices.size() + vertexCount > maxPendingVertices)))
        flushPendingDraws();

    m_pendingTopology = topology;
    m_pendingVertices.insert(m_pendingVertices.end(), vertices, vertices + vertexCount);
    m_hasPendingDraws.store(true, std::memory_order_relaxed);
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::flushPendingDraws()
{
    // Draws only become pending on the rendering thread, which always observes its own
    // appends, so flushes that have nothing to do can skip the lock entirely
    if (!m_hasPendingDraws.load(std::memory_order_relaxed))
        return;

    const ContextLock lock(*this);

    if (m_pendingVertices.empty())
        return;

    if (applyPendingState() && bindDrawPipeline(m_pendingTopology))
    {
        std::size_t firstVertex = 0;
        if (uploadVertices(m_pendingVertices.data(), m_pendingVertices.size(), firstVertex))
            m_fn.vkCmdDraw(m_frameSlots[m_currentSlot].commandBuffer,
                           static_cast<std::uint32_t>(m_pendingVertices.size()),
                           1,
                           static_cast<std::uint32_t>(firstVertex),
                           0);
    }

    m_pendingVertices.clear();
    m_hasPendingDraws.store(false, std::memory_order_relaxed);
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::endEncoding()
{
    const ContextLock lock(*this);

    if (!m_passOpen)
        return;

    VkCommandBuffer commandBuffer = m_frameSlots[m_currentSlot].commandBuffer;

    m_fn.vkCmdEndRenderPass(commandBuffer);
    m_passOpen       = false;
    m_openRenderPass = VK_NULL_HANDLE;

    // Attachments of render textures are sampled and copied after the pass, the
    // subpass dependencies of the pass only order attachment access
    if (m_attachments.colorGeneralLayout)
    {
        vulkanImageBarrier(m_fn,
                           commandBuffer,
                           m_attachments.colorImage,
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_LAYOUT_GENERAL);
    }
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::materializePendingClears()
{
    const ContextLock lock(*this);

    if (!m_pendingColorClear && !m_pendingStencilClear)
        return;

    if (ensureRenderPass())
    {
        endEncoding();
        return;
    }

    // The surface cannot be drawn to at all (minimized, or its swapchain is
    // being re-created). The clears belong to it, so they are dropped rather
    // than left pending for whichever surface is bound next.
    m_pendingColorClear   = false;
    m_pendingStencilClear = false;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::prepareForPresent(bool forceResolvePass)
{
    const ContextLock lock(*this);

    flushPendingDraws();
    materializePendingClears();
    endEncoding();

    // Multisampled content is resolved by an empty pass whose resolve
    // attachment is the presentation target
    if (!m_currentSurface)
        return;

    VulkanSurfaceAttachments attachments;
    if (!forceResolvePass || !m_currentSurface->prepareAttachments(attachments) || !attachments.resolveImage)
        return;

    if (getTrackedImageLayout(attachments.colorImage) == VK_IMAGE_LAYOUT_UNDEFINED)
        return;

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    if (!commandBuffer)
        return;

    VkRenderPass renderPass = getRenderPass(attachments.colorFormat,
                                            attachments.depthStencilFormat,
                                            attachments.sampleCount,
                                            true,
                                            attachments.colorGeneralLayout,
                                            VK_ATTACHMENT_LOAD_OP_LOAD,
                                            (attachments.depthStencilFormat != VK_FORMAT_UNDEFINED &&
                                             getTrackedImageLayout(attachments.depthStencilImage) != VK_IMAGE_LAYOUT_UNDEFINED)
                                                ? VK_ATTACHMENT_LOAD_OP_LOAD
                                                : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                            getTrackedImageLayout(attachments.colorImage),
                                            attachments.depthStencilImage
                                                ? getTrackedImageLayout(attachments.depthStencilImage)
                                                : VK_IMAGE_LAYOUT_UNDEFINED);
    if (!renderPass)
        return;

    VkFramebuffer framebuffer = getFramebuffer(renderPass, attachments);
    if (!framebuffer)
        return;

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass        = renderPass;
    beginInfo.framebuffer       = framebuffer;
    beginInfo.renderArea.extent = {attachments.size.x, attachments.size.y};

    m_fn.vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    m_fn.vkCmdEndRenderPass(commandBuffer);

    const VkImageLayout resolveFinal = attachments.colorGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL
                                                                      : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    setTrackedImageLayout(attachments.resolveImage, resolveFinal);
}


////////////////////////////////////////////////////////////
VkCommandBuffer VulkanGraphicsDevice::currentCommandBuffer()
{
    const ContextLock lock(*this);

    if (!m_device)
        return VK_NULL_HANDLE;

    if (m_recording)
        return m_frameSlots[m_currentSlot].commandBuffer;

    if (!beginFrameSlot())
        return VK_NULL_HANDLE;

    return m_frameSlots[m_currentSlot].commandBuffer;
}


////////////////////////////////////////////////////////////
bool VulkanGraphicsDevice::beginFrameSlot()
{
    FrameSlot& slot = m_frameSlots[m_currentSlot];

    // Wait for the commands of the slot's previous use before its resources are recycled
    if (slot.submitted)
    {
        vkCheck(m_fn.vkWaitForFences(m_device, 1, &slot.fence, VK_TRUE, UINT64_MAX));
        slot.submitted = false;
        vkCheck(m_fn.vkResetFences(m_device, 1, &slot.fence));
    }

    recycleFrameSlot(slot);

    if (!vkCheck(m_fn.vkResetCommandPool(m_device, slot.commandPool, 0)))
        return false;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (!vkCheck(m_fn.vkBeginCommandBuffer(slot.commandBuffer, &beginInfo)))
        return false;

    m_recording = true;

    // A fresh command buffer holds none of the previously applied state, and
    // the transient constants of the recycled slot are gone
    m_appliedValid              = false;
    m_appliedTextureValid       = false;
    m_appliedPipelineValid      = false;
    m_appliedDescriptorValid    = false;
    m_builtinConstantsValid     = false;
    m_pushedValid               = false;
    m_builtinDescriptorSet      = VK_NULL_HANDLE;
    m_constantsBuffer           = VK_NULL_HANDLE;
    m_currentVertexBuffer       = VK_NULL_HANDLE;
    m_currentVertexBufferOffset = 0;
    m_currentIndexBuffer        = VK_NULL_HANDLE;

    return true;
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::recycleFrameSlot(FrameSlot& slot)
{
    // Keep the first chunk to avoid churn, drop the overflow chunks
    if (slot.transientChunks.size() > 1)
    {
        for (std::size_t i = 1; i < slot.transientChunks.size(); ++i)
            vmaDestroyBuffer(m_allocator, slot.transientChunks[i].buffer, slot.transientChunks[i].allocation);
        slot.transientChunks.resize(1);
    }
    if (!slot.transientChunks.empty())
        slot.transientChunks.front().cursor = 0;

    // Keep the first descriptor pool, drop the overflow pools; the sets of
    // the cache died with the pool reset
    if (slot.descriptorPools.size() > 1)
    {
        for (std::size_t i = 1; i < slot.descriptorPools.size(); ++i)
            m_fn.vkDestroyDescriptorPool(m_device, slot.descriptorPools[i], nullptr);
        slot.descriptorPools.resize(1);
    }
    if (!slot.descriptorPools.empty())
        vkCheck(m_fn.vkResetDescriptorPool(m_device, slot.descriptorPools.front(), 0));

    for (const auto& [buffer, allocation] : slot.deadBuffers)
        vmaDestroyBuffer(m_allocator, buffer, allocation);
    slot.deadBuffers.clear();

    for (const auto& [image, view, allocation] : slot.deadImages)
    {
        if (view)
            m_fn.vkDestroyImageView(m_device, view, nullptr);
        vmaDestroyImage(m_allocator, image, allocation);
    }
    slot.deadImages.clear();

    for (VkFramebuffer framebuffer : slot.deadFramebuffers)
        m_fn.vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    slot.deadFramebuffers.clear();

    for (VkPipeline pipeline : slot.deadPipelines)
        m_fn.vkDestroyPipeline(m_device, pipeline, nullptr);
    slot.deadPipelines.clear();

    // After the pipelines built from them, never before
    for (VkPipelineLayout layout : slot.deadPipelineLayouts)
        m_fn.vkDestroyPipelineLayout(m_device, layout, nullptr);
    slot.deadPipelineLayouts.clear();

    for (VkDescriptorSetLayout layout : slot.deadSetLayouts)
        m_fn.vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
    slot.deadSetLayouts.clear();

    for (const auto& [pool, set] : slot.deadBuiltinSets)
        m_fn.vkFreeDescriptorSets(m_device, pool, 1, &set);
    slot.deadBuiltinSets.clear();
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::addAcquireWaitSemaphore(VkSemaphore semaphore)
{
    const ContextLock lock(*this);

    m_acquireWaitSemaphores.push_back(semaphore);
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::commitCommandBuffer(bool waitUntilCompleted, VkSemaphore signalSemaphore)
{
    const ContextLock lock(*this);

    if (!m_device)
        return;

    if (!m_recording)
    {
        // An outstanding semaphore still needs a submission to wait on or signal
        if (m_acquireWaitSemaphores.empty() && !signalSemaphore)
            return;

        if (!currentCommandBuffer())
            return;
    }

    flushPendingDraws();
    endEncoding();

    FrameSlot& slot = m_frameSlots[m_currentSlot];

    if (!vkCheck(m_fn.vkEndCommandBuffer(slot.commandBuffer)))
        return;

    const std::vector<VkPipelineStageFlags> waitStages(m_acquireWaitSemaphores.size(),
                                                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = static_cast<std::uint32_t>(m_acquireWaitSemaphores.size());
    submitInfo.pWaitSemaphores      = m_acquireWaitSemaphores.data();
    submitInfo.pWaitDstStageMask    = waitStages.data();
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &slot.commandBuffer;
    submitInfo.signalSemaphoreCount = signalSemaphore ? 1u : 0u;
    submitInfo.pSignalSemaphores    = signalSemaphore ? &signalSemaphore : nullptr;

    if (vkCheck(m_fn.vkQueueSubmit(m_queue, 1, &submitInfo, slot.fence)))
        slot.submitted = true;

    m_acquireWaitSemaphores.clear();
    m_recording = false;

    if (waitUntilCompleted && slot.submitted)
    {
        vkCheck(m_fn.vkWaitForFences(m_device, 1, &slot.fence, VK_TRUE, UINT64_MAX));
        slot.submitted = false;
        vkCheck(m_fn.vkResetFences(m_device, 1, &slot.fence));
    }
    else
    {
        m_currentSlot = (m_currentSlot + 1) % FrameSlotCount;
    }
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::waitForInFlightFrames(unsigned int maxPending)
{
    const ContextLock lock(*this);

    if (!m_device)
        return;

    unsigned int pending = 0;
    for (const FrameSlot& slot : m_frameSlots)
    {
        if (slot.submitted)
            ++pending;
    }

    // The slot commands are recorded into next is the oldest submission
    for (unsigned int i = 0; (i < FrameSlotCount) && (pending > maxPending); ++i)
    {
        FrameSlot& slot = m_frameSlots[(m_currentSlot + i) % FrameSlotCount];
        if (!slot.submitted)
            continue;

        vkCheck(m_fn.vkWaitForFences(m_device, 1, &slot.fence, VK_TRUE, UINT64_MAX));
        slot.submitted = false;
        vkCheck(m_fn.vkResetFences(m_device, 1, &slot.fence));
        --pending;
    }
}


////////////////////////////////////////////////////////////
VulkanGraphicsDevice::FrameSlot& VulkanGraphicsDevice::deferSlot()
{
    if (!m_recording)
    {
        FrameSlot& previous = m_frameSlots[(m_currentSlot + FrameSlotCount - 1) % FrameSlotCount];
        if (previous.submitted)
            return previous;
    }

    return m_frameSlots[m_currentSlot];
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::deferDestroyBuffer(VkBuffer buffer, VmaAllocation allocation)
{
    const ContextLock lock(*this);

    deferSlot().deadBuffers.emplace_back(buffer, allocation);
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::deferDestroyImage(VkImage image, VkImageView view, VmaAllocation allocation)
{
    const ContextLock lock(*this);

    forgetImage(image);
    purgeFramebuffers(view);
    deferSlot().deadImages.emplace_back(image, view, allocation);
}


////////////////////////////////////////////////////////////
void VulkanGraphicsDevice::deferDestroyShaderLayouts(VkPipelineLayout pipelineLayout, VkDescriptorSetLayout setLayout)
{
    const ContextLock lock(*this);

    FrameSlot& slot = deferSlot();

    if (pipelineLayout)
        slot.deadPipelineLayouts.push_back(pipelineLayout);
    if (setLayout)
        slot.deadSetLayouts.push_back(setLayout);
}


////////////////////////////////////////////////////////////
VkRenderPass VulkanGraphicsDevice::getOpenRenderPass() const
{
    return m_openRenderPass;
}


////////////////////////////////////////////////////////////
VkDescriptorSetLayout VulkanGraphicsDevice::getBuiltinDescriptorSetLayout() const
{
    return m_builtinSetLayout;
}


////////////////////////////////////////////////////////////
VkPipelineLayout VulkanGraphicsDevice::getBuiltinPipelineLayout() const
{
    return m_builtinPipelineLayout;
}


////////////////////////////////////////////////////////////
VkShaderModule VulkanGraphicsDevice::getDefaultVertexShader() const
{
    return m_defaultVertexShader;
}


////////////////////////////////////////////////////////////
VkShaderModule VulkanGraphicsDevice::getDefaultFragmentShader() const
{
    return m_defaultFragmentShader;
}

} // namespace sf::priv
