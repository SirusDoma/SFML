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
#include <SFML/Graphics/D3D11/D3D11GraphicsDevice.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/VertexBufferImpl.hpp>

#include <vector>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Direct3D 11 implementation of the vertex buffer
///
////////////////////////////////////////////////////////////
class D3D11VertexBufferImpl : public VertexBufferImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param device Graphics device the buffer lives on
    ///
    ////////////////////////////////////////////////////////////
    explicit D3D11VertexBufferImpl(D3D11GraphicsDevice& device);

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
    /// \brief Get the backend buffer object, used by the render pipeline
    ///
    /// \return Pointer to the buffer, null if creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ID3D11Buffer* getBuffer() const;

private:
    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    D3D11GraphicsDevice& m_device;    //!< Graphics device the buffer lives on
    ComPtr<ID3D11Buffer> m_buffer;    //!< Backend buffer object
    std::vector<Vertex>  m_shadow;    //!< CPU copy of dynamic buffers, allows partial updates through discard maps
    bool                 m_dynamic{}; //!< Whether the buffer uses dynamic memory
};

} // namespace sf::priv
