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
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>
#include <SFML/Graphics/Vulkan/VulkanRenderTargetImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanShaderImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanTextureImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanVertexBufferImpl.hpp>

#include <algorithm>

#include <cstring>


namespace
{
// clang-format off
constexpr std::array<float, 16> identityMatrix = {1.f, 0.f, 0.f, 0.f,
                                                  0.f, 1.f, 0.f, 0.f,
                                                  0.f, 0.f, 1.f, 0.f,
                                                  0.f, 0.f, 0.f, 1.f};
// clang-format on
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
VulkanRenderTargetImpl::VulkanRenderTargetImpl(VulkanGraphicsDevice& device) :
    m_device(device),
    m_modelView(identityMatrix),
    m_projection(identityMatrix),
    m_textureMatrix(identityMatrix)
{
}


////////////////////////////////////////////////////////////
bool VulkanRenderTargetImpl::isActive(std::uint64_t id) const
{
    return m_device.getCurrentRenderTargetId() == id;
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::activate(RenderTarget& target, std::uint64_t id, bool active)
{
    auto& cache = getCache(target);

    const std::uint64_t currentId = m_device.getCurrentRenderTargetId();

    if (active)
    {
        if (currentId == 0)
        {
            m_device.setCurrentRenderTargetId(id);

            cache.statesSet = false;
            cache.enable    = false;
        }
        else if (currentId != id)
        {
            m_device.setCurrentRenderTargetId(id);

            cache.enable = false;
        }
    }
    else
    {
        if (currentId == id)
            m_device.setCurrentRenderTargetId(0);

        cache.enable = false;
    }
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::clear(RenderTarget& target, Color color)
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getDevice() || !m_device.getCurrentSurface())
        return;

    // Apply the view so the projection and scissor stay in sync
    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    if (cache.scissorEnabled)
    {
        // Vulkan clears rectangles natively inside a render pass
        clearRect(target, VK_IMAGE_ASPECT_COLOR_BIT, color, {0});
        return;
    }

    // The clear becomes the load operation of the next render pass
    m_device.setPendingClearColor(color);
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::clearStencil(RenderTarget& target, StencilValue stencilValue)
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getDevice() || !m_device.getCurrentSurface())
        return;

    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    if (cache.scissorEnabled)
    {
        clearRect(target, VK_IMAGE_ASPECT_STENCIL_BIT, Color(), stencilValue);
        return;
    }

    m_device.setPendingClearStencil(static_cast<std::uint8_t>(stencilValue.value));
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::clear(RenderTarget& target, Color color, StencilValue stencilValue)
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getDevice() || !m_device.getCurrentSurface())
        return;

    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    if (cache.scissorEnabled)
    {
        clearRect(target, VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, color, stencilValue);
        return;
    }

    m_device.setPendingClearColor(color);
    m_device.setPendingClearStencil(static_cast<std::uint8_t>(stencilValue.value));
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::clearRect(RenderTarget& target, VkImageAspectFlags aspects, Color color, StencilValue stencilValue)
{
    // Clear rectangles must have a positive extent; an empty scissor (as on a
    // minimized window) has nothing to clear anyway
    const IntRect pixelScissor = target.getScissor(target.getView());
    if ((pixelScissor.size.x <= 0) || (pixelScissor.size.y <= 0))
        return;

    // Draws merged so far were issued before this clear and have to reach the
    // command buffer first, or they would be recorded after it and survive
    // inside the cleared rectangle
    m_device.flushPendingDraws();

    // A pass has to be open for a rectangle clear, pending full-target clears
    // become its load operations first
    if (!m_device.applyPendingState())
        return;

    VkCommandBuffer commandBuffer = m_device.getRenderCommandBuffer();
    if (!commandBuffer)
        return;

    // The rectangle comes from the size the target believes it has, which can
    // run ahead of the attachments the open pass was built from while a resize
    // is in flight; clearing outside the render area is invalid
    const Vector2u attachmentSize = m_device.getAttachmentSize();

    const auto clampAxis = [](int position, int size, unsigned int limit)
    {
        const auto offset = static_cast<unsigned int>(std::clamp(position, 0, static_cast<int>(limit)));
        const auto extent = static_cast<unsigned int>(std::max(size, 0));
        return std::pair{offset, std::min(extent, limit - offset)};
    };

    const auto [offsetX, extentX] = clampAxis(pixelScissor.position.x, pixelScissor.size.x, attachmentSize.x);
    const auto [offsetY, extentY] = clampAxis(pixelScissor.position.y, pixelScissor.size.y, attachmentSize.y);

    const Vector2u clampedOffset = {offsetX, offsetY};
    const Vector2u clampedSize   = {extentX, extentY};

    if ((clampedSize.x == 0) || (clampedSize.y == 0))
        return;

    VkClearRect rect{};
    rect.rect.offset    = {static_cast<std::int32_t>(clampedOffset.x), static_cast<std::int32_t>(clampedOffset.y)};
    rect.rect.extent    = {clampedSize.x, clampedSize.y};
    rect.baseArrayLayer = 0;
    rect.layerCount     = 1;

    std::array<VkClearAttachment, 2> attachments{};
    std::uint32_t                    attachmentCount = 0;

    if (aspects & VK_IMAGE_ASPECT_COLOR_BIT)
    {
        auto& attachment            = attachments[attachmentCount++];
        attachment.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT;
        attachment.colorAttachment  = 0;
        attachment.clearValue.color = {{color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f}};
    }

    // Clearing an aspect the pass has no attachment for is invalid, and a
    // target without a stencil buffer simply has nothing to clear
    if ((aspects & VK_IMAGE_ASPECT_STENCIL_BIT) && m_device.hasStencilAttachment())
    {
        auto& attachment                   = attachments[attachmentCount++];
        attachment.aspectMask              = VK_IMAGE_ASPECT_STENCIL_BIT;
        attachment.clearValue.depthStencil = {1.f, stencilValue.value};
    }

    if (attachmentCount == 0)
        return;

    m_device.fn().vkCmdClearAttachments(commandBuffer, attachmentCount, attachments.data(), 1, &rect);
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::draw(RenderTarget&       target,
                                  const Vertex*       vertices,
                                  std::size_t         vertexCount,
                                  PrimitiveType       type,
                                  const RenderStates& states,
                                  bool                useVertexCache)
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getDevice() || !m_device.getCurrentSurface())
        return;

    auto& cache = getCache(target);

    setupDraw(target, useVertexCache, states);
    setPendingConstants();

    const auto* data = useVertexCache ? cache.vertexCache.data() : vertices;

    // Merge list draws and single quads into the pending draw while the pipeline state
    // is unchanged, one draw call then covers all of them. Everything else, like fans,
    // strips, large draws and draws with a user shader, is submitted directly.
    constexpr std::size_t maxMergedVertices = 1024;

    const bool isList = (type == PrimitiveType::Triangles) || (type == PrimitiveType::Lines) ||
                        (type == PrimitiveType::Points);
    const bool isQuad = (type == PrimitiveType::TriangleStrip) && (vertexCount == 4);

    if (!states.shader && (isList || isQuad) && (vertexCount <= maxMergedVertices))
    {
        if (isQuad)
        {
            const std::array<Vertex, 6> quad = {data[0], data[1], data[2], data[2], data[1], data[3]};
            m_device.appendPendingVertices(quad.data(), quad.size(), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        }
        else
        {
            // A trailing incomplete primitive is dropped by the rasterizer of a
            // draw of its own, but merging would pair its vertices with the
            // first ones of the next draw into a primitive that never existed
            const std::size_t primitiveSize = (type == PrimitiveType::Triangles) ? 3
                                              : (type == PrimitiveType::Lines)   ? 2
                                                                                 : 1;
            const std::size_t mergedCount = vertexCount - (vertexCount % primitiveSize);

            if (mergedCount > 0)
                m_device.appendPendingVertices(data,
                                               mergedCount,
                                               (type == PrimitiveType::Triangles) ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
                                               : (type == PrimitiveType::Lines)   ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                                                                                  : VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
        }
    }
    else
    {
        m_device.flushPendingDraws();

        // The textures of a user shader have to reach their sampling layout
        // before the draw's render pass opens
        const auto* shaderImpl = states.shader ? static_cast<const VulkanShaderImpl*>(getShaderImpl(*states.shader)) : nullptr;
        if (shaderImpl)
            const_cast<VulkanShaderImpl*>(shaderImpl)->prepareTextures();

        if (!m_device.applyPendingState())
            return;

        // A user shader binds its own descriptors and modules on top of the
        // applied state; without them the draw has no set 0 fitting its layout
        if (states.shader && !applyShader(states.shader))
            return;

        std::size_t firstVertex = 0;
        if (!m_device.uploadVertices(data, vertexCount, firstVertex))
            return;

        drawPrimitives(type, firstVertex, vertexCount);
    }

    cleanupDraw(target, states);

    // Update the cache
    cache.useVertexCache = useVertexCache;
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::draw(RenderTarget&       target,
                                  const VertexBuffer& vertexBuffer,
                                  std::size_t         firstVertex,
                                  std::size_t         vertexCount,
                                  const RenderStates& states)
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getDevice() || !m_device.getCurrentSurface())
        return;

    auto* impl = static_cast<VulkanVertexBufferImpl*>(getVertexBufferImpl(vertexBuffer));
    if (!impl || !impl->getBuffer())
        return;

    auto& cache = getCache(target);

    setupDraw(target, false, states);
    setPendingConstants();

    m_device.flushPendingDraws();

    const auto* shaderImpl = states.shader ? static_cast<const VulkanShaderImpl*>(getShaderImpl(*states.shader)) : nullptr;
    if (shaderImpl)
        const_cast<VulkanShaderImpl*>(shaderImpl)->prepareTextures();

    if (!m_device.applyPendingState())
        return;

    // A user shader binds its own descriptors and modules on top of the
    // applied state; without them the draw has no set 0 fitting its layout
    if (states.shader && !applyShader(states.shader))
        return;

    m_device.bindVertexBuffer(impl->getBuffer());

    drawPrimitives(vertexBuffer.getPrimitiveType(), firstVertex, vertexCount);
    cleanupDraw(target, states);

    // Update the cache
    cache.useVertexCache = false;
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::pushStates([[maybe_unused]] RenderTarget& target)
{
    // Vulkan state is fully owned by SFML, there is no user state to save
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::popStates([[maybe_unused]] RenderTarget& target)
{
    // Vulkan state is fully owned by SFML, there is no user state to restore
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::resetStates(RenderTarget& target, std::uint64_t id)
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getDevice())
        return;

    if (isActive(id) || target.setActive(true))
    {
        auto& cache = getCache(target);

        // Raw user code may have changed any binding, the pending
        // state is fully re-applied when the next draw is submitted
        m_device.flushPendingDraws();
        m_device.invalidatePipeline();

        cache.scissorEnabled = false;
        cache.stencilEnabled = false;
        cache.statesSet      = true;

        // Reset the pending state to the SFML defaults
        applyBlendMode(target, BlendAlpha, true);
        applyStencilMode(target, StencilMode());
        applyTexture(target, nullptr);

        m_modelView          = identityMatrix;
        cache.useVertexCache = false;

        // Set the default view
        target.setView(target.getView());

        cache.enable = true;
    }
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::applyCurrentView(RenderTarget& target)
{
    auto&       cache = getCache(target);
    const View& view  = target.getView();

    // Set the pending viewport in top-left window coordinates, the device
    // applies the clip-space flip so the shared projection matrices work
    const IntRect viewport = target.getViewport(view);

    VkViewport vkViewport{};
    vkViewport.x        = static_cast<float>(viewport.position.x);
    vkViewport.y        = static_cast<float>(viewport.position.y);
    vkViewport.width    = static_cast<float>(viewport.size.x);
    vkViewport.height   = static_cast<float>(viewport.size.y);
    vkViewport.minDepth = 0.f;
    vkViewport.maxDepth = 1.f;
    m_device.setPendingViewport(vkViewport);

    // Set the pending scissor rectangle and enable/disable scissor testing
    const bool scissorEnabled = view.getScissor() != FloatRect({0, 0}, {1, 1});
    VkRect2D   scissorRect{};
    if (scissorEnabled)
    {
        const IntRect pixelScissor = target.getScissor(view);

        scissorRect.offset = {pixelScissor.position.x, pixelScissor.position.y};
        scissorRect.extent = {static_cast<std::uint32_t>(pixelScissor.size.x),
                              static_cast<std::uint32_t>(pixelScissor.size.y)};
    }
    m_device.setPendingScissor(scissorEnabled, scissorRect);
    cache.scissorEnabled = scissorEnabled;

    // Set the projection matrix
    std::memcpy(m_projection.data(), view.getTransform().getMatrix(), sizeof(float) * 16);

    cache.viewChanged = false;
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::applyBlendMode(RenderTarget& target, const BlendMode& mode, bool colorWrite)
{
    m_device.setPendingBlendMode(mode, colorWrite);

    getCache(target).lastBlendMode = mode;
    m_lastColorWrite               = colorWrite;
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::applyStencilMode(RenderTarget& target, const StencilMode& mode)
{
    m_device.setPendingStencilMode(mode);

    auto& cache           = getCache(target);
    cache.stencilEnabled  = !(mode == StencilMode());
    cache.lastStencilMode = mode;
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::applyTexture(RenderTarget& target, const Texture* texture, CoordinateType coordinateType)
{
    auto* impl = texture ? static_cast<VulkanTextureImpl*>(getTextureImpl(*texture)) : nullptr;

    // The draw's texture has to be in its sampling layout before the pass opens
    if (impl)
        impl->prepareForSampling();

    const VkImageView   view    = impl ? impl->getImageView() : m_device.getWhiteTextureView();
    const VkImageLayout layout  = impl ? impl->getSampleLayout() : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkSampler           sampler = m_device.getSamplerState(texture && texture->isSmooth(),
                                                 texture && texture->isRepeated(),
                                                 texture && hasTextureMipmap(*texture));

    m_device.setPendingTexture(view, sampler, layout);

    // Setup the texture coordinate matrix, converting pixel coordinates to the range [0 .. 1].
    // Unlike the OpenGL backend there is no padding and no flipped pixels to compensate for.
    m_textureMatrix = identityMatrix;
    if (texture && (coordinateType == CoordinateType::Pixels))
    {
        m_textureMatrix[0] = 1.f / static_cast<float>(texture->getSize().x);
        m_textureMatrix[5] = 1.f / static_cast<float>(texture->getSize().y);
    }

    auto& cache              = getCache(target);
    cache.lastTextureId      = texture ? getTextureCacheId(*texture) : 0;
    cache.lastCoordinateType = coordinateType;
}


////////////////////////////////////////////////////////////
bool VulkanRenderTargetImpl::applyShader(const Shader* shader)
{
    const auto* impl = shader ? static_cast<const VulkanShaderImpl*>(getShaderImpl(*shader)) : nullptr;
    if (!impl)
        return true;

    // The command buffer keeps holding the shader's descriptors across draws,
    // binding is only needed when the pending shader is not the one it holds
    if (!m_device.takeUserShaderBindPending())
        return true;

    return impl->bindForDraw();
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::setupDraw(RenderTarget& target, bool useVertexCache, const RenderStates& states)
{
    auto& cache = getCache(target);

    // First bind the pipeline if it's the very first call
    if (!cache.statesSet)
        resetStates(target, getId(target));

    if (useVertexCache)
    {
        // Since vertices are transformed, we must use an identity transform to render them
        if (!cache.enable || !cache.useVertexCache)
            m_modelView = identityMatrix;
    }
    else
    {
        std::memcpy(m_modelView.data(), states.transform.getMatrix(), sizeof(float) * 16);
    }

    // Apply the view
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    // Apply the blend mode, the color mask is part of the pipeline state
    const bool colorWrite = !states.stencilMode.stencilOnly;
    if (!cache.enable || (states.blendMode != cache.lastBlendMode) || (colorWrite != m_lastColorWrite))
        applyBlendMode(target, states.blendMode, colorWrite);

    // Apply the stencil mode
    if (!cache.enable || (states.stencilMode != cache.lastStencilMode))
        applyStencilMode(target, states.stencilMode);

    // Apply the texture
    if (!cache.enable || (states.texture && isTextureAttachment(*states.texture)))
    {
        applyTexture(target, states.texture, states.coordinateType);
    }
    else
    {
        const std::uint64_t textureId = states.texture ? getTextureCacheId(*states.texture) : 0;
        if (textureId != cache.lastTextureId || states.coordinateType != cache.lastCoordinateType)
            applyTexture(target, states.texture, states.coordinateType);
    }

    // Set the pending shader program of the draw
    const auto* shaderImpl = states.shader ? static_cast<const VulkanShaderImpl*>(getShaderImpl(*states.shader)) : nullptr;
    m_device.setPendingUserShader(shaderImpl, shaderImpl ? shaderImpl->getPipelineStateId() : 0);
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::drawPrimitives(PrimitiveType type, std::size_t firstVertex, std::size_t vertexCount)
{
    VkCommandBuffer commandBuffer = m_device.getRenderCommandBuffer();
    if (!commandBuffer)
        return;

    // The backend draws fans as an indexed triangle list, the shared pattern
    // works on every device regardless of triangle-fan support
    if (type == PrimitiveType::TriangleFan)
    {
        std::size_t indexCount  = 0;
        VkBuffer    indexBuffer = m_device.getTriangleFanIndexBuffer(vertexCount, indexCount);
        if (!indexBuffer)
            return;

        if (!m_device.bindDrawPipeline(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST))
            return;

        m_device.bindIndexBuffer(indexBuffer);
        m_device.fn().vkCmdDrawIndexed(commandBuffer,
                                       static_cast<std::uint32_t>(indexCount),
                                       1,
                                       0,
                                       static_cast<std::int32_t>(firstVertex),
                                       0);
        return;
    }

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    // clang-format off
    switch (type)
    {
        case PrimitiveType::Points:        topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;     break;
        case PrimitiveType::Lines:         topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;      break;
        case PrimitiveType::LineStrip:     topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;     break;
        case PrimitiveType::Triangles:     topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  break;
        case PrimitiveType::TriangleStrip: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
        case PrimitiveType::TriangleFan:   break;
    }
    // clang-format on

    if (!m_device.bindDrawPipeline(topology))
        return;

    m_device.fn()
        .vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(vertexCount), 1, static_cast<std::uint32_t>(firstVertex), 0);
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::cleanupDraw(RenderTarget& target, const RenderStates& states)
{
    // If the texture we used to draw belonged to a RenderTexture, then forcibly unbind that texture.
    if (states.texture && isTextureAttachment(*states.texture))
        applyTexture(target, nullptr);

    // Turn the color writes back on if necessary
    if (states.stencilMode.stencilOnly)
        applyBlendMode(target, states.blendMode, true);

    // Re-enable the cache at the end of the draw if it was disabled
    getCache(target).enable = true;
}


////////////////////////////////////////////////////////////
void VulkanRenderTargetImpl::setPendingConstants()
{
    std::array<float, 48> constants{};
    std::memcpy(constants.data(), m_modelView.data(), sizeof(float) * 16);
    std::memcpy(constants.data() + 16, m_projection.data(), sizeof(float) * 16);
    std::memcpy(constants.data() + 32, m_textureMatrix.data(), sizeof(float) * 16);

    m_device.setPendingConstants(constants);
}

} // namespace sf::priv
