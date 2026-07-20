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
#include <SFML/Graphics/RenderTargetImpl.hpp>

#include <array>


namespace sf::priv
{
class D3D11GraphicsDevice;

////////////////////////////////////////////////////////////
/// \brief Direct3D 11 implementation of the render target pipeline
///
////////////////////////////////////////////////////////////
class D3D11RenderTargetImpl : public RenderTargetImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param device Device this render target renders through
    ///
    ////////////////////////////////////////////////////////////
    explicit D3D11RenderTargetImpl(D3D11GraphicsDevice& device);

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a render target is active for rendering
    ///
    /// \param id Unique id of the render target to check
    ///
    /// \return `true` if the render target with the given id is active
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isActive(std::uint64_t id) const override;

    ////////////////////////////////////////////////////////////
    /// \brief Mark a render target as active or inactive for rendering
    ///
    /// \param target Render target being activated
    /// \param id     Unique id of the render target
    /// \param active `true` to activate, `false` to deactivate
    ///
    ////////////////////////////////////////////////////////////
    void activate(RenderTarget& target, std::uint64_t id, bool active) override;

    ////////////////////////////////////////////////////////////
    /// \brief Clear the color buffer
    ///
    /// \param target Render target to clear
    /// \param color  Fill color
    ///
    ////////////////////////////////////////////////////////////
    void clear(RenderTarget& target, Color color) override;

    ////////////////////////////////////////////////////////////
    /// \brief Clear the stencil buffer
    ///
    /// \param target       Render target to clear
    /// \param stencilValue Stencil value to clear to
    ///
    ////////////////////////////////////////////////////////////
    void clearStencil(RenderTarget& target, StencilValue stencilValue) override;

    ////////////////////////////////////////////////////////////
    /// \brief Clear both the color and stencil buffers
    ///
    /// \param target       Render target to clear
    /// \param color        Fill color
    /// \param stencilValue Stencil value to clear to
    ///
    ////////////////////////////////////////////////////////////
    void clear(RenderTarget& target, Color color, StencilValue stencilValue) override;

    ////////////////////////////////////////////////////////////
    /// \brief Draw primitives defined by an array of vertices
    ///
    /// When `useVertexCache` is `true` the pre-transformed
    /// vertices are in the cache of the target, not in `vertices`.
    ///
    /// \param target         Render target to draw to
    /// \param vertices       Pointer to the vertices
    /// \param vertexCount    Number of vertices in the array
    /// \param type           Type of primitives to draw
    /// \param states         Render states to use for drawing
    /// \param useVertexCache Use the pre-transformed vertex cache of the target
    ///
    ////////////////////////////////////////////////////////////
    void draw(RenderTarget&       target,
              const Vertex*       vertices,
              std::size_t         vertexCount,
              PrimitiveType       type,
              const RenderStates& states,
              bool                useVertexCache) override;

    ////////////////////////////////////////////////////////////
    /// \brief Draw primitives defined by a vertex buffer
    ///
    /// \param target       Render target to draw to
    /// \param vertexBuffer Vertex buffer to draw
    /// \param firstVertex  Index of the first vertex to render
    /// \param vertexCount  Number of vertices to render
    /// \param states       Render states to use for drawing
    ///
    ////////////////////////////////////////////////////////////
    void draw(RenderTarget&       target,
              const VertexBuffer& vertexBuffer,
              std::size_t         firstVertex,
              std::size_t         vertexCount,
              const RenderStates& states) override;

    ////////////////////////////////////////////////////////////
    /// \brief Save the current backend state
    ///
    /// Backing of `sf::OpenGL::pushStates`, only meaningful
    /// for backends whose state can be observed by user code.
    ///
    /// \param target Render target whose state to save
    ///
    ////////////////////////////////////////////////////////////
    void pushStates(RenderTarget& target) override;

    ////////////////////////////////////////////////////////////
    /// \brief Restore the previously saved backend state
    ///
    /// \param target Render target whose state to restore
    ///
    ////////////////////////////////////////////////////////////
    void popStates(RenderTarget& target) override;

    ////////////////////////////////////////////////////////////
    /// \brief Reset the backend state to its default
    ///
    /// \param target Render target whose state to reset
    /// \param id     Unique id of the render target
    ///
    ////////////////////////////////////////////////////////////
    void resetStates(RenderTarget& target, std::uint64_t id) override;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Apply the current view of the target
    ///
    ////////////////////////////////////////////////////////////
    void applyCurrentView(RenderTarget& target);

    ////////////////////////////////////////////////////////////
    /// \brief Apply a new blending mode
    ///
    ////////////////////////////////////////////////////////////
    void applyBlendMode(RenderTarget& target, const BlendMode& mode, bool colorWrite);

    ////////////////////////////////////////////////////////////
    /// \brief Apply a new stencil mode
    ///
    ////////////////////////////////////////////////////////////
    void applyStencilMode(RenderTarget& target, const StencilMode& mode);

    ////////////////////////////////////////////////////////////
    /// \brief Apply a new texture
    ///
    ////////////////////////////////////////////////////////////
    void applyTexture(RenderTarget& target, const Texture* texture, CoordinateType coordinateType = CoordinateType::Pixels);

    ////////////////////////////////////////////////////////////
    /// \brief Apply a new shader
    ///
    ////////////////////////////////////////////////////////////
    void applyShader(const Shader* shader);

    ////////////////////////////////////////////////////////////
    /// \brief Setup environment for drawing
    ///
    ////////////////////////////////////////////////////////////
    void setupDraw(RenderTarget& target, bool useVertexCache, const RenderStates& states);

    ////////////////////////////////////////////////////////////
    /// \brief Draw primitives from the currently bound vertex buffer
    ///
    ////////////////////////////////////////////////////////////
    void drawPrimitives(PrimitiveType type, std::size_t firstVertex, std::size_t vertexCount);

    ////////////////////////////////////////////////////////////
    /// \brief Clean up environment after drawing
    ///
    ////////////////////////////////////////////////////////////
    void cleanupDraw(RenderTarget& target, const RenderStates& states);

    ////////////////////////////////////////////////////////////
    /// \brief Upload the matrices to the constant buffer
    ///
    ////////////////////////////////////////////////////////////
    void uploadConstants();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    D3D11GraphicsDevice& m_device; //!< Device this render target renders through

    std::array<float, 16> m_modelView;     //!< Current model-view matrix
    std::array<float, 16> m_projection;    //!< Current projection matrix
    std::array<float, 16> m_textureMatrix; //!< Current texture coordinate matrix

    bool m_lastColorWrite{true}; //!< Color write state of the currently bound blend state
};

} // namespace sf::priv
