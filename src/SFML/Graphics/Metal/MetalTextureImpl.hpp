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
#include <SFML/Graphics/Metal/MetalGraphicsDevice.hpp>
#include <SFML/Graphics/TextureImpl.hpp>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Metal implementation of the texture
///
/// Textures are created with a single mip level, the first
/// mipmap generation re-creates the texture with the full chain.
/// CPU updates are encoded as blits behind the draws already
/// recorded, unless the GPU is idle and a direct write is safe.
///
////////////////////////////////////////////////////////////
class MetalTextureImpl : public TextureImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param device Graphics device the texture lives on
    ///
    ////////////////////////////////////////////////////////////
    explicit MetalTextureImpl(MetalGraphicsDevice& device);

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
    /// \brief Get the Metal texture
    ///
    /// The returned object changes when a mipmap is generated for
    /// the first time, do not cache it across operations.
    ///
    /// \return The texture, null if none was created
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalTexturePtr getTexture() const;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Upload pixel rows to a region of the texture
    ///
    /// Writes directly while the GPU is idle, otherwise encodes a
    /// blit from staging memory so the upload lands behind the
    /// draws already recorded.
    ///
    /// \param pixels      First pixel row to upload
    /// \param bytesPerRow Stride between the rows, in bytes
    /// \param size        Width and height of the region
    /// \param dest        Coordinates of the destination position
    ///
    ////////////////////////////////////////////////////////////
    void uploadPixels(const std::uint8_t* pixels, std::size_t bytesPerRow, Vector2u size, Vector2u dest);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    MetalGraphicsDevice&    m_device;  //!< Graphics device the texture lives on
    NSPtr<MetalTexturePtr>  m_texture; //!< The Metal texture
};

} // namespace sf::priv
