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
#include <SFML/Graphics/VertexBufferImpl.hpp>

#include <SFML/Window/GlResource.hpp>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief OpenGL implementation of the vertex buffer
///
////////////////////////////////////////////////////////////
class GlVertexBufferImpl : public VertexBufferImpl, GlResource
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    GlVertexBufferImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~GlVertexBufferImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create or re-create the backend buffer
    ///
    /// The existing backend buffer object is reused if one exists.
    ///
    /// \param vertexCount Number of vertices worth of memory to allocate
    /// \param usage       Usage specifier
    ///
    /// \return `true` if creation was successful
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool create(std::size_t vertexCount, VertexBuffer::Usage usage) override;

    ////////////////////////////////////////////////////////////
    /// \brief Update a part of the buffer from an array of vertices
    ///
    /// \param vertices    Array of vertices to copy to the buffer
    /// \param vertexCount Number of vertices to copy
    /// \param offset      Offset in the buffer to copy to
    /// \param size        Current buffer size, updated if the buffer had to grow
    /// \param usage       Usage specifier
    ///
    /// \return `true` if the update was successful
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool update(const Vertex*       vertices,
                              std::size_t         vertexCount,
                              unsigned int        offset,
                              std::size_t&        size,
                              VertexBuffer::Usage usage) override;

    ////////////////////////////////////////////////////////////
    /// \brief Copy the contents of another buffer into this buffer
    ///
    /// \param other     Buffer to copy from
    /// \param otherSize Vertex count of the source buffer
    /// \param usage     Usage specifier of this buffer
    ///
    /// \return `true` if the copy was successful
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool update(const VertexBufferImpl& other, std::size_t otherSize, VertexBuffer::Usage usage) override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the underlying native handle of the buffer
    ///
    /// \return Native handle of the buffer, 0 if the backend has none
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] unsigned int getNativeHandle() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Bind a vertex buffer for rendering
    ///
    /// \param vertexBuffer Pointer to the vertex buffer to bind, can be null to use no vertex buffer
    ///
    ////////////////////////////////////////////////////////////
    static void bind(const VertexBuffer* vertexBuffer);

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether or not the system supports vertex buffers
    ///
    /// \return `true` if vertex buffers are supported, `false` otherwise
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static bool isAvailable();

private:
    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    unsigned int m_buffer{}; //!< Internal OpenGL buffer identifier
};

} // namespace sf::priv
