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
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>
#include <SFML/Graphics/Vulkan/VulkanTextureImpl.hpp>

#include <SFML/System/Err.hpp>

#include <vma/vk_mem_alloc.h>

#include <algorithm>
#include <ostream>
#include <vector>

#include <cassert>
#include <cstring>


namespace sf::priv
{
////////////////////////////////////////////////////////////
VulkanTextureImpl::VulkanTextureImpl(VulkanGraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
VulkanTextureImpl::~VulkanTextureImpl()
{
    if (m_image)
    {
        const VulkanGraphicsDevice::ContextLock lock(m_device);

        // Draws collected so far may still sample the texture
        m_device.flushPendingDraws();
        if (m_attachmentView != m_view)
            m_device.deferDestroyImage(VK_NULL_HANDLE, m_attachmentView, nullptr);
        m_device.deferDestroyImage(m_image, m_view, m_allocation);
    }
}


////////////////////////////////////////////////////////////
bool VulkanTextureImpl::create(Vector2u size, bool& sRgb, [[maybe_unused]] bool smooth, [[maybe_unused]] bool repeated, Vector2u& actualSize)
{
    // No padding is required on Vulkan
    actualSize = size;

    const unsigned int maxSize = m_device.getMaximumTextureSize();
    if ((actualSize.x > maxSize) || (actualSize.y > maxSize))
    {
        err() << "Failed to create texture, its internal size is too high "
              << "(" << actualSize.x << "x" << actualSize.y << ", "
              << "maximum is " << maxSize << "x" << maxSize << ")" << std::endl;
        return false;
    }

    if (!m_device.getDevice())
    {
        err() << "Failed to create texture, no Vulkan device available" << std::endl;
        return false;
    }

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    const VkFormat format = sRgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    // The attachment usage allows render textures to attach and GPU copies to
    // resolve into the texture. A single mip level is allocated, generateMipmap
    // allocates the full chain on demand.
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = format;
    imageInfo.extent        = {size.x, size.y, 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

    // Built aside and swapped in only once complete, so a failure leaves the
    // texture with the contents it already had, like generateMipmap does
    VkImage       newImage      = VK_NULL_HANDLE;
    VmaAllocation newAllocation = nullptr;
    if (!vkCheck(vmaCreateImage(m_device.getAllocator(), &imageInfo, &allocationInfo, &newImage, &newAllocation, nullptr)))
        return false;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                       = newImage;
    viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                      = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView newView = VK_NULL_HANDLE;
    if (!vkCheck(m_device.fn().vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &newView)))
    {
        vmaDestroyImage(m_device.getAllocator(), newImage, newAllocation);
        return false;
    }

    if (m_image)
    {
        m_device.flushPendingDraws();
        if (m_attachmentView != m_view)
            m_device.deferDestroyImage(VK_NULL_HANDLE, m_attachmentView, nullptr);
        m_device.deferDestroyImage(m_image, m_view, m_allocation);
    }

    m_image      = newImage;
    m_view       = newView;
    m_allocation = newAllocation;
    m_format     = format;
    m_size       = size;
    m_mipLevels  = 1;

    // With a single mip level the sampling view doubles as the attachment view
    m_attachmentView = m_view;

    // A re-created attachment texture keeps living in the GENERAL layout
    if (m_attachment)
    {
        // Barriers are illegal inside a render pass, and one can be open here:
        // the flush above records the draws it collected into a pass, and a
        // texture that became an attachment while it had no image never flushed
        m_device.flushPendingDraws();
        m_device.materializePendingClears();
        m_device.endEncoding();

        if (VkCommandBuffer commandBuffer = m_device.currentCommandBuffer())
        {
            vulkanImageBarrier(m_device.fn(),
                               commandBuffer,
                               m_image,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_GENERAL);
            m_device.setTrackedImageLayout(m_image, VK_IMAGE_LAYOUT_GENERAL);
        }
    }

    return true;
}


////////////////////////////////////////////////////////////
VkCommandBuffer VulkanTextureImpl::beginTransfer(VkImageLayout newLayout)
{
    // Draws collected so far must sample the texture before it changes, and
    // transfer commands are illegal inside a render pass
    m_device.flushPendingDraws();
    m_device.materializePendingClears();
    m_device.endEncoding();

    VkCommandBuffer commandBuffer = m_device.currentCommandBuffer();
    if (!commandBuffer)
        return VK_NULL_HANDLE;

    const VkImageLayout current = m_device.getTrackedImageLayout(m_image);
    if (current != newLayout)
    {
        vulkanImageBarrier(m_device.fn(), commandBuffer, m_image, VK_IMAGE_ASPECT_COLOR_BIT, current, newLayout);
        m_device.setTrackedImageLayout(m_image, newLayout);
    }

    return commandBuffer;
}


////////////////////////////////////////////////////////////
VkImageLayout VulkanTextureImpl::getSampleLayout() const
{
    return m_attachment ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}


////////////////////////////////////////////////////////////
void VulkanTextureImpl::prepareForSampling()
{
    if (!m_image)
        return;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    const VkImageLayout current = m_device.getTrackedImageLayout(m_image);
    const VkImageLayout wanted  = getSampleLayout();
    if ((current == wanted) || (m_attachment && (current == VK_IMAGE_LAYOUT_GENERAL)))
        return;

    // The transition has to happen outside of the open pass, whose draws
    // sample other textures and are unaffected
    m_device.flushPendingDraws();
    m_device.materializePendingClears();
    m_device.endEncoding();

    if (VkCommandBuffer commandBuffer = m_device.currentCommandBuffer())
    {
        vulkanImageBarrier(m_device.fn(), commandBuffer, m_image, VK_IMAGE_ASPECT_COLOR_BIT, current, wanted);
        m_device.setTrackedImageLayout(m_image, wanted);
    }
}


////////////////////////////////////////////////////////////
void VulkanTextureImpl::update(const std::uint8_t* pixels, Vector2u size, Vector2u dest, [[maybe_unused]] bool smooth)
{
    if (!m_image || !pixels)
        return;

    // A zero-extent copy is not a legal command, and there is nothing to write
    if ((size.x == 0) || (size.y == 0))
        return;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    const VkImageLayout transferLayout = m_attachment ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkCommandBuffer commandBuffer = beginTransfer(transferLayout);
    if (!commandBuffer)
        return;

    VulkanTransientAllocation staging;
    if (!m_device.allocateTransient(pixels, std::size_t{size.x} * size.y * 4, 4, staging))
        return;

    VkBufferImageCopy region{};
    region.bufferOffset                = staging.offset;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset                 = {static_cast<std::int32_t>(dest.x), static_cast<std::int32_t>(dest.y), 0};
    region.imageExtent                 = {size.x, size.y, 1};

    m_device.fn().vkCmdCopyBufferToImage(commandBuffer, staging.buffer, m_image, transferLayout, 1, &region);

    prepareForSampling();
}


////////////////////////////////////////////////////////////
TextureImpl::UpdateResult VulkanTextureImpl::update(
    const TextureImpl&    source,
    Vector2u              sourceSize,
    [[maybe_unused]] bool sourcePixelsFlipped,
    Vector2u              dest,
    [[maybe_unused]] bool smooth)
{
    // Pixels are never flipped on this backend
    assert(!sourcePixelsFlipped && "Flipped source pixels are not produced by the Vulkan backend");

    const auto& vulkanSource = static_cast<const VulkanTextureImpl&>(source);

    if (!m_image || !vulkanSource.m_image)
        return UpdateResult::Failed;

    // A texture already holds its own pixels, and copying an image into itself
    // with fully overlapping regions is undefined
    if (vulkanSource.m_image == m_image)
        return UpdateResult::Updated;

    // A zero-extent copy is not a legal command
    if ((sourceSize.x == 0) || (sourceSize.y == 0))
        return UpdateResult::Updated;

    // Copies between different formats (like sRGB into linear) reinterpret the
    // raw data on GPU copies; both SFML formats share the same bit layout
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    const VkImageLayout sourceLayout = vulkanSource.m_attachment ? VK_IMAGE_LAYOUT_GENERAL
                                                                 : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const VkImageLayout destLayout   = m_attachment ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkCommandBuffer commandBuffer = beginTransfer(destLayout);
    if (!commandBuffer)
        return UpdateResult::Failed;

    const VkImageLayout sourceCurrent = m_device.getTrackedImageLayout(vulkanSource.m_image);
    if (sourceCurrent != sourceLayout)
    {
        vulkanImageBarrier(m_device.fn(),
                           commandBuffer,
                           vulkanSource.m_image,
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           sourceCurrent,
                           sourceLayout);
        m_device.setTrackedImageLayout(vulkanSource.m_image, sourceLayout);
    }

    VkImageCopy region{};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;
    region.dstOffset                 = {static_cast<std::int32_t>(dest.x), static_cast<std::int32_t>(dest.y), 0};
    region.extent                    = {sourceSize.x, sourceSize.y, 1};

    m_device.fn().vkCmdCopyImage(commandBuffer, vulkanSource.m_image, sourceLayout, m_image, destLayout, 1, &region);

    const_cast<VulkanTextureImpl&>(vulkanSource).prepareForSampling();
    prepareForSampling();

    return UpdateResult::Updated;
}


////////////////////////////////////////////////////////////
void VulkanTextureImpl::update(const Image& image, const IntRect& rectangle, [[maybe_unused]] bool smooth)
{
    if (!m_image)
        return;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    const VkImageLayout transferLayout = m_attachment ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkCommandBuffer commandBuffer = beginTransfer(transferLayout);
    if (!commandBuffer)
        return;

    const auto imageSize = Vector2i(image.getSize());

    // The source row length covers the full image, no row-by-row copy is
    // needed; the staged data spans from the first pixel of the sub-rectangle
    // to the last pixel of its final row
    const std::uint8_t* pixels = image.getPixelsPtr() + 4 * (rectangle.position.x + (imageSize.x * rectangle.position.y));
    const std::size_t bytes = (static_cast<std::size_t>(rectangle.size.y - 1) * static_cast<std::size_t>(imageSize.x) +
                               static_cast<std::size_t>(rectangle.size.x)) *
                              4;

    VulkanTransientAllocation staging;
    if (!m_device.allocateTransient(pixels, bytes, 4, staging))
        return;

    VkBufferImageCopy region{};
    region.bufferOffset                = staging.offset;
    region.bufferRowLength             = static_cast<std::uint32_t>(imageSize.x);
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset                 = {0, 0, 0};
    region.imageExtent = {static_cast<std::uint32_t>(rectangle.size.x), static_cast<std::uint32_t>(rectangle.size.y), 1};

    m_device.fn().vkCmdCopyBufferToImage(commandBuffer, staging.buffer, m_image, transferLayout, 1, &region);

    prepareForSampling();
}


////////////////////////////////////////////////////////////
bool VulkanTextureImpl::update(const Window& window, Vector2u dest, [[maybe_unused]] bool smooth, bool& pixelsFlipped)
{
    if (!m_image)
        return false;

    // Only render windows have a Vulkan surface to copy from
    const auto* renderWindow = dynamic_cast<const RenderWindow*>(&window);
    if (!renderWindow)
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "Updating a texture is only supported from render windows on the Vulkan backend" << std::endl;

            warned = true;
        }

        return false;
    }

    // Activate the window so its surface is bound to the device
    if (!const_cast<RenderWindow*>(renderWindow)->setActive(true))
        return false;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    VulkanRenderSurface* surface = m_device.getCurrentSurface();
    if (!surface)
        return false;

    Vector2u      surfaceSize;
    const VkImage sourceImage = surface->acquireReadableColorImage(surfaceSize);
    if (!sourceImage)
        return false;

    const VkImageLayout destLayout = m_attachment ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkCommandBuffer commandBuffer = beginTransfer(destLayout);
    if (!commandBuffer)
        return false;

    const VkImageLayout sourceCurrent = m_device.getTrackedImageLayout(sourceImage);
    if (sourceCurrent != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        vulkanImageBarrier(m_device.fn(),
                           commandBuffer,
                           sourceImage,
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           sourceCurrent,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        m_device.setTrackedImageLayout(sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }

    const Vector2u size(std::min(window.getSize().x, surfaceSize.x), std::min(window.getSize().y, surfaceSize.y));

    // Swapchain images commonly store their channels in BGRA order; a blit
    // converts between the formats where a raw copy would swap the channels
    VkImageBlit region{};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.srcOffsets[1]             = {static_cast<std::int32_t>(size.x), static_cast<std::int32_t>(size.y), 1};
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;
    region.dstOffsets[0]             = {static_cast<std::int32_t>(dest.x), static_cast<std::int32_t>(dest.y), 0};
    region.dstOffsets[1]             = {static_cast<std::int32_t>(dest.x + size.x), static_cast<std::int32_t>(dest.y + size.y), 1};

    m_device.fn().vkCmdBlitImage(commandBuffer,
                                 sourceImage,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 m_image,
                                 destLayout,
                                 1,
                                 &region,
                                 VK_FILTER_NEAREST);

    prepareForSampling();

    // Vulkan framebuffers are stored top-down, matching the texture's pixel order
    pixelsFlipped = false;

    return true;
}


////////////////////////////////////////////////////////////
Image VulkanTextureImpl::copyToImage(Vector2u size, [[maybe_unused]] Vector2u actualSize, [[maybe_unused]] bool pixelsFlipped) const
{
    std::vector<std::uint8_t> pixels(std::size_t{size.x} * size.y * 4);

    if (m_image && m_device.getDevice())
    {
        const VulkanGraphicsDevice::ContextLock lock(m_device);

        auto& self = const_cast<VulkanTextureImpl&>(*this);

        const VkImageLayout transferLayout = m_attachment ? VK_IMAGE_LAYOUT_GENERAL
                                                          : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        // Draws collected so far may render into this texture
        if (VkCommandBuffer commandBuffer = self.beginTransfer(transferLayout))
        {
            // The image is copied into a host-readable buffer the CPU maps
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size  = pixels.size();
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

            VmaAllocationCreateInfo allocationCreateInfo{};
            allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VkBuffer          stagingBuffer     = VK_NULL_HANDLE;
            VmaAllocation     stagingAllocation = nullptr;
            VmaAllocationInfo stagingInfo{};
            if (vkCheck(vmaCreateBuffer(m_device.getAllocator(),
                                        &bufferInfo,
                                        &allocationCreateInfo,
                                        &stagingBuffer,
                                        &stagingAllocation,
                                        &stagingInfo)))
            {
                VkBufferImageCopy region{};
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.layerCount = 1;
                region.imageExtent                 = {size.x, size.y, 1};

                m_device.fn().vkCmdCopyImageToBuffer(commandBuffer, m_image, transferLayout, stagingBuffer, 1, &region);

                self.prepareForSampling();

                m_device.commitCommandBuffer(true);

                vmaInvalidateAllocation(m_device.getAllocator(), stagingAllocation, 0, VK_WHOLE_SIZE);
                std::memcpy(pixels.data(), stagingInfo.pMappedData, pixels.size());

                vmaDestroyBuffer(m_device.getAllocator(), stagingBuffer, stagingAllocation);
            }
        }
    }

    return {size, pixels.data()};
}


////////////////////////////////////////////////////////////
void VulkanTextureImpl::setSmooth([[maybe_unused]] bool smooth, [[maybe_unused]] bool hasMipmap)
{
    // Sampler state is applied per draw by the render pipeline
}


////////////////////////////////////////////////////////////
void VulkanTextureImpl::setRepeated([[maybe_unused]] bool repeated)
{
    // Sampler state is applied per draw by the render pipeline
}


////////////////////////////////////////////////////////////
bool VulkanTextureImpl::generateMipmap([[maybe_unused]] bool smooth)
{
    if (!m_image || !m_device.getDevice())
        return false;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    // Textures are created with a single mip level, the first mipmap generation
    // re-creates the image with the full chain and carries the base level over
    if (m_mipLevels == 1)
    {
        std::uint32_t levels = 1;
        for (unsigned int dimension = std::max(m_size.x, m_size.y); dimension > 1; dimension /= 2)
            ++levels;

        if (levels == 1)
            return true;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = m_format;
        imageInfo.extent        = {m_size.x, m_size.y, 1};
        imageInfo.mipLevels     = levels;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VkImage       mippedImage      = VK_NULL_HANDLE;
        VmaAllocation mippedAllocation = nullptr;
        if (!vkCheck(vmaCreateImage(m_device.getAllocator(), &imageInfo, &allocationCreateInfo, &mippedImage, &mippedAllocation, nullptr)))
            return false;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                       = mippedImage;
        viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                      = m_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView mippedView = VK_NULL_HANDLE;
        if (!vkCheck(m_device.fn().vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &mippedView)))
        {
            vmaDestroyImage(m_device.getAllocator(), mippedImage, mippedAllocation);
            return false;
        }

        // Framebuffer attachments must cover a single mip level, render
        // textures keep rendering into the base level
        viewInfo.subresourceRange.levelCount = 1;

        VkImageView mippedAttachmentView = VK_NULL_HANDLE;
        if (!vkCheck(m_device.fn().vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &mippedAttachmentView)))
        {
            m_device.fn().vkDestroyImageView(m_device.getDevice(), mippedView, nullptr);
            vmaDestroyImage(m_device.getAllocator(), mippedImage, mippedAllocation);
            return false;
        }

        // Copy the base level over
        VkCommandBuffer commandBuffer = beginTransfer(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        if (!commandBuffer)
        {
            m_device.fn().vkDestroyImageView(m_device.getDevice(), mippedAttachmentView, nullptr);
            m_device.fn().vkDestroyImageView(m_device.getDevice(), mippedView, nullptr);
            vmaDestroyImage(m_device.getAllocator(), mippedImage, mippedAllocation);
            return false;
        }

        vulkanImageBarrier(m_device.fn(),
                           commandBuffer,
                           mippedImage,
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageCopy region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.extent                    = {m_size.x, m_size.y, 1};

        m_device.fn().vkCmdCopyImage(commandBuffer,
                                     m_image,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     mippedImage,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     1,
                                     &region);

        // A render texture bound to this texture keeps rendering into the old
        // image, force a re-activation so it attaches to the replacement
        if (m_attachment)
            m_device.setCurrentRenderTargetId(0);

        if (m_attachmentView != m_view)
            m_device.deferDestroyImage(VK_NULL_HANDLE, m_attachmentView, nullptr);
        m_device.deferDestroyImage(m_image, m_view, m_allocation);
        m_image          = mippedImage;
        m_view           = mippedView;
        m_attachmentView = mippedAttachmentView;
        m_allocation     = mippedAllocation;
        m_mipLevels      = levels;
        m_device.setTrackedImageLayout(m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    // Blit each level from the one above it
    m_device.flushPendingDraws();
    m_device.materializePendingClears();
    m_device.endEncoding();

    VkCommandBuffer commandBuffer = m_device.currentCommandBuffer();
    if (!commandBuffer)
        return false;

    // Bring the whole chain into the transfer destination layout first
    const VkImageLayout current = m_device.getTrackedImageLayout(m_image);
    if (current != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        vulkanImageBarrier(m_device.fn(),
                           commandBuffer,
                           m_image,
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           current,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    std::int32_t mipWidth  = static_cast<std::int32_t>(m_size.x);
    std::int32_t mipHeight = static_cast<std::int32_t>(m_size.y);

    for (std::uint32_t level = 1; level < m_mipLevels; ++level)
    {
        // The level above becomes the blit source
        VkImageMemoryBarrier barrier{};
        barrier.sType                         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                         = m_image;
        barrier.subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = level - 1;
        barrier.subresourceRange.levelCount   = 1;
        barrier.subresourceRange.layerCount   = 1;

        m_device.fn().vkCmdPipelineBarrier(commandBuffer,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           0,
                                           0,
                                           nullptr,
                                           0,
                                           nullptr,
                                           1,
                                           &barrier);

        const std::int32_t nextWidth  = std::max(mipWidth / 2, 1);
        const std::int32_t nextHeight = std::max(mipHeight / 2, 1);

        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel   = level - 1;
        blit.srcSubresource.layerCount = 1;
        blit.srcOffsets[1]             = {mipWidth, mipHeight, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel   = level;
        blit.dstSubresource.layerCount = 1;
        blit.dstOffsets[1]             = {nextWidth, nextHeight, 1};

        m_device.fn().vkCmdBlitImage(commandBuffer,
                                     m_image,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     m_image,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     1,
                                     &blit,
                                     VK_FILTER_LINEAR);

        // The consumed source level moves on to the sampling layout
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = getSampleLayout();

        m_device.fn().vkCmdPipelineBarrier(commandBuffer,
                                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                           0,
                                           0,
                                           nullptr,
                                           0,
                                           nullptr,
                                           1,
                                           &barrier);

        mipWidth  = nextWidth;
        mipHeight = nextHeight;
    }

    // The deepest level was only written, bring it in line with the rest
    VkImageMemoryBarrier barrier{};
    barrier.sType                         = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = getSampleLayout();
    barrier.srcQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex           = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                         = m_image;
    barrier.subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
    barrier.subresourceRange.levelCount   = 1;
    barrier.subresourceRange.layerCount   = 1;

    m_device.fn().vkCmdPipelineBarrier(commandBuffer,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                       0,
                                       0,
                                       nullptr,
                                       0,
                                       nullptr,
                                       1,
                                       &barrier);

    m_device.setTrackedImageLayout(m_image, getSampleLayout());

    return true;
}


////////////////////////////////////////////////////////////
void VulkanTextureImpl::invalidateMipmap([[maybe_unused]] bool smooth)
{
    // The extra levels are hidden by the samplers of non-mipmapped draws
}


////////////////////////////////////////////////////////////
unsigned int VulkanTextureImpl::getNativeHandle() const
{
    // Vulkan resources have no GL-style integer handle
    return 0;
}


////////////////////////////////////////////////////////////
VkImage VulkanTextureImpl::getImage() const
{
    return m_image;
}


////////////////////////////////////////////////////////////
VkImageView VulkanTextureImpl::getImageView() const
{
    return m_view;
}


////////////////////////////////////////////////////////////
VkImageView VulkanTextureImpl::getAttachmentView() const
{
    return m_attachmentView;
}


////////////////////////////////////////////////////////////
VkFormat VulkanTextureImpl::getFormat() const
{
    return m_format;
}


////////////////////////////////////////////////////////////
Vector2u VulkanTextureImpl::getSize() const
{
    return m_size;
}


////////////////////////////////////////////////////////////
void VulkanTextureImpl::setAttachment()
{
    if (m_attachment)
        return;

    m_attachment = true;

    if (!m_image)
        return;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    // Attachment textures live in the GENERAL layout so render passes and
    // sampling can alternate without transitions
    m_device.flushPendingDraws();
    m_device.materializePendingClears();
    m_device.endEncoding();

    if (VkCommandBuffer commandBuffer = m_device.currentCommandBuffer())
    {
        const VkImageLayout current = m_device.getTrackedImageLayout(m_image);
        if (current != VK_IMAGE_LAYOUT_GENERAL)
        {
            vulkanImageBarrier(m_device.fn(), commandBuffer, m_image, VK_IMAGE_ASPECT_COLOR_BIT, current, VK_IMAGE_LAYOUT_GENERAL);
            m_device.setTrackedImageLayout(m_image, VK_IMAGE_LAYOUT_GENERAL);
        }
    }
}

} // namespace sf::priv
