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
#include <SFML/Graphics/D3D11/D3D11Utils.hpp>
#include <SFML/Graphics/GraphicsDevice.hpp>
#include <SFML/Graphics/StencilMode.hpp>

#include <SFML/System/Vector2.hpp>

#include <array>
#include <d3d11_1.h>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <cstddef>
#include <cstdint>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Direct3D 11 implementation of the graphics device
///
/// Owns the single device and immediate context all resources
/// and render targets share, the built-in pipeline replicating
/// the fixed-function behavior of the OpenGL backend, and the
/// caches for immutable state objects.
///
////////////////////////////////////////////////////////////
class D3D11GraphicsDevice : public GraphicsDevice
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor, creates the device and built-in pipeline
    ///
    ////////////////////////////////////////////////////////////
    D3D11GraphicsDevice();

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~D3D11GraphicsDevice() override;

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
    /// The surface presents to the window through a swap chain.
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
    /// \brief Get the backend this device renders through
    ///
    /// \return The graphics backend
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] GraphicsBackend getBackend() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the shading language this backend consumes
    ///
    /// \return The shading language
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ShadingLanguage getShadingLanguage() const override;

    ////////////////////////////////////////////////////////////
    /// \brief RAII guard serializing access to the immediate context
    ///
    /// The immediate context is not thread-safe, every backend
    /// operation that touches it has to hold one of these. This
    /// is the moral equivalent of the OpenGL TransientContextLock.
    ///
    ////////////////////////////////////////////////////////////
    class ContextLock
    {
    public:
        explicit ContextLock(const D3D11GraphicsDevice& device) : m_lock(device.m_mutex)
        {
        }

    private:
        std::lock_guard<std::recursive_mutex> m_lock;
    };

    ////////////////////////////////////////////////////////////
    /// \brief Get the Direct3D device
    ///
    /// \return Pointer to the device, null if device creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11Device* getDevice() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the immediate context, only use while holding a ContextLock
    ///
    /// \return Pointer to the immediate context, null if device creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11DeviceContext* getContext() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the 11.1 immediate context, only use while holding a ContextLock
    ///
    /// \return Pointer to the 11.1 context, null when only Direct3D 11.0 is available
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11DeviceContext1* getContext1() const;

    ////////////////////////////////////////////////////////////
    /// \brief Clamp an anti-aliasing level to what the device supports
    ///
    /// \param level  Requested sample count
    /// \param format Format the samples will be used with
    ///
    /// \return Highest supported sample count not greater than the request, at least 1
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] unsigned int clampAntiAliasingLevel(unsigned int level, DXGI_FORMAT format) const;

    ////////////////////////////////////////////////////////////
    /// \brief Bind a presentation surface as the current output
    ///
    /// The views stay bound until another surface is bound; they
    /// must stay alive until then or until unbindSurface is called.
    ///
    /// \param renderTargetView Color output view
    /// \param depthStencilView Depth-stencil view, can be null
    ///
    ////////////////////////////////////////////////////////////
    void bindSurface(ID3D11RenderTargetView* renderTargetView, ID3D11DepthStencilView* depthStencilView);

    ////////////////////////////////////////////////////////////
    /// \brief Unbind a surface if it is the current output
    ///
    /// \param renderTargetView Color output view to unbind
    ///
    ////////////////////////////////////////////////////////////
    void unbindSurface(ID3D11RenderTargetView* renderTargetView);

    ////////////////////////////////////////////////////////////
    /// \brief Get the currently bound color output view
    ///
    /// \return Current view, null if no surface is bound
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11RenderTargetView* getCurrentRenderTargetView() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the currently bound depth-stencil view
    ///
    /// \return Current view, can be null
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11DepthStencilView* getCurrentDepthStencilView() const;

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
    /// \brief Remember the texture view and sampler bound for the current draw
    ///
    /// Shaders use these to resolve their CurrentTexture uniform
    /// and any texture they declare without assigning one, which
    /// samples the draw's texture like an unset GLSL sampler does.
    ///
    /// \param view    View of the texture bound to the built-in texture slot
    /// \param sampler Sampler bound alongside the texture
    ///
    ////////////////////////////////////////////////////////////
    void setCurrentTextureView(ID3D11ShaderResourceView* view, ID3D11SamplerState* sampler);

    ////////////////////////////////////////////////////////////
    /// \brief Get the texture view bound for the current draw
    ///
    /// \return View of the texture bound to the built-in texture slot
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11ShaderResourceView* getCurrentTextureView() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the sampler bound for the current draw
    ///
    /// \return Sampler bound alongside the current texture
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11SamplerState* getCurrentTextureSampler() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get or create the blend state for a blend mode
    ///
    /// \param mode       Blend mode to translate
    /// \param colorWrite Whether the color channels are written
    ///
    /// \return Blend state object, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11BlendState* getBlendState(const BlendMode& mode, bool colorWrite);

    ////////////////////////////////////////////////////////////
    /// \brief Get or create the depth-stencil state for a stencil mode
    ///
    /// \param mode Stencil mode to translate
    ///
    /// \return Depth-stencil state object, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11DepthStencilState* getDepthStencilState(const StencilMode& mode);

    ////////////////////////////////////////////////////////////
    /// \brief Get the rasterizer state
    ///
    /// \param scissorEnabled Whether scissor testing is enabled
    ///
    /// \return Rasterizer state object, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11RasterizerState* getRasterizerState(bool scissorEnabled);

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
    [[nodiscard]] ID3D11SamplerState* getSamplerState(bool smooth, bool repeated, bool mipmapped = false);

    ////////////////////////////////////////////////////////////
    /// \brief Get the built-in vertex shader replicating the fixed pipeline
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11VertexShader* getDefaultVertexShader() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the built-in pixel shader replicating the fixed pipeline
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11PixelShader* getDefaultPixelShader() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the input layout matching `sf::Vertex`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11InputLayout* getInputLayout() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the constant buffer holding the transform matrices
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11Buffer* getConstantBuffer() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the shader resource view of the 1x1 white texture
    ///
    /// Bound for draws without texture so a single pixel shader
    /// covers both the textured and untextured case.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11ShaderResourceView* getWhiteTextureView() const;

    ////////////////////////////////////////////////////////////
    /// \brief Upload vertices to the streaming vertex buffer
    ///
    /// \param vertices    Vertices to upload
    /// \param vertexCount Number of vertices to upload
    /// \param firstVertex Location of the first uploaded vertex in the buffer
    ///
    /// \return `true` if the upload succeeded
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool uploadVertices(const void* vertices, std::size_t vertexCount, std::size_t& firstVertex);

    ////////////////////////////////////////////////////////////
    /// \brief Get the streaming vertex buffer
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11Buffer* getStreamVertexBuffer() const;

    ////////////////////////////////////////////////////////////
    /// \brief Upload triangle-fan indices to the streaming index buffer
    ///
    /// Direct3D 11 has no triangle-fan topology, fans are drawn as
    /// an indexed triangle list instead.
    ///
    /// \param firstVertex Index of the fan's first vertex
    /// \param vertexCount Number of vertices in the fan
    /// \param firstIndex  Location of the first index in the buffer
    /// \param indexCount  Number of indices written
    ///
    /// \return `true` if the upload succeeded
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool uploadTriangleFanIndices(std::size_t  firstVertex,
                                                std::size_t  vertexCount,
                                                std::size_t& firstIndex,
                                                std::size_t& indexCount);

    ////////////////////////////////////////////////////////////
    /// \brief Get the streaming index buffer
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11Buffer* getStreamIndexBuffer() const;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Create the built-in pipeline objects
    ///
    ////////////////////////////////////////////////////////////
    void createPipeline();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    ComPtr<ID3D11Device>         m_device;         //!< Direct3D device, free-threaded
    ComPtr<ID3D11DeviceContext>  m_context;        //!< Immediate context, guarded by m_mutex
    ComPtr<ID3D11DeviceContext1> m_context1;       //!< 11.1 view of the immediate context, null on plain 11.0
    D3D_FEATURE_LEVEL            m_featureLevel{}; //!< Feature level the device was created with
    mutable std::recursive_mutex m_mutex;          //!< Serializes access to the immediate context and caches

    ID3D11RenderTargetView*   m_currentRenderTargetView{}; //!< Color view of the bound surface, not owned
    ID3D11DepthStencilView*   m_currentDepthStencilView{}; //!< Depth-stencil view of the bound surface, not owned
    std::uint64_t             m_currentRenderTargetId{};   //!< Id of the render target active on the device
    ID3D11ShaderResourceView* m_currentTextureView{};      //!< Texture view bound for the current draw, not owned
    ID3D11SamplerState*       m_currentTextureSampler{};   //!< Sampler bound for the current draw, not owned

    ComPtr<ID3D11VertexShader> m_defaultVertexShader; //!< Built-in vertex shader
    ComPtr<ID3D11PixelShader>  m_defaultPixelShader;  //!< Built-in pixel shader
    ComPtr<ID3D11InputLayout>  m_inputLayout;         //!< Input layout matching sf::Vertex
    ComPtr<ID3D11Buffer>       m_constantBuffer;      //!< Constant buffer holding the matrices

    ComPtr<ID3D11Texture2D>          m_whiteTexture;     //!< 1x1 white texture for untextured draws
    ComPtr<ID3D11ShaderResourceView> m_whiteTextureView; //!< View of the white texture

    ComPtr<ID3D11Buffer> m_streamVertexBuffer;         //!< Growable streaming vertex buffer
    std::size_t          m_streamVertexBufferSize{};   //!< Capacity of the streaming vertex buffer, in vertices
    std::size_t          m_streamVertexBufferCursor{}; //!< Append position in the streaming vertex buffer
    ComPtr<ID3D11Buffer> m_streamIndexBuffer;          //!< Growable streaming index buffer for triangle fans
    std::size_t          m_streamIndexBufferSize{};    //!< Capacity of the streaming index buffer, in indices
    std::size_t          m_streamIndexBufferCursor{};  //!< Append position in the streaming index buffer

    std::unordered_map<std::uint32_t, ComPtr<ID3D11BlendState>> m_blendStates; //!< Cache of blend state objects, keyed by packed blend mode
    std::unordered_map<std::uint32_t, ComPtr<ID3D11DepthStencilState>> m_depthStencilStates; //!< Cache of depth-stencil state objects, keyed by packed stencil mode
    std::array<ComPtr<ID3D11RasterizerState>, 2> m_rasterizerStates; //!< Rasterizer states, indexed by scissor enable
    std::array<ComPtr<ID3D11SamplerState>, 8> m_samplerStates; //!< Sampler states, indexed by smooth, repeat and mipmap
};

} // namespace sf::priv
