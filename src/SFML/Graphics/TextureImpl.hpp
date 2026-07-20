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
#include <SFML/Graphics/Rect.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>


namespace sf
{
class Image;
class Window;

namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Abstract base class for texture implementations
///
/// The texture implementation owns the backend texture object.
/// All bookkeeping state (sizes, flags, cache id) is kept by
/// `sf::Texture`; it is passed in where an operation needs it.
///
////////////////////////////////////////////////////////////
class TextureImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Result of a texture-to-texture update
    ///
    ////////////////////////////////////////////////////////////
    enum class UpdateResult
    {
        Updated,    //!< The texture was updated on the GPU
        Failed,     //!< The update failed, no fallback should be attempted
        Unsupported //!< GPU copies are not supported, fall back to a CPU copy
    };

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    TextureImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~TextureImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy constructor
    ///
    ////////////////////////////////////////////////////////////
    TextureImpl(const TextureImpl&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy assignment
    ///
    ////////////////////////////////////////////////////////////
    TextureImpl& operator=(const TextureImpl&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Create or re-create the backend texture
    ///
    /// The existing backend texture object is reused if one exists.
    ///
    /// \param size       Requested texture size
    /// \param sRgb       Requested sRGB encoding, cleared if unsupported
    /// \param smooth     Current smooth filter state
    /// \param repeated   Current repeat mode
    /// \param actualSize Size actually allocated by the backend (can be greater because of padding)
    ///
    /// \return `true` if creation has been successful
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual bool create(Vector2u size, bool& sRgb, bool smooth, bool repeated, Vector2u& actualSize) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Update a part of the texture from an array of pixels
    ///
    /// \param pixels Array of pixels to copy to the texture
    /// \param size   Width and height of the pixel region
    /// \param dest   Coordinates of the destination position
    /// \param smooth Current smooth filter state
    ///
    ////////////////////////////////////////////////////////////
    virtual void update(const std::uint8_t* pixels, Vector2u size, Vector2u dest, bool smooth) = 0;

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
    [[nodiscard]] virtual UpdateResult update(const TextureImpl& source,
                                              Vector2u           sourceSize,
                                              bool               sourcePixelsFlipped,
                                              Vector2u           dest,
                                              bool               smooth) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Upload a sub-area of an image to the texture
    ///
    /// \param image     Source image
    /// \param rectangle Area of the image to upload, already clamped to the image bounds
    /// \param smooth    Current smooth filter state
    ///
    ////////////////////////////////////////////////////////////
    virtual void update(const Image& image, const IntRect& rectangle, bool smooth) = 0;

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
    [[nodiscard]] virtual bool update(const Window& window, Vector2u dest, bool smooth, bool& pixelsFlipped) = 0;

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
    [[nodiscard]] virtual Image copyToImage(Vector2u size, Vector2u actualSize, bool pixelsFlipped) const = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Apply the smooth filter state to the backend texture
    ///
    /// \param smooth    New smooth filter state
    /// \param hasMipmap Whether the texture currently has a mipmap
    ///
    ////////////////////////////////////////////////////////////
    virtual void setSmooth(bool smooth, bool hasMipmap) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Apply the repeat mode to the backend texture
    ///
    /// \param repeated New repeat mode
    ///
    ////////////////////////////////////////////////////////////
    virtual void setRepeated(bool repeated) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Generate a mipmap for the texture
    ///
    /// \param smooth Current smooth filter state
    ///
    /// \return `true` if mipmap generation was successful
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual bool generateMipmap(bool smooth) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Invalidate the texture's mipmap
    ///
    /// \param smooth Current smooth filter state
    ///
    ////////////////////////////////////////////////////////////
    virtual void invalidateMipmap(bool smooth) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Get the underlying native handle of the texture
    ///
    /// \return Native handle of the texture, 0 if the backend has none
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual unsigned int getNativeHandle() const = 0;
};

} // namespace priv

} // namespace sf
