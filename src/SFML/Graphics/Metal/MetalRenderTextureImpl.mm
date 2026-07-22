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
#include <SFML/Graphics/Metal/MetalRenderTextureImpl.hpp>
#include <SFML/Graphics/Metal/MetalTextureImpl.hpp>

#include <SFML/Window/ContextSettings.hpp>

#include <algorithm>


namespace sf::priv
{
////////////////////////////////////////////////////////////
MetalRenderTextureImpl::MetalRenderTextureImpl(MetalGraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
MetalRenderTextureImpl::~MetalRenderTextureImpl()
{
    m_device.unbindSurface(this);
}


////////////////////////////////////////////////////////////
bool MetalRenderTextureImpl::create(Vector2u size, TextureImpl& texture, const ContextSettings& settings)
{
    auto& metalTexture = static_cast<MetalTextureImpl&>(texture);

    id<MTLDevice>  device        = m_device.getDevice();
    id<MTLTexture> targetTexture = metalTexture.getTexture();
    if (!device || !targetTexture)
        return false;

    m_multisampleTexture.reset();
    m_depthStencilTexture.reset();
    m_depthStencilFormat = 0;

    const MTLPixelFormat format = [targetTexture pixelFormat];

    m_sampleCount = m_device.clampAntiAliasingLevel(std::max(settings.antiAliasingLevel, 1u));

    @autoreleasepool
    {
        if (m_sampleCount > 1)
        {
            // Render into a multisampled color buffer, resolved into the target texture on display
            MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                                                  width:size.x
                                                                                                 height:size.y
                                                                                              mipmapped:NO];
            descriptor.textureType = MTLTextureType2DMultisample;
            descriptor.sampleCount = m_sampleCount;
            descriptor.storageMode = MTLStorageModePrivate;
            descriptor.usage       = MTLTextureUsageRenderTarget;

            m_multisampleTexture.reset([device newTextureWithDescriptor:descriptor]);
            if (!m_multisampleTexture)
                return false;
        }

        if (settings.depthBits > 0 || settings.stencilBits > 0)
        {
            // SFML itself only ever tests stencil, a depth plane is allocated when
            // the user asked for one for their own Metal rendering
            m_depthStencilFormat = static_cast<std::uint32_t>(
                (settings.depthBits > 0) ? MTLPixelFormatDepth32Float_Stencil8 : MTLPixelFormatStencil8);

            MTLTextureDescriptor* descriptor =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:static_cast<MTLPixelFormat>(m_depthStencilFormat)
                                                                   width:size.x
                                                                  height:size.y
                                                               mipmapped:NO];
            if (m_sampleCount > 1)
            {
                descriptor.textureType = MTLTextureType2DMultisample;
                descriptor.sampleCount = m_sampleCount;
            }
            descriptor.storageMode = MTLStorageModePrivate;
            descriptor.usage       = MTLTextureUsageRenderTarget;

            m_depthStencilTexture.reset([device newTextureWithDescriptor:descriptor]);
            if (!m_depthStencilTexture)
                return false;
        }
    }

    m_size          = size;
    m_sRgb          = (format == MTLPixelFormatRGBA8Unorm_sRGB);
    m_targetTexture = &metalTexture;

    return true;
}


////////////////////////////////////////////////////////////
bool MetalRenderTextureImpl::activate(bool active)
{
    // Deactivation is a no-op, all surfaces share the single device
    if (!active)
        return true;

    m_device.bindSurface(this);

    return m_targetTexture != nullptr;
}


////////////////////////////////////////////////////////////
bool MetalRenderTextureImpl::isSrgb() const
{
    return m_sRgb;
}


////////////////////////////////////////////////////////////
void MetalRenderTextureImpl::updateTexture(TextureImpl& texture)
{
    // Without multisampling the rendering happened directly into the target texture
    if (!m_multisampleTexture)
        return;

    auto& metalTexture = static_cast<MetalTextureImpl&>(texture);

    id<MTLTexture> targetTexture = metalTexture.getTexture();
    if (!targetTexture)
        return;

    // Resolving is a pass store action, an empty pass carries it out
    m_device.resolvePass(m_multisampleTexture.get(), targetTexture);
}


////////////////////////////////////////////////////////////
bool MetalRenderTextureImpl::arePixelsFlipped() const
{
    // Metal renders top-down, matching sf::Texture's pixel order
    return false;
}


////////////////////////////////////////////////////////////
bool MetalRenderTextureImpl::needsFullActivationForDisplay() const
{
    return false;
}


////////////////////////////////////////////////////////////
bool MetalRenderTextureImpl::isTextureAttachment() const
{
    return true;
}


////////////////////////////////////////////////////////////
bool MetalRenderTextureImpl::prepareAttachments(MetalSurfaceAttachments& attachments)
{
    // Re-fetch the target texture, mipmap generation may have replaced it
    id<MTLTexture> targetTexture = m_targetTexture ? m_targetTexture->getTexture() : nullptr;
    if (!targetTexture)
        return false;

    attachments.color              = m_multisampleTexture ? m_multisampleTexture.get() : targetTexture;
    attachments.resolve            = nullptr;
    attachments.depthStencil       = m_depthStencilTexture.get();
    attachments.colorFormat        = static_cast<std::uint32_t>([targetTexture pixelFormat]);
    attachments.depthStencilFormat = m_depthStencilTexture ? m_depthStencilFormat : 0;
    attachments.sampleCount        = m_multisampleTexture ? m_sampleCount : 1;
    attachments.size               = m_size;

    return true;
}


////////////////////////////////////////////////////////////
MetalTexturePtr MetalRenderTextureImpl::acquireReadableColorTexture()
{
    id<MTLTexture> targetTexture = m_targetTexture ? m_targetTexture->getTexture() : nullptr;
    if (!targetTexture)
        return nullptr;

    const MetalGraphicsDevice::ContextLock lock(m_device);

    m_device.flushPendingDraws();
    m_device.materializePendingClears();
    m_device.endEncoding(false);

    if (m_multisampleTexture)
        m_device.resolvePass(m_multisampleTexture.get(), targetTexture);

    return targetTexture;
}

} // namespace sf::priv
