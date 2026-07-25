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
#include <SFML/Graphics/TextureImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Vulkan implementation of the texture
///
////////////////////////////////////////////////////////////
class VulkanTextureImpl : public TextureImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param device Graphics device the texture lives on
    ///
    ////////////////////////////////////////////////////////////
    explicit VulkanTextureImpl(VulkanGraphicsDevice& device);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~VulkanTextureImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create or re-create the backend texture
    ///
    /// \param size       Requested texture size
    /// \param sRgb       Requested sRGB encoding, cleared if unsupported
    /// \param smooth     Current smooth filter state
    /// \param repeated   Current repeat mode
    /// \param actualSize Size actually allocated by the backend
    ///
    /// \return `true` if creation has been successful
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool create(Vector2u size, bool& sRgb, bool smooth, bool repeated, Vector2u& actualSize) override;

    ////////////////////////////////////////////////////////////
    /// \brief Update a part of the texture from an array of pixels
    ///
    /// \param pixels Array of pixels to copy to the texture
    /// \param size   Width and height of the pixel region
    /// \param dest   Coordinates of the destination position
    /// \param smooth Current smooth filter state
    ///
    ////////////////////////////////////////////////////////////
    void update(const std::uint8_t* pixels, Vector2u size, Vector2u dest, bool smooth) override;

    ////////////////////////////////////////////////////////////
    /// \brief Update a part of this texture from another texture, on the GPU
    ///
    /// \param source              Source texture implementation
    /// \param sourceSize          Size of the source texture
    /// \param sourcePixelsFlipped Whether the source pixels are flipped vertically
    /// \param dest                Coordinates of the destination position
    /// \param smooth              Current smooth filter state
    ///
    /// \return Whether the texture was updated, the update failed or a CPU fallback is required
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] UpdateResult update(const TextureImpl& source,
                                      Vector2u           sourceSize,
                                      bool               sourcePixelsFlipped,
                                      Vector2u           dest,
                                      bool               smooth) override;

    ////////////////////////////////////////////////////////////
    /// \brief Upload a sub-area of an image to the texture
    ///
    /// \param image     Source image
    /// \param rectangle Area of the image to upload, already clamped to the image bounds
    /// \param smooth    Current smooth filter state
    ///
    ////////////////////////////////////////////////////////////
    void update(const Image& image, const IntRect& rectangle, bool smooth) override;

    ////////////////////////////////////////////////////////////
    /// \brief Update the texture from the contents of a window
    ///
    /// \param window        Window to copy to the texture
    /// \param dest          Coordinates of the destination position
    /// \param smooth        Current smooth filter state
    /// \param pixelsFlipped Set to whether the copied pixels are flipped vertically
    ///
    /// \return `true` if the texture was updated
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool update(const Window& window, Vector2u dest, bool smooth, bool& pixelsFlipped) override;

    ////////////////////////////////////////////////////////////
    /// \brief Copy the texture pixels to an image
    ///
    /// \param size          Public texture size
    /// \param actualSize    Actual backend texture size
    /// \param pixelsFlipped Whether the pixels are flipped vertically
    ///
    /// \return Image containing the texture's pixels
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] Image copyToImage(Vector2u size, Vector2u actualSize, bool pixelsFlipped) const override;

    ////////////////////////////////////////////////////////////
    /// \brief Apply the smooth filter state to the backend texture
    ///
    /// \param smooth    New smooth filter state
    /// \param hasMipmap Whether the texture currently has a mipmap
    ///
    ////////////////////////////////////////////////////////////
    void setSmooth(bool smooth, bool hasMipmap) override;

    ////////////////////////////////////////////////////////////
    /// \brief Apply the repeat mode to the backend texture
    ///
    /// \param repeated New repeat mode
    ///
    ////////////////////////////////////////////////////////////
    void setRepeated(bool repeated) override;

    ////////////////////////////////////////////////////////////
    /// \brief Generate a mipmap for the texture
    ///
    /// \param smooth Current smooth filter state
    ///
    /// \return `true` if mipmap generation was successful
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool generateMipmap(bool smooth) override;

    ////////////////////////////////////////////////////////////
    /// \brief Invalidate the texture's mipmap
    ///
    /// \param smooth Current smooth filter state
    ///
    ////////////////////////////////////////////////////////////
    void invalidateMipmap(bool smooth) override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the underlying native handle of the texture
    ///
    /// \return Native handle of the texture, 0 if the backend has none
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] unsigned int getNativeHandle() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the backend image, used by the render pipeline
    ///
    /// \return The image, null if creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkImage getImage() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the view used to sample the texture, used by the render pipeline
    ///
    /// \return The view, null if creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkImageView getImageView() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the single-level view render textures attach to
    ///
    /// Framebuffer attachments must cover a single mip level, for
    /// mipmapped textures this view covers only the base level.
    ///
    /// \return The attachment view, null if creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkImageView getAttachmentView() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the format of the backend image
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkFormat getFormat() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the size the image was created with
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] Vector2u getSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Mark the texture as a render texture attachment
    ///
    /// Attachment textures live in the GENERAL layout so render
    /// passes and sampling can alternate without transitions.
    ///
    ////////////////////////////////////////////////////////////
    void setAttachment();

    ////////////////////////////////////////////////////////////
    /// \brief Get the layout the texture is sampled in
    ///
    /// \return The layout descriptors referencing the texture use
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkImageLayout getSampleLayout() const;

    ////////////////////////////////////////////////////////////
    /// \brief Make sure the texture is in its sampling layout
    ///
    /// Uploads leave the texture in the transfer layout, the
    /// transition to the sampling layout is recorded here.
    ///
    ////////////////////////////////////////////////////////////
    void prepareForSampling();

private:
    ////////////////////////////////////////////////////////////
    /// \brief Transition the image to a layout for transfer commands
    ///
    /// Ends the open render pass, transfer commands are illegal
    /// inside one, and flushes pending draws sampling the old
    /// contents.
    ///
    /// \param newLayout Layout to transition to
    ///
    /// \return Command buffer to record the transfer into, null on failure
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] VkCommandBuffer beginTransfer(VkImageLayout newLayout);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    VulkanGraphicsDevice& m_device;           //!< Graphics device the texture lives on
    VkImage               m_image{};          //!< Backend image
    VmaAllocation         m_allocation{};     //!< Allocation backing the image
    VkImageView           m_view{};           //!< View used to sample the image, covers every mip level
    VkImageView           m_attachmentView{}; //!< Base-level view for attaching, equals m_view without mipmaps
    VkFormat              m_format{VK_FORMAT_R8G8B8A8_UNORM}; //!< Format of the image
    Vector2u              m_size;             //!< Size the image was created with
    std::uint32_t         m_mipLevels{1};     //!< Mip levels the image was created with
    bool                  m_attachment{};     //!< Whether the texture is a render texture attachment
};

} // namespace sf::priv
