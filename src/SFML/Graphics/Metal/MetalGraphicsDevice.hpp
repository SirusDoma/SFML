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

#pragma once

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/GraphicsDevice.hpp>
#include <SFML/Graphics/Metal/MetalUtils.hpp>
#include <SFML/Graphics/StencilMode.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <SFML/System/Vector2.hpp>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <cstddef>
#include <cstdint>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Attachments a render pass draws into
///
/// The textures are not owned, the surface that filled in the
/// struct keeps them alive while the pass is encoded.
///
////////////////////////////////////////////////////////////
struct MetalSurfaceAttachments
{
    MetalTexturePtr color{};              //!< Color texture, the multisampled one when multisampling
    MetalTexturePtr resolve{};            //!< Resolve target of the color texture, null when not multisampling
    MetalTexturePtr depthStencil{};       //!< Depth-stencil texture, can be null
    std::uint32_t   colorFormat{};        //!< Raw MTLPixelFormat of the color attachment
    std::uint32_t   depthStencilFormat{}; //!< Raw MTLPixelFormat of the depth-stencil attachment, 0 when none
    unsigned int    sampleCount{1};       //!< Samples per pixel of the color attachment
    Vector2u        size;                 //!< Size of the attachments, in pixels
};

////////////////////////////////////////////////////////////
/// \brief Interface of a surface render passes can target
///
/// Implemented by the presentation surfaces and render textures.
/// Attachments are requested lazily every time a pass opens, a
/// window surface acquires its drawable at that point.
///
////////////////////////////////////////////////////////////
class MetalRenderSurface
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~MetalRenderSurface() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Provide the attachments of the next render pass
    ///
    /// \param attachments Attachments to fill in
    ///
    /// \return `true` if the attachments are ready to be drawn into
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual bool prepareAttachments(MetalSurfaceAttachments& attachments) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Get a texture holding the surface's rendered content
    ///
    /// Pending content of the surface is submitted first, and
    /// multisampled content resolved. The texture stays owned by
    /// the surface and is only valid until its next present.
    ///
    /// \return Texture with the rendered content, null when the surface cannot be read
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual MetalTexturePtr acquireReadableColorTexture() = 0;
};

////////////////////////////////////////////////////////////
/// \brief Viewport of the pending draws
///
////////////////////////////////////////////////////////////
struct MetalViewport
{
    double x{};      //!< Left edge, in pixels
    double y{};      //!< Top edge, in pixels
    double width{};  //!< Width, in pixels
    double height{}; //!< Height, in pixels
};

////////////////////////////////////////////////////////////
/// \brief Scissor rectangle of the pending draws
///
////////////////////////////////////////////////////////////
struct MetalScissor
{
    std::uint32_t x{};      //!< Left edge, in pixels
    std::uint32_t y{};      //!< Top edge, in pixels
    std::uint32_t width{};  //!< Width, in pixels
    std::uint32_t height{}; //!< Height, in pixels
};

////////////////////////////////////////////////////////////
/// \brief Metal implementation of the graphics device
///
/// Owns the device and command queue all resources and render
/// targets share, the built-in pipeline replicating the
/// fixed-function behavior of the OpenGL backend, and the
/// caches for immutable state objects.
///
/// Commands are recorded into one current command buffer, render
/// passes open lazily on the first draw and close on target
/// switches, clears, presents and readbacks. The pipeline state
/// of pending draws is tracked and applied on submission.
///
////////////////////////////////////////////////////////////
class MetalGraphicsDevice : public GraphicsDevice
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor, creates the device and built-in pipeline
    ///
    ////////////////////////////////////////////////////////////
    MetalGraphicsDevice();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~MetalGraphicsDevice() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a render target implementation for this backend
    ///
    /// \return New render target implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<RenderTargetImpl> createRenderTargetImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a render texture implementation for this backend
    ///
    /// \return New render texture implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<RenderTextureImpl> createRenderTextureImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a presentation surface for a render window
    ///
    /// The surface presents to the window through a Core
    /// Animation Metal layer.
    ///
    /// \param handle       Native handle of the window to present to
    /// \param settings     Requested settings for the surface
    /// \param bitsPerPixel Pixel depth of the window, in bits per pixel
    ///
    /// \return New presentation surface
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<RenderWindowImpl> createRenderWindowImpl(WindowHandle           handle,
                                                                           const ContextSettings& settings,
                                                                           unsigned int bitsPerPixel) override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a shader implementation for this backend
    ///
    /// \return New shader implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<ShaderImpl> createShaderImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a texture implementation for this backend
    ///
    /// \return New texture implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<TextureImpl> createTextureImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a vertex buffer implementation for this backend
    ///
    /// \return New vertex buffer implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<VertexBufferImpl> createVertexBufferImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the maximum anti-aliasing level supported for render textures
    ///
    /// \return The maximum anti-aliasing level supported
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] unsigned int getMaximumAntiAliasingLevel() override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the maximum texture size supported by this backend
    ///
    /// \return Maximum size allowed for textures, in pixels
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] unsigned int getMaximumTextureSize() override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether this backend supports shaders
    ///
    /// \return `true` if shaders are supported
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isShaderAvailable() override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether this backend supports geometry shaders
    ///
    /// \return `true` if geometry shaders are supported
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isGeometryShaderAvailable() override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether this backend supports vertex buffers
    ///
    /// \return `true` if vertex buffers are supported
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isVertexBufferAvailable() override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the renderer this device renders through
    ///
    /// \return The renderer
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] Renderer getRenderer() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the shading language this backend consumes
    ///
    /// \return The shading language
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ShadingLanguage getShadingLanguage() const override;

    ////////////////////////////////////////////////////////////
    /// \brief RAII guard serializing access to the encoding state
    ///
    /// The command queue is thread-safe but the encoding state
    /// (current command buffer, encoder and caches) is not, every
    /// backend operation that touches it has to hold one of these.
    ///
    ////////////////////////////////////////////////////////////
    class ContextLock
    {
    public:
        explicit ContextLock(const MetalGraphicsDevice& device) : m_lock(device.m_mutex)
        {
        }

    private:
        std::lock_guard<std::recursive_mutex> m_lock;
    };

    ////////////////////////////////////////////////////////////
    /// \brief Get the Metal device
    ///
    /// \return The device, null if device creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalDevicePtr getDevice() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the command queue all rendering is submitted through
    ///
    /// \return The command queue, null if device creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalCommandQueuePtr getCommandQueue() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the storage mode CPU-accessed textures should use
    ///
    /// \return Raw MTLStorageMode, shared on unified memory, managed otherwise
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::uint32_t getTextureStorageMode() const;

    ////////////////////////////////////////////////////////////
    /// \brief Clamp an anti-aliasing level to what the device supports
    ///
    /// \param level Requested sample count
    ///
    /// \return Highest supported sample count not greater than the request, at least 1
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] unsigned int clampAntiAliasingLevel(unsigned int level) const;

    ////////////////////////////////////////////////////////////
    /// \brief Bind a surface as the current output of render passes
    ///
    /// Pending draws and clears of the previous surface are
    /// submitted first. The surface must stay alive until another
    /// surface is bound or unbindSurface is called.
    ///
    /// \param surface Surface subsequent render passes draw into
    ///
    ////////////////////////////////////////////////////////////
    void bindSurface(MetalRenderSurface* surface);

    ////////////////////////////////////////////////////////////
    /// \brief Unbind a surface if it is the current output
    ///
    /// \param surface Surface to unbind
    ///
    ////////////////////////////////////////////////////////////
    void unbindSurface(MetalRenderSurface* surface);

    ////////////////////////////////////////////////////////////
    /// \brief Get the surface render passes currently draw into
    ///
    /// \return Current surface, null if no surface is bound
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalRenderSurface* getCurrentSurface() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the attachments of the current surface
    ///
    /// A window surface acquires its drawable if it has none yet.
    ///
    /// \param attachments Attachments to fill in
    ///
    /// \return `true` if a surface is bound and its attachments are ready
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool getCurrentAttachments(MetalSurfaceAttachments& attachments);

    ////////////////////////////////////////////////////////////
    /// \brief Track which render target is active on the device
    ///
    /// \param id Unique id of the render target, 0 for none
    ///
    ////////////////////////////////////////////////////////////
    void setCurrentRenderTargetId(std::uint64_t id);

    ////////////////////////////////////////////////////////////
    /// \brief Get the id of the render target active on the device
    ///
    /// \return Unique id of the active render target, 0 for none
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::uint64_t getCurrentRenderTargetId() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the pending texture of the current draw
    ///
    /// Shaders use this to resolve their CurrentTexture uniform
    /// and any texture they declare without assigning one, which
    /// samples the draw's texture like an unset GLSL sampler does.
    ///
    /// \return Texture pending for the built-in texture slot
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalTexturePtr getCurrentTextureView() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the pending sampler of the current draw
    ///
    /// \return Sampler pending alongside the current texture
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalSamplerStatePtr getCurrentTextureSampler() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get or create the depth-stencil state for a stencil mode
    ///
    /// \param mode Stencil mode to translate
    ///
    /// \return Depth-stencil state object, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalDepthStencilStatePtr getDepthStencilState(const StencilMode& mode);

    ////////////////////////////////////////////////////////////
    /// \brief Get the sampler state for texture filter settings
    ///
    /// \param smooth    Whether the smooth filter is enabled
    /// \param repeated  Whether the texture repeats
    /// \param mipmapped Whether the texture has a generated mipmap
    ///
    /// \return Sampler state object, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalSamplerStatePtr getSamplerState(bool smooth, bool repeated, bool mipmapped = false);

    ////////////////////////////////////////////////////////////
    /// \brief Get the vertex layout matching `sf::Vertex`
    ///
    /// \return New autoreleased vertex descriptor
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalVertexDescriptorPtr makeVertexDescriptor() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the built-in vertex function replicating the fixed pipeline
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalFunctionPtr getDefaultVertexFunction() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the built-in fragment function replicating the fixed pipeline
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalFunctionPtr getDefaultFragmentFunction() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the 1x1 white texture
    ///
    /// Bound for draws without texture so a single fragment
    /// function covers both the textured and untextured case.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalTexturePtr getWhiteTexture() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the shared triangle-fan index buffer
    ///
    /// Metal has no triangle-fan topology, fans are drawn as an
    /// indexed triangle list instead. All fans share one index
    /// pattern (0, i + 1, i + 2): smaller fans draw a prefix of it
    /// and the fan's first vertex is applied through the base
    /// vertex of the draw call.
    ///
    /// \param vertexCount Number of vertices in the fan
    /// \param indexCount  Number of indices to draw
    ///
    /// \return The index buffer, or a null pointer on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalBufferPtr getTriangleFanIndexBuffer(std::size_t vertexCount, std::size_t& indexCount);

    ////////////////////////////////////////////////////////////
    /// \brief Set the blend mode the next draws use
    ///
    /// Setting a different mode submits the collected draws,
    /// they belong to the previously pending state.
    ///
    /// \param mode       Blend mode to set
    /// \param colorWrite Whether the color channels are written
    ///
    ////////////////////////////////////////////////////////////
    void setPendingBlendMode(const BlendMode& mode, bool colorWrite);

    ////////////////////////////////////////////////////////////
    /// \brief Set the depth-stencil state the next draws use
    ///
    /// \param state     Depth-stencil state to set
    /// \param reference Stencil reference value to set
    ///
    ////////////////////////////////////////////////////////////
    void setPendingDepthStencilState(MetalDepthStencilStatePtr state, std::uint32_t reference);

    ////////////////////////////////////////////////////////////
    /// \brief Set the texture and sampler the next draws use
    ///
    /// \param texture Texture for the built-in texture slot
    /// \param sampler Sampler used alongside the texture
    ///
    ////////////////////////////////////////////////////////////
    void setPendingTexture(MetalTexturePtr texture, MetalSamplerStatePtr sampler);

    ////////////////////////////////////////////////////////////
    /// \brief Set the viewport the next draws use
    ///
    /// \param viewport Viewport to set
    ///
    ////////////////////////////////////////////////////////////
    void setPendingViewport(const MetalViewport& viewport);

    ////////////////////////////////////////////////////////////
    /// \brief Set the scissor rectangle the next draws use
    ///
    /// The rectangle is ignored while scissor testing is disabled,
    /// draws are then clipped to the whole attachment.
    ///
    /// \param enabled Whether scissor testing is enabled
    /// \param rect    Scissor rectangle to set
    ///
    ////////////////////////////////////////////////////////////
    void setPendingScissor(bool enabled, const MetalScissor& rect);

    ////////////////////////////////////////////////////////////
    /// \brief Set the matrices the next draws use
    ///
    /// \param constants Model-view, projection and texture matrix, 16 floats each
    ///
    ////////////////////////////////////////////////////////////
    void setPendingConstants(const std::array<float, 48>& constants);

    ////////////////////////////////////////////////////////////
    /// \brief Set the user shader the next draws are drawn with
    ///
    /// Pending draws are flushed first when the shader or its
    /// state stamp changes, they belong to the previously pending
    /// state.
    ///
    /// \param shader  Identity of the user shader, a null pointer for the built-in pipeline
    /// \param stateId Stamp of the shader's bindable state
    ///
    ////////////////////////////////////////////////////////////
    void setPendingUserShader(const void* shader, std::uint64_t stateId);

    ////////////////////////////////////////////////////////////
    /// \brief Record a clear of the color attachment
    ///
    /// The clear becomes the load action of the next render pass,
    /// an open pass is ended first so already-drawn content is
    /// cleared as well.
    ///
    /// \param color Fill color
    ///
    ////////////////////////////////////////////////////////////
    void setPendingClearColor(Color color);

    ////////////////////////////////////////////////////////////
    /// \brief Record a clear of the stencil attachment
    ///
    /// \param value Stencil value to clear to
    ///
    ////////////////////////////////////////////////////////////
    void setPendingClearStencil(std::uint8_t value);

    ////////////////////////////////////////////////////////////
    /// \brief Set the encoder to the pending pipeline state
    ///
    /// Opens a render pass on the current surface when none is
    /// open. Only the parts that differ from what the encoder is
    /// known to hold are set. Must be called before encoding a
    /// draw outside of `flushPendingDraws`.
    ///
    /// \return `true` if a render pass is open and the state is applied
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool applyPendingState();

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the pending user shader still has to be bound
    ///
    /// Set by `applyPendingState` when the pending user shader is
    /// not the one the encoder holds, and reset when read. The
    /// draw that set it as pending binds it when this is `true`.
    ///
    /// \return `true` if the caller has to bind the pending user shader
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool takeUserShaderBindPending();

    ////////////////////////////////////////////////////////////
    /// \brief Forget what the encoder is known to hold
    ///
    /// Must be called when the encoder state may have been changed
    /// outside of `applyPendingState`, like by raw Metal user
    /// code. The pending state is fully re-applied on the next
    /// draw.
    ///
    ////////////////////////////////////////////////////////////
    void invalidatePipeline();

    ////////////////////////////////////////////////////////////
    /// \brief Forget which texture the built-in texture slot holds
    ///
    /// Must be called when a bound user shader replaced the
    /// texture at the built-in slot with one of its own, the
    /// pending texture is re-applied on the next draw.
    ///
    ////////////////////////////////////////////////////////////
    void invalidateTextureBinding();

    ////////////////////////////////////////////////////////////
    /// \brief Set the pipeline state of a user shader on the open encoder
    ///
    /// The pipeline is created against the current attachments
    /// and pending blend mode, and cached like the built-in ones.
    /// Only valid after `applyPendingState` returned `true`.
    ///
    /// \param vertexFunction   Vertex function, null to use the built-in one
    /// \param fragmentFunction Fragment function, null to use the built-in one
    /// \param shaderId         Number identifying the function pair, never reused across shaders
    ///
    /// \return `true` if the pipeline state was set
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool applyUserShaderPipeline(MetalFunctionPtr vertexFunction,
                                               MetalFunctionPtr fragmentFunction,
                                               std::uint32_t    shaderId);

    ////////////////////////////////////////////////////////////
    /// \brief Get the encoder of the open render pass
    ///
    /// Only valid after `applyPendingState` returned `true`, and
    /// only while holding a ContextLock.
    ///
    /// \return The render command encoder, null when no pass is open
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalRenderEncoderPtr getRenderEncoder() const;

    ////////////////////////////////////////////////////////////
    /// \brief Upload vertices and bind them as the vertex source of the next draw
    ///
    /// The vertices live in transient memory owned by the current
    /// command buffer. Requires an open render pass.
    ///
    /// \param vertices    Vertices to upload
    /// \param vertexCount Number of vertices to upload
    /// \param firstVertex Location of the first uploaded vertex in the bound buffer
    ///
    /// \return `true` if the upload succeeded
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool uploadVertices(const void* vertices, std::size_t vertexCount, std::size_t& firstVertex);

    ////////////////////////////////////////////////////////////
    /// \brief Bind a vertex buffer as the vertex source unless it is already bound
    ///
    /// Requires an open render pass.
    ///
    /// \param buffer Vertex buffer holding `sf::Vertex` data
    ///
    ////////////////////////////////////////////////////////////
    void bindVertexBuffer(MetalBufferPtr buffer);

    ////////////////////////////////////////////////////////////
    /// \brief Collect vertices of a draw to submit them together with following compatible draws
    ///
    /// Consecutive draws whose pipeline state is identical are
    /// merged into a single draw call. Anything that changes what
    /// the pending vertices would render must call
    /// `flushPendingDraws` first.
    ///
    /// \param vertices      Vertices to collect, in list order
    /// \param vertexCount   Number of vertices to collect
    /// \param primitiveType Raw MTLPrimitiveType the vertices belong to
    ///
    ////////////////////////////////////////////////////////////
    void appendPendingVertices(const Vertex* vertices, std::size_t vertexCount, std::uint32_t primitiveType);

    ////////////////////////////////////////////////////////////
    /// \brief Submit the collected vertices in a single draw call
    ///
    /// Does nothing when no vertices are pending.
    ///
    ////////////////////////////////////////////////////////////
    void flushPendingDraws();

    ////////////////////////////////////////////////////////////
    /// \brief End the open render pass
    ///
    /// \param resolve Resolve the multisampled color attachment into its resolve target
    ///
    ////////////////////////////////////////////////////////////
    void endEncoding(bool resolve);

    ////////////////////////////////////////////////////////////
    /// \brief Finish encoding the frame of the current surface before presenting
    ///
    /// Submits pending draws, materializes pending clears and ends
    /// the pass, resolving into the presentation target.
    ///
    /// \param forceResolvePass Open an empty pass if none is open so a resolve happens
    ///
    ////////////////////////////////////////////////////////////
    void prepareForPresent(bool forceResolvePass);

    ////////////////////////////////////////////////////////////
    /// \brief Get the current command buffer, creating one if none is recording
    ///
    /// \return The command buffer, null if device creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalCommandBufferPtr currentCommandBuffer();

    ////////////////////////////////////////////////////////////
    /// \brief Commit the current command buffer to the queue
    ///
    /// Any open render pass is ended first. Does nothing when no
    /// command buffer is recording.
    ///
    /// \param waitUntilCompleted Block until the GPU finished executing the commands
    ///
    ////////////////////////////////////////////////////////////
    void commitCommandBuffer(bool waitUntilCompleted);

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the GPU holds no recorded or in-flight work
    ///
    /// CPU writes to shared resources are only safe while this is
    /// `true`, otherwise the write has to be encoded as a blit so
    /// it lands after the draws already recorded.
    ///
    /// \return `true` if no command buffer is recording or executing
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isGpuIdle() const;

    ////////////////////////////////////////////////////////////
    /// \brief Give pending clears a render pass that carries them out
    ///
    /// Clears only execute as pass load actions, anything reading
    /// rendered content has to call this first in case the content
    /// so far is only a recorded clear.
    ///
    ////////////////////////////////////////////////////////////
    void materializePendingClears();

    ////////////////////////////////////////////////////////////
    /// \brief Resolve a multisampled texture into its resolve target
    ///
    /// Resolving is a render pass store action on Metal, this
    /// encodes an empty pass whose end performs the resolve. The
    /// multisampled contents are kept.
    ///
    /// \param multisampleTexture Multisampled texture to resolve
    /// \param resolveTarget      Texture receiving the resolved content
    ///
    ////////////////////////////////////////////////////////////
    void resolvePass(MetalTexturePtr multisampleTexture, MetalTexturePtr resolveTarget);

    ////////////////////////////////////////////////////////////
    /// \brief Copy a texture region into another texture through a sampling draw
    ///
    /// Blits cannot convert between pixel formats, copies between
    /// differing formats sample the source in a small render pass
    /// instead. Runs on its own encoder, the pending pipeline
    /// state is not disturbed.
    ///
    /// \param source     Texture to copy from
    /// \param dest       Texture to copy into
    /// \param sourceSize Size of the region to copy, starting at the source origin
    /// \param destPos    Coordinates of the destination position
    ///
    /// \return `true` if the copy was encoded
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool copyTextureThroughDraw(MetalTexturePtr source, MetalTexturePtr dest, Vector2u sourceSize, Vector2u destPos);

private:
    ////////////////////////////////////////////////////////////
    /// \brief Create the built-in pipeline objects
    ///
    ////////////////////////////////////////////////////////////
    void createPipeline();

    ////////////////////////////////////////////////////////////
    /// \brief Open a render pass on the current surface if none is open
    ///
    /// Consumes the pending clears as load actions.
    ///
    /// \return `true` if a render pass is open
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool ensureRenderPass();

    ////////////////////////////////////////////////////////////
    /// \brief Copy data into transient memory of the recording command buffer
    ///
    /// Allocations bump-allocate shared chunks that are recycled
    /// once their command buffer completed, no allocation happens
    /// on the steady-state path.
    ///
    /// \param data   Data to copy
    /// \param size   Byte size of the data
    /// \param buffer Buffer the data was placed in
    /// \param offset Byte offset of the data inside the buffer
    ///
    /// \return `true` if the allocation succeeded
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool allocateTransient(const void* data, std::size_t size, MetalBufferPtr& buffer, std::size_t& offset);

    ////////////////////////////////////////////////////////////
    /// \brief Get or create a pipeline state
    ///
    /// \param mode               Blend mode baked into the pipeline
    /// \param colorWrite         Whether the color channels are written
    /// \param blendKey           Packed blend mode and color write flag
    /// \param colorFormat        Raw MTLPixelFormat of the color attachment
    /// \param sampleCount        Samples per pixel of the attachments
    /// \param depthStencilFormat Raw MTLPixelFormat of the depth-stencil attachment, 0 when none
    /// \param vertexFunction     Vertex function, null to use the built-in one
    /// \param fragmentFunction   Fragment function, null to use the built-in one
    /// \param shaderId           Number identifying the function pair, 0 for the built-in pipeline
    ///
    /// \return Pipeline state object, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalRenderPipelinePtr getPipelineState(const BlendMode& mode,
                                                          bool             colorWrite,
                                                          std::uint32_t    blendKey,
                                                          std::uint32_t    colorFormat,
                                                          unsigned int     sampleCount,
                                                          std::uint32_t    depthStencilFormat,
                                                          MetalFunctionPtr vertexFunction   = nullptr,
                                                          MetalFunctionPtr fragmentFunction = nullptr,
                                                          std::uint32_t    shaderId         = 0);

    ////////////////////////////////////////////////////////////
    /// \brief Pipeline state the pending draws use
    ///
    /// Nothing in it owns what it points to, the pointed-to state
    /// objects live in the device's caches.
    ///
    ////////////////////////////////////////////////////////////
    struct PipelineState
    {
        BlendMode                 blendMode;           //!< Blend mode of the draws
        bool                      colorWrite{true};    //!< Whether the draws write the color channels
        std::uint32_t             blendKey{};          //!< Packed blend mode and color write flag
        MetalDepthStencilStatePtr depthStencilState{}; //!< Depth-stencil state of the draws
        std::uint32_t             stencilReference{};  //!< Stencil reference value of the draws
        MetalTexturePtr           texture{};           //!< Texture the draws sample
        MetalSamplerStatePtr      textureSampler{};    //!< Sampler the draws sample with
        bool                      scissorEnabled{};    //!< Whether scissor testing is enabled
        MetalViewport             viewport{};          //!< Viewport of the draws
        MetalScissor              scissorRect{};       //!< Scissor rectangle of the draws
        std::array<float, 48>     constants{};         //!< Model-view, projection and texture matrix of the draws
        const void*               userShader{}; //!< User shader drawn with instead of the built-in pipeline, null for none
        std::uint64_t             userShaderState{}; //!< Stamp of the user shader's bindable state when it was set
    };

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    NSPtr<MetalDevicePtr>        m_device;       //!< Metal device, free-threaded
    NSPtr<MetalCommandQueuePtr>  m_commandQueue; //!< Command queue, free-threaded
    mutable std::recursive_mutex m_mutex;        //!< Serializes access to the encoding state and caches

    NSPtr<MetalCommandBufferPtr> m_commandBuffer; //!< Command buffer commands are recorded into, created lazily
    NSPtr<MetalRenderEncoderPtr> m_encoder;       //!< Encoder of the open render pass, null between passes
    MetalSurfaceAttachments      m_attachments;   //!< Attachments of the open render pass, valid while m_encoder

    MetalRenderSurface* m_currentSurface{};        //!< Surface render passes draw into, not owned
    std::uint64_t       m_currentRenderTargetId{}; //!< Id of the render target active on the device

    bool         m_pendingColorClear{};        //!< Whether the next pass clears the color attachment
    Color        m_pendingClearColor;          //!< Color the next pass clears to
    bool         m_pendingStencilClear{};      //!< Whether the next pass clears the stencil attachment
    std::uint8_t m_pendingStencilClearValue{}; //!< Stencil value the next pass clears to

    NSPtr<MetalLibraryPtr>  m_library;                 //!< Library holding the built-in shaders
    NSPtr<MetalFunctionPtr> m_defaultVertexFunction;   //!< Built-in vertex function
    NSPtr<MetalFunctionPtr> m_defaultFragmentFunction; //!< Built-in fragment function
    NSPtr<MetalTexturePtr>  m_whiteTexture;            //!< 1x1 white texture for untextured draws

    NSPtr<MetalBufferPtr> m_fanIndexBuffer;           //!< Index buffer holding the shared triangle-fan pattern
    std::size_t           m_fanIndexBufferVertices{}; //!< Largest fan vertex count the pattern covers

    MetalBufferPtr m_currentVertexBuffer{}; //!< Vertex buffer bound to the open encoder, not owned

    PipelineState m_pending;              //!< State the next draws use
    PipelineState m_applied;              //!< State the open encoder is known to hold
    bool          m_appliedValid{};       //!< Whether the encoder is known to hold m_applied
    bool m_appliedTextureValid{};    //!< Whether the encoder is known to hold the texture and sampler of m_applied
    std::uint64_t m_appliedPipelineKey{}; //!< Cache key of the pipeline state object the encoder holds, 0 for none
    bool m_userShaderBindPending{}; //!< Whether the draw that set the pending user shader still has to bind it

    std::vector<Vertex> m_pendingVertices;    //!< Vertices of merged draws awaiting submission
    std::uint32_t       m_pendingTopology{3}; //!< Raw MTLPrimitiveType of the pending vertices, triangles by default
    std::atomic<bool>   m_hasPendingDraws{};  //!< Lock-free hint that vertices are pending, verified under the lock

    std::shared_ptr<std::atomic<unsigned int>> m_inFlight; //!< Command buffers committed but not yet completed

    ////////////////////////////////////////////////////////////
    /// \brief Buffers returned by completed command buffers, shared with their handlers
    ///
    ////////////////////////////////////////////////////////////
    struct TransientFreeList
    {
        std::mutex                         mutex;   //!< Guards the buffers, handlers run on GPU threads
        std::vector<NSPtr<MetalBufferPtr>> buffers; //!< Chunks ready for reuse
    };

    std::shared_ptr<TransientFreeList> m_transientFreeList; //!< Chunks recycled once the GPU is done with them
    std::vector<NSPtr<MetalBufferPtr>> m_transientBuffers;  //!< Chunks owned by the recording command buffer
    std::size_t                        m_transientCursor{}; //!< Append position in the newest chunk

    std::unordered_map<std::uint64_t, NSPtr<MetalRenderPipelinePtr>> m_pipelineStates; //!< Cache of pipeline state objects
    std::unordered_map<std::uint32_t, NSPtr<MetalDepthStencilStatePtr>> m_depthStencilStates; //!< Cache of depth-stencil state objects, keyed by packed stencil mode
    std::array<NSPtr<MetalSamplerStatePtr>, 8> m_samplerStates; //!< Sampler states, indexed by smooth, repeat and mipmap
};

} // namespace sf::priv
