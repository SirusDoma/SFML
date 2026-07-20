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
#include <SFML/Graphics/D3D11/D3D11GraphicsDevice.hpp>
#include <SFML/Graphics/D3D11/D3D11RenderTargetImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11ShaderImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11TextureImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11VertexBufferImpl.hpp>
#include <SFML/Graphics/Vertex.hpp>

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
D3D11RenderTargetImpl::D3D11RenderTargetImpl(D3D11GraphicsDevice& device) :
    m_device(device),
    m_modelView(identityMatrix),
    m_projection(identityMatrix),
    m_textureMatrix(identityMatrix)
{
}


////////////////////////////////////////////////////////////
bool D3D11RenderTargetImpl::isActive(std::uint64_t id) const
{
    return m_device.getCurrentRenderTargetId() == id;
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::activate(RenderTarget& target, std::uint64_t id, bool active)
{
    auto& cache = getCache(target);

    const std::uint64_t currentId = m_device.getCurrentRenderTargetId();

    if (active)
    {
        if (currentId == 0)
        {
            m_device.setCurrentRenderTargetId(id);

            cache.glStatesSet = false;
            cache.enable      = false;
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
void D3D11RenderTargetImpl::clear(RenderTarget& target, Color color)
{
    const D3D11GraphicsDevice::ContextLock lock(m_device);

    auto* context          = m_device.getContext();
    auto* renderTargetView = m_device.getCurrentRenderTargetView();
    if (!context || !renderTargetView)
        return;

    // Apply the view so the projection and scissor stay in sync
    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    const std::array clearColor = {color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f};

    // ClearRenderTargetView is not affected by scissor testing like glClear is,
    // scissored clears go through the 11.1 ClearView which takes a rectangle
    if (cache.scissorEnabled)
    {
        if (auto* context1 = m_device.getContext1())
        {
            const IntRect pixelScissor = target.getScissor(target.getView());

            D3D11_RECT rect{};
            rect.left   = pixelScissor.position.x;
            rect.top    = pixelScissor.position.y;
            rect.right  = pixelScissor.position.x + pixelScissor.size.x;
            rect.bottom = pixelScissor.position.y + pixelScissor.size.y;

            context1->ClearView(renderTargetView, clearColor.data(), &rect, 1);
            return;
        }

        static bool warned = false;
        if (!warned)
        {
            err() << "Scissored clears require Direct3D 11.1, clearing the whole target instead" << std::endl;
            warned = true;
        }
    }

    context->ClearRenderTargetView(renderTargetView, clearColor.data());
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::clearStencil(RenderTarget& target, StencilValue stencilValue)
{
    const D3D11GraphicsDevice::ContextLock lock(m_device);

    auto* context          = m_device.getContext();
    auto* depthStencilView = m_device.getCurrentDepthStencilView();
    if (!context)
        return;

    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    if (depthStencilView)
        context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_STENCIL, 1.f, static_cast<UINT8>(stencilValue.value));
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::clear(RenderTarget& target, Color color, StencilValue stencilValue)
{
    clear(target, color);

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    auto* context          = m_device.getContext();
    auto* depthStencilView = m_device.getCurrentDepthStencilView();
    if (context && depthStencilView)
        context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_STENCIL, 1.f, static_cast<UINT8>(stencilValue.value));
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::draw(RenderTarget&       target,
                                 const Vertex*       vertices,
                                 std::size_t         vertexCount,
                                 PrimitiveType       type,
                                 const RenderStates& states,
                                 bool                useVertexCache)
{
    const D3D11GraphicsDevice::ContextLock lock(m_device);

    auto* context = m_device.getContext();
    if (!context || !m_device.getCurrentRenderTargetView())
        return;

    auto& cache = getCache(target);

    setupDraw(target, useVertexCache, states);

    // Upload the vertices to the streaming vertex buffer
    const void* data = useVertexCache ? static_cast<const void*>(cache.vertexCache.data())
                                      : static_cast<const void*>(vertices);

    std::size_t firstVertex = 0;
    if (!m_device.uploadVertices(data, vertexCount, firstVertex))
        return;

    auto*      vertexBuffer = m_device.getStreamVertexBuffer();
    const UINT stride       = sizeof(Vertex);
    const UINT offset       = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    drawPrimitives(type, firstVertex, vertexCount);
    cleanupDraw(target, states);

    // Update the cache
    cache.useVertexCache = useVertexCache;
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::draw(RenderTarget&       target,
                                 const VertexBuffer& vertexBuffer,
                                 std::size_t         firstVertex,
                                 std::size_t         vertexCount,
                                 const RenderStates& states)
{
    const D3D11GraphicsDevice::ContextLock lock(m_device);

    auto* context = m_device.getContext();
    if (!context || !m_device.getCurrentRenderTargetView())
        return;

    auto* impl = static_cast<D3D11VertexBufferImpl*>(getVertexBufferImpl(vertexBuffer));
    if (!impl || !impl->getBuffer())
        return;

    auto& cache = getCache(target);

    setupDraw(target, false, states);

    auto*      buffer = impl->getBuffer();
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);

    drawPrimitives(vertexBuffer.getPrimitiveType(), firstVertex, vertexCount);
    cleanupDraw(target, states);

    // Update the cache
    cache.useVertexCache = false;
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::pushStates([[maybe_unused]] RenderTarget& target)
{
    // Direct3D state is fully owned by SFML, there is no user state to save
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::popStates([[maybe_unused]] RenderTarget& target)
{
    // Direct3D state is fully owned by SFML, there is no user state to restore
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::resetStates(RenderTarget& target, std::uint64_t id)
{
    const D3D11GraphicsDevice::ContextLock lock(m_device);

    auto* context = m_device.getContext();
    if (!context)
        return;

    if (isActive(id) || target.setActive(true))
    {
        auto& cache = getCache(target);

        // Bind the built-in pipeline
        context->IASetInputLayout(m_device.getInputLayout());
        context->RSSetState(m_device.getRasterizerState(false));

        cache.scissorEnabled = false;
        cache.stencilEnabled = false;
        cache.glStatesSet    = true;

        // Apply the default SFML states
        applyBlendMode(target, BlendAlpha, true);
        applyStencilMode(target, StencilMode());
        applyTexture(target, nullptr);
        applyShader(nullptr);

        m_modelView          = identityMatrix;
        cache.useVertexCache = false;

        // Set the default view
        target.setView(target.getView());

        cache.enable = true;
    }
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::applyCurrentView(RenderTarget& target)
{
    auto* context = m_device.getContext();

    auto&       cache = getCache(target);
    const View& view  = target.getView();

    // Set the viewport, no bottom-up flip: Direct3D window coordinates start at the top-left corner
    const IntRect viewport = target.getViewport(view);

    D3D11_VIEWPORT d3dViewport{};
    d3dViewport.TopLeftX = static_cast<float>(viewport.position.x);
    d3dViewport.TopLeftY = static_cast<float>(viewport.position.y);
    d3dViewport.Width    = static_cast<float>(viewport.size.x);
    d3dViewport.Height   = static_cast<float>(viewport.size.y);
    d3dViewport.MinDepth = 0.f;
    d3dViewport.MaxDepth = 1.f;
    context->RSSetViewports(1, &d3dViewport);

    // Set the scissor rectangle and enable/disable scissor testing
    if (view.getScissor() == FloatRect({0, 0}, {1, 1}))
    {
        if (!cache.enable || cache.scissorEnabled)
        {
            context->RSSetState(m_device.getRasterizerState(false));
            cache.scissorEnabled = false;
        }
    }
    else
    {
        const IntRect pixelScissor = target.getScissor(view);

        D3D11_RECT scissorRect{};
        scissorRect.left   = pixelScissor.position.x;
        scissorRect.top    = pixelScissor.position.y;
        scissorRect.right  = pixelScissor.position.x + pixelScissor.size.x;
        scissorRect.bottom = pixelScissor.position.y + pixelScissor.size.y;
        context->RSSetScissorRects(1, &scissorRect);

        if (!cache.enable || !cache.scissorEnabled)
        {
            context->RSSetState(m_device.getRasterizerState(true));
            cache.scissorEnabled = true;
        }
    }

    // Set the projection matrix
    std::memcpy(m_projection.data(), view.getTransform().getMatrix(), sizeof(float) * 16);

    cache.viewChanged = false;
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::applyBlendMode(RenderTarget& target, const BlendMode& mode, bool colorWrite)
{
    auto* context = m_device.getContext();

    context->OMSetBlendState(m_device.getBlendState(mode, colorWrite), nullptr, 0xFFFFFFFF);

    getCache(target).lastBlendMode = mode;
    m_lastColorWrite               = colorWrite;
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::applyStencilMode(RenderTarget& target, const StencilMode& mode)
{
    auto* context = m_device.getContext();

    context->OMSetDepthStencilState(m_device.getDepthStencilState(mode), mode.stencilReference.value);

    auto& cache           = getCache(target);
    cache.stencilEnabled  = !(mode == StencilMode());
    cache.lastStencilMode = mode;
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::applyTexture(RenderTarget& target, const Texture* texture, CoordinateType coordinateType)
{
    auto* context = m_device.getContext();

    auto* impl = texture ? static_cast<D3D11TextureImpl*>(getTextureImpl(*texture)) : nullptr;

    ID3D11ShaderResourceView* view = impl ? impl->getShaderResourceView() : m_device.getWhiteTextureView();
    context->PSSetShaderResources(0, 1, &view);

    ID3D11SamplerState* sampler = m_device.getSamplerState(texture && texture->isSmooth(),
                                                           texture && texture->isRepeated(),
                                                           texture && hasTextureMipmap(*texture));
    context->PSSetSamplers(0, 1, &sampler);

    // Setup the texture coordinate matrix, converting pixel coordinates to the range [0 .. 1].
    // Unlike the OpenGL backend there is no padding and no flipped pixels to compensate for.
    m_textureMatrix = identityMatrix;
    if (texture && (coordinateType == CoordinateType::Pixels))
    {
        m_textureMatrix[0] = 1.f / static_cast<float>(texture->getSize().x);
        m_textureMatrix[5] = 1.f / static_cast<float>(texture->getSize().y);
    }

    // Remember the bindings so shaders can resolve their CurrentTexture uniform
    m_device.setCurrentTextureView(view, sampler);

    auto& cache              = getCache(target);
    cache.lastTextureId      = texture ? getTextureCacheId(*texture) : 0;
    cache.lastCoordinateType = coordinateType;
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::applyShader(const Shader* shader)
{
    auto* context = m_device.getContext();

    const auto* impl = shader ? static_cast<const D3D11ShaderImpl*>(getShaderImpl(*shader)) : nullptr;

    if (impl)
    {
        impl->bind();
    }
    else
    {
        // Restore the built-in pipeline
        context->VSSetShader(m_device.getDefaultVertexShader(), nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(m_device.getDefaultPixelShader(), nullptr, 0);

        auto* constantBuffer = m_device.getConstantBuffer();
        context->VSSetConstantBuffers(0, 1, &constantBuffer);
    }
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::setupDraw(RenderTarget& target, bool useVertexCache, const RenderStates& states)
{
    auto& cache = getCache(target);

    // First bind the pipeline if it's the very first call
    if (!cache.glStatesSet)
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

    // Apply the blend mode, the color mask is part of the blend state
    const bool colorWrite = !states.stencilMode.stencilOnly;
    if (!cache.enable || (states.blendMode != cache.lastBlendMode) || (colorWrite != m_lastColorWrite))
        applyBlendMode(target, states.blendMode, colorWrite);

    // Apply the stencil mode
    if (!cache.enable || (states.stencilMode != cache.lastStencilMode))
        applyStencilMode(target, states.stencilMode);

    // Apply the texture
    if (!cache.enable || (states.texture && isTextureFboAttachment(*states.texture)))
    {
        applyTexture(target, states.texture, states.coordinateType);
    }
    else
    {
        const std::uint64_t textureId = states.texture ? getTextureCacheId(*states.texture) : 0;
        if (textureId != cache.lastTextureId || states.coordinateType != cache.lastCoordinateType)
            applyTexture(target, states.texture, states.coordinateType);
    }

    // Apply the shader
    if (states.shader)
        applyShader(states.shader);
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::drawPrimitives(PrimitiveType type, std::size_t firstVertex, std::size_t vertexCount)
{
    auto* context = m_device.getContext();

    uploadConstants();

    // Direct3D has no triangle-fan topology, draw fans as an indexed triangle list
    if (type == PrimitiveType::TriangleFan)
    {
        std::size_t indexCount  = 0;
        auto*       indexBuffer = m_device.getTriangleFanIndexBuffer(vertexCount, indexCount);
        if (!indexBuffer)
            return;

        context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->DrawIndexed(static_cast<UINT>(indexCount), 0, static_cast<INT>(firstVertex));
        return;
    }

    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;

    // clang-format off
    switch (type)
    {
        case PrimitiveType::Points:        topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;     break;
        case PrimitiveType::Lines:         topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;      break;
        case PrimitiveType::LineStrip:     topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;     break;
        case PrimitiveType::Triangles:     topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  break;
        case PrimitiveType::TriangleStrip: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
        case PrimitiveType::TriangleFan:   break;
    }
    // clang-format on

    context->IASetPrimitiveTopology(topology);
    context->Draw(static_cast<UINT>(vertexCount), static_cast<UINT>(firstVertex));
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::cleanupDraw(RenderTarget& target, const RenderStates& states)
{
    // Unbind the shader, if any
    if (states.shader)
        applyShader(nullptr);

    // If the texture we used to draw belonged to a RenderTexture, then forcibly unbind that texture.
    if (states.texture && isTextureFboAttachment(*states.texture))
        applyTexture(target, nullptr);

    // Turn the color writes back on if necessary
    if (states.stencilMode.stencilOnly)
        applyBlendMode(target, states.blendMode, true);

    // Re-enable the cache at the end of the draw if it was disabled
    getCache(target).enable = true;
}


////////////////////////////////////////////////////////////
void D3D11RenderTargetImpl::uploadConstants()
{
    auto* context        = m_device.getContext();
    auto* constantBuffer = m_device.getConstantBuffer();
    if (!constantBuffer)
        return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!d3dCheck(context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    auto* destination = static_cast<float*>(mapped.pData);
    std::memcpy(destination, m_modelView.data(), sizeof(float) * 16);
    std::memcpy(destination + 16, m_projection.data(), sizeof(float) * 16);
    std::memcpy(destination + 32, m_textureMatrix.data(), sizeof(float) * 16);
    context->Unmap(constantBuffer, 0);
}

} // namespace sf::priv
