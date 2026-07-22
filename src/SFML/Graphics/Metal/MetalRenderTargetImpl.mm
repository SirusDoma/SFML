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
#include <SFML/Graphics/Metal/MetalRenderTargetImpl.hpp>
#include <SFML/Graphics/Metal/MetalShaderImpl.hpp>
#include <SFML/Graphics/Metal/MetalTextureImpl.hpp>
#include <SFML/Graphics/Metal/MetalVertexBufferImpl.hpp>
#include <SFML/Graphics/Vertex.hpp>

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
MetalRenderTargetImpl::MetalRenderTargetImpl(MetalGraphicsDevice& device) :
    m_device(device),
    m_modelView(identityMatrix),
    m_projection(identityMatrix),
    m_textureMatrix(identityMatrix)
{
}


////////////////////////////////////////////////////////////
bool MetalRenderTargetImpl::isActive(std::uint64_t id) const
{
    return m_device.getCurrentRenderTargetId() == id;
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::activate(RenderTarget& target, std::uint64_t id, bool active)
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
void MetalRenderTargetImpl::clear(RenderTarget& target, Color color)
{
    const MetalGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getCurrentSurface())
        return;

    // Pending draws may have stencil side effects that must land before the clear
    m_device.flushPendingDraws();

    // Apply the view so the projection and scissor stay in sync
    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    // A load action clears the whole attachment like glClear without scissor does,
    // scissored clears draw a quad the scissor rectangle clips
    if (cache.scissorEnabled)
    {
        clearWithQuad(target, color);
        return;
    }

    m_device.setPendingClearColor(color);
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::clearStencil(RenderTarget& target, StencilValue stencilValue)
{
    const MetalGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getCurrentSurface())
        return;

    m_device.flushPendingDraws();

    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    m_device.setPendingClearStencil(static_cast<std::uint8_t>(stencilValue.value));
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::clear(RenderTarget& target, Color color, StencilValue stencilValue)
{
    clear(target, color);

    const MetalGraphicsDevice::ContextLock lock(m_device);

    if (m_device.getCurrentSurface())
        m_device.setPendingClearStencil(static_cast<std::uint8_t>(stencilValue.value));
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::draw(RenderTarget&       target,
                                 const Vertex*       vertices,
                                 std::size_t         vertexCount,
                                 PrimitiveType       type,
                                 const RenderStates& states,
                                 bool                useVertexCache)
{
    const MetalGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getCurrentSurface())
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
            m_device.appendPendingVertices(quad.data(), quad.size(), MTLPrimitiveTypeTriangle);
        }
        else
        {
            m_device.appendPendingVertices(data,
                                           vertexCount,
                                           (type == PrimitiveType::Triangles) ? MTLPrimitiveTypeTriangle
                                           : (type == PrimitiveType::Lines)   ? MTLPrimitiveTypeLine
                                                                              : MTLPrimitiveTypePoint);
        }
    }
    else
    {
        m_device.flushPendingDraws();

        if (m_device.applyPendingState())
        {
            // A user shader binds its own pipeline instead of the applied built-in one
            if (states.shader)
                applyShader(states.shader);

            std::size_t firstVertex = 0;
            if (m_device.uploadVertices(data, vertexCount, firstVertex))
                drawPrimitives(type, firstVertex, vertexCount);
        }
    }

    cleanupDraw(target, states);

    // Update the cache
    cache.useVertexCache = useVertexCache;
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::draw(RenderTarget&       target,
                                 const VertexBuffer& vertexBuffer,
                                 std::size_t         firstVertex,
                                 std::size_t         vertexCount,
                                 const RenderStates& states)
{
    const MetalGraphicsDevice::ContextLock lock(m_device);

    if (!m_device.getCurrentSurface())
        return;

    auto* impl = static_cast<MetalVertexBufferImpl*>(getVertexBufferImpl(vertexBuffer));
    if (!impl || !impl->getBuffer())
        return;

    auto& cache = getCache(target);

    setupDraw(target, false, states);
    setPendingConstants();

    m_device.flushPendingDraws();

    if (m_device.applyPendingState())
    {
        // A user shader binds its own pipeline instead of the applied built-in one
        if (states.shader)
            applyShader(states.shader);

        m_device.bindVertexBuffer(impl->getBuffer());

        drawPrimitives(vertexBuffer.getPrimitiveType(), firstVertex, vertexCount);
    }

    cleanupDraw(target, states);

    // Update the cache
    cache.useVertexCache = false;
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::pushStates([[maybe_unused]] RenderTarget& target)
{
    // Metal state is fully owned by SFML, there is no user state to save
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::popStates([[maybe_unused]] RenderTarget& target)
{
    // Metal state is fully owned by SFML, there is no user state to restore
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::resetStates(RenderTarget& target, std::uint64_t id)
{
    const MetalGraphicsDevice::ContextLock lock(m_device);

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
void MetalRenderTargetImpl::applyCurrentView(RenderTarget& target)
{
    auto&       cache = getCache(target);
    const View& view  = target.getView();

    // Set the pending viewport, no bottom-up flip: Metal window coordinates start at the top-left corner
    const IntRect viewport = target.getViewport(view);

    MetalViewport metalViewport;
    metalViewport.x      = viewport.position.x;
    metalViewport.y      = viewport.position.y;
    metalViewport.width  = viewport.size.x;
    metalViewport.height = viewport.size.y;
    m_device.setPendingViewport(metalViewport);

    // Set the pending scissor rectangle and enable/disable scissor testing
    const bool scissorEnabled = view.getScissor() != FloatRect({0, 0}, {1, 1});

    MetalScissor scissorRect;
    if (scissorEnabled)
    {
        const IntRect pixelScissor = target.getScissor(view);

        scissorRect.x      = static_cast<std::uint32_t>(std::max(pixelScissor.position.x, 0));
        scissorRect.y      = static_cast<std::uint32_t>(std::max(pixelScissor.position.y, 0));
        scissorRect.width  = static_cast<std::uint32_t>(std::max(pixelScissor.size.x, 0));
        scissorRect.height = static_cast<std::uint32_t>(std::max(pixelScissor.size.y, 0));
    }
    m_device.setPendingScissor(scissorEnabled, scissorRect);
    cache.scissorEnabled = scissorEnabled;

    // Set the projection matrix
    std::memcpy(m_projection.data(), view.getTransform().getMatrix(), sizeof(float) * 16);

    cache.viewChanged = false;
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::applyBlendMode(RenderTarget& target, const BlendMode& mode, bool colorWrite)
{
    m_device.setPendingBlendMode(mode, colorWrite);

    getCache(target).lastBlendMode = mode;
    m_lastColorWrite               = colorWrite;
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::applyStencilMode(RenderTarget& target, const StencilMode& mode)
{
    m_device.setPendingDepthStencilState(m_device.getDepthStencilState(mode), mode.stencilReference.value);

    auto& cache           = getCache(target);
    cache.stencilEnabled  = !(mode == StencilMode());
    cache.lastStencilMode = mode;
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::applyTexture(RenderTarget& target, const Texture* texture, CoordinateType coordinateType)
{
    auto* impl = texture ? static_cast<MetalTextureImpl*>(getTextureImpl(*texture)) : nullptr;

    MetalTexturePtr metalTexture = (impl && impl->getTexture()) ? impl->getTexture() : m_device.getWhiteTexture();

    MetalSamplerStatePtr sampler = m_device.getSamplerState(texture && texture->isSmooth(),
                                                            texture && texture->isRepeated(),
                                                            texture && hasTextureMipmap(*texture));
    m_device.setPendingTexture(metalTexture, sampler);

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
void MetalRenderTargetImpl::applyShader(const Shader* shader)
{
    const auto* impl = shader ? static_cast<const MetalShaderImpl*>(getShaderImpl(*shader)) : nullptr;
    if (!impl)
        return;

    // The encoder keeps holding the shader across draws, binding is only needed
    // when the pending shader is not the one the encoder holds
    if (!m_device.takeUserShaderBindPending())
        return;

    impl->bind();

    // Textures assigned to the shader may have replaced the built-in texture binding
    if (impl->bindsExternalTextures())
        m_device.invalidateTextureBinding();
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::setupDraw(RenderTarget& target, bool useVertexCache, const RenderStates& states)
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
    const auto* shaderImpl = states.shader ? static_cast<const MetalShaderImpl*>(getShaderImpl(*states.shader)) : nullptr;
    m_device.setPendingUserShader(shaderImpl, shaderImpl ? shaderImpl->getPipelineStateId() : 0);
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::drawPrimitives(PrimitiveType type, std::size_t firstVertex, std::size_t vertexCount)
{
    id<MTLRenderCommandEncoder> encoder = m_device.getRenderEncoder();
    if (!encoder)
        return;

    // Metal has no triangle-fan topology, draw fans as an indexed triangle list
    if (type == PrimitiveType::TriangleFan)
    {
        std::size_t indexCount  = 0;
        auto*       indexBuffer = m_device.getTriangleFanIndexBuffer(vertexCount, indexCount);
        if (!indexBuffer)
            return;

        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:indexCount
                             indexType:MTLIndexTypeUInt32
                           indexBuffer:indexBuffer
                     indexBufferOffset:0
                         instanceCount:1
                            baseVertex:static_cast<NSInteger>(firstVertex)
                          baseInstance:0];
        return;
    }

    MTLPrimitiveType topology = MTLPrimitiveTypePoint;

    // clang-format off
    switch (type)
    {
        case PrimitiveType::Points:        topology = MTLPrimitiveTypePoint;         break;
        case PrimitiveType::Lines:         topology = MTLPrimitiveTypeLine;          break;
        case PrimitiveType::LineStrip:     topology = MTLPrimitiveTypeLineStrip;     break;
        case PrimitiveType::Triangles:     topology = MTLPrimitiveTypeTriangle;      break;
        case PrimitiveType::TriangleStrip: topology = MTLPrimitiveTypeTriangleStrip; break;
        case PrimitiveType::TriangleFan:   break;
    }
    // clang-format on

    [encoder drawPrimitives:topology vertexStart:firstVertex vertexCount:vertexCount];
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::cleanupDraw(RenderTarget& target, const RenderStates& states)
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
void MetalRenderTargetImpl::setPendingConstants()
{
    std::array<float, 48> constants{};
    std::memcpy(constants.data(), m_modelView.data(), sizeof(float) * 16);
    std::memcpy(constants.data() + 16, m_projection.data(), sizeof(float) * 16);
    std::memcpy(constants.data() + 32, m_textureMatrix.data(), sizeof(float) * 16);

    m_device.setPendingConstants(constants);
}


////////////////////////////////////////////////////////////
void MetalRenderTargetImpl::clearWithQuad(RenderTarget& target, Color color)
{
    const auto& cache = getCache(target);

    // Draw an untextured full-target quad in clip space with blending off, the
    // scissor rectangle clips it; the viewport has to cover the whole target so
    // the quad reaches scissor regions outside of the view's viewport
    const MetalTexturePtr      savedTexture = m_device.getCurrentTextureView();
    const MetalSamplerStatePtr savedSampler = m_device.getCurrentTextureSampler();

    MetalViewport fullViewport;
    fullViewport.width  = target.getSize().x;
    fullViewport.height = target.getSize().y;
    m_device.setPendingViewport(fullViewport);

    m_device.setPendingBlendMode(BlendNone, true);
    m_device.setPendingDepthStencilState(m_device.getDepthStencilState(StencilMode()), 0);
    m_device.setPendingTexture(m_device.getWhiteTexture(), m_device.getSamplerState(false, false));
    m_device.setPendingUserShader(nullptr, 0);

    std::array<float, 48> constants{};
    std::memcpy(constants.data(), identityMatrix.data(), sizeof(float) * 16);
    std::memcpy(constants.data() + 16, identityMatrix.data(), sizeof(float) * 16);
    std::memcpy(constants.data() + 32, identityMatrix.data(), sizeof(float) * 16);
    m_device.setPendingConstants(constants);

    if (m_device.applyPendingState())
    {
        const std::array<Vertex, 6> quad = {{{{-1.f, -1.f}, color, {}},
                                             {{1.f, -1.f}, color, {}},
                                             {{-1.f, 1.f}, color, {}},
                                             {{-1.f, 1.f}, color, {}},
                                             {{1.f, -1.f}, color, {}},
                                             {{1.f, 1.f}, color, {}}}};

        std::size_t firstVertex = 0;
        if (m_device.uploadVertices(quad.data(), quad.size(), firstVertex))
            drawPrimitives(PrimitiveType::Triangles, firstVertex, quad.size());
    }

    // Restore the pending state of the interrupted draws
    m_device.setPendingTexture(savedTexture, savedSampler);
    applyBlendMode(target, cache.lastBlendMode, m_lastColorWrite);
    applyStencilMode(target, cache.lastStencilMode);
    setPendingConstants();
    applyCurrentView(target);
}

} // namespace sf::priv
