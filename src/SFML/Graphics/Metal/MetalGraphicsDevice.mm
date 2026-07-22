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
#include <SFML/Graphics/Metal/MetalGraphicsDevice.hpp>
#include <SFML/Graphics/Metal/MetalRenderTargetImpl.hpp>
#include <SFML/Graphics/Metal/MetalRenderTextureImpl.hpp>
#include <SFML/Graphics/Metal/MetalRenderWindowImpl.hpp>
#include <SFML/Graphics/Metal/MetalShaderImpl.hpp>
#include <SFML/Graphics/Metal/MetalTextureImpl.hpp>
#include <SFML/Graphics/Metal/MetalVertexBufferImpl.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <ostream>

#include <cstring>


// The built-in shaders live in DefaultShader.metal, either precompiled to a library
// at build time or embedded as source and compiled when the device is created
#ifdef SFML_METAL_PRECOMPILED_SHADERS
#include <MetalDefaultShaderLibrary.hpp>
#else
#include <MetalDefaultShaderSource.hpp>
#endif


namespace
{
// Convert an sf::BlendMode::Factor to the corresponding blend factor for the color channels
MTLBlendFactor factorToMetal(sf::BlendMode::Factor blendFactor)
{
    // clang-format off
    switch (blendFactor)
    {
        case sf::BlendMode::Factor::Zero:             return MTLBlendFactorZero;
        case sf::BlendMode::Factor::One:              return MTLBlendFactorOne;
        case sf::BlendMode::Factor::SrcColor:         return MTLBlendFactorSourceColor;
        case sf::BlendMode::Factor::OneMinusSrcColor: return MTLBlendFactorOneMinusSourceColor;
        case sf::BlendMode::Factor::DstColor:         return MTLBlendFactorDestinationColor;
        case sf::BlendMode::Factor::OneMinusDstColor: return MTLBlendFactorOneMinusDestinationColor;
        case sf::BlendMode::Factor::SrcAlpha:         return MTLBlendFactorSourceAlpha;
        case sf::BlendMode::Factor::OneMinusSrcAlpha: return MTLBlendFactorOneMinusSourceAlpha;
        case sf::BlendMode::Factor::DstAlpha:         return MTLBlendFactorDestinationAlpha;
        case sf::BlendMode::Factor::OneMinusDstAlpha: return MTLBlendFactorOneMinusDestinationAlpha;
    }
    // clang-format on

    return MTLBlendFactorZero;
}

// Convert an sf::BlendMode::Factor to the corresponding blend factor for the alpha channel.
// The alpha variants match what the color factors resolve to in the alpha slot of OpenGL.
MTLBlendFactor factorToMetalAlpha(sf::BlendMode::Factor blendFactor)
{
    // clang-format off
    switch (blendFactor)
    {
        case sf::BlendMode::Factor::SrcColor:         return MTLBlendFactorSourceAlpha;
        case sf::BlendMode::Factor::OneMinusSrcColor: return MTLBlendFactorOneMinusSourceAlpha;
        case sf::BlendMode::Factor::DstColor:         return MTLBlendFactorDestinationAlpha;
        case sf::BlendMode::Factor::OneMinusDstColor: return MTLBlendFactorOneMinusDestinationAlpha;
        default:                                      return factorToMetal(blendFactor);
    }
    // clang-format on
}

// Convert an sf::BlendMode::Equation to the corresponding blend operation
MTLBlendOperation equationToMetal(sf::BlendMode::Equation blendEquation)
{
    // clang-format off
    switch (blendEquation)
    {
        case sf::BlendMode::Equation::Add:             return MTLBlendOperationAdd;
        case sf::BlendMode::Equation::Subtract:        return MTLBlendOperationSubtract;
        case sf::BlendMode::Equation::ReverseSubtract: return MTLBlendOperationReverseSubtract;
        case sf::BlendMode::Equation::Min:             return MTLBlendOperationMin;
        case sf::BlendMode::Equation::Max:             return MTLBlendOperationMax;
    }
    // clang-format on

    return MTLBlendOperationAdd;
}

// Convert an sf::StencilComparison to the corresponding comparison function
MTLCompareFunction stencilFunctionToMetal(sf::StencilComparison comparison)
{
    // clang-format off
    switch (comparison)
    {
        case sf::StencilComparison::Never:        return MTLCompareFunctionNever;
        case sf::StencilComparison::Less:         return MTLCompareFunctionLess;
        case sf::StencilComparison::LessEqual:    return MTLCompareFunctionLessEqual;
        case sf::StencilComparison::Greater:      return MTLCompareFunctionGreater;
        case sf::StencilComparison::GreaterEqual: return MTLCompareFunctionGreaterEqual;
        case sf::StencilComparison::Equal:        return MTLCompareFunctionEqual;
        case sf::StencilComparison::NotEqual:     return MTLCompareFunctionNotEqual;
        case sf::StencilComparison::Always:       return MTLCompareFunctionAlways;
    }
    // clang-format on

    return MTLCompareFunctionAlways;
}

// Convert an sf::StencilUpdateOperation to the corresponding stencil operation.
// The OpenGL backend uses the clamping GL_INCR/GL_DECR, so the clamping variants match.
MTLStencilOperation stencilOperationToMetal(sf::StencilUpdateOperation operation)
{
    // clang-format off
    switch (operation)
    {
        case sf::StencilUpdateOperation::Keep:      return MTLStencilOperationKeep;
        case sf::StencilUpdateOperation::Zero:      return MTLStencilOperationZero;
        case sf::StencilUpdateOperation::Replace:   return MTLStencilOperationReplace;
        case sf::StencilUpdateOperation::Increment: return MTLStencilOperationIncrementClamp;
        case sf::StencilUpdateOperation::Decrement: return MTLStencilOperationDecrementClamp;
        case sf::StencilUpdateOperation::Invert:    return MTLStencilOperationInvert;
    }
    // clang-format on

    return MTLStencilOperationKeep;
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

// Compress the pixel formats a pipeline can encounter into a compact index
std::uint64_t packPixelFormat(std::uint32_t format)
{
    // clang-format off
    switch (static_cast<MTLPixelFormat>(format))
    {
        case MTLPixelFormatInvalid:                return 0;
        case MTLPixelFormatRGBA8Unorm:             return 1;
        case MTLPixelFormatRGBA8Unorm_sRGB:        return 2;
        case MTLPixelFormatBGRA8Unorm:             return 3;
        case MTLPixelFormatBGRA8Unorm_sRGB:        return 4;
        case MTLPixelFormatStencil8:               return 5;
        case MTLPixelFormatDepth32Float_Stencil8:  return 6;
        default:                                   return 7;
    }
    // clang-format on
}

// Pack everything baked into a pipeline state object into a cache key
std::uint64_t makePipelineKey(std::uint32_t blendKey,
                              std::uint32_t colorFormat,
                              unsigned int  sampleCount,
                              std::uint32_t depthStencilFormat,
                              std::uint32_t shaderId)
{
    return static_cast<std::uint64_t>(blendKey) | (packPixelFormat(colorFormat) << 25) |
           (static_cast<std::uint64_t>(sampleCount) << 28) | (packPixelFormat(depthStencilFormat) << 32) |
           (static_cast<std::uint64_t>(shaderId) << 35);
}

// Transient chunks are large enough for many merged draws
constexpr std::size_t transientChunkSize = 1024 * 1024;

// The vertex layout matching sf::Vertex
MTLVertexDescriptor* makeVertexDescriptor()
{
    MTLVertexDescriptor* descriptor = [MTLVertexDescriptor vertexDescriptor];

    descriptor.attributes[0].format      = MTLVertexFormatFloat2;
    descriptor.attributes[0].offset      = 0;
    descriptor.attributes[0].bufferIndex = 0;

    descriptor.attributes[1].format      = MTLVertexFormatUChar4Normalized;
    descriptor.attributes[1].offset      = 8;
    descriptor.attributes[1].bufferIndex = 0;

    descriptor.attributes[2].format      = MTLVertexFormatFloat2;
    descriptor.attributes[2].offset      = 12;
    descriptor.attributes[2].bufferIndex = 0;

    descriptor.layouts[0].stride       = sizeof(sf::Vertex);
    descriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

    return descriptor;
}
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
MetalGraphicsDevice::MetalGraphicsDevice() :
    m_inFlight(std::make_shared<std::atomic<unsigned int>>(0)),
    m_transientFreeList(std::make_shared<TransientFreeList>())
{
    m_device.reset(MTLCreateSystemDefaultDevice());
    if (!m_device)
    {
        err() << "Failed to create Metal device" << std::endl;
        return;
    }

    m_commandQueue.reset([m_device.get() newCommandQueue]);
    if (!m_commandQueue)
    {
        err() << "Failed to create Metal command queue" << std::endl;
        return;
    }

    createPipeline();
}


////////////////////////////////////////////////////////////
MetalGraphicsDevice::~MetalGraphicsDevice()
{
    endEncoding(false);
    commitCommandBuffer(false);
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderTargetImpl> MetalGraphicsDevice::createRenderTargetImpl()
{
    return std::make_unique<MetalRenderTargetImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderTextureImpl> MetalGraphicsDevice::createRenderTextureImpl()
{
    return std::make_unique<MetalRenderTextureImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderWindowImpl> MetalGraphicsDevice::createRenderWindowImpl(WindowHandle           handle,
                                                                              const ContextSettings& settings,
                                                                              unsigned int           bitsPerPixel)
{
    return std::make_unique<MetalRenderWindowImpl>(*this, handle, settings, bitsPerPixel);
}


////////////////////////////////////////////////////////////
std::unique_ptr<ShaderImpl> MetalGraphicsDevice::createShaderImpl()
{
    return std::make_unique<MetalShaderImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<TextureImpl> MetalGraphicsDevice::createTextureImpl()
{
    return std::make_unique<MetalTextureImpl>(*this);
}


////////////////////////////////////////////////////////////
std::unique_ptr<VertexBufferImpl> MetalGraphicsDevice::createVertexBufferImpl()
{
    return std::make_unique<MetalVertexBufferImpl>(*this);
}


////////////////////////////////////////////////////////////
unsigned int MetalGraphicsDevice::getMaximumAntiAliasingLevel()
{
    return clampAntiAliasingLevel(8);
}


////////////////////////////////////////////////////////////
unsigned int MetalGraphicsDevice::getMaximumTextureSize()
{
    if (!m_device)
        return 0;

    // Every Mac2 or Apple3 family device supports 16384, older Apple families are iOS-only
    if ([m_device.get() supportsFamily:MTLGPUFamilyMac2] || [m_device.get() supportsFamily:MTLGPUFamilyApple3])
        return 16384;

    return 8192;
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::isShaderAvailable()
{
    return true;
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::isGeometryShaderAvailable()
{
    // Metal has no geometry shader stage
    return false;
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::isVertexBufferAvailable()
{
    return true;
}


////////////////////////////////////////////////////////////
Renderer MetalGraphicsDevice::getRenderer() const
{
    return Renderer::Metal;
}


////////////////////////////////////////////////////////////
ShadingLanguage MetalGraphicsDevice::getShadingLanguage() const
{
    return ShadingLanguage::Msl;
}


////////////////////////////////////////////////////////////
MetalDevicePtr MetalGraphicsDevice::getDevice() const
{
    return m_device.get();
}


////////////////////////////////////////////////////////////
MetalCommandQueuePtr MetalGraphicsDevice::getCommandQueue() const
{
    return m_commandQueue.get();
}


////////////////////////////////////////////////////////////
std::uint32_t MetalGraphicsDevice::getTextureStorageMode() const
{
    return static_cast<std::uint32_t>(
        (m_device && [m_device.get() hasUnifiedMemory]) ? MTLStorageModeShared : MTLStorageModeManaged);
}


////////////////////////////////////////////////////////////
unsigned int MetalGraphicsDevice::clampAntiAliasingLevel(unsigned int level) const
{
    if (!m_device)
        return 1;

    for (unsigned int samples = std::min(level, 8u); samples > 1; --samples)
        if ([m_device.get() supportsTextureSampleCount:samples])
            return samples;

    return 1;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::bindSurface(MetalRenderSurface* surface)
{
    const ContextLock lock(*this);

    if (m_currentSurface == surface)
        return;

    flushPendingDraws();

    // Clears recorded for the previous surface must land on it before switching away
    if (m_pendingColorClear || m_pendingStencilClear)
        (void)ensureRenderPass();

    endEncoding(false);

    m_currentSurface = surface;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::unbindSurface(MetalRenderSurface* surface)
{
    const ContextLock lock(*this);

    if (m_currentSurface != surface)
        return;

    flushPendingDraws();
    endEncoding(false);

    m_currentSurface        = nullptr;
    m_currentRenderTargetId = 0;
    m_pendingColorClear     = false;
    m_pendingStencilClear   = false;
}


////////////////////////////////////////////////////////////
MetalRenderSurface* MetalGraphicsDevice::getCurrentSurface() const
{
    const ContextLock lock(*this);

    return m_currentSurface;
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::getCurrentAttachments(MetalSurfaceAttachments& attachments)
{
    const ContextLock lock(*this);

    return m_currentSurface && m_currentSurface->prepareAttachments(attachments);
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::setCurrentRenderTargetId(std::uint64_t id)
{
    const ContextLock lock(*this);

    m_currentRenderTargetId = id;
}


////////////////////////////////////////////////////////////
std::uint64_t MetalGraphicsDevice::getCurrentRenderTargetId() const
{
    const ContextLock lock(*this);

    return m_currentRenderTargetId;
}


////////////////////////////////////////////////////////////
MetalTexturePtr MetalGraphicsDevice::getCurrentTextureView() const
{
    const ContextLock lock(*this);

    return m_pending.texture;
}


////////////////////////////////////////////////////////////
MetalSamplerStatePtr MetalGraphicsDevice::getCurrentTextureSampler() const
{
    const ContextLock lock(*this);

    return m_pending.textureSampler;
}


////////////////////////////////////////////////////////////
MetalDepthStencilStatePtr MetalGraphicsDevice::getDepthStencilState(const StencilMode& mode)
{
    const ContextLock lock(*this);

    const std::uint32_t key = packStencilMode(mode);

    const auto it = m_depthStencilStates.find(key);
    if (it != m_depthStencilStates.end())
        return it->second.get();

    NSPtr<MetalDepthStencilStatePtr> state;
    if (m_device)
    {
        MTLDepthStencilDescriptor* descriptor = [[MTLDepthStencilDescriptor alloc] init];
        descriptor.depthCompareFunction       = MTLCompareFunctionAlways;
        descriptor.depthWriteEnabled          = NO;

        if (!(mode == StencilMode()))
        {
            MTLStencilDescriptor* stencil    = [[MTLStencilDescriptor alloc] init];
            stencil.stencilCompareFunction   = stencilFunctionToMetal(mode.stencilComparison);
            stencil.stencilFailureOperation  = MTLStencilOperationKeep;
            stencil.depthFailureOperation    = stencilOperationToMetal(mode.stencilUpdateOperation);
            stencil.depthStencilPassOperation = stencilOperationToMetal(mode.stencilUpdateOperation);
            stencil.readMask                 = mode.stencilMask.value;
            stencil.writeMask                = 0xFF;

            descriptor.frontFaceStencil = stencil;
            descriptor.backFaceStencil  = stencil;
            [stencil release];
        }

        state.reset([m_device.get() newDepthStencilStateWithDescriptor:descriptor]);
        [descriptor release];
    }

    return m_depthStencilStates.emplace(key, std::move(state)).first->second.get();
}


////////////////////////////////////////////////////////////
MetalSamplerStatePtr MetalGraphicsDevice::getSamplerState(bool smooth, bool repeated, bool mipmapped)
{
    const ContextLock lock(*this);

    auto& state = m_samplerStates[(smooth ? 1u : 0u) | (repeated ? 2u : 0u) | (mipmapped ? 4u : 0u)];
    if (!state && m_device)
    {
        const MTLSamplerAddressMode addressMode = repeated ? MTLSamplerAddressModeRepeat
                                                           : MTLSamplerAddressModeClampToEdge;
        const MTLSamplerMinMagFilter filter = smooth ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;

        MTLSamplerDescriptor* descriptor = [[MTLSamplerDescriptor alloc] init];
        descriptor.sAddressMode          = addressMode;
        descriptor.tAddressMode          = addressMode;
        descriptor.rAddressMode          = addressMode;
        descriptor.minFilter             = filter;
        descriptor.magFilter             = filter;

        if (mipmapped)
        {
            // Matches the GL_LINEAR_MIPMAP_LINEAR / GL_NEAREST_MIPMAP_LINEAR filters of the OpenGL backend
            descriptor.mipFilter = MTLSamplerMipFilterLinear;
        }
        else
        {
            // Ignoring the mip levels hides the extra levels of textures whose mipmap was invalidated
            descriptor.mipFilter   = MTLSamplerMipFilterNotMipmapped;
            descriptor.lodMaxClamp = 0.f;
        }

        state.reset([m_device.get() newSamplerStateWithDescriptor:descriptor]);
        [descriptor release];
    }

    return state.get();
}


////////////////////////////////////////////////////////////
MetalVertexDescriptorPtr MetalGraphicsDevice::makeVertexDescriptor() const
{
    return ::makeVertexDescriptor();
}


////////////////////////////////////////////////////////////
MetalFunctionPtr MetalGraphicsDevice::getDefaultVertexFunction() const
{
    return m_defaultVertexFunction.get();
}


////////////////////////////////////////////////////////////
MetalFunctionPtr MetalGraphicsDevice::getDefaultFragmentFunction() const
{
    return m_defaultFragmentFunction.get();
}


////////////////////////////////////////////////////////////
MetalTexturePtr MetalGraphicsDevice::getWhiteTexture() const
{
    return m_whiteTexture.get();
}


////////////////////////////////////////////////////////////
MetalBufferPtr MetalGraphicsDevice::getTriangleFanIndexBuffer(std::size_t vertexCount, std::size_t& indexCount)
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

        m_fanIndexBuffer.reset([m_device.get() newBufferWithBytes:indices.data()
                                                           length:sizeof(std::uint32_t) * indices.size()
                                                          options:MTLResourceStorageModeShared]);
        if (!m_fanIndexBuffer)
        {
            m_fanIndexBufferVertices = 0;
            return nullptr;
        }

        m_fanIndexBufferVertices = newVertices;
    }

    return m_fanIndexBuffer.get();
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::setPendingBlendMode(const BlendMode& mode, bool colorWrite)
{
    const ContextLock lock(*this);

    const std::uint32_t key = packBlendMode(mode, colorWrite);
    if (m_pending.blendKey == key)
        return;

    flushPendingDraws();
    m_pending.blendMode  = mode;
    m_pending.colorWrite = colorWrite;
    m_pending.blendKey   = key;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::setPendingDepthStencilState(MetalDepthStencilStatePtr state, std::uint32_t reference)
{
    const ContextLock lock(*this);

    if ((m_pending.depthStencilState == state) && (m_pending.stencilReference == reference))
        return;

    flushPendingDraws();
    m_pending.depthStencilState = state;
    m_pending.stencilReference  = reference;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::setPendingTexture(MetalTexturePtr texture, MetalSamplerStatePtr sampler)
{
    const ContextLock lock(*this);

    if ((m_pending.texture == texture) && (m_pending.textureSampler == sampler))
        return;

    flushPendingDraws();
    m_pending.texture        = texture;
    m_pending.textureSampler = sampler;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::setPendingViewport(const MetalViewport& viewport)
{
    const ContextLock lock(*this);

    if (std::memcmp(&m_pending.viewport, &viewport, sizeof(viewport)) == 0)
        return;

    flushPendingDraws();
    m_pending.viewport = viewport;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::setPendingScissor(bool enabled, const MetalScissor& rect)
{
    const ContextLock lock(*this);

    if ((m_pending.scissorEnabled == enabled) && (std::memcmp(&m_pending.scissorRect, &rect, sizeof(rect)) == 0))
        return;

    flushPendingDraws();
    m_pending.scissorEnabled = enabled;
    m_pending.scissorRect    = rect;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::setPendingConstants(const std::array<float, 48>& constants)
{
    const ContextLock lock(*this);

    if (std::memcmp(m_pending.constants.data(), constants.data(), sizeof(constants)) == 0)
        return;

    flushPendingDraws();
    m_pending.constants = constants;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::setPendingUserShader(const void* shader, std::uint64_t stateId)
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
void MetalGraphicsDevice::setPendingClearColor(Color color)
{
    const ContextLock lock(*this);

    flushPendingDraws();

    // Ending the pass makes the clear cover the content already drawn
    endEncoding(false);

    m_pendingColorClear = true;
    m_pendingClearColor = color;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::setPendingClearStencil(std::uint8_t value)
{
    const ContextLock lock(*this);

    flushPendingDraws();
    endEncoding(false);

    m_pendingStencilClear      = true;
    m_pendingStencilClearValue = value;
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::applyPendingState()
{
    const ContextLock lock(*this);

    if (!ensureRenderPass())
        return false;

    id<MTLRenderCommandEncoder> encoder = m_encoder.get();

    // Pick the pipeline state object matching the pending blend mode and current attachments.
    // A pending user shader replaces the built-in pipeline entirely, the draw that set it
    // binds the shader's own pipeline when takeUserShaderBindPending says so.
    if (!m_pending.userShader)
    {
        const std::uint64_t pipelineKey = makePipelineKey(m_pending.blendKey,
                                                          m_attachments.colorFormat,
                                                          m_attachments.sampleCount,
                                                          m_attachments.depthStencilFormat,
                                                          0);

        if (m_appliedPipelineKey != pipelineKey)
        {
            MetalRenderPipelinePtr pipeline = getPipelineState(m_pending.blendMode,
                                                               m_pending.colorWrite,
                                                               m_pending.blendKey,
                                                               m_attachments.colorFormat,
                                                               m_attachments.sampleCount,
                                                               m_attachments.depthStencilFormat);
            if (!pipeline)
                return false;

            [encoder setRenderPipelineState:pipeline];
            m_appliedPipelineKey = pipelineKey;
        }
    }
    else
    {
        // The shader binds its own pipeline, whatever key the encoder holds is stale
        m_appliedPipelineKey = 0;
    }

    // The blend mode and color mask are baked into the pipeline object, a change
    // requires the user shader to bind a matching pipeline again
    if (!m_appliedValid || (m_applied.userShader != m_pending.userShader) ||
        (m_applied.userShaderState != m_pending.userShaderState) || (m_applied.blendKey != m_pending.blendKey))
        m_userShaderBindPending = (m_pending.userShader != nullptr);

    if (m_attachments.depthStencilFormat != 0)
    {
        if (!m_appliedValid || (m_applied.depthStencilState != m_pending.depthStencilState))
            [encoder setDepthStencilState:m_pending.depthStencilState];

        if (!m_appliedValid || (m_applied.stencilReference != m_pending.stencilReference))
            [encoder setStencilReferenceValue:m_pending.stencilReference];
    }

    if (!m_appliedValid || !m_appliedTextureValid || (m_applied.texture != m_pending.texture))
        [encoder setFragmentTexture:m_pending.texture atIndex:0];

    if (!m_appliedValid || !m_appliedTextureValid || (m_applied.textureSampler != m_pending.textureSampler))
        [encoder setFragmentSamplerState:m_pending.textureSampler atIndex:0];

    if (!m_appliedValid || (std::memcmp(&m_applied.viewport, &m_pending.viewport, sizeof(m_pending.viewport)) != 0))
    {
        const MTLViewport viewport = {m_pending.viewport.x, m_pending.viewport.y, m_pending.viewport.width, m_pending.viewport.height, 0.0, 1.0};
        [encoder setViewport:viewport];
    }

    if (!m_appliedValid || (m_applied.scissorEnabled != m_pending.scissorEnabled) ||
        (std::memcmp(&m_applied.scissorRect, &m_pending.scissorRect, sizeof(m_pending.scissorRect)) != 0))
    {
        // Metal has no scissor enable, a disabled scissor covers the whole attachment.
        // The rectangle must not reach outside the attachment, unlike other APIs
        // Metal treats an out-of-bounds scissor as an error instead of clamping.
        MTLScissorRect rect = {0, 0, m_attachments.size.x, m_attachments.size.y};
        if (m_pending.scissorEnabled)
        {
            rect.x      = std::min<NSUInteger>(m_pending.scissorRect.x, m_attachments.size.x);
            rect.y      = std::min<NSUInteger>(m_pending.scissorRect.y, m_attachments.size.y);
            rect.width  = std::min<NSUInteger>(m_pending.scissorRect.width, m_attachments.size.x - rect.x);
            rect.height = std::min<NSUInteger>(m_pending.scissorRect.height, m_attachments.size.y - rect.y);
        }
        [encoder setScissorRect:rect];
    }

    if (!m_appliedValid || (std::memcmp(m_applied.constants.data(), m_pending.constants.data(), sizeof(m_pending.constants)) != 0))
        [encoder setVertexBytes:m_pending.constants.data() length:sizeof(m_pending.constants) atIndex:1];

    m_applied             = m_pending;
    m_appliedValid        = true;
    m_appliedTextureValid = true;

    return true;
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::takeUserShaderBindPending()
{
    const ContextLock lock(*this);

    const bool pending      = m_userShaderBindPending;
    m_userShaderBindPending = false;

    return pending;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::invalidatePipeline()
{
    const ContextLock lock(*this);

    m_appliedValid          = false;
    m_appliedTextureValid   = false;
    m_appliedPipelineKey    = 0;
    m_userShaderBindPending = false;
    m_currentVertexBuffer   = nullptr;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::invalidateTextureBinding()
{
    const ContextLock lock(*this);

    m_appliedTextureValid = false;
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::applyUserShaderPipeline(MetalFunctionPtr vertexFunction,
                                                  MetalFunctionPtr fragmentFunction,
                                                  std::uint32_t    shaderId)
{
    const ContextLock lock(*this);

    if (!m_encoder)
        return false;

    const std::uint64_t pipelineKey = makePipelineKey(m_pending.blendKey,
                                                      m_attachments.colorFormat,
                                                      m_attachments.sampleCount,
                                                      m_attachments.depthStencilFormat,
                                                      shaderId);

    if (m_appliedPipelineKey == pipelineKey)
        return true;

    MetalRenderPipelinePtr pipeline = getPipelineState(m_pending.blendMode,
                                                       m_pending.colorWrite,
                                                       m_pending.blendKey,
                                                       m_attachments.colorFormat,
                                                       m_attachments.sampleCount,
                                                       m_attachments.depthStencilFormat,
                                                       vertexFunction,
                                                       fragmentFunction,
                                                       shaderId);

    // Fall back to the built-in pipeline so the draw never runs without one bound
    if (!pipeline)
    {
        pipeline = getPipelineState(m_pending.blendMode,
                                    m_pending.colorWrite,
                                    m_pending.blendKey,
                                    m_attachments.colorFormat,
                                    m_attachments.sampleCount,
                                    m_attachments.depthStencilFormat);
        if (pipeline)
        {
            [m_encoder.get() setRenderPipelineState:pipeline];
            m_appliedPipelineKey = makePipelineKey(m_pending.blendKey,
                                                   m_attachments.colorFormat,
                                                   m_attachments.sampleCount,
                                                   m_attachments.depthStencilFormat,
                                                   0);
        }
        return false;
    }

    [m_encoder.get() setRenderPipelineState:pipeline];
    m_appliedPipelineKey = pipelineKey;

    return true;
}


////////////////////////////////////////////////////////////
MetalRenderEncoderPtr MetalGraphicsDevice::getRenderEncoder() const
{
    return m_encoder.get();
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::uploadVertices(const void* vertices, std::size_t vertexCount, std::size_t& firstVertex)
{
    const ContextLock lock(*this);

    if (!m_encoder)
        return false;

    MetalBufferPtr buffer = nullptr;
    std::size_t    offset = 0;
    if (!allocateTransient(vertices, sizeof(Vertex) * vertexCount, buffer, offset))
        return false;

    if (m_currentVertexBuffer == buffer)
        [m_encoder.get() setVertexBufferOffset:offset atIndex:0];
    else
        [m_encoder.get() setVertexBuffer:buffer offset:offset atIndex:0];
    m_currentVertexBuffer = buffer;

    firstVertex = 0;

    return true;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::bindVertexBuffer(MetalBufferPtr buffer)
{
    const ContextLock lock(*this);

    if (!m_encoder || (m_currentVertexBuffer == buffer))
        return;

    [m_encoder.get() setVertexBuffer:buffer offset:0 atIndex:0];

    m_currentVertexBuffer = buffer;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::appendPendingVertices(const Vertex* vertices, std::size_t vertexCount, std::uint32_t primitiveType)
{
    const ContextLock lock(*this);

    // Bound the staging memory and the latency of a single merged draw
    constexpr std::size_t maxPendingVertices = 16384;

    if (!m_pendingVertices.empty() &&
        ((primitiveType != m_pendingTopology) || (m_pendingVertices.size() + vertexCount > maxPendingVertices)))
        flushPendingDraws();

    m_pendingTopology = primitiveType;
    m_pendingVertices.insert(m_pendingVertices.end(), vertices, vertices + vertexCount);
    m_hasPendingDraws.store(true, std::memory_order_relaxed);
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::flushPendingDraws()
{
    // Draws only become pending on the rendering thread, which always observes its own
    // appends, so flushes that have nothing to do can skip the lock entirely
    if (!m_hasPendingDraws.load(std::memory_order_relaxed))
        return;

    const ContextLock lock(*this);

    if (m_pendingVertices.empty())
        return;

    // Clear the hint first, applyPendingState flushes on state changes and would recurse
    m_hasPendingDraws.store(false, std::memory_order_relaxed);

    std::size_t firstVertex = 0;
    if (applyPendingState() && uploadVertices(m_pendingVertices.data(), m_pendingVertices.size(), firstVertex))
    {
        [m_encoder.get() drawPrimitives:static_cast<MTLPrimitiveType>(m_pendingTopology)
                            vertexStart:firstVertex
                            vertexCount:m_pendingVertices.size()];
    }

    m_pendingVertices.clear();
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::endEncoding(bool resolve)
{
    const ContextLock lock(*this);

    if (!m_encoder)
        return;

    // The multisampled content only reaches its resolve target when a pass end
    // resolves; the multisampled texture always keeps its contents so rendering
    // and readbacks can continue within the frame
    const bool doResolve = resolve && m_attachments.resolve;
    [m_encoder.get() setColorStoreAction:(doResolve ? MTLStoreActionStoreAndMultisampleResolve : MTLStoreActionStore)
                                 atIndex:0];

    [m_encoder.get() endEncoding];
    m_encoder.reset();

    m_currentVertexBuffer = nullptr;
    m_appliedValid        = false;
    m_appliedPipelineKey  = 0;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::prepareForPresent(bool forceResolvePass)
{
    const ContextLock lock(*this);

    flushPendingDraws();

    // Materialize clears that never got a pass, and give multisampled frames
    // whose last pass already ended a pass whose end can resolve
    if (!m_encoder && (m_pendingColorClear || m_pendingStencilClear || forceResolvePass))
        (void)ensureRenderPass();

    endEncoding(true);
}


////////////////////////////////////////////////////////////
MetalCommandBufferPtr MetalGraphicsDevice::currentCommandBuffer()
{
    const ContextLock lock(*this);

    if (!m_commandBuffer && m_commandQueue)
    {
        @autoreleasepool
        {
            m_commandBuffer.reset([[m_commandQueue.get() commandBuffer] retain]);
        }
    }

    return m_commandBuffer.get();
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::commitCommandBuffer(bool waitUntilCompleted)
{
    const ContextLock lock(*this);

    endEncoding(false);

    if (!m_commandBuffer)
        return;

    m_inFlight->fetch_add(1, std::memory_order_relaxed);

    // The transient chunks of this command buffer return to the free list once the GPU is done
    const auto usedChunks = std::make_shared<std::vector<NSPtr<MetalBufferPtr>>>(std::move(m_transientBuffers));
    m_transientBuffers.clear();
    m_transientCursor = 0;

    const std::shared_ptr<std::atomic<unsigned int>> inFlight = m_inFlight;
    const std::shared_ptr<TransientFreeList>         freeList = m_transientFreeList;
    [m_commandBuffer.get() addCompletedHandler:^(id<MTLCommandBuffer>) {
        {
            // Keep a few standard chunks around, oversized or excess ones are freed
            const std::lock_guard freeLock(freeList->mutex);
            for (auto& chunk : *usedChunks)
                if ((freeList->buffers.size() < 4) && ([chunk.get() length] == transientChunkSize))
                    freeList->buffers.push_back(std::move(chunk));
        }
        usedChunks->clear();
        inFlight->fetch_sub(1, std::memory_order_relaxed);
    }];

    [m_commandBuffer.get() commit];

    if (waitUntilCompleted)
        [m_commandBuffer.get() waitUntilCompleted];

    m_commandBuffer.reset();
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::isGpuIdle() const
{
    const ContextLock lock(*this);

    return !m_commandBuffer && (m_inFlight->load(std::memory_order_relaxed) == 0);
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::createPipeline()
{
    @autoreleasepool
    {
        NSError* error = nil;

#ifdef SFML_METAL_PRECOMPILED_SHADERS
        dispatch_data_t libraryData = dispatch_data_create(defaultShaderLibrary,
                                                           sizeof(defaultShaderLibrary),
                                                           nullptr,
                                                           DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        m_library.reset([m_device.get() newLibraryWithData:libraryData error:&error]);
        dispatch_release(libraryData);

        if (!m_library)
        {
            err() << "Failed to load the built-in shaders: "
                  << (error ? [[error localizedDescription] UTF8String] : "unknown error") << std::endl;
            return;
        }
#else
        m_library.reset([m_device.get() newLibraryWithSource:@(defaultShaderSource) options:nil error:&error]);
        if (!m_library)
        {
            err() << "Failed to compile the built-in shaders: "
                  << (error ? [[error localizedDescription] UTF8String] : "unknown error") << std::endl;
            return;
        }
#endif

        m_defaultVertexFunction.reset([m_library.get() newFunctionWithName:@"sfmlDefaultVS"]);
        m_defaultFragmentFunction.reset([m_library.get() newFunctionWithName:@"sfmlDefaultFS"]);

        // 1x1 white texture for untextured draws
        MTLTextureDescriptor* descriptor =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                               width:1
                                                              height:1
                                                           mipmapped:NO];
        descriptor.storageMode = static_cast<MTLStorageMode>(getTextureStorageMode());
        descriptor.usage       = MTLTextureUsageShaderRead;

        m_whiteTexture.reset([m_device.get() newTextureWithDescriptor:descriptor]);
        if (m_whiteTexture)
        {
            constexpr std::uint32_t whitePixel = 0xFFFFFFFF;
            [m_whiteTexture.get() replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                                    mipmapLevel:0
                                      withBytes:&whitePixel
                                    bytesPerRow:sizeof(whitePixel)];
        }
    }
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::allocateTransient(const void* data, std::size_t size, MetalBufferPtr& buffer, std::size_t& offset)
{
    if (!m_device)
        return false;

    // Offsets stay aligned for any buffer use
    constexpr std::size_t alignment = 256;

    id<MTLBuffer> chunk = m_transientBuffers.empty() ? nil : m_transientBuffers.back().get();

    if (!chunk || (m_transientCursor + size > [chunk length]))
    {
        NSPtr<MetalBufferPtr> fresh;

        // Recycle a completed chunk, unless the allocation needs a dedicated larger one
        if (size <= transientChunkSize)
        {
            const std::lock_guard freeLock(m_transientFreeList->mutex);
            if (!m_transientFreeList->buffers.empty())
            {
                fresh = std::move(m_transientFreeList->buffers.back());
                m_transientFreeList->buffers.pop_back();
            }
        }

        if (!fresh)
            fresh.reset([m_device.get() newBufferWithLength:std::max(size, transientChunkSize)
                                                    options:MTLResourceStorageModeShared |
                                                            MTLResourceCPUCacheModeWriteCombined]);
        if (!fresh)
            return false;

        m_transientBuffers.push_back(std::move(fresh));
        m_transientCursor = 0;
        chunk             = m_transientBuffers.back().get();
    }

    std::memcpy(static_cast<std::byte*>([chunk contents]) + m_transientCursor, data, size);

    buffer = chunk;
    offset = m_transientCursor;

    m_transientCursor += ((size + alignment - 1) / alignment) * alignment;

    return true;
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::ensureRenderPass()
{
    if (m_encoder)
        return true;

    if (!m_currentSurface || !m_commandQueue)
        return false;

    MetalSurfaceAttachments attachments;
    if (!m_currentSurface->prepareAttachments(attachments) || !attachments.color)
        return false;

    if (!currentCommandBuffer())
        return false;

    @autoreleasepool
    {
        MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];

        MTLRenderPassColorAttachmentDescriptor* color = descriptor.colorAttachments[0];
        color.texture                                 = attachments.color;
        color.resolveTexture                          = attachments.resolve;
        color.loadAction  = m_pendingColorClear ? MTLLoadActionClear : MTLLoadActionLoad;
        color.clearColor  = MTLClearColorMake(m_pendingClearColor.r / 255.0,
                                             m_pendingClearColor.g / 255.0,
                                             m_pendingClearColor.b / 255.0,
                                             m_pendingClearColor.a / 255.0);
        color.storeAction = MTLStoreActionUnknown; // decided when the pass ends

        if (attachments.depthStencil)
        {
            // The unused depth plane of a combined format still has to be attached
            if (static_cast<MTLPixelFormat>(attachments.depthStencilFormat) == MTLPixelFormatDepth32Float_Stencil8)
            {
                descriptor.depthAttachment.texture     = attachments.depthStencil;
                descriptor.depthAttachment.loadAction  = MTLLoadActionDontCare;
                descriptor.depthAttachment.storeAction = MTLStoreActionDontCare;
            }

            descriptor.stencilAttachment.texture      = attachments.depthStencil;
            descriptor.stencilAttachment.loadAction   = m_pendingStencilClear ? MTLLoadActionClear : MTLLoadActionLoad;
            descriptor.stencilAttachment.clearStencil = m_pendingStencilClearValue;
            // The stencil contents survive pass breaks within a frame
            descriptor.stencilAttachment.storeAction  = MTLStoreActionStore;
        }

        m_encoder.reset([[m_commandBuffer.get() renderCommandEncoderWithDescriptor:descriptor] retain]);
    }

    if (!m_encoder)
        return false;

    m_attachments         = attachments;
    m_pendingColorClear   = false;
    m_pendingStencilClear = false;

    // A new encoder starts with clean state, everything has to be re-applied
    m_appliedValid          = false;
    m_appliedTextureValid   = false;
    m_appliedPipelineKey    = 0;
    m_userShaderBindPending = false;
    m_currentVertexBuffer   = nullptr;

    return true;
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::materializePendingClears()
{
    const ContextLock lock(*this);

    if (!m_encoder && (m_pendingColorClear || m_pendingStencilClear))
        (void)ensureRenderPass();
}


////////////////////////////////////////////////////////////
void MetalGraphicsDevice::resolvePass(MetalTexturePtr multisampleTexture, MetalTexturePtr resolveTarget)
{
    const ContextLock lock(*this);

    flushPendingDraws();
    materializePendingClears();
    endEncoding(false);

    if (!currentCommandBuffer())
        return;

    @autoreleasepool
    {
        MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];

        MTLRenderPassColorAttachmentDescriptor* color = descriptor.colorAttachments[0];
        color.texture                                 = multisampleTexture;
        color.resolveTexture                          = resolveTarget;
        color.loadAction                              = MTLLoadActionLoad;
        color.storeAction                             = MTLStoreActionStoreAndMultisampleResolve;

        id<MTLRenderCommandEncoder> encoder = [m_commandBuffer.get() renderCommandEncoderWithDescriptor:descriptor];
        [encoder endEncoding];
    }
}


////////////////////////////////////////////////////////////
bool MetalGraphicsDevice::copyTextureThroughDraw(MetalTexturePtr source, MetalTexturePtr dest, Vector2u sourceSize, Vector2u destPos)
{
    const ContextLock lock(*this);

    flushPendingDraws();
    endEncoding(false);

    MetalRenderPipelinePtr pipeline = getPipelineState(BlendNone,
                                                       true,
                                                       packBlendMode(BlendNone, true),
                                                       static_cast<std::uint32_t>([dest pixelFormat]),
                                                       1,
                                                       0);
    if (!pipeline || !currentCommandBuffer())
        return false;

    @autoreleasepool
    {
        MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];

        MTLRenderPassColorAttachmentDescriptor* color = descriptor.colorAttachments[0];
        color.texture                                 = dest;
        color.loadAction                              = MTLLoadActionLoad;
        color.storeAction                             = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> encoder = [m_commandBuffer.get() renderCommandEncoderWithDescriptor:descriptor];

        // The viewport frames the destination region, a full-viewport quad samples the source
        const MTLViewport viewport =
            {static_cast<double>(destPos.x), static_cast<double>(destPos.y), static_cast<double>(sourceSize.x), static_cast<double>(sourceSize.y), 0.0, 1.0};
        [encoder setRenderPipelineState:pipeline];
        [encoder setViewport:viewport];
        [encoder setFragmentTexture:source atIndex:0];
        [encoder setFragmentSamplerState:getSamplerState(false, false) atIndex:0];

        // Identity model-view, projection and texture matrices
        std::array<float, 48> constants{};
        constants[0] = constants[5] = constants[10] = constants[15] = 1.f;
        constants[16] = constants[21] = constants[26] = constants[31] = 1.f;
        constants[32] = constants[37] = constants[42] = constants[47] = 1.f;
        [encoder setVertexBytes:constants.data() length:sizeof(constants) atIndex:1];

        const float u = static_cast<float>(sourceSize.x) / static_cast<float>([source width]);
        const float v = static_cast<float>(sourceSize.y) / static_cast<float>([source height]);

        // Positions are in clip space, the texture origin sits at the top-left like the viewport's
        const std::array<Vertex, 6> quad = {{{{-1.f, -1.f}, Color::White, {0.f, v}},
                                             {{1.f, -1.f}, Color::White, {u, v}},
                                             {{-1.f, 1.f}, Color::White, {0.f, 0.f}},
                                             {{-1.f, 1.f}, Color::White, {0.f, 0.f}},
                                             {{1.f, -1.f}, Color::White, {u, v}},
                                             {{1.f, 1.f}, Color::White, {u, 0.f}}}};

        id<MTLBuffer> buffer = [m_device.get() newBufferWithBytes:quad.data()
                                                           length:sizeof(quad)
                                                          options:MTLResourceStorageModeShared];
        [encoder setVertexBuffer:buffer offset:0 atIndex:0];
        [buffer release];

        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:quad.size()];
        [encoder endEncoding];
    }

    return true;
}


////////////////////////////////////////////////////////////
MetalRenderPipelinePtr MetalGraphicsDevice::getPipelineState(
    const BlendMode& mode,
    bool             colorWrite,
    std::uint32_t    blendKey,
    std::uint32_t    colorFormat,
    unsigned int     sampleCount,
    std::uint32_t    depthStencilFormat,
    MetalFunctionPtr vertexFunction,
    MetalFunctionPtr fragmentFunction,
    std::uint32_t    shaderId)
{
    const std::uint64_t key = makePipelineKey(blendKey, colorFormat, sampleCount, depthStencilFormat, shaderId);

    const auto it = m_pipelineStates.find(key);
    if (it != m_pipelineStates.end())
        return it->second.get();

    NSPtr<MetalRenderPipelinePtr> pipeline;
    if (m_device && m_defaultVertexFunction && m_defaultFragmentFunction)
    {
        @autoreleasepool
        {
            MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
            descriptor.vertexFunction   = vertexFunction ? vertexFunction : m_defaultVertexFunction.get();
            descriptor.fragmentFunction = fragmentFunction ? fragmentFunction : m_defaultFragmentFunction.get();
            descriptor.vertexDescriptor = ::makeVertexDescriptor();
            descriptor.rasterSampleCount = sampleCount;

            MTLRenderPipelineColorAttachmentDescriptor* color = descriptor.colorAttachments[0];
            color.pixelFormat                = static_cast<MTLPixelFormat>(colorFormat);
            color.blendingEnabled            = YES;
            color.sourceRGBBlendFactor       = factorToMetal(mode.colorSrcFactor);
            color.destinationRGBBlendFactor  = factorToMetal(mode.colorDstFactor);
            color.rgbBlendOperation          = equationToMetal(mode.colorEquation);
            color.sourceAlphaBlendFactor     = factorToMetalAlpha(mode.alphaSrcFactor);
            color.destinationAlphaBlendFactor = factorToMetalAlpha(mode.alphaDstFactor);
            color.alphaBlendOperation        = equationToMetal(mode.alphaEquation);
            color.writeMask                  = colorWrite ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;

            const auto metalDepthStencilFormat = static_cast<MTLPixelFormat>(depthStencilFormat);
            if (metalDepthStencilFormat == MTLPixelFormatDepth32Float_Stencil8)
                descriptor.depthAttachmentPixelFormat = metalDepthStencilFormat;
            if (metalDepthStencilFormat != MTLPixelFormatInvalid)
                descriptor.stencilAttachmentPixelFormat = metalDepthStencilFormat;

            NSError* error = nil;
            pipeline.reset([m_device.get() newRenderPipelineStateWithDescriptor:descriptor error:&error]);
            [descriptor release];

            if (!pipeline)
                err() << "Failed to create render pipeline state: "
                      << (error ? [[error localizedDescription] UTF8String] : "unknown error") << std::endl;
        }
    }

    return m_pipelineStates.emplace(key, std::move(pipeline)).first->second.get();
}

} // namespace sf::priv
