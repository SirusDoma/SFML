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
#include <SFML/Graphics/VertexBuffer.hpp>

#include <cstddef>


namespace sf
{
struct Vertex;

namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Abstract base class for vertex buffer implementations
///
/// The vertex buffer implementation owns the backend buffer
/// object. The vertex count and usage are kept by
/// `sf::VertexBuffer` and passed in where an operation needs them.
///
////////////////////////////////////////////////////////////
class VertexBufferImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    VertexBufferImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~VertexBufferImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy constructor
    ///
    ////////////////////////////////////////////////////////////
    VertexBufferImpl(const VertexBufferImpl&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy assignment
    ///
    ////////////////////////////////////////////////////////////
    VertexBufferImpl& operator=(const VertexBufferImpl&) = delete;

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
    [[nodiscard]] virtual bool create(std::size_t vertexCount, VertexBuffer::Usage usage) = 0;

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
    [[nodiscard]] virtual bool update(const Vertex*       vertices,
                                      std::size_t         vertexCount,
                                      unsigned int        offset,
                                      std::size_t&        size,
                                      VertexBuffer::Usage usage) = 0;

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
    [[nodiscard]] virtual bool update(const VertexBufferImpl& other, std::size_t otherSize, VertexBuffer::Usage usage) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Get the underlying native handle of the buffer
    ///
    /// \return Native handle of the buffer, 0 if the backend has none
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual unsigned int getNativeHandle() const = 0;
};

} // namespace priv

} // namespace sf
