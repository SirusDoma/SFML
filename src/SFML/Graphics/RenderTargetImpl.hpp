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
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>

#include <cstddef>
#include <cstdint>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Abstract base class for render target implementations
///
/// The implementation drives the backend rendering pipeline of
/// one `sf::RenderTarget`. The render states cache and view math
/// are kept by `sf::RenderTarget`; implementations access the
/// cache through the protected accessors below.
///
////////////////////////////////////////////////////////////
class RenderTargetImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    RenderTargetImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~RenderTargetImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy constructor
    ///
    ////////////////////////////////////////////////////////////
    RenderTargetImpl(const RenderTargetImpl&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy assignment
    ///
    ////////////////////////////////////////////////////////////
    RenderTargetImpl& operator=(const RenderTargetImpl&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a render target is active for rendering
    ///
    /// \param id Unique id of the render target to check
    ///
    /// \return `true` if the render target with the given id is active
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual bool isActive(std::uint64_t id) const = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Mark a render target as active or inactive for rendering
    ///
    /// \param target Render target being activated
    /// \param id     Unique id of the render target
    /// \param active `true` to activate, `false` to deactivate
    ///
    ////////////////////////////////////////////////////////////
    virtual void activate(RenderTarget& target, std::uint64_t id, bool active) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Clear the color buffer
    ///
    /// \param target Render target to clear
    /// \param color  Fill color
    ///
    ////////////////////////////////////////////////////////////
    virtual void clear(RenderTarget& target, Color color) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Clear the stencil buffer
    ///
    /// \param target       Render target to clear
    /// \param stencilValue Stencil value to clear to
    ///
    ////////////////////////////////////////////////////////////
    virtual void clearStencil(RenderTarget& target, StencilValue stencilValue) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Clear both the color and stencil buffers
    ///
    /// \param target       Render target to clear
    /// \param color        Fill color
    /// \param stencilValue Stencil value to clear to
    ///
    ////////////////////////////////////////////////////////////
    virtual void clear(RenderTarget& target, Color color, StencilValue stencilValue) = 0;

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
    virtual void draw(RenderTarget&       target,
                      const Vertex*       vertices,
                      std::size_t         vertexCount,
                      PrimitiveType       type,
                      const RenderStates& states,
                      bool                useVertexCache) = 0;

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
    virtual void draw(RenderTarget&       target,
                      const VertexBuffer& vertexBuffer,
                      std::size_t         firstVertex,
                      std::size_t         vertexCount,
                      const RenderStates& states) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Save the current backend state
    ///
    /// Backing of `sf::OpenGL::pushStates`, only meaningful
    /// for backends whose state can be observed by user code.
    ///
    /// \param target Render target whose state to save
    ///
    ////////////////////////////////////////////////////////////
    virtual void pushStates(RenderTarget& target) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Restore the previously saved backend state
    ///
    /// \param target Render target whose state to restore
    ///
    ////////////////////////////////////////////////////////////
    virtual void popStates(RenderTarget& target) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Reset the backend state to its default
    ///
    /// \param target Render target whose state to reset
    /// \param id     Unique id of the render target
    ///
    ////////////////////////////////////////////////////////////
    virtual void resetStates(RenderTarget& target, std::uint64_t id) = 0;

protected:
    ////////////////////////////////////////////////////////////
    /// \brief Access the render states cache of a render target
    ///
    /// \param target Render target to access
    ///
    /// \return Reference to the render states cache
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static RenderTarget::StatesCache& getCache(RenderTarget& target)
    {
        return target.m_cache;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get the unique id of a render target
    ///
    /// \param target Render target to access
    ///
    /// \return Unique number identifying the render target
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static std::uint64_t getId(const RenderTarget& target)
    {
        return target.m_id;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get the backend implementation of a render target
    ///
    /// \param target Render target to access
    ///
    /// \return Pointer to the implementation, null if the target was moved away
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static RenderTargetImpl* getImpl(RenderTarget& target)
    {
        return target.m_impl.get();
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get the cache id of a texture
    ///
    /// \param texture Texture to access
    ///
    /// \return Unique number identifying the texture to the render target's cache
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static std::uint64_t getTextureCacheId(const Texture& texture)
    {
        return texture.m_cacheId;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a texture is the attachment of a render texture
    ///
    /// \param texture Texture to access
    ///
    /// \return `true` if the texture is a render texture attachment
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static bool isTextureAttachment(const Texture& texture)
    {
        return texture.m_renderTextureAttachment;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a texture has a generated mipmap
    ///
    /// \param texture Texture to access
    ///
    /// \return `true` if the texture currently has a mipmap
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static bool hasTextureMipmap(const Texture& texture)
    {
        return texture.m_hasMipmap;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get the backend implementation of a texture
    ///
    /// \param texture Texture to access
    ///
    /// \return Pointer to the implementation, null if no texture was created
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static TextureImpl* getTextureImpl(const Texture& texture)
    {
        return texture.m_impl.get();
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get the backend implementation of a shader
    ///
    /// \param shader Shader to access
    ///
    /// \return Pointer to the implementation, null if no shader was loaded
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static ShaderImpl* getShaderImpl(const Shader& shader)
    {
        return shader.m_impl.get();
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get the backend implementation of a vertex buffer
    ///
    /// \param vertexBuffer Vertex buffer to access
    ///
    /// \return Pointer to the implementation, null if no buffer was created
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static VertexBufferImpl* getVertexBufferImpl(const VertexBuffer& vertexBuffer)
    {
        return vertexBuffer.m_impl.get();
    }
};

} // namespace sf::priv
