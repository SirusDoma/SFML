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
#include <SFML/Graphics/Metal/MetalTextureImpl.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <ostream>
#include <vector>

#include <cassert>
#include <cstring>


namespace sf::priv
{
////////////////////////////////////////////////////////////
MetalTextureImpl::MetalTextureImpl(MetalGraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
bool MetalTextureImpl::create(Vector2u size, bool& sRgb, [[maybe_unused]] bool smooth, [[maybe_unused]] bool repeated, Vector2u& actualSize)
{
    // No padding is required on Metal
    actualSize = size;

    // Check the maximum texture size
    const unsigned int maxSize = m_device.getMaximumTextureSize();
    if ((actualSize.x > maxSize) || (actualSize.y > maxSize))
    {
        err() << "Failed to create texture, its internal size is too high "
              << "(" << actualSize.x << "x" << actualSize.y << ", "
              << "maximum is " << maxSize << "x" << maxSize << ")" << std::endl;
        return false;
    }

    id<MTLDevice> device = m_device.getDevice();
    if (!device)
    {
        err() << "Failed to create texture, no Metal device available" << std::endl;
        return false;
    }

    @autoreleasepool
    {
        // The render-target usage allows render textures to attach and GPU copies to resolve into
        // the texture. A single mip level is allocated, generateMipmap allocates the full chain.
        MTLTextureDescriptor* descriptor =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:sRgb ? MTLPixelFormatRGBA8Unorm_sRGB
                                                                          : MTLPixelFormatRGBA8Unorm
                                                               width:size.x
                                                              height:size.y
                                                           mipmapped:NO];
        descriptor.storageMode = static_cast<MTLStorageMode>(m_device.getTextureStorageMode());
        descriptor.usage       = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

        m_texture.reset([device newTextureWithDescriptor:descriptor]);
    }

    return m_texture.get() != nullptr;
}


////////////////////////////////////////////////////////////
void MetalTextureImpl::update(const std::uint8_t* pixels, Vector2u size, Vector2u dest, [[maybe_unused]] bool smooth)
{
    uploadPixels(pixels, size.x * 4, size, dest);
}


////////////////////////////////////////////////////////////
TextureImpl::UpdateResult MetalTextureImpl::update(
    const TextureImpl&    source,
    Vector2u              sourceSize,
    [[maybe_unused]] bool sourcePixelsFlipped,
    Vector2u              dest,
    [[maybe_unused]] bool smooth)
{
    // Pixels are never flipped on this backend
    assert(!sourcePixelsFlipped && "Flipped source pixels are not produced by the Metal backend");

    const auto& metalSource = static_cast<const MetalTextureImpl&>(source);

    if (!m_texture || !metalSource.m_texture)
        return UpdateResult::Failed;

    const MetalGraphicsDevice::ContextLock lock(m_device);

    // Draws collected so far must sample the textures before they change, and the
    // source content may still be a recorded clear
    m_device.flushPendingDraws();
    m_device.materializePendingClears();
    m_device.endEncoding(false);

    id<MTLCommandBuffer> commandBuffer = m_device.currentCommandBuffer();
    if (!commandBuffer)
        return UpdateResult::Failed;

    @autoreleasepool
    {
        id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
        [blit copyFromTexture:metalSource.m_texture.get()
                  sourceSlice:0
                  sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(sourceSize.x, sourceSize.y, 1)
                    toTexture:m_texture.get()
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(dest.x, dest.y, 0)];
        [blit endEncoding];
    }

    return UpdateResult::Updated;
}


////////////////////////////////////////////////////////////
void MetalTextureImpl::update(const Image& image, const IntRect& rectangle, [[maybe_unused]] bool smooth)
{
    const auto imageSize = Vector2i(image.getSize());

    // The source row pitch covers the full image, no row-by-row copy is needed for the
    // sub-rectangle; the rectangle selects the source area, the destination is the origin
    const std::uint8_t* pixels = image.getPixelsPtr() + 4 * (rectangle.position.x + (imageSize.x * rectangle.position.y));

    uploadPixels(pixels, static_cast<std::size_t>(imageSize.x) * 4, Vector2u(rectangle.size), Vector2u());
}


////////////////////////////////////////////////////////////
bool MetalTextureImpl::update(const Window& window, Vector2u dest, [[maybe_unused]] bool smooth, bool& pixelsFlipped)
{
    if (!m_texture)
        return false;

    // Only render windows have a Metal surface to copy from
    const auto* renderWindow = dynamic_cast<const RenderWindow*>(&window);
    if (!renderWindow)
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "Updating a texture is only supported from render windows on the Metal backend" << std::endl;

            warned = true;
        }

        return false;
    }

    // Activate the window so its surface is the one read from.
    // RenderWindow::setActive is not const like the Window::setActive the OpenGL backend calls.
    if (!const_cast<RenderWindow*>(renderWindow)->setActive(true))
        return false;

    const MetalGraphicsDevice::ContextLock lock(m_device);

    MetalRenderSurface* surface = m_device.getCurrentSurface();
    if (!surface)
        return false;

    id<MTLTexture> source = surface->acquireReadableColorTexture();
    if (!source)
        return false;

    const Vector2u size(std::min(window.getSize().x, static_cast<unsigned int>([source width])),
                        std::min(window.getSize().y, static_cast<unsigned int>([source height])));

    // The drawable format differs from the texture's, the copy converts through a sampling draw
    if (!m_device.copyTextureThroughDraw(source, m_texture.get(), size, dest))
        return false;

    // Metal drawables are stored top-down, matching the texture's pixel order
    pixelsFlipped = false;

    return true;
}


////////////////////////////////////////////////////////////
Image MetalTextureImpl::copyToImage(Vector2u size, [[maybe_unused]] Vector2u actualSize, [[maybe_unused]] bool pixelsFlipped) const
{
    // Create an array of pixels
    std::vector<std::uint8_t> pixels(std::size_t{size.x} * size.y * 4);

    id<MTLDevice> device = m_device.getDevice();
    if (m_texture && device)
    {
        const MetalGraphicsDevice::ContextLock lock(m_device);

        // Draws collected so far may render into this texture, and its content
        // may still be a recorded clear
        m_device.flushPendingDraws();
        m_device.materializePendingClears();
        m_device.endEncoding(false);

        if (id<MTLCommandBuffer> commandBuffer = m_device.currentCommandBuffer())
        {
            const std::size_t bytesPerRow = std::size_t{size.x} * 4;

            // The texture is copied to a shared buffer the CPU can read once the GPU is done
            id<MTLBuffer> staging = [device newBufferWithLength:pixels.size() options:MTLResourceStorageModeShared];
            if (staging)
            {
                @autoreleasepool
                {
                    id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
                    [blit copyFromTexture:m_texture.get()
                                     sourceSlice:0
                                     sourceLevel:0
                                    sourceOrigin:MTLOriginMake(0, 0, 0)
                                      sourceSize:MTLSizeMake(size.x, size.y, 1)
                                        toBuffer:staging
                               destinationOffset:0
                          destinationBytesPerRow:bytesPerRow
                        destinationBytesPerImage:0];
                    [blit endEncoding];
                }

                m_device.commitCommandBuffer(true);

                std::memcpy(pixels.data(), [staging contents], pixels.size());
                [staging release];
            }
        }
    }

    return {size, pixels.data()};
}


////////////////////////////////////////////////////////////
void MetalTextureImpl::setSmooth([[maybe_unused]] bool smooth, [[maybe_unused]] bool hasMipmap)
{
    // Sampler state is applied per draw by the render pipeline
}


////////////////////////////////////////////////////////////
void MetalTextureImpl::setRepeated([[maybe_unused]] bool repeated)
{
    // Sampler state is applied per draw by the render pipeline
}


////////////////////////////////////////////////////////////
bool MetalTextureImpl::generateMipmap([[maybe_unused]] bool smooth)
{
    id<MTLDevice> device = m_device.getDevice();
    if (!m_texture || !device)
        return false;

    const MetalGraphicsDevice::ContextLock lock(m_device);

    m_device.flushPendingDraws();
    m_device.materializePendingClears();
    m_device.endEncoding(false);

    id<MTLCommandBuffer> commandBuffer = m_device.currentCommandBuffer();
    if (!commandBuffer)
        return false;

    @autoreleasepool
    {
        // Textures are created with a single mip level, the first mipmap generation
        // re-creates the texture with the full chain and carries the base level over.
        // Render textures re-fetch the texture every pass, so they attach to the replacement.
        if ([m_texture.get() mipmapLevelCount] == 1)
        {
            MTLTextureDescriptor* descriptor =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:[m_texture.get() pixelFormat]
                                                                   width:[m_texture.get() width]
                                                                  height:[m_texture.get() height]
                                                               mipmapped:YES];
            descriptor.storageMode = static_cast<MTLStorageMode>(m_device.getTextureStorageMode());
            descriptor.usage       = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

            NSPtr<MetalTexturePtr> mippedTexture([device newTextureWithDescriptor:descriptor]);
            if (!mippedTexture)
                return false;

            id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
            [blit copyFromTexture:m_texture.get()
                      sourceSlice:0
                      sourceLevel:0
                     sourceOrigin:MTLOriginMake(0, 0, 0)
                       sourceSize:MTLSizeMake([m_texture.get() width], [m_texture.get() height], 1)
                        toTexture:mippedTexture.get()
                 destinationSlice:0
                 destinationLevel:0
                destinationOrigin:MTLOriginMake(0, 0, 0)];
            [blit endEncoding];

            m_texture = std::move(mippedTexture);
        }

        id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
        [blit generateMipmapsForTexture:m_texture.get()];
        [blit endEncoding];
    }

    return true;
}


////////////////////////////////////////////////////////////
void MetalTextureImpl::invalidateMipmap([[maybe_unused]] bool smooth)
{
}


////////////////////////////////////////////////////////////
unsigned int MetalTextureImpl::getNativeHandle() const
{
    // Metal resources have no GL-style integer handle
    return 0;
}


////////////////////////////////////////////////////////////
MetalTexturePtr MetalTextureImpl::getTexture() const
{
    return m_texture.get();
}


////////////////////////////////////////////////////////////
void MetalTextureImpl::uploadPixels(const std::uint8_t* pixels, std::size_t bytesPerRow, Vector2u size, Vector2u dest)
{
    if (!m_texture || (size.x == 0) || (size.y == 0))
        return;

    const MetalGraphicsDevice::ContextLock lock(m_device);

    // Draws collected so far must sample the texture before it changes, and a clear
    // recorded for this texture must land before the upload instead of wiping it
    m_device.flushPendingDraws();
    m_device.materializePendingClears();

    // While the GPU holds no work referencing the texture the pixels can be written directly
    if (m_device.isGpuIdle())
    {
        [m_texture.get() replaceRegion:MTLRegionMake2D(dest.x, dest.y, size.x, size.y)
                           mipmapLevel:0
                             withBytes:pixels
                           bytesPerRow:bytesPerRow];
        return;
    }

    // Otherwise the upload is encoded as a blit so it lands behind the recorded draws
    m_device.endEncoding(false);

    id<MTLDevice>        device        = m_device.getDevice();
    id<MTLCommandBuffer> commandBuffer = m_device.currentCommandBuffer();
    if (!device || !commandBuffer)
        return;

    // The last row does not span the full pitch, reading past it would overrun the source
    const std::size_t length = bytesPerRow * (size.y - 1) + std::size_t{size.x} * 4;

    id<MTLBuffer> staging = [device newBufferWithBytes:pixels length:length options:MTLResourceStorageModeShared];
    if (!staging)
        return;

    @autoreleasepool
    {
        id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
        [blit copyFromBuffer:staging
                   sourceOffset:0
              sourceBytesPerRow:bytesPerRow
            sourceBytesPerImage:0
                     sourceSize:MTLSizeMake(size.x, size.y, 1)
                      toTexture:m_texture.get()
               destinationSlice:0
               destinationLevel:0
              destinationOrigin:MTLOriginMake(dest.x, dest.y, 0)];
        [blit endEncoding];
    }

    // The command buffer keeps the staging memory alive until the copy executed
    [staging release];
}

} // namespace sf::priv
