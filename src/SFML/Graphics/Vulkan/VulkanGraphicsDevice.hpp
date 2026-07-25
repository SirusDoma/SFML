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
#include <SFML/Graphics/StencilMode.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/Vulkan/VulkanUtils.hpp>

#include <SFML/System/Vector2.hpp>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <cstddef>
#include <cstdint>

// Forward declarations of the VulkanMemoryAllocator handles
struct VmaAllocator_T;
struct VmaAllocation_T;
using VmaAllocator  = VmaAllocator_T*;
using VmaAllocation = VmaAllocation_T*;


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Attachments a render pass draws into
///
/// The images are not owned, the surface that filled in the
/// struct keeps them alive while the pass is recorded.
///
////////////////////////////////////////////////////////////
struct VulkanSurfaceAttachments
{
    VkImage      colorImage{};         //!< Color image, the multisampled one when multisampling
    VkImageView  colorView{};          //!< View of the color image
    VkImage      resolveImage{};       //!< Resolve target of the color image, null when not multisampling
    VkImageView  resolveView{};        //!< View of the resolve target
    VkImage      depthStencilImage{};  //!< Depth-stencil image, can be null
    VkImageView  depthStencilView{};   //!< View of the depth-stencil image
    VkFormat     colorFormat{};        //!< Format of the color attachment
    VkFormat     depthStencilFormat{}; //!< Format of the depth-stencil attachment, VK_FORMAT_UNDEFINED when none
    unsigned int sampleCount{1};       //!< Samples per pixel of the color attachment
    Vector2u     size;                 //!< Size of the attachments, in pixels
    bool         colorGeneralLayout{}; //!< Whether the color images live in the GENERAL layout (render textures)
};

////////////////////////////////////////////////////////////
/// \brief Interface of a surface render passes can target
///
/// Implemented by the presentation surfaces and render textures.
/// Attachments are requested lazily every time a pass opens, a
/// window surface acquires its swapchain image at that point.
///
////////////////////////////////////////////////////////////
class VulkanRenderSurface
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~VulkanRenderSurface() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Provide the attachments of the next render pass
    ///
    /// \param attachments Attachments to fill in
    ///
    /// \return `true` if the attachments are ready to be drawn into
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual bool prepareAttachments(VulkanSurfaceAttachments& attachments) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Get an image holding the surface's rendered content
    ///
    /// Pending content of the surface is submitted first, and
    /// multisampled content resolved. The image stays owned by
    /// the surface and is only valid until its next present.
    ///
    /// \param size Filled with the size of the returned image
    ///
    /// \return Image with the rendered content, null when the surface cannot be read
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual VkImage acquireReadableColorImage(Vector2u& size) = 0;
};

////////////////////////////////////////////////////////////
/// \brief Location of data placed in a transient buffer
///
////////////////////////////////////////////////////////////
struct VulkanTransientAllocation
{
    VkBuffer    buffer{}; //!< Buffer the data was placed in
    std::size_t offset{}; //!< Byte offset of the data inside the buffer
};

////////////////////////////////////////////////////////////
/// \brief Vulkan implementation of the graphics device
///
/// Owns the instance, device and graphics queue all resources
/// and render targets share, the built-in pipeline replicating
/// the fixed-function behavior of the OpenGL backend, and the
/// caches for render passes, framebuffers and pipelines.
///
/// Commands are recorded into one current command buffer, render
/// passes open lazily on the first draw and close on target
/// switches, clears, presents and readbacks. The pipeline state
/// of pending draws is tracked and applied on submission.
///
////////////////////////////////////////////////////////////
class VulkanGraphicsDevice : public GraphicsDevice
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor, creates the device and built-in pipeline
    ///
    ////////////////////////////////////////////////////////////
    VulkanGraphicsDevice();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~VulkanGraphicsDevice() override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether a device fit for the backend exists at run time
    ///
    /// Checks the loader, the instance version and the presence of
    /// a Vulkan 1.2 device with a graphics queue and swapchain
    /// support. The probe runs once and its result is cached.
    ///
    /// \return `true` if the Vulkan renderer can be used on this system
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static bool isAvailable();

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
    /// The surface presents to the window through a swapchain.
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
    /// \brief RAII guard serializing access to the recording state
    ///
    /// The queue and recording state (current command buffer,
    /// open render pass and caches) are not thread-safe, every
    /// backend operation that touches them has to hold one of
    /// these. This is the moral equivalent of the OpenGL
    /// TransientContextLock.
    ///
    ////////////////////////////////////////////////////////////
    class ContextLock
    {
    public:
        explicit ContextLock(const VulkanGraphicsDevice& device) : m_lock(device.m_mutex)
        {
        }

    private:
        std::lock_guard<std::recursive_mutex> m_lock;
    };

    ////////////////////////////////////////////////////////////
    /// \brief Get the resolved Vulkan entry points
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] const VulkanFunctions& fn() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the Vulkan instance, null if creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkInstance getInstance() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the physical device the logical device runs on
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the logical device, null if creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkDevice getDevice() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the graphics queue all rendering is submitted through
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkQueue getQueue() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the family index of the graphics queue
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::uint32_t getQueueFamilyIndex() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the memory allocator of the device
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VmaAllocator getAllocator() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the alignment uniform data placed in buffers requires
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::size_t getUniformBufferAlignment() const;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether presents can be waited on until they reach the screen
    ///
    /// \return `true` when the device enabled VK_KHR_present_wait
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool hasPresentWait() const;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether points default to a size of 1.0 without a shader write
    ///
    /// \return `true` when the device enabled VK_KHR_maintenance5
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool hasMaintenance5() const;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the current target has a stencil buffer
    ///
    /// Stencil work is dropped without one, the same way the
    /// pipeline drops the stencil mode.
    ///
    /// \return `true` when the attachments include a stencil aspect
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool hasStencilAttachment() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the size the current attachments were prepared with
    ///
    /// Commands bounded by the render area, like rectangle
    /// clears, have to fit inside this rather than inside the
    /// size the target believes it has.
    ///
    /// \return Size of the attachments, in pixels
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] Vector2u getAttachmentSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the push constant range the built-in vertex stage reads
    ///
    /// The built-in vertex shader receives the combined model-view
    /// projection matrix and the texture matrix as push constants,
    /// 128 bytes in total, the amount every Vulkan implementation
    /// guarantees. It also runs under the pipeline layout of a user
    /// shader that only replaces the fragment stage, so those
    /// layouts have to declare the same range.
    ///
    /// \return The range covering the built-in matrices
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static VkPushConstantRange builtinMatricesRange();

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
    /// \brief Look up the layout an image is known to be in
    ///
    /// \param image Image to look up
    ///
    /// \return Tracked layout, VK_IMAGE_LAYOUT_UNDEFINED when never seen
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkImageLayout getTrackedImageLayout(VkImage image) const;

    ////////////////////////////////////////////////////////////
    /// \brief Record the layout an image was transitioned to
    ///
    /// \param image  Image to track
    /// \param layout Layout the image is in now
    ///
    ////////////////////////////////////////////////////////////
    void setTrackedImageLayout(VkImage image, VkImageLayout layout);

    ////////////////////////////////////////////////////////////
    /// \brief Forget an image, called when it is destroyed
    ///
    /// Also drops framebuffers referencing the image's views.
    ///
    /// \param image Image being destroyed
    ///
    ////////////////////////////////////////////////////////////
    void forgetImage(VkImage image);

    ////////////////////////////////////////////////////////////
    /// \brief Drop cached framebuffers referencing a view
    ///
    /// \param view View being destroyed
    ///
    ////////////////////////////////////////////////////////////
    void purgeFramebuffers(VkImageView view);

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
    void bindSurface(VulkanRenderSurface* surface);

    ////////////////////////////////////////////////////////////
    /// \brief Unbind a surface if it is the current output
    ///
    /// \param surface Surface to unbind
    ///
    ////////////////////////////////////////////////////////////
    void unbindSurface(VulkanRenderSurface* surface);

    ////////////////////////////////////////////////////////////
    /// \brief Get the surface render passes currently draw into
    ///
    /// \return Current surface, null if no surface is bound
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VulkanRenderSurface* getCurrentSurface() const;

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
    /// \brief Get the pending texture view of the current draw
    ///
    /// Shaders use this to resolve their CurrentTexture uniform
    /// and any texture they declare without assigning one, which
    /// samples the draw's texture like an unset GLSL sampler does.
    ///
    /// \return View of the texture pending for the built-in texture slot
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkImageView getCurrentTextureView() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the pending sampler of the current draw
    ///
    /// \return Sampler pending alongside the current texture
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkSampler getCurrentTextureSampler() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the layout of the pending texture of the current draw
    ///
    /// \return Layout the pending texture is sampled in
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkImageLayout getCurrentTextureLayout() const;

    ////////////////////////////////////////////////////////////
    /// \brief Place the pending matrices in transient memory
    ///
    /// The upload is skipped when the transient data already holds
    /// the pending constants. Used by the built-in pipeline and by
    /// user shaders consuming the SFML matrices.
    ///
    /// \param allocation Location of the constants
    ///
    /// \return `true` if the constants are available
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool uploadPendingConstants(VulkanTransientAllocation& allocation);

    ////////////////////////////////////////////////////////////
    /// \brief Get the sampler state for texture filter settings
    ///
    /// \param smooth    Whether the smooth filter is enabled
    /// \param repeated  Whether the texture repeats
    /// \param mipmapped Whether the texture has a generated mipmap
    ///
    /// \return Sampler object, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkSampler getSamplerState(bool smooth, bool repeated, bool mipmapped = false);

    ////////////////////////////////////////////////////////////
    /// \brief Get the view of the 1x1 white texture
    ///
    /// Bound for draws without texture so a single pixel shader
    /// covers both the textured and untextured case.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkImageView getWhiteTextureView() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the shared triangle-fan index buffer
    ///
    /// Vulkan has no triangle-fan topology the backend could rely
    /// on, fans are drawn as an indexed triangle list instead. All
    /// fans share one index pattern (0, i + 1, i + 2): smaller
    /// fans draw a prefix of it and the fan's first vertex is
    /// applied through the vertex offset of the draw call.
    ///
    /// \param vertexCount Number of vertices in the fan
    /// \param indexCount  Number of indices to draw
    ///
    /// \return The index buffer, or a null handle on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkBuffer getTriangleFanIndexBuffer(std::size_t vertexCount, std::size_t& indexCount);

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
    /// \brief Set the stencil mode the next draws use
    ///
    /// \param mode Stencil mode to set
    ///
    ////////////////////////////////////////////////////////////
    void setPendingStencilMode(const StencilMode& mode);

    ////////////////////////////////////////////////////////////
    /// \brief Set the texture and sampler the next draws use
    ///
    /// \param view    View of the texture for the built-in texture slot
    /// \param sampler Sampler used alongside the texture
    /// \param layout  Layout the texture is in while it is sampled
    ///
    ////////////////////////////////////////////////////////////
    void setPendingTexture(VkImageView view, VkSampler sampler, VkImageLayout layout);

    ////////////////////////////////////////////////////////////
    /// \brief Set the viewport the next draws use
    ///
    /// The rectangle is in top-left window coordinates, the
    /// upside-down flip Vulkan needs is applied internally.
    ///
    /// \param viewport Viewport to set
    ///
    ////////////////////////////////////////////////////////////
    void setPendingViewport(const VkViewport& viewport);

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
    void setPendingScissor(bool enabled, const VkRect2D& rect);

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
    /// \brief Set the shader modules and layout of the pending user shader
    ///
    /// Called by the shader implementation when it is bound; the
    /// modules are used when the draw's pipeline is created.
    ///
    /// \param vertexModule      Vertex shader module, null to use the built-in one
    /// \param pointVertexModule Vertex module variant for point pipelines, null to use vertexModule
    /// \param geometryModule    Geometry shader module, can be null
    /// \param fragmentModule    Fragment shader module, null to use the built-in one
    /// \param layout            Pipeline layout of the shader
    /// \param shaderId          Number identifying the module set, never reused across shaders
    ///
    ////////////////////////////////////////////////////////////
    void setPendingUserPipelineSource(VkShaderModule   vertexModule,
                                      VkShaderModule   pointVertexModule,
                                      VkShaderModule   geometryModule,
                                      VkShaderModule   fragmentModule,
                                      VkPipelineLayout layout,
                                      std::uint32_t    shaderId);

    ////////////////////////////////////////////////////////////
    /// \brief Drop the cached pipelines of a user shader
    ///
    /// Shader ids are never reused, without this the pipelines of
    /// destroyed or recompiled shaders would stay cached forever.
    ///
    /// \param shaderId Identity of the module set to clear
    ///
    ////////////////////////////////////////////////////////////
    void clearShaderPipelines(std::uint32_t shaderId);

    ////////////////////////////////////////////////////////////
    /// \brief Record a clear of the color attachment
    ///
    /// The clear becomes the load operation of the next render
    /// pass, an open pass is ended first so already-drawn content
    /// is cleared as well.
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
    /// \brief Set the recorded state to the pending pipeline state
    ///
    /// Opens a render pass on the current surface when none is
    /// open. Only the parts that differ from what the command
    /// buffer is known to hold are set. Must be called before
    /// recording a draw outside of `flushPendingDraws`.
    ///
    /// \return `true` if a render pass is open and the state is applied
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool applyPendingState();

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the pending user shader still has to be bound
    ///
    /// Set by `applyPendingState` when the pending user shader is
    /// not the one the command buffer holds, and reset when read.
    /// The draw that set it as pending binds it when this is `true`.
    ///
    /// \return `true` if the caller has to bind the pending user shader
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool takeUserShaderBindPending();

    ////////////////////////////////////////////////////////////
    /// \brief Bind the pipeline of the pending state for a topology
    ///
    /// Pipelines bake the primitive topology on Vulkan, the bind
    /// happens once the topology of the draw is known. Only valid
    /// after `applyPendingState` returned `true`.
    ///
    /// \param topology Topology the draw uses
    ///
    /// \return `true` if a pipeline is bound
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool bindDrawPipeline(VkPrimitiveTopology topology);

    ////////////////////////////////////////////////////////////
    /// \brief Push the matrices the built-in vertex stage reads
    ///
    /// Combines the pending model-view and projection matrices and
    /// records them with the texture matrix when they changed since
    /// the last push, or when the layout they were pushed with is
    /// no longer the one in use.
    ///
    /// \param layout Pipeline layout of the draw
    ///
    ////////////////////////////////////////////////////////////
    void pushBuiltinMatrices(VkPipelineLayout layout);

    ////////////////////////////////////////////////////////////
    /// \brief Get the built-in descriptor set for a texture and sampler
    ///
    /// The sets only reference resources that outlive a frame, so
    /// they are written once and kept until the texture they
    /// reference is destroyed, unlike the per-frame sets user
    /// shaders allocate.
    ///
    /// \param view    Image view the set samples
    /// \param layout  Layout the image is sampled in
    /// \param sampler Sampler the set uses
    ///
    /// \return The set, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkDescriptorSet getBuiltinDescriptorSet(VkImageView view, VkImageLayout layout, VkSampler sampler);

    ////////////////////////////////////////////////////////////
    /// \brief Forget what the command buffer is known to hold
    ///
    /// Must be called when the state may have been changed outside
    /// of `applyPendingState`, like by raw Vulkan user code. The
    /// pending state is fully re-applied on the next draw.
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
    /// \brief Get the command buffer of the open render pass
    ///
    /// Only valid after `applyPendingState` returned `true`, and
    /// only while holding a ContextLock.
    ///
    /// \return The command buffer, null when no pass is open
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkCommandBuffer getRenderCommandBuffer() const;

    ////////////////////////////////////////////////////////////
    /// \brief Upload vertices and bind them as the vertex source of the next draw
    ///
    /// The vertices live in transient memory recycled once the
    /// frame's commands completed. Requires an open render pass.
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
    void bindVertexBuffer(VkBuffer buffer);

    ////////////////////////////////////////////////////////////
    /// \brief Bind a vertex buffer at a byte offset unless it is already bound
    ///
    /// \param buffer Vertex buffer holding `sf::Vertex` data
    /// \param offset Byte offset the vertex data starts at
    ///
    ////////////////////////////////////////////////////////////
    void bindVertexBuffer(VkBuffer buffer, std::size_t offset);

    ////////////////////////////////////////////////////////////
    /// \brief Bind an index buffer unless it is already bound
    ///
    /// Requires an open render pass.
    ///
    /// \param buffer Index buffer holding 32-bit indices
    ///
    ////////////////////////////////////////////////////////////
    void bindIndexBuffer(VkBuffer buffer);

    ////////////////////////////////////////////////////////////
    /// \brief Copy data into transient memory of the current frame
    ///
    /// Allocations bump-allocate host-visible chunks that are
    /// recycled once the frame's commands completed, no allocation
    /// happens on the steady-state path.
    ///
    /// \param data       Data to copy
    /// \param size       Byte size of the data
    /// \param alignment  Alignment the placed data requires
    /// \param allocation Location the data was placed at
    ///
    /// \return `true` if the allocation succeeded
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool allocateTransient(const void*                data,
                                         std::size_t                size,
                                         std::size_t                alignment,
                                         VulkanTransientAllocation& allocation);

    ////////////////////////////////////////////////////////////
    /// \brief Allocate a descriptor set from the current frame's pool
    ///
    /// The set lives until the frame's commands completed and its
    /// pool is reset.
    ///
    /// \param layout Layout of the set to allocate
    ///
    /// \return The descriptor set, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout);

    ////////////////////////////////////////////////////////////
    /// \brief Collect vertices of a draw to submit them together with following compatible draws
    ///
    /// Consecutive draws whose pipeline state is identical are
    /// merged into a single draw call. Anything that changes what
    /// the pending vertices would render must call
    /// `flushPendingDraws` first.
    ///
    /// \param vertices    Vertices to collect, in list order
    /// \param vertexCount Number of vertices to collect
    /// \param topology    List topology the vertices belong to
    ///
    ////////////////////////////////////////////////////////////
    void appendPendingVertices(const Vertex* vertices, std::size_t vertexCount, VkPrimitiveTopology topology);

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
    /// Rendered attachment images are barriered so later sampling
    /// and copies see the rendered content.
    ///
    ////////////////////////////////////////////////////////////
    void endEncoding();

    ////////////////////////////////////////////////////////////
    /// \brief Finish recording the frame of the current surface before presenting
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
    [[nodiscard]] VkCommandBuffer currentCommandBuffer();

    ////////////////////////////////////////////////////////////
    /// \brief Add a semaphore the next submission waits on
    ///
    /// Used for swapchain image acquisition, the wait happens at
    /// the color attachment output stage.
    ///
    /// \param semaphore Semaphore to wait on
    ///
    ////////////////////////////////////////////////////////////
    void addAcquireWaitSemaphore(VkSemaphore semaphore);

    ////////////////////////////////////////////////////////////
    /// \brief Commit the current command buffer to the queue
    ///
    /// Any open render pass is ended first. Does nothing when no
    /// command buffer is recording, unless a wait semaphore or
    /// signal semaphore is outstanding.
    ///
    /// \param waitUntilCompleted Block until the GPU finished executing the commands
    /// \param signalSemaphore    Semaphore to signal on completion, can be null
    ///
    ////////////////////////////////////////////////////////////
    void commitCommandBuffer(bool waitUntilCompleted, VkSemaphore signalSemaphore = VK_NULL_HANDLE);

    ////////////////////////////////////////////////////////////
    /// \brief Wait until at most the given number of frames is in flight
    ///
    /// Paces the application to the GPU: waiting here bounds the
    /// frames in flight and the delay until frames reach the
    /// screen.
    ///
    /// \param maxPending Number of submitted frames allowed to still be executing
    ///
    ////////////////////////////////////////////////////////////
    void waitForInFlightFrames(unsigned int maxPending);

    ////////////////////////////////////////////////////////////
    /// \brief Defer destruction of a buffer until the GPU is done with it
    ///
    /// \param buffer     Buffer to destroy
    /// \param allocation Allocation backing the buffer
    ///
    ////////////////////////////////////////////////////////////
    void deferDestroyBuffer(VkBuffer buffer, VmaAllocation allocation);

    ////////////////////////////////////////////////////////////
    /// \brief Defer destruction of an image until the GPU is done with it
    ///
    /// \param image      Image to destroy
    /// \param view       View of the image, can be null
    /// \param allocation Allocation backing the image
    ///
    ////////////////////////////////////////////////////////////
    void deferDestroyImage(VkImage image, VkImageView view, VmaAllocation allocation);

    ////////////////////////////////////////////////////////////
    /// \brief Destroy the layouts of a shader once the frame recycles
    ///
    /// Commands already recorded reference the pipeline layout
    /// they were bound through, so it has to outlive them just
    /// like the pipelines built from it do.
    ///
    /// \param pipelineLayout Pipeline layout to destroy, can be null
    /// \param setLayout      Descriptor set layout to destroy, can be null
    ///
    ////////////////////////////////////////////////////////////
    void deferDestroyShaderLayouts(VkPipelineLayout pipelineLayout, VkDescriptorSetLayout setLayout);

    ////////////////////////////////////////////////////////////
    /// \brief Give pending clears a render pass that carries them out
    ///
    /// Clears only execute as pass load operations, anything
    /// reading rendered content has to call this first in case the
    /// content so far is only a recorded clear.
    ///
    ////////////////////////////////////////////////////////////
    void materializePendingClears();

    ////////////////////////////////////////////////////////////
    /// \brief Get or create a render pass
    ///
    /// Passes are cached, the same combination is returned again.
    ///
    /// \param colorFormat        Format of the color attachment
    /// \param depthStencilFormat Format of the depth-stencil attachment, VK_FORMAT_UNDEFINED for none
    /// \param sampleCount        Samples per pixel of the color attachment
    /// \param hasResolve         Whether a resolve attachment is present
    /// \param generalLayout      Whether the color images live in the GENERAL layout
    /// \param colorLoad          Load operation of the color attachment
    /// \param stencilLoad        Load operation of the stencil aspect
    /// \param colorInitialLayout Layout the color attachment is in when the pass begins
    /// \param dsInitialLayout    Layout the depth-stencil attachment is in when the pass begins
    ///
    /// \return The render pass, or a null handle on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkRenderPass getRenderPass(
        VkFormat           colorFormat,
        VkFormat           depthStencilFormat,
        unsigned int       sampleCount,
        bool               hasResolve,
        bool               generalLayout,
        VkAttachmentLoadOp colorLoad,
        VkAttachmentLoadOp stencilLoad,
        VkImageLayout      colorInitialLayout,
        VkImageLayout      dsInitialLayout);

    ////////////////////////////////////////////////////////////
    /// \brief Get the render pass currently open, if any
    ///
    /// \return The open render pass, or a null handle
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkRenderPass getOpenRenderPass() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the descriptor set layout of the built-in pipeline
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkDescriptorSetLayout getBuiltinDescriptorSetLayout() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the pipeline layout of the built-in pipeline
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkPipelineLayout getBuiltinPipelineLayout() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the built-in vertex shader module replicating the fixed pipeline
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkShaderModule getDefaultVertexShader() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the built-in fragment shader module replicating the fixed pipeline
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkShaderModule getDefaultFragmentShader() const;

private:
    ////////////////////////////////////////////////////////////
    /// \brief One frame of recording resources
    ///
    /// Everything in a slot is recycled once the fence of its
    /// submission signals.
    ///
    ////////////////////////////////////////////////////////////
    struct FrameSlot
    {
        VkCommandPool   commandPool{};   //!< Pool the slot's command buffer allocates from
        VkCommandBuffer commandBuffer{}; //!< Command buffer commands are recorded into
        VkFence         fence{};         //!< Signaled when the slot's submission completed
        bool            submitted{};     //!< Whether the slot's commands are executing

        ////////////////////////////////////////////////////////////
        /// \brief Host-visible chunk transient data bump-allocates from
        ///
        ////////////////////////////////////////////////////////////
        struct TransientChunk
        {
            VkBuffer      buffer{};     //!< Buffer of the chunk
            VmaAllocation allocation{}; //!< Allocation backing the buffer
            void*         mapped{};     //!< Persistently mapped pointer
            std::size_t   capacity{};   //!< Byte capacity of the chunk
            std::size_t   cursor{};     //!< Append position in the chunk
            bool          coherent{};   //!< Whether the memory is host-coherent, writes then need no flush
        };

        ////////////////////////////////////////////////////////////
        /// \brief Key identifying a written built-in descriptor set
        ///
        ////////////////////////////////////////////////////////////
        struct BuiltinSetKey
        {
            VkImageView view;
            VkSampler   sampler;

            bool operator==(const BuiltinSetKey& rhs) const
            {
                return (view == rhs.view) && (sampler == rhs.sampler);
            }
        };

        struct BuiltinSetKeyHasher
        {
            std::size_t operator()(const BuiltinSetKey& key) const
            {
                std::size_t seed = vulkanHandleHash(key.view);
                seed ^= vulkanHandleHash(key.sampler) + 0x9e3779b97f4a7c15ull + (seed << 6);
                return seed;
            }
        };

        std::vector<TransientChunk>   transientChunks; //!< Chunks of the slot, the last one is appended to
        std::vector<VkDescriptorPool> descriptorPools; //!< Descriptor pools of the slot, the last one is allocated from
        std::vector<std::pair<VkBuffer, VmaAllocation>> deadBuffers; //!< Buffers destroyed once the slot recycles
        std::vector<std::tuple<VkImage, VkImageView, VmaAllocation>> deadImages; //!< Images destroyed once the slot recycles
        std::vector<VkFramebuffer>         deadFramebuffers;    //!< Framebuffers destroyed once the slot recycles
        std::vector<VkPipeline>            deadPipelines;       //!< Pipelines destroyed once the slot recycles
        std::vector<VkPipelineLayout>      deadPipelineLayouts; //!< Pipeline layouts destroyed once the slot recycles
        std::vector<VkDescriptorSetLayout> deadSetLayouts; //!< Descriptor set layouts destroyed once the slot recycles
        std::vector<std::pair<VkDescriptorPool, VkDescriptorSet>> deadBuiltinSets; //!< Built-in sets freed once the slot recycles
    };

    ////////////////////////////////////////////////////////////
    /// \brief Pipeline state the pending draws use
    ///
    ////////////////////////////////////////////////////////////
    struct PipelineState
    {
        BlendMode             blendMode;        //!< Blend mode of the draws
        bool                  colorWrite{true}; //!< Whether the draws write the color channels
        std::uint32_t         blendKey{};       //!< Packed blend mode and color write flag
        StencilMode           stencilMode;      //!< Stencil mode of the draws
        std::uint32_t         stencilKey{};     //!< Packed stencil mode without the reference value
        VkImageView           textureView{};    //!< Texture the draws sample
        VkSampler             textureSampler{}; //!< Sampler the draws sample with
        VkImageLayout         textureLayout{};  //!< Layout the texture is in while it is sampled
        bool                  scissorEnabled{}; //!< Whether scissor testing is enabled
        VkViewport            viewport{};       //!< Viewport of the draws, in top-left coordinates
        VkRect2D              scissorRect{};    //!< Scissor rectangle of the draws
        std::array<float, 48> constants{};      //!< Model-view, projection and texture matrix of the draws
        const void*           userShader{}; //!< User shader drawn with instead of the built-in pipeline, null for none
        std::uint64_t         userShaderState{}; //!< Stamp of the user shader's bindable state when it was set
    };

    ////////////////////////////////////////////////////////////
    /// \brief Key identifying a cached render pass
    ///
    ////////////////////////////////////////////////////////////
    struct RenderPassKey
    {
        VkFormat           colorFormat;
        VkFormat           depthStencilFormat;
        std::uint32_t      sampleCount;
        bool               hasResolve;
        bool               generalLayout;
        VkAttachmentLoadOp colorLoad;
        VkAttachmentLoadOp stencilLoad;
        VkImageLayout      colorInitialLayout;
        VkImageLayout      dsInitialLayout;

        bool operator==(const RenderPassKey& rhs) const
        {
            return (colorFormat == rhs.colorFormat) && (depthStencilFormat == rhs.depthStencilFormat) &&
                   (sampleCount == rhs.sampleCount) && (hasResolve == rhs.hasResolve) &&
                   (generalLayout == rhs.generalLayout) && (colorLoad == rhs.colorLoad) &&
                   (stencilLoad == rhs.stencilLoad) && (colorInitialLayout == rhs.colorInitialLayout) &&
                   (dsInitialLayout == rhs.dsInitialLayout);
        }
    };

    struct RenderPassKeyHasher
    {
        std::size_t operator()(const RenderPassKey& key) const;
    };

    ////////////////////////////////////////////////////////////
    /// \brief Key identifying a cached framebuffer
    ///
    ////////////////////////////////////////////////////////////
    struct FramebufferKey
    {
        VkRenderPass  renderPass;
        VkImageView   colorView;
        VkImageView   resolveView;
        VkImageView   depthStencilView;
        std::uint32_t width;
        std::uint32_t height;

        bool operator==(const FramebufferKey& rhs) const
        {
            return (renderPass == rhs.renderPass) && (colorView == rhs.colorView) && (resolveView == rhs.resolveView) &&
                   (depthStencilView == rhs.depthStencilView) && (width == rhs.width) && (height == rhs.height);
        }
    };

    struct FramebufferKeyHasher
    {
        std::size_t operator()(const FramebufferKey& key) const;
    };

    ////////////////////////////////////////////////////////////
    /// \brief Key identifying a cached pipeline
    ///
    ////////////////////////////////////////////////////////////
    struct PipelineKey
    {
        std::uint32_t       shaderId;   //!< 0 for the built-in pipeline
        std::uint32_t       blendKey;   //!< Packed blend mode and color write flag
        std::uint32_t       stencilKey; //!< Packed stencil mode
        VkPrimitiveTopology topology;
        VkFormat            colorFormat;
        VkFormat            depthStencilFormat;
        std::uint32_t       sampleCount;
        bool                generalLayout;

        bool operator==(const PipelineKey& rhs) const
        {
            return (shaderId == rhs.shaderId) && (blendKey == rhs.blendKey) && (stencilKey == rhs.stencilKey) &&
                   (topology == rhs.topology) && (colorFormat == rhs.colorFormat) &&
                   (depthStencilFormat == rhs.depthStencilFormat) && (sampleCount == rhs.sampleCount) &&
                   (generalLayout == rhs.generalLayout);
        }
    };

    struct PipelineKeyHasher
    {
        std::size_t operator()(const PipelineKey& key) const;
    };

    ////////////////////////////////////////////////////////////
    /// \brief Create the instance, device and queue
    ///
    /// \return `true` on success
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool createDevice();

    ////////////////////////////////////////////////////////////
    /// \brief Create the built-in pipeline objects
    ///
    ////////////////////////////////////////////////////////////
    void createPipeline();

    ////////////////////////////////////////////////////////////
    /// \brief Open a render pass on the current surface if none is open
    ///
    /// Consumes the pending clears as load operations.
    ///
    /// \return `true` if a render pass is open
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool ensureRenderPass();

    ////////////////////////////////////////////////////////////
    /// \brief Get or create the framebuffer for the current attachments
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkFramebuffer getFramebuffer(VkRenderPass renderPass, const VulkanSurfaceAttachments& attachments);

    ////////////////////////////////////////////////////////////
    /// \brief Get or create a pipeline
    ///
    /// \param key            Key identifying the pipeline
    /// \param vertexModule   Vertex module, null for the built-in one
    /// \param geometryModule Geometry module, can be null
    /// \param fragmentModule Fragment module, null for the built-in one
    /// \param layout         Pipeline layout the pipeline is created with
    ///
    /// \return The pipeline, or a null handle on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkPipeline getPipeline(const PipelineKey& key,
                                         VkShaderModule     vertexModule,
                                         VkShaderModule     geometryModule,
                                         VkShaderModule     fragmentModule,
                                         VkPipelineLayout   layout);

    ////////////////////////////////////////////////////////////
    /// \brief Update and bind the descriptor set of the built-in pipeline
    ///
    /// \return `true` if a set is bound
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool bindBuiltinDescriptors();

    ////////////////////////////////////////////////////////////
    /// \brief Make sure a frame slot is ready for recording
    ///
    /// Waits for the slot's previous submission and recycles its
    /// transient resources.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool beginFrameSlot();

    ////////////////////////////////////////////////////////////
    /// \brief Recycle a slot whose fence has signaled
    ///
    /// \param slot Slot to recycle
    ///
    ////////////////////////////////////////////////////////////
    void recycleFrameSlot(FrameSlot& slot);

    ////////////////////////////////////////////////////////////
    /// \brief Pick the slot deferred destructions wait on
    ///
    /// While recording, that is the recording slot: references can
    /// only live in it or older, already-fenced submissions. When
    /// nothing is recording, it is the most recently submitted
    /// slot, whose fence covers every possible reference.
    ///
    /// \return Slot whose recycling destroys the deferred objects
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] FrameSlot& deferSlot();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    VulkanFunctions              m_fn;                 //!< Resolved Vulkan entry points
    VkInstance                   m_instance{};         //!< Vulkan instance, owned
    VkPhysicalDevice             m_physicalDevice{};   //!< Physical device the logical device runs on
    VkDevice                     m_device{};           //!< Logical device, owned
    VkQueue                      m_queue{};            //!< Graphics queue, owned by the device
    std::uint32_t                m_queueFamilyIndex{}; //!< Family index of the graphics queue
    VmaAllocator                 m_allocator{};        //!< Memory allocator
    VkPhysicalDeviceProperties   m_properties{};       //!< Properties of the physical device
    VkPhysicalDeviceFeatures     m_features{};         //!< Features enabled on the device
    bool                         m_hasPresentWait{};   //!< Whether VK_KHR_present_wait is enabled
    bool                         m_hasMaintenance5{};  //!< Whether VK_KHR_maintenance5 is enabled
    mutable std::recursive_mutex m_mutex;              //!< Serializes access to the recording state and caches

    static constexpr unsigned int FrameSlotCount = 2; //!< Frames that can be recorded before waiting

    std::array<FrameSlot, FrameSlotCount> m_frameSlots;    //!< Per-frame recording resources
    unsigned int                          m_currentSlot{}; //!< Slot commands are currently recorded into
    bool                                  m_recording{};   //!< Whether the current slot's command buffer is recording

    std::vector<VkSemaphore> m_acquireWaitSemaphores; //!< Semaphores the next submission waits on

    VulkanRenderSurface* m_currentSurface{};        //!< Surface render passes draw into, not owned
    std::uint64_t        m_currentRenderTargetId{}; //!< Id of the render target active on the device

    bool                     m_passOpen{};       //!< Whether a render pass is open
    VkRenderPass             m_openRenderPass{}; //!< Handle of the open render pass, null between passes
    VulkanSurfaceAttachments m_attachments;      //!< Attachments of the open render pass, valid while m_passOpen

    bool         m_pendingColorClear{};        //!< Whether the next pass clears the color attachment
    Color        m_pendingClearColor;          //!< Color the next pass clears to
    bool         m_pendingStencilClear{};      //!< Whether the next pass clears the stencil attachment
    std::uint8_t m_pendingStencilClearValue{}; //!< Stencil value the next pass clears to

    VkShaderModule        m_defaultVertexShader{};   //!< Built-in vertex shader module
    VkShaderModule        m_defaultFragmentShader{}; //!< Built-in fragment shader module
    VkDescriptorSetLayout m_builtinSetLayout{};      //!< Descriptor set layout of the built-in pipeline
    VkPipelineLayout      m_builtinPipelineLayout{}; //!< Pipeline layout of the built-in pipeline

    VkImage       m_whiteImage{};           //!< 1x1 white texture for untextured draws
    VmaAllocation m_whiteImageAllocation{}; //!< Allocation backing the white texture
    VkImageView   m_whiteImageView{};       //!< View of the white texture

    VkBuffer      m_fanIndexBuffer{};           //!< Index buffer holding the shared triangle-fan pattern
    VmaAllocation m_fanIndexBufferAllocation{}; //!< Allocation backing the fan index buffer
    std::size_t   m_fanIndexBufferVertices{};   //!< Largest fan vertex count the pattern covers

    VkBuffer    m_currentVertexBuffer{};       //!< Vertex buffer bound to the command buffer, not owned
    std::size_t m_currentVertexBufferOffset{}; //!< Byte offset the bound vertex buffer was bound at
    VkBuffer    m_currentIndexBuffer{};        //!< Index buffer bound to the command buffer, not owned

    PipelineState m_pending;                 //!< State the next draws use
    PipelineState m_applied;                 //!< State the command buffer is known to hold
    bool          m_appliedValid{};          //!< Whether the command buffer is known to hold m_applied
    bool          m_appliedTextureValid{};   //!< Whether the bound descriptors hold the applied texture and sampler
    PipelineKey   m_appliedPipelineKey{};    //!< Key of the pipeline the command buffer holds
    bool          m_appliedPipelineValid{};  //!< Whether the command buffer is known to hold m_appliedPipelineKey
    bool          m_userShaderBindPending{}; //!< Whether the draw that set the pending user shader still has to bind it

    //!< Built-in sets live across frames, they only reference the texture and its sampler
    std::unordered_map<FrameSlot::BuiltinSetKey, std::pair<VkDescriptorPool, VkDescriptorSet>, FrameSlot::BuiltinSetKeyHasher> m_builtinSets;
    std::vector<VkDescriptorPool> m_builtinSetPools; //!< Pools the built-in sets are allocated from, never reset

    std::array<float, 32> m_pushedMatrices{}; //!< Matrices last pushed as constants
    VkPipelineLayout      m_pushedLayout{};   //!< Layout the matrices were last pushed with
    bool                  m_pushedValid{};    //!< Whether the pushed matrices are known to be current

    VkShaderModule m_userVertexModule{};      //!< Vertex module of the pending user shader, null for the built-in one
    VkShaderModule m_userPointVertexModule{}; //!< Vertex module variant for point pipelines, null to use the module above
    VkShaderModule   m_userGeometryModule{}; //!< Geometry module of the pending user shader
    VkShaderModule   m_userFragmentModule{}; //!< Fragment module of the pending user shader, null for the built-in one
    VkPipelineLayout m_userPipelineLayout{}; //!< Pipeline layout of the pending user shader
    std::uint32_t    m_userShaderId{};       //!< Id of the pending user shader's modules, 0 for none

    VkDescriptorSet       m_builtinDescriptorSet{};   //!< Built-in descriptor set bound last, from the frame's pool
    VkImageView           m_builtinSetView{};         //!< Texture view the bound built-in set was written with
    VkSampler             m_builtinSetSampler{};      //!< Sampler the bound built-in set was written with
    VkBuffer              m_constantsBuffer{};        //!< Transient buffer holding the uploaded constants
    std::size_t           m_constantsOffset{};        //!< Offset of the uploaded constants in their buffer
    bool                  m_builtinConstantsValid{};  //!< Whether the uploaded constants are current
    bool                  m_appliedDescriptorValid{}; //!< Whether the command buffer holds the built-in set and offset
    std::array<float, 48> m_uploadedConstants{};      //!< Constants the transient uniform data currently holds

    std::vector<Vertex> m_pendingVertices; //!< Vertices of merged draws awaiting submission
    VkPrimitiveTopology m_pendingTopology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST}; //!< Topology of the pending vertices
    std::atomic<bool>   m_hasPendingDraws{}; //!< Lock-free hint that vertices are pending, verified under the lock

    std::unordered_map<VkImage, VkImageLayout> m_imageLayouts; //!< Layout every image is known to be in

    std::unordered_map<RenderPassKey, VkRenderPass, RenderPassKeyHasher>    m_renderPasses; //!< Cache of render passes
    std::unordered_map<FramebufferKey, VkFramebuffer, FramebufferKeyHasher> m_framebuffers; //!< Cache of framebuffers
    std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHasher>          m_pipelines;    //!< Cache of pipelines
    std::array<VkSampler, 8> m_samplerStates{}; //!< Sampler states, indexed by smooth, repeat and mipmap
};

} // namespace sf::priv
