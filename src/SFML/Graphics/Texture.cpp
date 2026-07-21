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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/GraphicsDevice.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/TextureImpl.hpp>

#include <SFML/Window/Window.hpp>

#include <SFML/System/Err.hpp>
#include <SFML/System/Exception.hpp>

#include <algorithm>
#include <atomic>
#include <ostream>
#include <utility>

#include <cassert>
#include <cstddef>


namespace
{
// A nested named namespace is used here to allow unity builds of SFML.
namespace TextureImpl
{
// Thread-safe unique identifier generator,
// is used for states cache (see RenderTarget)
std::uint64_t getUniqueId() noexcept
{
    static std::atomic<std::uint64_t> id(1); // start at 1, zero is "no texture"

    return id.fetch_add(1);
}
} // namespace TextureImpl
} // namespace


namespace sf
{
////////////////////////////////////////////////////////////
Texture::Texture() : m_device(priv::ensureGraphicsDevice()), m_cacheId(TextureImpl::getUniqueId())
{
}


////////////////////////////////////////////////////////////
Texture::Texture(const std::filesystem::path& filename, bool sRgb) : Texture()
{
    if (!loadFromFile(filename, sRgb))
        throw Exception("Failed to load texture from file");
}


////////////////////////////////////////////////////////////
Texture::Texture(const std::filesystem::path& filename, bool sRgb, const IntRect& area) : Texture()
{
    if (!loadFromFile(filename, sRgb, area))
        throw Exception("Failed to load texture from file");
}


////////////////////////////////////////////////////////////
Texture::Texture(const void* data, std::size_t size, bool sRgb) : Texture()
{
    if (!loadFromMemory(data, size, sRgb))
        throw Exception("Failed to load texture from memory");
}


////////////////////////////////////////////////////////////
Texture::Texture(const void* data, std::size_t size, bool sRgb, const IntRect& area) : Texture()
{
    if (!loadFromMemory(data, size, sRgb, area))
        throw Exception("Failed to load texture from memory");
}


////////////////////////////////////////////////////////////
Texture::Texture(InputStream& stream, bool sRgb) : Texture()
{
    if (!loadFromStream(stream, sRgb))
        throw Exception("Failed to load texture from stream");
}


////////////////////////////////////////////////////////////
Texture::Texture(InputStream& stream, bool sRgb, const IntRect& area) : Texture()
{
    if (!loadFromStream(stream, sRgb, area))
        throw Exception("Failed to load texture from stream");
}


////////////////////////////////////////////////////////////
Texture::Texture(const Image& image, bool sRgb) : Texture()
{
    if (!loadFromImage(image, sRgb))
        throw Exception("Failed to load texture from image");
}


////////////////////////////////////////////////////////////
Texture::Texture(const Image& image, bool sRgb, const IntRect& area) : Texture()
{
    if (!loadFromImage(image, sRgb, area))
        throw Exception("Failed to load texture from image");
}


////////////////////////////////////////////////////////////
Texture::Texture(Vector2u size, bool sRgb) : Texture()
{
    if (!resize(size, sRgb))
        throw Exception("Failed to create texture");
}


////////////////////////////////////////////////////////////
Texture::Texture(const Texture& copy) :
    m_device(copy.m_device),
    m_isSmooth(copy.m_isSmooth),
    m_sRgb(copy.m_sRgb),
    m_isRepeated(copy.m_isRepeated),
    m_cacheId(TextureImpl::getUniqueId())
{
    if (!copy.m_impl)
    {
        return;
    }

    if (resize(copy.getSize(), copy.isSrgb()))
    {
        update(copy);
    }
    else
    {
        err() << "Failed to copy texture, failed to resize texture" << std::endl;
    }
}


////////////////////////////////////////////////////////////
Texture::~Texture()
{
#ifndef NDEBUG
    // Set m_cacheId to an invalid value to help detect trying to use this texture in cases
    // where it has already been destroyed but its memory not yet deallocated (the
    // implementation poisons its texture handle the same way for the assert in bind)
    m_cacheId = 0xFFFFFFFFFFFFFFFFull;
#endif
}

////////////////////////////////////////////////////////////
Texture::Texture(Texture&& right) noexcept :
    // The device is copied rather than moved so that the moved-from texture can still be resized
    m_device(right.m_device),
    m_impl(std::move(right.m_impl)),
    m_size(std::exchange(right.m_size, {})),
    m_actualSize(std::exchange(right.m_actualSize, {})),
    m_isSmooth(std::exchange(right.m_isSmooth, false)),
    m_sRgb(std::exchange(right.m_sRgb, false)),
    m_isRepeated(std::exchange(right.m_isRepeated, false)),
    m_pixelsFlipped(std::exchange(right.m_pixelsFlipped, false)),
    m_renderTextureAttachment(std::exchange(right.m_renderTextureAttachment, false)),
    m_hasMipmap(std::exchange(right.m_hasMipmap, false)),
    m_cacheId(std::exchange(right.m_cacheId, 0))
{
}

////////////////////////////////////////////////////////////
Texture& Texture::operator=(Texture&& right) noexcept
{
    // Catch self-moving.
    if (&right == this)
    {
        return *this;
    }

    // Move old to new, destroying our previous backend texture in the process.
    m_device                  = right.m_device;
    m_impl                    = std::move(right.m_impl);
    m_size                    = std::exchange(right.m_size, {});
    m_actualSize              = std::exchange(right.m_actualSize, {});
    m_isSmooth                = std::exchange(right.m_isSmooth, false);
    m_sRgb                    = std::exchange(right.m_sRgb, false);
    m_isRepeated              = std::exchange(right.m_isRepeated, false);
    m_pixelsFlipped           = std::exchange(right.m_pixelsFlipped, false);
    m_renderTextureAttachment = std::exchange(right.m_renderTextureAttachment, false);
    m_hasMipmap               = std::exchange(right.m_hasMipmap, false);
    m_cacheId                 = std::exchange(right.m_cacheId, 0);
    return *this;
}


////////////////////////////////////////////////////////////
bool Texture::resize(Vector2u size, bool sRgb)
{
    // Check if texture parameters are valid before creating it
    if ((size.x == 0) || (size.y == 0))
    {
        err() << "Failed to resize texture, invalid size (" << size.x << "x" << size.y << ")" << std::endl;
        return false;
    }

    const bool hadImpl = (m_impl != nullptr);

    if (!hadImpl)
        m_impl = m_device->createTextureImpl();

    Vector2u actualSize;
    if (!m_impl->create(size, sRgb, m_isSmooth, m_isRepeated, actualSize))
    {
        // Don't leave an implementation without a backend texture behind
        if (!hadImpl)
            m_impl.reset();

        // Error message generated in called function.
        return false;
    }

    // All the validity checks passed, we can store the new texture settings
    m_size                    = size;
    m_actualSize              = actualSize;
    m_sRgb                    = sRgb;
    m_pixelsFlipped           = false;
    m_renderTextureAttachment = false;
    m_cacheId                 = TextureImpl::getUniqueId();
    m_hasMipmap               = false;

    return true;
}


////////////////////////////////////////////////////////////
bool Texture::loadFromFile(const std::filesystem::path& filename, bool sRgb, const IntRect& area)
{
    Image image;
    return image.loadFromFile(filename) && loadFromImage(image, sRgb, area);
}


////////////////////////////////////////////////////////////
bool Texture::loadFromMemory(const void* data, std::size_t size, bool sRgb, const IntRect& area)
{
    Image image;
    return image.loadFromMemory(data, size) && loadFromImage(image, sRgb, area);
}


////////////////////////////////////////////////////////////
bool Texture::loadFromStream(InputStream& stream, bool sRgb, const IntRect& area)
{
    Image image;
    return image.loadFromStream(stream) && loadFromImage(image, sRgb, area);
}


////////////////////////////////////////////////////////////
bool Texture::loadFromImage(const Image& image, bool sRgb, const IntRect& area)
{
    // Retrieve the image size
    const auto size = Vector2i(image.getSize());

    // Load the entire image if the source area is either empty or contains the whole image
    if (area.size.x == 0 || (area.size.y == 0) ||
        ((area.position.x <= 0) && (area.position.y <= 0) && (area.size.x >= size.x) && (area.size.y >= size.y)))
    {
        // Load the entire image
        if (resize(image.getSize(), sRgb))
        {
            update(image);
            return true;
        }

        // Error message generated in called function.
        return false;
    }

    // Load a sub-area of the image
    assert(area.size.x > 0 && "Area size x cannot be negative");
    assert(area.size.y > 0 && "Area size y cannot be negative");
    assert(area.position.x < size.x && "Area position x is out of image bounds");
    assert(area.position.y < size.y && "Area position y is out of image bounds");

    // Adjust the rectangle to the size of the image
    IntRect rectangle    = area;
    rectangle.position.x = std::max(rectangle.position.x, 0);
    rectangle.position.y = std::max(rectangle.position.y, 0);
    rectangle.size.x     = std::min(rectangle.size.x, size.x - rectangle.position.x);
    rectangle.size.y     = std::min(rectangle.size.y, size.y - rectangle.position.y);

    // Create the texture and upload the pixels
    if (resize(Vector2u(rectangle.size), sRgb))
    {
        m_impl->update(image, rectangle, m_isSmooth);
        m_hasMipmap = false;

        return true;
    }

    // Error message generated in called function.
    return false;
}


////////////////////////////////////////////////////////////
Vector2u Texture::getSize() const
{
    return m_size;
}


////////////////////////////////////////////////////////////
Image Texture::copyToImage() const
{
    // Easy case: empty texture
    if (!m_impl)
        return {};

    return m_impl->copyToImage(m_size, m_actualSize, m_pixelsFlipped);
}


////////////////////////////////////////////////////////////
void Texture::update(const std::uint8_t* pixels)
{
    // Update the whole texture
    update(pixels, m_size, {0, 0});
}


////////////////////////////////////////////////////////////
void Texture::update(const std::uint8_t* pixels, Vector2u size, Vector2u dest)
{
    assert(dest.x + size.x <= m_size.x && "Destination x coordinate is outside of texture");
    assert(dest.y + size.y <= m_size.y && "Destination y coordinate is outside of texture");

    if (!pixels || !m_impl)
    {
        return;
    }

    m_impl->update(pixels, size, dest, m_isSmooth);

    m_hasMipmap     = false;
    m_pixelsFlipped = false;
    m_cacheId       = TextureImpl::getUniqueId();
}


////////////////////////////////////////////////////////////
void Texture::update(const Texture& texture)
{
    // Update the whole texture
    update(texture, {0, 0});
}


////////////////////////////////////////////////////////////
void Texture::update(const Texture& texture, Vector2u dest)
{
    assert(dest.x + texture.m_size.x <= m_size.x && "Destination x coordinate is outside of texture");
    assert(dest.y + texture.m_size.y <= m_size.y && "Destination y coordinate is outside of texture");

    if (!m_impl || !texture.m_impl)
        return;

    switch (m_impl->update(*texture.m_impl, texture.m_size, texture.m_pixelsFlipped, dest, m_isSmooth))
    {
        case priv::TextureImpl::UpdateResult::Updated:
            m_hasMipmap     = false;
            m_pixelsFlipped = false;
            m_cacheId       = TextureImpl::getUniqueId();
            return;

        case priv::TextureImpl::UpdateResult::Failed:
            return;

        case priv::TextureImpl::UpdateResult::Unsupported:
            break;
    }

    update(texture.copyToImage(), dest);
}


////////////////////////////////////////////////////////////
void Texture::update(const Image& image)
{
    // Update the whole texture
    update(image.getPixelsPtr(), image.getSize(), {0, 0});
}


////////////////////////////////////////////////////////////
void Texture::update(const Image& image, Vector2u dest)
{
    update(image.getPixelsPtr(), image.getSize(), dest);
}


////////////////////////////////////////////////////////////
void Texture::update(const Window& window)
{
    update(window, {0, 0});
}


////////////////////////////////////////////////////////////
void Texture::update(const Window& window, Vector2u dest)
{
    assert(dest.x + window.getSize().x <= m_size.x && "Destination x coordinate is outside of texture");
    assert(dest.y + window.getSize().y <= m_size.y && "Destination y coordinate is outside of texture");

    if (!m_impl)
    {
        return;
    }

    bool pixelsFlipped = false;
    if (!m_impl->update(window, dest, m_isSmooth, pixelsFlipped))
    {
        return;
    }

    m_hasMipmap     = false;
    m_pixelsFlipped = pixelsFlipped;
    m_cacheId       = TextureImpl::getUniqueId();
}


////////////////////////////////////////////////////////////
void Texture::setSmooth(bool smooth)
{
    if (smooth == m_isSmooth)
    {
        return;
    }

    m_isSmooth = smooth;

    if (!m_impl)
    {
        return;
    }

    m_impl->setSmooth(m_isSmooth, m_hasMipmap);
}


////////////////////////////////////////////////////////////
bool Texture::isSmooth() const
{
    return m_isSmooth;
}


////////////////////////////////////////////////////////////
bool Texture::isSrgb() const
{
    return m_sRgb;
}


////////////////////////////////////////////////////////////
void Texture::setRepeated(bool repeated)
{
    if (repeated == m_isRepeated)
    {
        return;
    }

    m_isRepeated = repeated;

    if (!m_impl)
    {
        return;
    }

    m_impl->setRepeated(m_isRepeated);
}


////////////////////////////////////////////////////////////
bool Texture::isRepeated() const
{
    return m_isRepeated;
}


////////////////////////////////////////////////////////////
bool Texture::generateMipmap()
{
    if (!m_impl)
        return false;

    if (!m_impl->generateMipmap(m_isSmooth))
        return false;

    m_hasMipmap = true;

    // Invalidate the render target cache so backends that select their sampler
    // at bind time pick up the mipmap
    m_cacheId = TextureImpl::getUniqueId();

    return true;
}


////////////////////////////////////////////////////////////
void Texture::invalidateMipmap()
{
    if (!m_hasMipmap)
        return;

    m_impl->invalidateMipmap(m_isSmooth);

    m_hasMipmap = false;
    m_cacheId   = TextureImpl::getUniqueId();
}


////////////////////////////////////////////////////////////
unsigned int Texture::getMaximumSize()
{
    return priv::ensureGraphicsDevice()->getMaximumTextureSize();
}


////////////////////////////////////////////////////////////
Texture& Texture::operator=(const Texture& right)
{
    Texture temp(right);

    swap(temp);

    return *this;
}


////////////////////////////////////////////////////////////
void Texture::swap(Texture& right) noexcept
{
    std::swap(m_device, right.m_device);
    std::swap(m_impl, right.m_impl);
    std::swap(m_size, right.m_size);
    std::swap(m_actualSize, right.m_actualSize);
    std::swap(m_isSmooth, right.m_isSmooth);
    std::swap(m_sRgb, right.m_sRgb);
    std::swap(m_isRepeated, right.m_isRepeated);
    std::swap(m_pixelsFlipped, right.m_pixelsFlipped);
    std::swap(m_renderTextureAttachment, right.m_renderTextureAttachment);
    std::swap(m_hasMipmap, right.m_hasMipmap);
    std::swap(m_cacheId, right.m_cacheId);
}


////////////////////////////////////////////////////////////
unsigned int Texture::getNativeHandle() const
{
    return m_impl ? m_impl->getNativeHandle() : 0;
}


////////////////////////////////////////////////////////////
void swap(Texture& left, Texture& right) noexcept
{
    left.swap(right);
}

} // namespace sf
