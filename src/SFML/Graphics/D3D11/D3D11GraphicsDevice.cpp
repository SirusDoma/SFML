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
#include <SFML/Graphics/D3D11/D3D11RenderTextureImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11RenderWindowImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11ShaderImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11TextureImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11VertexBufferImpl.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <d3dcompiler.h>
#include <memory>
#include <ostream>
#include <vector>

#include <cstring>


// The built-in shaders live in DefaultShader.hlsl, either precompiled to bytecode
// at build time or embedded as source and compiled when the device is created
#ifdef SFML_D3D11_PRECOMPILED_SHADERS
#include <D3D11DefaultPixelShader.hpp>
#include <D3D11DefaultVertexShader.hpp>
#else
#include <D3D11DefaultShaderSource.hpp>
#endif


namespace
{
// Convert an sf::BlendMode::Factor to the corresponding blend factor for the color channels
D3D11_BLEND factorToD3d(sf::BlendMode::Factor blendFactor)
{
    // clang-format off
    switch (blendFactor)
    {
        case sf::BlendMode::Factor::Zero:             return D3D11_BLEND_ZERO;
        case sf::BlendMode::Factor::One:              return D3D11_BLEND_ONE;
        case sf::BlendMode::Factor::SrcColor:         return D3D11_BLEND_SRC_COLOR;
        case sf::BlendMode::Factor::OneMinusSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
        case sf::BlendMode::Factor::DstColor:         return D3D11_BLEND_DEST_COLOR;
        case sf::BlendMode::Factor::OneMinusDstColor: return D3D11_BLEND_INV_DEST_COLOR;
        case sf::BlendMode::Factor::SrcAlpha:         return D3D11_BLEND_SRC_ALPHA;
        case sf::BlendMode::Factor::OneMinusSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
        case sf::BlendMode::Factor::DstAlpha:         return D3D11_BLEND_DEST_ALPHA;
        case sf::BlendMode::Factor::OneMinusDstAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
    }
    // clang-format on

    return D3D11_BLEND_ZERO;
}

// Convert an sf::BlendMode::Factor to the corresponding blend factor for the alpha channel.
// Direct3D rejects color factors in the alpha slots, the alpha variants are equivalent there.
D3D11_BLEND factorToD3dAlpha(sf::BlendMode::Factor blendFactor)
{
    // clang-format off
    switch (blendFactor)
    {
        case sf::BlendMode::Factor::SrcColor:         return D3D11_BLEND_SRC_ALPHA;
        case sf::BlendMode::Factor::OneMinusSrcColor: return D3D11_BLEND_INV_SRC_ALPHA;
        case sf::BlendMode::Factor::DstColor:         return D3D11_BLEND_DEST_ALPHA;
        case sf::BlendMode::Factor::OneMinusDstColor: return D3D11_BLEND_INV_DEST_ALPHA;
        default:                                      return factorToD3d(blendFactor);
    }
    // clang-format on
}

// Convert an sf::BlendMode::Equation to the corresponding blend operation
D3D11_BLEND_OP equationToD3d(sf::BlendMode::Equation blendEquation)
{
    // clang-format off
    switch (blendEquation)
    {
        case sf::BlendMode::Equation::Add:             return D3D11_BLEND_OP_ADD;
        case sf::BlendMode::Equation::Subtract:        return D3D11_BLEND_OP_SUBTRACT;
        case sf::BlendMode::Equation::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
        case sf::BlendMode::Equation::Min:             return D3D11_BLEND_OP_MIN;
        case sf::BlendMode::Equation::Max:             return D3D11_BLEND_OP_MAX;
    }
    // clang-format on

    return D3D11_BLEND_OP_ADD;
}

// Convert an sf::StencilComparison to the corresponding comparison function
D3D11_COMPARISON_FUNC stencilFunctionToD3d(sf::StencilComparison comparison)
{
    // clang-format off
    switch (comparison)
    {
        case sf::StencilComparison::Never:        return D3D11_COMPARISON_NEVER;
        case sf::StencilComparison::Less:         return D3D11_COMPARISON_LESS;
        case sf::StencilComparison::LessEqual:    return D3D11_COMPARISON_LESS_EQUAL;
        case sf::StencilComparison::Greater:      return D3D11_COMPARISON_GREATER;
        case sf::StencilComparison::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
        case sf::StencilComparison::Equal:        return D3D11_COMPARISON_EQUAL;
        case sf::StencilComparison::NotEqual:     return D3D11_COMPARISON_NOT_EQUAL;
        case sf::StencilComparison::Always:       return D3D11_COMPARISON_ALWAYS;
    }
    // clang-format on

    return D3D11_COMPARISON_ALWAYS;
}

// Convert an sf::StencilUpdateOperation to the corresponding stencil operation.
// The OpenGL backend uses the clamping GL_INCR/GL_DECR, so the saturating variants match.
D3D11_STENCIL_OP stencilOperationToD3d(sf::StencilUpdateOperation operation)
{
    // clang-format off
    switch (operation)
    {
        case sf::StencilUpdateOperation::Keep:      return D3D11_STENCIL_OP_KEEP;
        case sf::StencilUpdateOperation::Zero:      return D3D11_STENCIL_OP_ZERO;
        case sf::StencilUpdateOperation::Replace:   return D3D11_STENCIL_OP_REPLACE;
        case sf::StencilUpdateOperation::Increment: return D3D11_STENCIL_OP_INCR_SAT;
        case sf::StencilUpdateOperation::Decrement: return D3D11_STENCIL_OP_DECR_SAT;
        case sf::StencilUpdateOperation::Invert:    return D3D11_STENCIL_OP_INVERT;
    }
    // clang-format on

    return D3D11_STENCIL_OP_KEEP;
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
std::uint32_t packStencilMode(const sf::StencilMode& mode)
{
    const bool enabled = !(mode == sf::StencilMode());
    return (mode.stencilMask.value & 0xFFu) | (static_cast<std::uint32_t>(mode.stencilComparison) << 8) |
           (static_cast<std::uint32_t>(mode.stencilUpdateOperation) << 12) | (enabled ? 1u << 16 : 0u);
}
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
D3D11GraphicsDevice::D3D11GraphicsDevice()
{
    UINT flags = 0;
#ifdef SFML_DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    constexpr std::array featureLevels =
        {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

    HRESULT result = D3D11CreateDevice(nullptr,
                                       D3D_DRIVER_TYPE_HARDWARE,
                                       nullptr,
                                       flags,
                                       featureLevels.data(),
                                       static_cast<UINT>(featureLevels.size()),
                                       D3D11_SDK_VERSION,
                                       &m_device,
                                       &m_featureLevel,
                                       &m_context);

#ifdef SFML_DEBUG
    // The debug layer requires the Graphics Tools optional feature, retry without it
    if (FAILED(result))
    {
        flags &= ~static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG);
        result = D3D11CreateDevice(nullptr,
                                   D3D_DRIVER_TYPE_HARDWARE,
                                   nullptr,
                                   flags,
                                   featureLevels.data(),
                                   static_cast<UINT>(featureLevels.size()),
                                   D3D11_SDK_VERSION,
                                   &m_device,
                                   &m_featureLevel,
                                   &m_context);
    }
#endif

    // Fall back to the software rasterizer so the backend still works without a GPU
    if (FAILED(result))
    {
        result = D3D11CreateDevice(nullptr,
                                   D3D_DRIVER_TYPE_WARP,
                                   nullptr,
                                   flags,
                                   featureLevels.data(),
                                   static_cast<UINT>(featureLevels.size()),
                                   D3D11_SDK_VERSION,
                                   &m_device,
                                   &m_featureLevel,
                                   &m_context);
    }

    if (!d3dCheckResult(result, "D3D11CreateDevice"))
    {
        err() << "Failed to create the Direct3D 11 device" << std::endl;
        return;
    }

    // The 11.1 context enables scissored clears, its absence is handled gracefully
    m_context.As(&m_context1);

    createPipeline();
}


////////////////////////////////////////////////////////////
D3D11GraphicsDevice::~D3D11GraphicsDevice() = default;


////////////////////////////////////////////////////////////
std::unique_ptr<RenderTargetImpl> D3D11GraphicsDevice::createRenderTargetImpl()
{
    return std::make_unique<D3D11RenderTargetImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderTextureImpl> D3D11GraphicsDevice::createRenderTextureImpl()
{
    return std::make_unique<D3D11RenderTextureImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderWindowImpl> D3D11GraphicsDevice::createRenderWindowImpl(WindowHandle           handle,
                                                                              const ContextSettings& settings,
                                                                              unsigned int           bitsPerPixel)
{
    return std::make_unique<D3D11RenderWindowImpl>(*this, handle, settings, bitsPerPixel);
}


////////////////////////////////////////////////////////////
std::unique_ptr<ShaderImpl> D3D11GraphicsDevice::createShaderImpl()
{
    return std::make_unique<D3D11ShaderImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<TextureImpl> D3D11GraphicsDevice::createTextureImpl()
{
    return std::make_unique<D3D11TextureImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<VertexBufferImpl> D3D11GraphicsDevice::createVertexBufferImpl()
{
    return std::make_unique<D3D11VertexBufferImpl>(*this);
}


////////////////////////////////////////////////////////////
unsigned int D3D11GraphicsDevice::getMaximumAntiAliasingLevel()
{
    return clampAntiAliasingLevel(D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT, DXGI_FORMAT_R8G8B8A8_UNORM);
}


////////////////////////////////////////////////////////////
GraphicsBackend D3D11GraphicsDevice::getBackend() const
{
    return GraphicsBackend::Direct3D11;
}


////////////////////////////////////////////////////////////
ShadingLanguage D3D11GraphicsDevice::getShadingLanguage() const
{
    return ShadingLanguage::Hlsl;
}


////////////////////////////////////////////////////////////
ID3D11Device* D3D11GraphicsDevice::getDevice() const
{
    return m_device.Get();
}


////////////////////////////////////////////////////////////
ID3D11DeviceContext* D3D11GraphicsDevice::getContext() const
{
    return m_context.Get();
}


////////////////////////////////////////////////////////////
ID3D11DeviceContext1* D3D11GraphicsDevice::getContext1() const
{
    return m_context1.Get();
}


////////////////////////////////////////////////////////////
unsigned int D3D11GraphicsDevice::getMaximumTextureSize()
{
    return m_featureLevel >= D3D_FEATURE_LEVEL_11_0
               ? D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION
               : D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
}


////////////////////////////////////////////////////////////
bool D3D11GraphicsDevice::isShaderAvailable()
{
    return true;
}


////////////////////////////////////////////////////////////
bool D3D11GraphicsDevice::isGeometryShaderAvailable()
{
    return m_featureLevel >= D3D_FEATURE_LEVEL_10_0;
}


////////////////////////////////////////////////////////////
bool D3D11GraphicsDevice::isVertexBufferAvailable()
{
    return true;
}


////////////////////////////////////////////////////////////
unsigned int D3D11GraphicsDevice::clampAntiAliasingLevel(unsigned int level, DXGI_FORMAT format) const
{
    if (!m_device)
        return 1;

    const auto maxLevel = std::min<unsigned int>(level, D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT);

    for (unsigned int samples = maxLevel; samples > 1; --samples)
    {
        UINT quality = 0;
        if (SUCCEEDED(m_device->CheckMultisampleQualityLevels(format, samples, &quality)) && (quality > 0))
            return samples;
    }

    return 1;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::bindSurface(ID3D11RenderTargetView* renderTargetView, ID3D11DepthStencilView* depthStencilView)
{
    const ContextLock lock(*this);

    if (renderTargetView != m_currentRenderTargetView)
        flushPendingDraws();

    m_currentRenderTargetView = renderTargetView;
    m_currentDepthStencilView = depthStencilView;

    if (m_context)
        m_context->OMSetRenderTargets(1, &renderTargetView, depthStencilView);
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::unbindSurface(ID3D11RenderTargetView* renderTargetView)
{
    const ContextLock lock(*this);

    if (m_currentRenderTargetView != renderTargetView)
        return;

    flushPendingDraws();

    m_currentRenderTargetView = nullptr;
    m_currentDepthStencilView = nullptr;
    m_currentRenderTargetId   = 0;

    if (m_context)
    {
        ID3D11RenderTargetView* nullView = nullptr;
        m_context->OMSetRenderTargets(1, &nullView, nullptr);
    }
}


////////////////////////////////////////////////////////////
ID3D11RenderTargetView* D3D11GraphicsDevice::getCurrentRenderTargetView() const
{
    return m_currentRenderTargetView;
}


////////////////////////////////////////////////////////////
ID3D11DepthStencilView* D3D11GraphicsDevice::getCurrentDepthStencilView() const
{
    return m_currentDepthStencilView;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::setCurrentRenderTargetId(std::uint64_t id)
{
    const ContextLock lock(*this);

    m_currentRenderTargetId = id;
}


////////////////////////////////////////////////////////////
std::uint64_t D3D11GraphicsDevice::getCurrentRenderTargetId() const
{
    const ContextLock lock(*this);

    return m_currentRenderTargetId;
}


////////////////////////////////////////////////////////////
ID3D11ShaderResourceView* D3D11GraphicsDevice::getCurrentTextureView() const
{
    const ContextLock lock(*this);

    return m_pending.textureView;
}


////////////////////////////////////////////////////////////
ID3D11SamplerState* D3D11GraphicsDevice::getCurrentTextureSampler() const
{
    const ContextLock lock(*this);

    return m_pending.textureSampler;
}


////////////////////////////////////////////////////////////
ID3D11BlendState* D3D11GraphicsDevice::getBlendState(const BlendMode& mode, bool colorWrite)
{
    const ContextLock lock(*this);

    const std::uint32_t key = packBlendMode(mode, colorWrite);

    const auto it = m_blendStates.find(key);
    if (it != m_blendStates.end())
        return it->second.Get();

    D3D11_BLEND_DESC desc{};
    desc.RenderTarget[0].BlendEnable           = TRUE;
    desc.RenderTarget[0].SrcBlend              = factorToD3d(mode.colorSrcFactor);
    desc.RenderTarget[0].DestBlend             = factorToD3d(mode.colorDstFactor);
    desc.RenderTarget[0].BlendOp               = equationToD3d(mode.colorEquation);
    desc.RenderTarget[0].SrcBlendAlpha         = factorToD3dAlpha(mode.alphaSrcFactor);
    desc.RenderTarget[0].DestBlendAlpha        = factorToD3dAlpha(mode.alphaDstFactor);
    desc.RenderTarget[0].BlendOpAlpha          = equationToD3d(mode.alphaEquation);
    desc.RenderTarget[0].RenderTargetWriteMask = colorWrite ? D3D11_COLOR_WRITE_ENABLE_ALL : 0;

    ComPtr<ID3D11BlendState> state;
    if (m_device)
        d3dCheck(m_device->CreateBlendState(&desc, &state));

    return m_blendStates.emplace(key, std::move(state)).first->second.Get();
}


////////////////////////////////////////////////////////////
ID3D11DepthStencilState* D3D11GraphicsDevice::getDepthStencilState(const StencilMode& mode)
{
    const ContextLock lock(*this);

    const std::uint32_t key = packStencilMode(mode);

    const auto it = m_depthStencilStates.find(key);
    if (it != m_depthStencilStates.end())
        return it->second.Get();

    D3D11_DEPTH_STENCIL_DESC desc{};
    desc.DepthEnable   = FALSE;
    desc.DepthFunc     = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable = mode == StencilMode() ? FALSE : TRUE;
    // The stencil mask is truncated, Direct3D read masks are limited to 8 bits
    desc.StencilReadMask  = static_cast<UINT8>(mode.stencilMask.value & 0xFFu);
    desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

    const D3D11_DEPTH_STENCILOP_DESC operations{/* StencilFailOp */ D3D11_STENCIL_OP_KEEP,
                                                /* StencilDepthFailOp */
                                                stencilOperationToD3d(mode.stencilUpdateOperation),
                                                /* StencilPassOp */
                                                stencilOperationToD3d(mode.stencilUpdateOperation),
                                                /* StencilFunc */
                                                stencilFunctionToD3d(mode.stencilComparison)};
    desc.FrontFace = operations;
    desc.BackFace  = operations;

    ComPtr<ID3D11DepthStencilState> state;
    if (m_device)
        d3dCheck(m_device->CreateDepthStencilState(&desc, &state));

    return m_depthStencilStates.emplace(key, std::move(state)).first->second.Get();
}


////////////////////////////////////////////////////////////
ID3D11RasterizerState* D3D11GraphicsDevice::getRasterizerState(bool scissorEnabled)
{
    const ContextLock lock(*this);

    auto& state = m_rasterizerStates[scissorEnabled ? 1 : 0];
    if (!state && m_device)
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode              = D3D11_FILL_SOLID;
        desc.CullMode              = D3D11_CULL_NONE;
        desc.DepthClipEnable       = TRUE;
        desc.ScissorEnable         = scissorEnabled ? TRUE : FALSE;
        desc.MultisampleEnable     = TRUE;
        desc.AntialiasedLineEnable = FALSE;

        d3dCheck(m_device->CreateRasterizerState(&desc, &state));
    }

    return state.Get();
}


////////////////////////////////////////////////////////////
ID3D11SamplerState* D3D11GraphicsDevice::getSamplerState(bool smooth, bool repeated, bool mipmapped)
{
    const ContextLock lock(*this);

    auto& state = m_samplerStates[(smooth ? 1u : 0u) | (repeated ? 2u : 0u) | (mipmapped ? 4u : 0u)];
    if (!state && m_device)
    {
        const D3D11_TEXTURE_ADDRESS_MODE addressMode = repeated ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;

        D3D11_SAMPLER_DESC desc{};
        desc.AddressU       = addressMode;
        desc.AddressV       = addressMode;
        desc.AddressW       = addressMode;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;

        if (mipmapped)
        {
            // Matches the GL_LINEAR_MIPMAP_LINEAR / GL_NEAREST_MIPMAP_LINEAR filters of the OpenGL backend
            desc.Filter = smooth ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            desc.MaxLOD = D3D11_FLOAT32_MAX;
        }
        else
        {
            // A zero MaxLOD hides the extra levels of textures whose mipmap was invalidated
            desc.Filter = smooth ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
            desc.MaxLOD = 0.f;
        }

        d3dCheck(m_device->CreateSamplerState(&desc, &state));
    }

    return state.Get();
}


////////////////////////////////////////////////////////////
ID3D11VertexShader* D3D11GraphicsDevice::getDefaultVertexShader() const
{
    return m_defaultVertexShader.Get();
}


////////////////////////////////////////////////////////////
ID3D11PixelShader* D3D11GraphicsDevice::getDefaultPixelShader() const
{
    return m_defaultPixelShader.Get();
}


////////////////////////////////////////////////////////////
ID3D11InputLayout* D3D11GraphicsDevice::getInputLayout() const
{
    return m_inputLayout.Get();
}


////////////////////////////////////////////////////////////
ID3D11Buffer* D3D11GraphicsDevice::getConstantBuffer() const
{
    return m_constantBuffer.Get();
}


////////////////////////////////////////////////////////////
ID3D11ShaderResourceView* D3D11GraphicsDevice::getWhiteTextureView() const
{
    return m_whiteTextureView.Get();
}


////////////////////////////////////////////////////////////
bool D3D11GraphicsDevice::uploadVertices(const void* vertices, std::size_t vertexCount, std::size_t& firstVertex)
{
    const ContextLock lock(*this);

    if (!m_device || !m_context)
        return false;

    // Grow the streaming buffer when the batch doesn't fit at all
    if (vertexCount > m_streamVertexBufferSize)
    {
        const std::size_t newSize = std::max<std::size_t>({vertexCount, m_streamVertexBufferSize * 2, 4096});

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth      = static_cast<UINT>(sizeof(Vertex) * newSize);
        desc.Usage          = D3D11_USAGE_DYNAMIC;
        desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        m_streamVertexBuffer.Reset();
        if (!d3dCheck(m_device->CreateBuffer(&desc, nullptr, &m_streamVertexBuffer)))
            return false;

        m_streamVertexBufferSize   = newSize;
        m_streamVertexBufferCursor = 0;
    }

    // Append with no-overwrite, orphan the buffer when wrapping around
    D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
    if (m_streamVertexBufferCursor + vertexCount > m_streamVertexBufferSize)
    {
        m_streamVertexBufferCursor = 0;
        mapType                    = D3D11_MAP_WRITE_DISCARD;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!d3dCheck(m_context->Map(m_streamVertexBuffer.Get(), 0, mapType, 0, &mapped)))
        return false;

    std::memcpy(static_cast<std::byte*>(mapped.pData) + sizeof(Vertex) * m_streamVertexBufferCursor,
                vertices,
                sizeof(Vertex) * vertexCount);
    m_context->Unmap(m_streamVertexBuffer.Get(), 0);

    firstVertex = m_streamVertexBufferCursor;
    m_streamVertexBufferCursor += vertexCount;

    return true;
}


////////////////////////////////////////////////////////////
ID3D11Buffer* D3D11GraphicsDevice::getStreamVertexBuffer() const
{
    return m_streamVertexBuffer.Get();
}


////////////////////////////////////////////////////////////
ID3D11Buffer* D3D11GraphicsDevice::getTriangleFanIndexBuffer(std::size_t vertexCount, std::size_t& indexCount)
{
    const ContextLock lock(*this);

    if (!m_device || (vertexCount < 3))
        return nullptr;

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

        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = static_cast<UINT>(sizeof(std::uint32_t) * indices.size());
        desc.Usage     = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = indices.data();

        m_fanIndexBuffer.Reset();
        if (!d3dCheck(m_device->CreateBuffer(&desc, &data, &m_fanIndexBuffer)))
        {
            m_fanIndexBufferVertices = 0;
            return nullptr;
        }

        m_fanIndexBufferVertices = newVertices;
    }

    return m_fanIndexBuffer.Get();
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::setPendingBlendState(ID3D11BlendState* state)
{
    const ContextLock lock(*this);

    if (m_pending.blendState == state)
        return;

    flushPendingDraws();
    m_pending.blendState = state;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::setPendingDepthStencilState(ID3D11DepthStencilState* state, UINT reference)
{
    const ContextLock lock(*this);

    if ((m_pending.depthStencilState == state) && (m_pending.stencilReference == reference))
        return;

    flushPendingDraws();
    m_pending.depthStencilState = state;
    m_pending.stencilReference  = reference;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::setPendingTexture(ID3D11ShaderResourceView* view, ID3D11SamplerState* sampler)
{
    const ContextLock lock(*this);

    if ((m_pending.textureView == view) && (m_pending.textureSampler == sampler))
        return;

    flushPendingDraws();
    m_pending.textureView    = view;
    m_pending.textureSampler = sampler;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::setPendingUserShader(const void* shader, std::uint64_t stateId)
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
void D3D11GraphicsDevice::setPendingRasterizerState(ID3D11RasterizerState* state)
{
    const ContextLock lock(*this);

    if (m_pending.rasterizerState == state)
        return;

    flushPendingDraws();
    m_pending.rasterizerState = state;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::setPendingViewport(const D3D11_VIEWPORT& viewport)
{
    const ContextLock lock(*this);

    if (std::memcmp(&m_pending.viewport, &viewport, sizeof(viewport)) == 0)
        return;

    flushPendingDraws();
    m_pending.viewport = viewport;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::setPendingScissorRect(const D3D11_RECT& rect)
{
    const ContextLock lock(*this);

    if (std::memcmp(&m_pending.scissorRect, &rect, sizeof(rect)) == 0)
        return;

    flushPendingDraws();
    m_pending.scissorRect = rect;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::setPendingConstants(const std::array<float, 48>& constants)
{
    const ContextLock lock(*this);

    if (std::memcmp(m_pending.constants.data(), constants.data(), sizeof(constants)) == 0)
        return;

    flushPendingDraws();
    m_pending.constants = constants;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::applyPendingState()
{
    const ContextLock lock(*this);

    if (!m_context)
        return;

    // Restore the built-in pipeline when the context holds a different program; a
    // pending user shader is bound on top of it afterwards by the draw that set it
    if (!m_appliedProgramValid || (m_applied.userShader != m_pending.userShader) ||
        (m_applied.userShaderState != m_pending.userShaderState))
    {
        // The rebind is skipped when the context holds the plain built-in pipeline,
        // a user shader only adds its own stages on top of it
        if (!m_appliedProgramValid || (m_applied.userShader && (m_applied.userShader != m_pending.userShader)))
        {
            m_context->VSSetShader(m_defaultVertexShader.Get(), nullptr, 0);
            m_context->GSSetShader(nullptr, nullptr, 0);
            m_context->PSSetShader(m_defaultPixelShader.Get(), nullptr, 0);
            m_context->IASetInputLayout(m_inputLayout.Get());

            auto* constantBuffer = m_constantBuffer.Get();
            m_context->VSSetConstantBuffers(0, 1, &constantBuffer);
        }

        m_userShaderBindPending = (m_pending.userShader != nullptr);
        m_appliedProgramValid   = true;
    }

    if (!m_appliedValid || (m_applied.blendState != m_pending.blendState))
        m_context->OMSetBlendState(m_pending.blendState, nullptr, 0xFFFFFFFF);

    if (!m_appliedValid || (m_applied.depthStencilState != m_pending.depthStencilState) ||
        (m_applied.stencilReference != m_pending.stencilReference))
        m_context->OMSetDepthStencilState(m_pending.depthStencilState, m_pending.stencilReference);

    if (!m_appliedValid || !m_appliedTextureValid || (m_applied.textureView != m_pending.textureView))
        m_context->PSSetShaderResources(0, 1, &m_pending.textureView);

    if (!m_appliedValid || !m_appliedTextureValid || (m_applied.textureSampler != m_pending.textureSampler))
        m_context->PSSetSamplers(0, 1, &m_pending.textureSampler);

    if (!m_appliedValid || (m_applied.rasterizerState != m_pending.rasterizerState))
        m_context->RSSetState(m_pending.rasterizerState);

    if (!m_appliedValid || (std::memcmp(&m_applied.viewport, &m_pending.viewport, sizeof(m_pending.viewport)) != 0))
        m_context->RSSetViewports(1, &m_pending.viewport);

    if (!m_appliedValid ||
        (std::memcmp(&m_applied.scissorRect, &m_pending.scissorRect, sizeof(m_pending.scissorRect)) != 0))
        m_context->RSSetScissorRects(1, &m_pending.scissorRect);

    if (m_constantBuffer &&
        (!m_appliedValid ||
         (std::memcmp(m_applied.constants.data(), m_pending.constants.data(), sizeof(m_pending.constants)) != 0)))
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (d3dCheck(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            std::memcpy(mapped.pData, m_pending.constants.data(), sizeof(m_pending.constants));
            m_context->Unmap(m_constantBuffer.Get(), 0);
        }
    }

    m_applied             = m_pending;
    m_appliedValid        = true;
    m_appliedTextureValid = true;
}


////////////////////////////////////////////////////////////
bool D3D11GraphicsDevice::takeUserShaderBindPending()
{
    const ContextLock lock(*this);

    const bool pending      = m_userShaderBindPending;
    m_userShaderBindPending = false;

    return pending;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::invalidatePipeline()
{
    const ContextLock lock(*this);

    m_appliedValid          = false;
    m_appliedTextureValid   = false;
    m_appliedProgramValid   = false;
    m_userShaderBindPending = false;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::invalidateTextureBinding()
{
    const ContextLock lock(*this);

    m_appliedTextureValid = false;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::bindVertexBuffer(ID3D11Buffer* buffer)
{
    const ContextLock lock(*this);

    if (!m_context || (m_currentVertexBuffer == buffer))
        return;

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);

    m_currentVertexBuffer = buffer;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::bindIndexBuffer(ID3D11Buffer* buffer)
{
    const ContextLock lock(*this);

    if (!m_context || (m_currentIndexBuffer == buffer))
        return;

    m_context->IASetIndexBuffer(buffer, DXGI_FORMAT_R32_UINT, 0);

    m_currentIndexBuffer = buffer;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::bindTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
{
    const ContextLock lock(*this);

    if (!m_context || (m_currentTopology == topology))
        return;

    m_context->IASetPrimitiveTopology(topology);

    m_currentTopology = topology;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::invalidateInputBindings()
{
    const ContextLock lock(*this);

    m_currentVertexBuffer = nullptr;
    m_currentIndexBuffer  = nullptr;
    m_currentTopology     = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::appendPendingVertices(const Vertex* vertices, std::size_t vertexCount, D3D11_PRIMITIVE_TOPOLOGY topology)
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
void D3D11GraphicsDevice::flushPendingDraws()
{
    // Draws only become pending on the rendering thread, which always observes its own
    // appends, so flushes that have nothing to do can skip the lock entirely
    if (!m_hasPendingDraws.load(std::memory_order_relaxed))
        return;

    const ContextLock lock(*this);

    if (m_pendingVertices.empty())
        return;

    std::size_t firstVertex = 0;
    if (m_context && uploadVertices(m_pendingVertices.data(), m_pendingVertices.size(), firstVertex))
    {
        applyPendingState();
        bindVertexBuffer(getStreamVertexBuffer());
        bindTopology(m_pendingTopology);
        m_context->Draw(static_cast<UINT>(m_pendingVertices.size()), static_cast<UINT>(firstVertex));
    }

    m_pendingVertices.clear();
    m_hasPendingDraws.store(false, std::memory_order_relaxed);
}


////////////////////////////////////////////////////////////
void D3D11GraphicsDevice::createPipeline()
{
#ifdef SFML_D3D11_PRECOMPILED_SHADERS
    const void*       vertexShaderBytecode = defaultVertexShaderBytecode;
    const std::size_t vertexShaderSize     = sizeof(defaultVertexShaderBytecode);
    const void*       pixelShaderBytecode  = defaultPixelShaderBytecode;
    const std::size_t pixelShaderSize      = sizeof(defaultPixelShaderBytecode);
#else
    // Compile the built-in shaders
    ComPtr<ID3DBlob> vertexShaderBlob;
    ComPtr<ID3DBlob> pixelShaderBlob;
    ComPtr<ID3DBlob> errors;

    if (!d3dCheck(D3DCompile(defaultShaderSource,
                             std::strlen(defaultShaderSource),
                             "sfml-default",
                             nullptr,
                             nullptr,
                             "VSMain",
                             "vs_4_0",
                             D3DCOMPILE_OPTIMIZATION_LEVEL3,
                             0,
                             &vertexShaderBlob,
                             &errors)) ||
        !d3dCheck(D3DCompile(defaultShaderSource,
                             std::strlen(defaultShaderSource),
                             "sfml-default",
                             nullptr,
                             nullptr,
                             "PSMain",
                             "ps_4_0",
                             D3DCOMPILE_OPTIMIZATION_LEVEL3,
                             0,
                             &pixelShaderBlob,
                             &errors)))
    {
        if (errors)
            err() << "Failed to compile the built-in shaders:" << '\n'
                  << static_cast<const char*>(errors->GetBufferPointer()) << std::endl;
        return;
    }

    const void*       vertexShaderBytecode = vertexShaderBlob->GetBufferPointer();
    const std::size_t vertexShaderSize     = vertexShaderBlob->GetBufferSize();
    const void*       pixelShaderBytecode  = pixelShaderBlob->GetBufferPointer();
    const std::size_t pixelShaderSize      = pixelShaderBlob->GetBufferSize();
#endif

    d3dCheck(m_device->CreateVertexShader(vertexShaderBytecode, vertexShaderSize, nullptr, &m_defaultVertexShader));
    d3dCheck(m_device->CreatePixelShader(pixelShaderBytecode, pixelShaderSize, nullptr, &m_defaultPixelShader));

    // Input layout matching sf::Vertex
    constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 3> layout = {
        {{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
         {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
         {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}}};

    d3dCheck(m_device->CreateInputLayout(layout.data(),
                                         static_cast<UINT>(layout.size()),
                                         vertexShaderBytecode,
                                         vertexShaderSize,
                                         &m_inputLayout));

    // Constant buffer for the three matrices
    D3D11_BUFFER_DESC constantBufferDesc{};
    constantBufferDesc.ByteWidth      = sizeof(float) * 16 * 3;
    constantBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
    constantBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    d3dCheck(m_device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer));

    // 1x1 white texture for untextured draws
    constexpr std::uint32_t whitePixel = 0xFFFFFFFF;

    D3D11_TEXTURE2D_DESC whiteDesc{};
    whiteDesc.Width            = 1;
    whiteDesc.Height           = 1;
    whiteDesc.MipLevels        = 1;
    whiteDesc.ArraySize        = 1;
    whiteDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    whiteDesc.SampleDesc.Count = 1;
    whiteDesc.Usage            = D3D11_USAGE_IMMUTABLE;
    whiteDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA whiteData{};
    whiteData.pSysMem     = &whitePixel;
    whiteData.SysMemPitch = sizeof(whitePixel);

    if (d3dCheck(m_device->CreateTexture2D(&whiteDesc, &whiteData, &m_whiteTexture)))
        d3dCheck(m_device->CreateShaderResourceView(m_whiteTexture.Get(), nullptr, &m_whiteTextureView));
}

} // namespace sf::priv
