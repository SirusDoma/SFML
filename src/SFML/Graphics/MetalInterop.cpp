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
#include <SFML/Graphics/GraphicsDevice.hpp>
#include <SFML/Graphics/MetalInterop.hpp>
#include <SFML/Graphics/RenderTargetImpl.hpp>
#include <SFML/Graphics/Renderer.hpp>

#ifdef SFML_ENABLE_METAL
#include <SFML/Graphics/Metal/MetalGraphicsDevice.hpp>
#include <SFML/Graphics/Metal/MetalRenderWindowImpl.hpp>
#include <SFML/Graphics/Metal/MetalTextureImpl.hpp>
#include <SFML/Graphics/Metal/MetalVertexBufferImpl.hpp>
#endif


namespace
{
// Never instantiated, re-exposes the protected resource accessors of the implementation base
struct MetalImplAccess : sf::priv::RenderTargetImpl
{
    using RenderTargetImpl::getId;
    using RenderTargetImpl::getImpl;
    using RenderTargetImpl::getTextureImpl;
    using RenderTargetImpl::getVertexBufferImpl;
};

#ifdef SFML_ENABLE_METAL
// The Metal device, null unless the Metal renderer is the one alive
sf::priv::MetalGraphicsDevice* getMetalDevice()
{
    auto* device = sf::priv::getGraphicsDevice();
    if (!device || (device->getRenderer() != sf::Renderer::Metal))
        return nullptr;

    return static_cast<sf::priv::MetalGraphicsDevice*>(device);
}
#endif
} // namespace


namespace sf::Metal
{
////////////////////////////////////////////////////////////
DevicePtr getDevice()
{
#ifdef SFML_ENABLE_METAL
    if (auto* device = getMetalDevice())
        return device->getDevice();
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
CommandQueuePtr getCommandQueue()
{
#ifdef SFML_ENABLE_METAL
    if (auto* device = getMetalDevice())
        return device->getCommandQueue();
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
CommandBufferPtr getCommandBuffer()
{
#ifdef SFML_ENABLE_METAL
    if (auto* device = getMetalDevice())
        return device->currentCommandBuffer();
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
TexturePtr getRenderTargetTexture()
{
#ifdef SFML_ENABLE_METAL
    if (auto* device = getMetalDevice())
    {
        priv::MetalSurfaceAttachments attachments;
        if (device->getCurrentAttachments(attachments))
            return attachments.color;
    }
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
TexturePtr getDepthStencilTexture()
{
#ifdef SFML_ENABLE_METAL
    if (auto* device = getMetalDevice())
    {
        priv::MetalSurfaceAttachments attachments;
        if (device->getCurrentAttachments(attachments))
            return attachments.depthStencil;
    }
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
LayerPtr getLayer()
{
#ifdef SFML_ENABLE_METAL
    if (auto* device = getMetalDevice())
    {
        if (auto* window = dynamic_cast<priv::MetalRenderWindowImpl*>(device->getCurrentSurface()))
            return window->getLayer();
    }
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
TexturePtr getTexture([[maybe_unused]] const Texture& texture)
{
#ifdef SFML_ENABLE_METAL
    if (getMetalDevice())
    {
        if (auto* impl = MetalImplAccess::getTextureImpl(texture))
            return static_cast<priv::MetalTextureImpl*>(impl)->getTexture();
    }
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
BufferPtr getBuffer([[maybe_unused]] const VertexBuffer& vertexBuffer)
{
#ifdef SFML_ENABLE_METAL
    if (getMetalDevice())
    {
        if (auto* impl = MetalImplAccess::getVertexBufferImpl(vertexBuffer))
            return static_cast<priv::MetalVertexBufferImpl*>(impl)->getBuffer();
    }
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
void flush()
{
#ifdef SFML_ENABLE_METAL
    if (auto* device = getMetalDevice())
    {
        device->flushPendingDraws();
        device->materializePendingClears();
        device->endEncoding(false);
    }
#endif
}


////////////////////////////////////////////////////////////
void resetStates(RenderTarget& target)
{
    if (auto* impl = MetalImplAccess::getImpl(target))
        impl->resetStates(target, MetalImplAccess::getId(target));
}

} // namespace sf::Metal
