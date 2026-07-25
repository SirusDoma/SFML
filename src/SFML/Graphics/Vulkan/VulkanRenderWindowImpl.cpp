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
#include <SFML/Graphics/Vulkan/VulkanRenderWindowImpl.hpp>

#include <SFML/Window/VulkanImpl.hpp>

#include <SFML/System/Err.hpp>
#include <SFML/System/Sleep.hpp>
#include <SFML/System/Time.hpp>

#include <algorithm>
#include <ostream>
#include <vma/vk_mem_alloc.h>


namespace
{
// A window that cannot present paces nothing: no image is acquired and no
// present blocks, so the frame loop would spin at full speed. Yielding for a
// fraction of a frame keeps it idle until the window can be drawn again.
constexpr sf::Time unpresentableFrameDelay = sf::milliseconds(10);
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
VulkanRenderWindowImpl::VulkanRenderWindowImpl(VulkanGraphicsDevice&         device,
                                               WindowHandle                  handle,
                                               const ContextSettings&        settings,
                                               [[maybe_unused]] unsigned int bitsPerPixel) :
    m_device(device),
    m_handle(handle),
    m_presentation(settings.presentation),
    m_sRgb(settings.sRgbCapable)
{
    if (!m_device.getDevice())
        return;

    const VulkanFunctions& fn = m_device.fn();

    // The window module owns surface creation for every platform
    if (!VulkanImpl::createVulkanSurface(m_device.getInstance(), m_handle, m_surface, nullptr))
    {
        err() << "Failed to create the Vulkan presentation surface" << std::endl;
        return;
    }

    // The single graphics queue has to be able to present to the surface
    VkBool32 presentSupported = VK_FALSE;
    fn.vkGetPhysicalDeviceSurfaceSupportKHR(m_device.getPhysicalDevice(), m_device.getQueueFamilyIndex(), m_surface, &presentSupported);
    if (!presentSupported)
    {
        err() << "The Vulkan device cannot present to the window" << std::endl;
        return;
    }

    // Pick the swapchain format, preferring the common BGRA order
    std::uint32_t formatCount = 0;
    fn.vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.getPhysicalDevice(), m_surface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    fn.vkGetPhysicalDeviceSurfaceFormatsKHR(m_device.getPhysicalDevice(), m_surface, &formatCount, formats.data());

    // A format is only usable with the color spaces the surface pairs it with,
    // and the standard one is what the SFML color pipeline expects
    const auto pickFormat = [&](VkFormat wanted) -> bool
    {
        for (const VkSurfaceFormatKHR& format : formats)
        {
            if ((format.format == wanted) && (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
            {
                m_format     = wanted;
                m_colorSpace = format.colorSpace;
                return true;
            }
        }
        return false;
    };

    if (m_sRgb)
    {
        if (!pickFormat(VK_FORMAT_B8G8R8A8_SRGB) && !pickFormat(VK_FORMAT_R8G8B8A8_SRGB))
            m_sRgb = false;
    }
    if (!m_sRgb)
    {
        if (!pickFormat(VK_FORMAT_B8G8R8A8_UNORM) && !pickFormat(VK_FORMAT_R8G8B8A8_UNORM) && !formats.empty())
        {
            // Nothing preferred is available: the pair has to be taken whole,
            // picking a format alone would not be a supported combination
            m_format     = formats.front().format;
            m_colorSpace = formats.front().colorSpace;

            // Whatever it turned out to be is what gets reported back
            m_sRgb = (m_format == VK_FORMAT_B8G8R8A8_SRGB) || (m_format == VK_FORMAT_R8G8B8A8_SRGB);
        }
    }

    // Present modes the intents can map to
    std::uint32_t modeCount = 0;
    fn.vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.getPhysicalDevice(), m_surface, &modeCount, nullptr);
    m_availablePresentModes.resize(modeCount);
    fn.vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.getPhysicalDevice(),
                                                 m_surface,
                                                 &modeCount,
                                                 m_availablePresentModes.data());

    m_sampleCount = m_device.clampAntiAliasingLevel(std::max(settings.antiAliasingLevel, 1u));

    // Pick the depth-stencil format: SFML itself only ever tests stencil, a
    // depth plane is allocated when the user asked for one for their own
    // Vulkan rendering. One of the combined formats is always supported.
    if ((settings.depthBits > 0) || (settings.stencilBits > 0))
    {
        const auto supportsDepthStencil = [&](VkFormat format)
        {
            VkFormatProperties properties{};
            fn.vkGetPhysicalDeviceFormatProperties(m_device.getPhysicalDevice(), format, &properties);
            return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
        };

        if (supportsDepthStencil(VK_FORMAT_D24_UNORM_S8_UINT))
            m_depthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
        else if (supportsDepthStencil(VK_FORMAT_D32_SFLOAT_S8_UINT))
            m_depthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    if (!createSwapchain())
        return;

    // Report what was actually created
    m_settings.depthBits         = (m_depthStencilFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) ? 32u
                                   : (m_depthStencilFormat == VK_FORMAT_D24_UNORM_S8_UINT)
                                       ? 24u
                                       : 0u;
    m_settings.stencilBits       = (m_depthStencilFormat != VK_FORMAT_UNDEFINED) ? 8 : 0;
    m_settings.antiAliasingLevel = (m_sampleCount > 1) ? m_sampleCount : 0;
    m_settings.majorVersion      = 1;
    m_settings.minorVersion      = 2;
    m_settings.attributeFlags    = ContextSettings::Default;
    m_settings.sRgbCapable       = m_sRgb;
    m_settings.presentation      = (m_presentation == ContextSettings::Presentation::LowLatency)
                                       ? ContextSettings::Presentation::LowLatency
                                       : ContextSettings::Presentation::Throughput;
}


////////////////////////////////////////////////////////////
VulkanRenderWindowImpl::~VulkanRenderWindowImpl()
{
    m_device.unbindSurface(this);

    if (m_device.getDevice())
    {
        // Held across the whole teardown: the queue is not externally
        // synchronized, so another window must not be presenting meanwhile
        const VulkanGraphicsDevice::ContextLock lock(m_device);

        // The GPU may still be using the swapchain resources, and the
        // presentation engine holds images and semaphores past the fences
        m_device.commitCommandBuffer(true);
        m_device.waitForInFlightFrames(0);
        m_device.fn().vkQueueWaitIdle(m_device.getQueue());

        destroySwapchainResources(false);
    }

    if (m_surface && m_device.getInstance())
        m_device.fn().vkDestroySurfaceKHR(m_device.getInstance(), m_surface, nullptr);
}


////////////////////////////////////////////////////////////
VkPresentModeKHR VulkanRenderWindowImpl::choosePresentMode() const
{
    const auto available = [&](VkPresentModeKHR mode)
    {
        return std::find(m_availablePresentModes.begin(), m_availablePresentModes.end(), mode) !=
               m_availablePresentModes.end();
    };

    // FIFO is the only mode every Vulkan device supports and the only paced
    // one; the uncapped intents map to the unpaced modes when they exist
    if (m_vsync)
        return VK_PRESENT_MODE_FIFO_KHR;

    if (m_presentation == ContextSettings::Presentation::LowLatency)
    {
        // Mailbox replaces the queued frame instead of lining up behind it,
        // keeping the newest frame on its way to the screen
        if (available(VK_PRESENT_MODE_MAILBOX_KHR))
            return VK_PRESENT_MODE_MAILBOX_KHR;
        if (available(VK_PRESENT_MODE_IMMEDIATE_KHR))
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    else
    {
        // Immediate presents without waiting at all, maximizing throughput
        if (available(VK_PRESENT_MODE_IMMEDIATE_KHR))
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        if (available(VK_PRESENT_MODE_MAILBOX_KHR))
            return VK_PRESENT_MODE_MAILBOX_KHR;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}


////////////////////////////////////////////////////////////
bool VulkanRenderWindowImpl::createSwapchain()
{
    if (!m_surface)
        return false;

    const VulkanFunctions& fn = m_device.fn();

    VkSurfaceCapabilitiesKHR capabilities{};
    if (!vkCheck(fn.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.getPhysicalDevice(), m_surface, &capabilities)))
        return false;

    // Nothing can be rendered or presented while the window is minimized, and
    // drivers refuse to create swapchains for such windows: keep the old
    // swapchain around until the window is restored
    m_minimized = (capabilities.currentExtent.width == 0) || (capabilities.currentExtent.height == 0);
    if (m_minimized)
    {
        // Whatever this call was meant to change -- a resize, a v-sync toggle --
        // is still pending: the frame boundary re-tries until the window is
        // restored, otherwise the old swapchain would outlive the change
        m_needsRecreate = true;
        return m_swapchain != VK_NULL_HANDLE;
    }

    VkSwapchainKHR oldSwapchain = m_swapchain;

    // The image count bounds how many presented frames can queue up before
    // acquisition blocks: the low-latency intent keeps the queue as short as
    // the surface allows, everything else keeps the deeper queue that absorbs
    // frame time spikes
    const std::uint32_t wantedImages = (m_presentation == ContextSettings::Presentation::LowLatency) ? 2u : 3u;

    std::uint32_t imageCount = std::max(capabilities.minImageCount, wantedImages);
    if (capabilities.maxImageCount > 0)
        imageCount = std::min(imageCount, capabilities.maxImageCount);

    // Reading the window contents back needs transfer access to the images
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    m_readable              = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    if (m_readable)
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface          = m_surface;
    swapchainInfo.minImageCount    = imageCount;
    swapchainInfo.imageFormat      = m_format;
    swapchainInfo.imageColorSpace  = m_colorSpace;
    swapchainInfo.imageExtent      = capabilities.currentExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage       = usage;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform     = capabilities.currentTransform;
    swapchainInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode      = choosePresentMode();
    swapchainInfo.clipped          = VK_TRUE;
    swapchainInfo.oldSwapchain     = oldSwapchain;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VkResult       createResult = fn.vkCreateSwapchainKHR(m_device.getDevice(), &swapchainInfo, nullptr, &newSwapchain);
    if ((createResult != VK_SUCCESS) && oldSwapchain)
    {
        // Drivers exist that refuse re-creations chained through oldSwapchain
        // for certain window transitions (observed on Windows after restoring
        // a minimized sRGB window) while creating from scratch works. A failed
        // creation retires the old swapchain anyway, destroy it and retry
        m_device.commitCommandBuffer(true);
        m_device.waitForInFlightFrames(0);
        fn.vkQueueWaitIdle(m_device.getQueue());
        destroySwapchainResources(false);
        oldSwapchain = VK_NULL_HANDLE;

        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
        createResult = fn.vkCreateSwapchainKHR(m_device.getDevice(), &swapchainInfo, nullptr, &newSwapchain);
    }

    if (createResult != VK_SUCCESS)
    {
        vkCheck(createResult);
        return false;
    }

    // The old resources may still be referenced by in-flight frames and by
    // presentation operations the fences do not cover
    if (oldSwapchain)
    {
        m_device.commitCommandBuffer(true);
        m_device.waitForInFlightFrames(0);
        fn.vkQueueWaitIdle(m_device.getQueue());
        destroySwapchainResources(true);
        fn.vkDestroySwapchainKHR(m_device.getDevice(), oldSwapchain, nullptr);
    }

    m_swapchain     = newSwapchain;
    m_extent        = {capabilities.currentExtent.width, capabilities.currentExtent.height};
    m_acquired      = false;
    m_needsRecreate = false;
    m_suboptimal    = false;

    std::uint32_t actualImageCount = 0;
    fn.vkGetSwapchainImagesKHR(m_device.getDevice(), m_swapchain, &actualImageCount, nullptr);
    m_images.resize(actualImageCount);
    fn.vkGetSwapchainImagesKHR(m_device.getDevice(), m_swapchain, &actualImageCount, m_images.data());

    m_imageViews.resize(actualImageCount);
    m_renderFinished.resize(actualImageCount);
    m_acquireSemaphores.resize(actualImageCount);

    for (std::uint32_t i = 0; i < actualImageCount; ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                       = m_images[i];
        viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                      = m_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        if (!vkCheck(fn.vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &m_imageViews[i])))
            return false;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (!vkCheck(fn.vkCreateSemaphore(m_device.getDevice(), &semaphoreInfo, nullptr, &m_renderFinished[i])) ||
            !vkCheck(fn.vkCreateSemaphore(m_device.getDevice(), &semaphoreInfo, nullptr, &m_acquireSemaphores[i])))
            return false;
    }

    createAncillaryImages();

    return true;
}


////////////////////////////////////////////////////////////
void VulkanRenderWindowImpl::destroySwapchainResources(bool keepSwapchain)
{
    const VulkanFunctions& fn = m_device.fn();

    for (VkImage image : m_images)
        m_device.forgetImage(image);

    for (VkImageView view : m_imageViews)
    {
        if (view)
        {
            m_device.purgeFramebuffers(view);
            fn.vkDestroyImageView(m_device.getDevice(), view, nullptr);
        }
    }
    m_imageViews.clear();
    m_images.clear();

    for (VkSemaphore semaphore : m_renderFinished)
    {
        if (semaphore)
            fn.vkDestroySemaphore(m_device.getDevice(), semaphore, nullptr);
    }
    m_renderFinished.clear();

    for (VkSemaphore semaphore : m_acquireSemaphores)
    {
        if (semaphore)
            fn.vkDestroySemaphore(m_device.getDevice(), semaphore, nullptr);
    }
    m_acquireSemaphores.clear();
    m_acquireIndex = 0;

    if (m_multisampleView)
    {
        m_device.purgeFramebuffers(m_multisampleView);
        fn.vkDestroyImageView(m_device.getDevice(), m_multisampleView, nullptr);
        m_multisampleView = VK_NULL_HANDLE;
    }
    if (m_multisampleImage)
    {
        m_device.forgetImage(m_multisampleImage);
        vmaDestroyImage(m_device.getAllocator(), m_multisampleImage, m_multisampleAllocation);
        m_multisampleImage      = VK_NULL_HANDLE;
        m_multisampleAllocation = nullptr;
    }

    if (m_depthStencilView)
    {
        m_device.purgeFramebuffers(m_depthStencilView);
        fn.vkDestroyImageView(m_device.getDevice(), m_depthStencilView, nullptr);
        m_depthStencilView = VK_NULL_HANDLE;
    }
    if (m_depthStencilImage)
    {
        m_device.forgetImage(m_depthStencilImage);
        vmaDestroyImage(m_device.getAllocator(), m_depthStencilImage, m_depthStencilAllocation);
        m_depthStencilImage      = VK_NULL_HANDLE;
        m_depthStencilAllocation = nullptr;
    }

    if (!keepSwapchain && m_swapchain)
    {
        fn.vkDestroySwapchainKHR(m_device.getDevice(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    m_acquired = false;
}


////////////////////////////////////////////////////////////
bool VulkanRenderWindowImpl::recreateSurface()
{
    const VulkanFunctions& fn = m_device.fn();

    // Nothing may reference the swapchain or the surface anymore
    m_device.commitCommandBuffer(true);
    m_device.waitForInFlightFrames(0);
    fn.vkQueueWaitIdle(m_device.getQueue());
    destroySwapchainResources(false);

    if (m_surface)
        fn.vkDestroySurfaceKHR(m_device.getInstance(), m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;

    if (!VulkanImpl::createVulkanSurface(m_device.getInstance(), m_handle, m_surface, nullptr))
    {
        err() << "Failed to re-create the Vulkan presentation surface" << std::endl;
        return false;
    }

    std::uint32_t modeCount = 0;
    fn.vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.getPhysicalDevice(), m_surface, &modeCount, nullptr);
    m_availablePresentModes.resize(modeCount);
    fn.vkGetPhysicalDeviceSurfacePresentModesKHR(m_device.getPhysicalDevice(),
                                                 m_surface,
                                                 &modeCount,
                                                 m_availablePresentModes.data());

    return true;
}


////////////////////////////////////////////////////////////
void VulkanRenderWindowImpl::createAncillaryImages()
{
    if ((m_extent.x == 0) || (m_extent.y == 0))
        return;

    if (m_sampleCount > 1)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = m_format;
        imageInfo.extent        = {m_extent.x, m_extent.y, 1};
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = static_cast<VkSampleCountFlagBits>(m_sampleCount);
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (!vkCheck(vmaCreateImage(m_device.getAllocator(),
                                    &imageInfo,
                                    &allocationInfo,
                                    &m_multisampleImage,
                                    &m_multisampleAllocation,
                                    nullptr)))
        {
            // Fall back to a plain allocation when lazily allocated memory is unavailable
            imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (!vkCheck(vmaCreateImage(m_device.getAllocator(),
                                        &imageInfo,
                                        &allocationInfo,
                                        &m_multisampleImage,
                                        &m_multisampleAllocation,
                                        nullptr)))
            {
                m_multisampleImage = VK_NULL_HANDLE;
                m_sampleCount      = 1;
            }
        }

        if (m_multisampleImage)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                       = m_multisampleImage;
            viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                      = m_format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;

            if (!vkCheck(m_device.fn().vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &m_multisampleView)))
            {
                vmaDestroyImage(m_device.getAllocator(), m_multisampleImage, m_multisampleAllocation);
                m_multisampleImage = VK_NULL_HANDLE;
                m_sampleCount      = 1;
            }
        }
    }

    if (m_depthStencilFormat != VK_FORMAT_UNDEFINED)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = m_depthStencilFormat;
        imageInfo.extent        = {m_extent.x, m_extent.y, 1};
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = static_cast<VkSampleCountFlagBits>(m_sampleCount);
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (vkCheck(vmaCreateImage(m_device.getAllocator(),
                                   &imageInfo,
                                   &allocationInfo,
                                   &m_depthStencilImage,
                                   &m_depthStencilAllocation,
                                   nullptr)))
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                       = m_depthStencilImage;
            viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                      = m_depthStencilFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;

            if (!vkCheck(m_device.fn().vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &m_depthStencilView)))
            {
                vmaDestroyImage(m_device.getAllocator(), m_depthStencilImage, m_depthStencilAllocation);
                m_depthStencilImage = VK_NULL_HANDLE;
            }
        }
        else
        {
            m_depthStencilImage = VK_NULL_HANDLE;
        }
    }
}


////////////////////////////////////////////////////////////
bool VulkanRenderWindowImpl::prepareAttachments(VulkanSurfaceAttachments& attachments)
{
    // The device may already be mid-recording when this runs, which makes any
    // swapchain surgery unsafe here: a swapchain that went out of date waits
    // for the frame boundary (present) to be re-created, its frames drop
    if (m_needsRecreate || !m_swapchain)
        return false;

    const VulkanFunctions& fn = m_device.fn();

    if (!m_acquired)
    {
        const VkSemaphore semaphore = m_acquireSemaphores[m_acquireIndex];

        const VkResult
            result = fn.vkAcquireNextImageKHR(m_device.getDevice(), m_swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &m_imageIndex);

        if ((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))
        {
            m_device.addAcquireWaitSemaphore(semaphore);
            m_acquireIndex = (m_acquireIndex + 1) % m_acquireSemaphores.size();
            m_acquired     = true;

            // The image can still be drawn into, but the swapchain has drifted
            // from the surface: rebuild it once this frame reached the screen
            if (result == VK_SUBOPTIMAL_KHR)
                m_suboptimal = true;
        }
        else if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_ERROR_SURFACE_LOST_KHR))
        {
            // The surface changed under the swapchain (resize, minimize,
            // restore) or was lost entirely: normal situations that the frame
            // boundary resolves, a lost surface through its re-creation
            m_needsRecreate = true;
            return false;
        }
        else
        {
            vkCheck(result);
            return false;
        }
    }

    attachments.colorImage         = m_multisampleImage ? m_multisampleImage : m_images[m_imageIndex];
    attachments.colorView          = m_multisampleImage ? m_multisampleView : m_imageViews[m_imageIndex];
    attachments.resolveImage       = m_multisampleImage ? m_images[m_imageIndex] : VK_NULL_HANDLE;
    attachments.resolveView        = m_multisampleImage ? m_imageViews[m_imageIndex] : VK_NULL_HANDLE;
    attachments.depthStencilImage  = m_depthStencilImage;
    attachments.depthStencilView   = m_depthStencilView;
    attachments.colorFormat        = m_format;
    attachments.depthStencilFormat = m_depthStencilImage ? m_depthStencilFormat : VK_FORMAT_UNDEFINED;
    attachments.sampleCount        = m_multisampleImage ? m_sampleCount : 1;
    attachments.size               = m_extent;
    attachments.colorGeneralLayout = false;

    return true;
}


////////////////////////////////////////////////////////////
void VulkanRenderWindowImpl::present()
{
    if (!m_device.getDevice())
        return;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    // The frame boundary is the one safe point for swapchain surgery: nothing
    // is being recorded anymore once the frame's commands are committed, so
    // re-creating the swapchain here cannot corrupt an in-flight frame
    if (m_needsRecreate || !m_swapchain)
    {
        m_device.commitCommandBuffer(false);
        m_needsRecreate = false;

        bool recovered = createSwapchain() && (m_swapchain != VK_NULL_HANDLE);
        if (!recovered && !m_minimized)
        {
            // The surface itself may be the sick part: a lost surface fails
            // the capability query, and drivers exist that refuse every
            // further swapchain of a surface after certain window transitions
            if (recreateSurface())
                recovered = createSwapchain() && (m_swapchain != VK_NULL_HANDLE);
        }

        if (!recovered || m_minimized)
            sleep(unpresentableFrameDelay);

        return;
    }

    const VulkanFunctions& fn = m_device.fn();

    // Finish the frame: submit pending draws, materialize pending clears and
    // resolve multisampled content into the swapchain image
    if (m_device.getCurrentSurface() == this)
    {
        m_device.prepareForPresent(m_multisampleImage != VK_NULL_HANDLE);
    }
    else
    {
        m_device.flushPendingDraws();
        m_device.materializePendingClears();
        m_device.endEncoding();

        // Another surface is current, the multisampled content still has to
        // reach the swapchain image before it is presented
        if (m_multisampleImage && m_acquired &&
            (m_device.getTrackedImageLayout(m_multisampleImage) != VK_IMAGE_LAYOUT_UNDEFINED))
        {
            if (VkCommandBuffer commandBuffer = m_device.currentCommandBuffer())
            {
                const VkImage target = m_images[m_imageIndex];

                vulkanImageBarrier(fn,
                                   commandBuffer,
                                   target,
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   m_device.getTrackedImageLayout(target),
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                VkImageResolve region{};
                region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.srcSubresource.layerCount = 1;
                region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.dstSubresource.layerCount = 1;
                region.extent                    = {m_extent.x, m_extent.y, 1};

                vulkanImageBarrier(fn,
                                   commandBuffer,
                                   m_multisampleImage,
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   m_device.getTrackedImageLayout(m_multisampleImage),
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                fn.vkCmdResolveImage(commandBuffer,
                                     m_multisampleImage,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     target,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     1,
                                     &region);

                vulkanImageBarrier(fn,
                                   commandBuffer,
                                   m_multisampleImage,
                                   VK_IMAGE_ASPECT_COLOR_BIT,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                m_device.setTrackedImageLayout(m_multisampleImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                m_device.setTrackedImageLayout(target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            }
        }
    }

    // A frame that never rendered has no image to present
    if (!m_acquired)
    {
        m_device.commitCommandBuffer(false);

        if (m_minimized)
            sleep(unpresentableFrameDelay);

        return;
    }

    const VkImage presentImage = m_images[m_imageIndex];

    // The presentation engine needs the image in the present layout
    if (VkCommandBuffer commandBuffer = m_device.currentCommandBuffer())
    {
        const VkImageLayout current = m_device.getTrackedImageLayout(presentImage);
        if (current != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            vulkanImageBarrier(fn, commandBuffer, presentImage, VK_IMAGE_ASPECT_COLOR_BIT, current, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            m_device.setTrackedImageLayout(presentImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }
    }

    const VkSemaphore renderFinished = m_renderFinished[m_imageIndex];
    m_device.commitCommandBuffer(false, renderFinished);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinished;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_swapchain;
    presentInfo.pImageIndices      = &m_imageIndex;

    // Tag the present so present-wait can pace to the moment it reaches the screen
    std::uint64_t  presentId = 0;
    VkPresentIdKHR presentIdInfo{};
    if (m_device.hasPresentWait())
    {
        presentId                    = ++m_presentId;
        presentIdInfo.sType          = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
        presentIdInfo.swapchainCount = 1;
        presentIdInfo.pPresentIds    = &presentId;
        presentInfo.pNext            = &presentIdInfo;
    }

    const VkResult presentResult = fn.vkQueuePresentKHR(m_device.getQueue(), &presentInfo);
    m_acquired                   = false;

    // An out-of-date or lost-surface present only flags the re-creation for
    // the next frame boundary: the surface change that invalidated it (like a
    // window in the middle of being minimized) can still be in flight, and
    // re-creating against it races into a creation failure. A suboptimal one
    // presented fine but no longer matches the surface exactly, so it takes
    // the same path rather than staying mismatched for the rest of its life.
    if ((presentResult == VK_ERROR_OUT_OF_DATE_KHR) || (presentResult == VK_ERROR_SURFACE_LOST_KHR) ||
        (presentResult == VK_SUBOPTIMAL_KHR) || m_suboptimal)
    {
        m_needsRecreate = true;
        m_suboptimal    = false;
    }
    else if ((presentResult != VK_SUCCESS) && (presentResult != VK_SUBOPTIMAL_KHR))
    {
        vkCheck(presentResult);
    }

    // Pace the application to the GPU here, before the next frame samples its
    // input: the low-latency intent keeps a single frame in flight, trading
    // the deeper queue that absorbs frame time spikes for less input latency
    const unsigned int maxPending = (m_presentation == ContextSettings::Presentation::LowLatency) ? 1u : 2u;
    m_device.waitForInFlightFrames(maxPending);

    // With v-sync the low-latency intent additionally waits until the frame is
    // on screen, collapsing the presentation queue the driver keeps between
    // the present call and the display; this is the moral equivalent of the
    // frame latency waitable object of the Direct3D 11 backend
    if ((m_presentation == ContextSettings::Presentation::LowLatency) && m_vsync && (presentId != 0) &&
        ((presentResult == VK_SUCCESS) || (presentResult == VK_SUBOPTIMAL_KHR)) && fn.vkWaitForPresentKHR)
    {
        constexpr std::uint64_t timeoutNanoseconds = 100000000; // 100ms, occluded windows may never display
        fn.vkWaitForPresentKHR(m_device.getDevice(), m_swapchain, presentId, timeoutNanoseconds);
    }

    monitorPresentationPacing(presentResult);
}


////////////////////////////////////////////////////////////
void VulkanRenderWindowImpl::monitorPresentationPacing(VkResult presentResult)
{
    const auto now      = std::chrono::steady_clock::now();
    const auto previous = m_lastPresentTime;
    m_lastPresentTime   = now;

    // Only fully v-synced successful cycles are meaningful
    if (!m_vsync || m_pacingWarned || (presentResult != VK_SUCCESS) ||
        (previous == std::chrono::steady_clock::time_point{}))
    {
        m_unsyncedPresentStreak = 0;
        return;
    }

    // No display refreshes fast enough to legitimately complete v-synced cycles
    // this quickly, sustaining them means the driver is not pacing the presents
    constexpr std::chrono::microseconds pacingFloor{1000};
    if (now - previous >= pacingFloor)
    {
        m_unsyncedPresentStreak = 0;
        return;
    }

    constexpr unsigned int unsyncedStreakLimit = 30;
    if (++m_unsyncedPresentStreak < unsyncedStreakLimit)
        return;

    // FIFO presentation is already the most compatible path Vulkan has, there
    // is no other model to fall back to like on Direct3D
    err() << "V-synced presentation is not being paced by the display driver" << std::endl;
    m_pacingWarned = true;
}


////////////////////////////////////////////////////////////
void VulkanRenderWindowImpl::setVerticalSyncEnabled(bool enabled)
{
    if (m_vsync == enabled)
        return;

    m_vsync = enabled;

    // The present mode is baked into the swapchain, toggling v-sync re-creates it
    if (m_swapchain)
    {
        const VulkanGraphicsDevice::ContextLock lock(m_device);

        if (!createSwapchain())
            err() << "Failed to re-create the swapchain after toggling vertical synchronization" << std::endl;
    }
}


////////////////////////////////////////////////////////////
void VulkanRenderWindowImpl::resize([[maybe_unused]] Vector2u size)
{
    if (!m_swapchain)
        return;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    // The swapchain images are sized from the surface capabilities
    if (!createSwapchain())
        err() << "Failed to re-create the swapchain after resizing the window" << std::endl;
}


////////////////////////////////////////////////////////////
const ContextSettings& VulkanRenderWindowImpl::getSettings() const
{
    return m_settings;
}


////////////////////////////////////////////////////////////
bool VulkanRenderWindowImpl::isSrgb() const
{
    return m_sRgb;
}


////////////////////////////////////////////////////////////
bool VulkanRenderWindowImpl::activate(bool active)
{
    // Deactivation is a no-op, all surfaces share the single device
    if (active)
        m_device.bindSurface(this);

    return m_swapchain != VK_NULL_HANDLE;
}


////////////////////////////////////////////////////////////
VkImage VulkanRenderWindowImpl::acquireReadableColorImage(Vector2u& size)
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    if (!m_readable)
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "The window contents cannot be read, the presentation engine forbids copying its images" << std::endl;

            warned = true;
        }

        return VK_NULL_HANDLE;
    }

    // Submit the pending content of the frame, resolving multisampled draws
    // into the swapchain image
    if (m_device.getCurrentSurface() == this)
    {
        m_device.prepareForPresent(m_multisampleImage != VK_NULL_HANDLE);
    }
    else
    {
        m_device.flushPendingDraws();
        m_device.materializePendingClears();
        m_device.endEncoding();
    }

    // A frame that never rendered has no image whose contents could be read
    if (!m_acquired)
        return VK_NULL_HANDLE;

    size = m_extent;

    return m_images[m_imageIndex];
}

} // namespace sf::priv
