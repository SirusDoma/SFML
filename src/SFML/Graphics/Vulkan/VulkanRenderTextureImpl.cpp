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
#include <SFML/Graphics/Vulkan/VulkanRenderTextureImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanTextureImpl.hpp>

#include <SFML/Window/ContextSettings.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <ostream>
#include <vma/vk_mem_alloc.h>


namespace sf::priv
{
////////////////////////////////////////////////////////////
VulkanRenderTextureImpl::VulkanRenderTextureImpl(VulkanGraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
VulkanRenderTextureImpl::~VulkanRenderTextureImpl()
{
    m_device.unbindSurface(this);

    destroyImages();
}


////////////////////////////////////////////////////////////
void VulkanRenderTextureImpl::destroyImages()
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    if (m_multisampleImage)
    {
        m_device.deferDestroyImage(m_multisampleImage, m_multisampleView, m_multisampleAllocation);
        m_multisampleImage      = VK_NULL_HANDLE;
        m_multisampleView       = VK_NULL_HANDLE;
        m_multisampleAllocation = nullptr;
    }

    if (m_depthStencilImage)
    {
        m_device.deferDestroyImage(m_depthStencilImage, m_depthStencilView, m_depthStencilAllocation);
        m_depthStencilImage      = VK_NULL_HANDLE;
        m_depthStencilView       = VK_NULL_HANDLE;
        m_depthStencilAllocation = nullptr;
    }
}


////////////////////////////////////////////////////////////
bool VulkanRenderTextureImpl::create(Vector2u size, TextureImpl& texture, const ContextSettings& settings)
{
    auto& vulkanTexture = static_cast<VulkanTextureImpl&>(texture);

    if (!m_device.getDevice() || !vulkanTexture.getImage())
        return false;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    destroyImages();

    // The target texture becomes an attachment living in the GENERAL layout,
    // so rendering into it and sampling it can alternate freely
    vulkanTexture.setAttachment();

    m_targetTexture = &vulkanTexture;
    m_attachedImage = vulkanTexture.getImage();
    m_size          = size;
    m_sRgb          = (vulkanTexture.getFormat() == VK_FORMAT_R8G8B8A8_SRGB);
    m_sampleCount   = m_device.clampAntiAliasingLevel(std::max(settings.antiAliasingLevel, 1u));

    if (m_sampleCount > 1)
    {
        // Render into a multisampled color image, resolved into the target texture on display
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = vulkanTexture.getFormat();
        imageInfo.extent        = {size.x, size.y, 1};
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = static_cast<VkSampleCountFlagBits>(m_sampleCount);
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

        // The level was already clamped to what the device supports, so a
        // failure here is a real one: report it instead of quietly handing
        // back a render texture without the anti-aliasing that was asked for
        if (!vkCheck(vmaCreateImage(m_device.getAllocator(),
                                    &imageInfo,
                                    &allocationInfo,
                                    &m_multisampleImage,
                                    &m_multisampleAllocation,
                                    nullptr)))
        {
            m_multisampleImage = VK_NULL_HANDLE;
            err() << "Failed to create the multisampled color buffer of the render texture" << std::endl;
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                       = m_multisampleImage;
        viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                      = vulkanTexture.getFormat();
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        if (!vkCheck(m_device.fn().vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &m_multisampleView)))
        {
            vmaDestroyImage(m_device.getAllocator(), m_multisampleImage, m_multisampleAllocation);
            m_multisampleImage = VK_NULL_HANDLE;
            err() << "Failed to create the view of the multisampled color buffer of the render texture" << std::endl;
            return false;
        }
    }

    if ((settings.depthBits > 0) || (settings.stencilBits > 0))
    {
        const auto supportsDepthStencil = [&](VkFormat format)
        {
            VkFormatProperties properties{};
            m_device.fn().vkGetPhysicalDeviceFormatProperties(m_device.getPhysicalDevice(), format, &properties);
            return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
        };

        m_depthStencilFormat = supportsDepthStencil(VK_FORMAT_D24_UNORM_S8_UINT) ? VK_FORMAT_D24_UNORM_S8_UINT
                                                                                 : VK_FORMAT_D32_SFLOAT_S8_UINT;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = m_depthStencilFormat;
        imageInfo.extent        = {size.x, size.y, 1};
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = static_cast<VkSampleCountFlagBits>(m_sampleCount);
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

        if (!vkCheck(vmaCreateImage(m_device.getAllocator(),
                                    &imageInfo,
                                    &allocationInfo,
                                    &m_depthStencilImage,
                                    &m_depthStencilAllocation,
                                    nullptr)))
        {
            m_depthStencilImage  = VK_NULL_HANDLE;
            m_depthStencilFormat = VK_FORMAT_UNDEFINED;
            return false;
        }

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
            m_depthStencilImage  = VK_NULL_HANDLE;
            m_depthStencilFormat = VK_FORMAT_UNDEFINED;
            return false;
        }
    }

    return true;
}


////////////////////////////////////////////////////////////
bool VulkanRenderTextureImpl::activate(bool active)
{
    // Deactivation is a no-op, all surfaces share the single device
    if (!active)
        return true;

    if (!m_targetTexture)
        return false;

    // generateMipmap re-creates the attached image, re-attach when it changed
    if (m_targetTexture->getImage() != m_attachedImage)
        m_attachedImage = m_targetTexture->getImage();

    m_device.bindSurface(this);

    return m_attachedImage != VK_NULL_HANDLE;
}


////////////////////////////////////////////////////////////
bool VulkanRenderTextureImpl::isSrgb() const
{
    return m_sRgb;
}


////////////////////////////////////////////////////////////
void VulkanRenderTextureImpl::updateTexture(TextureImpl& texture)
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    auto& vulkanTexture = static_cast<VulkanTextureImpl&>(texture);

    // Draws collected so far must land before the content is consumed, and a
    // clear that never got a pass still has to be carried out
    if (m_device.getCurrentSurface() == this)
    {
        m_device.prepareForPresent(m_multisampleImage != VK_NULL_HANDLE);
        return;
    }

    m_device.flushPendingDraws();
    m_device.materializePendingClears();
    m_device.endEncoding();

    // Without multisampling the rendering happened directly into the target texture
    if (!m_multisampleImage || !vulkanTexture.getImage())
        return;

    if (m_device.getTrackedImageLayout(m_multisampleImage) == VK_IMAGE_LAYOUT_UNDEFINED)
        return;

    VkCommandBuffer commandBuffer = m_device.currentCommandBuffer();
    if (!commandBuffer)
        return;

    // Both images stay in the GENERAL layout, which resolve accepts on both ends
    VkImageResolve region{};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;
    region.extent                    = {m_size.x, m_size.y, 1};

    const VkImageLayout multisampleLayout = m_device.getTrackedImageLayout(m_multisampleImage);

    m_device.fn().vkCmdResolveImage(commandBuffer,
                                    m_multisampleImage,
                                    multisampleLayout == VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_GENERAL
                                                                                 : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    vulkanTexture.getImage(),
                                    VK_IMAGE_LAYOUT_GENERAL,
                                    1,
                                    &region);

    // Order the resolve against later sampling of the texture
    vulkanImageBarrier(m_device.fn(),
                       commandBuffer,
                       vulkanTexture.getImage(),
                       VK_IMAGE_ASPECT_COLOR_BIT,
                       VK_IMAGE_LAYOUT_GENERAL,
                       VK_IMAGE_LAYOUT_GENERAL);
}


////////////////////////////////////////////////////////////
bool VulkanRenderTextureImpl::arePixelsFlipped() const
{
    // Vulkan renders top-down with the backend's viewport handling, matching
    // sf::Texture's pixel order
    return false;
}


////////////////////////////////////////////////////////////
bool VulkanRenderTextureImpl::needsFullActivationForDisplay() const
{
    return false;
}


////////////////////////////////////////////////////////////
bool VulkanRenderTextureImpl::isTextureAttachment() const
{
    return true;
}


////////////////////////////////////////////////////////////
bool VulkanRenderTextureImpl::prepareAttachments(VulkanSurfaceAttachments& attachments)
{
    if (!m_targetTexture || !m_targetTexture->getImage())
        return false;

    // generateMipmap re-creates the attached image, follow the replacement
    m_attachedImage = m_targetTexture->getImage();

    attachments.colorImage         = m_multisampleImage ? m_multisampleImage : m_attachedImage;
    attachments.colorView          = m_multisampleImage ? m_multisampleView : m_targetTexture->getAttachmentView();
    attachments.resolveImage       = m_multisampleImage ? m_attachedImage : VK_NULL_HANDLE;
    attachments.resolveView        = m_multisampleImage ? m_targetTexture->getAttachmentView() : VK_NULL_HANDLE;
    attachments.depthStencilImage  = m_depthStencilImage;
    attachments.depthStencilView   = m_depthStencilView;
    attachments.colorFormat        = m_targetTexture->getFormat();
    attachments.depthStencilFormat = m_depthStencilImage ? m_depthStencilFormat : VK_FORMAT_UNDEFINED;
    attachments.sampleCount        = m_multisampleImage ? m_sampleCount : 1;
    attachments.size               = m_size;
    attachments.colorGeneralLayout = true;

    return true;
}


////////////////////////////////////////////////////////////
VkImage VulkanRenderTextureImpl::acquireReadableColorImage(Vector2u& size)
{
    if (!m_targetTexture)
        return VK_NULL_HANDLE;

    updateTexture(*m_targetTexture);

    size = m_size;

    return m_targetTexture->getImage();
}

} // namespace sf::priv
