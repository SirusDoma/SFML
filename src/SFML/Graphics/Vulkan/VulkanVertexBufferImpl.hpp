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
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Vulkan implementation of the vertex buffer
///
/// The buffer lives in device memory for every usage; updates
/// are recorded as transfers so they order correctly against the
/// draws already recorded, like updates do on the other backends.
///
////////////////////////////////////////////////////////////
class VulkanVertexBufferImpl : public VertexBufferImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param device Graphics device the buffer lives on
    ///
    ////////////////////////////////////////////////////////////
    explicit VulkanVertexBufferImpl(VulkanGraphicsDevice& device);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~VulkanVertexBufferImpl() override;

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
    /// \brief Get the backend buffer, used by the render pipeline
    ///
    /// \return The buffer, null if creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkBuffer getBuffer() const;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Prepare recording a transfer into the buffer
    ///
    /// Flushes draws reading the old contents and suspends the
    /// open render pass, transfers are illegal inside one.
    ///
    /// \return Command buffer to record the transfer into, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkCommandBuffer beginTransfer();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    VulkanGraphicsDevice& m_device;       //!< Graphics device the buffer lives on
    VkBuffer              m_buffer{};     //!< Backend buffer
    VmaAllocation         m_allocation{}; //!< Allocation backing the buffer
    std::size_t           m_capacity{};   //!< Vertices the buffer was created for, bounds every copy into it
};

} // namespace sf::priv
