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
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/OpenGL/GLCheck.hpp>
#include <SFML/Graphics/OpenGL/GLExtensions.hpp>
#include <SFML/Graphics/OpenGL/GlTextureImpl.hpp>
#include <SFML/Graphics/OpenGL/TextureSaver.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <SFML/Window/Context.hpp>
#include <SFML/Window/Window.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <array>
#include <ostream>
#include <vector>

#include <cassert>
#include <cstddef>
#include <cstring>


namespace sf::priv
{
////////////////////////////////////////////////////////////
GlTextureImpl::~GlTextureImpl()
{
    // Destroy the OpenGL texture
    if (m_texture)
    {
        const TransientContextLock lock;

        const GLuint texture = m_texture;
        glCheck(glDeleteTextures(1, &texture));
    }

#ifndef NDEBUG
    // Set m_texture to an invalid value to help the assert and glIsTexture in bind detect trying
    // to bind this texture in cases where it has already been destroyed but its memory not yet deallocated
    m_texture = 0xFFFFFFFFu;
#endif
}


////////////////////////////////////////////////////////////
bool GlTextureImpl::create(Vector2u size, bool& sRgb, bool smooth, bool repeated, Vector2u& actualSize)
{
    const TransientContextLock lock;

    // Make sure that extensions are initialized
    ensureExtensionsInit();

    // Compute the internal texture dimensions depending on NPOT textures support
    actualSize = Vector2u(getValidSize(size.x), getValidSize(size.y));

    // Check the maximum texture size
    const unsigned int maxSize = getMaximumSize();
    if ((actualSize.x > maxSize) || (actualSize.y > maxSize))
    {
        err() << "Failed to create texture, its internal size is too high "
              << "(" << actualSize.x << "x" << actualSize.y << ", "
              << "maximum is " << maxSize << "x" << maxSize << ")" << std::endl;
        return false;
    }

    // Create the OpenGL texture if it doesn't exist yet
    if (!m_texture)
    {
        GLuint texture = 0;
        glCheck(glGenTextures(1, &texture));
        m_texture = texture;
    }

    // Make sure that the current texture binding will be preserved
    const TextureSaver save;

    static const bool textureEdgeClamp = GLEXT_texture_edge_clamp || GLEXT_GL_VERSION_1_2 ||
                                         Context::isExtensionAvailable("GL_EXT_texture_edge_clamp");

    if (!textureEdgeClamp)
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "OpenGL extension SGIS_texture_edge_clamp unavailable" << '\n'
                  << "Artifacts may occur along texture edges" << '\n'
                  << "Ensure that hardware acceleration is enabled if available" << std::endl;

            warned = true;
        }
    }

    static const bool textureSrgb = GLEXT_texture_sRGB;

    if (sRgb && !textureSrgb)
    {
        static bool warned = false;

        if (!warned)
        {
#ifndef SFML_OPENGL_ES
            err() << "OpenGL extension EXT_texture_sRGB unavailable" << '\n';
#else
            err() << "OpenGL ES extension EXT_sRGB unavailable" << '\n';
#endif
            err() << "Automatic sRGB to linear conversion disabled" << std::endl;

            warned = true;
        }

        sRgb = false;
    }

#ifndef SFML_OPENGL_ES
    const GLint textureWrapParam = repeated ? GL_REPEAT : (textureEdgeClamp ? GLEXT_GL_CLAMP_TO_EDGE : GLEXT_GL_CLAMP);
#else
    const GLint textureWrapParam = repeated ? GL_REPEAT : GLEXT_GL_CLAMP_TO_EDGE;
#endif

    // Initialize the texture
    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexImage2D(GL_TEXTURE_2D,
                         0,
                         (sRgb ? GLEXT_GL_SRGB8_ALPHA8 : GL_RGBA),
                         static_cast<GLsizei>(actualSize.x),
                         static_cast<GLsizei>(actualSize.y),
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         nullptr));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, textureWrapParam));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, textureWrapParam));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST));

    return true;
}


////////////////////////////////////////////////////////////
void GlTextureImpl::update(const std::uint8_t* pixels, Vector2u size, Vector2u dest, bool smooth)
{
    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const TextureSaver save;

    // Copy pixels from the given array to the texture
    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            static_cast<GLint>(dest.x),
                            static_cast<GLint>(dest.y),
                            static_cast<GLsizei>(size.x),
                            static_cast<GLsizei>(size.y),
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            pixels));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST));

    // Force an OpenGL flush, so that the texture data will appear updated
    // in all contexts immediately (solves problems in multi-threaded apps)
    glCheck(glFlush());
}


////////////////////////////////////////////////////////////
TextureImpl::UpdateResult GlTextureImpl::update(
    [[maybe_unused]] const TextureImpl& source,
    [[maybe_unused]] Vector2u           sourceSize,
    [[maybe_unused]] bool               sourcePixelsFlipped,
    [[maybe_unused]] Vector2u           dest,
    [[maybe_unused]] bool               smooth)
{
#ifndef SFML_OPENGL_ES

    {
        const TransientContextLock lock;

        // Make sure that extensions are initialized
        ensureExtensionsInit();
    }

    if (GLEXT_framebuffer_object && GLEXT_framebuffer_blit)
    {
        const auto& glSource = static_cast<const GlTextureImpl&>(source);

        const TransientContextLock lock;

        // Save the current bindings so we can restore them after we are done
        GLint readFramebuffer = 0;
        GLint drawFramebuffer = 0;

        glCheck(glGetIntegerv(GLEXT_GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer));
        glCheck(glGetIntegerv(GLEXT_GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer));

        // Create the framebuffers
        GLuint sourceFrameBuffer = 0;
        GLuint destFrameBuffer   = 0;
        glCheck(GLEXT_glGenFramebuffers(1, &sourceFrameBuffer));
        glCheck(GLEXT_glGenFramebuffers(1, &destFrameBuffer));

        if (!sourceFrameBuffer || !destFrameBuffer)
        {
            err() << "Cannot copy texture, failed to create a frame buffer object" << std::endl;
            return UpdateResult::Failed;
        }

        // Link the source texture to the source frame buffer
        glCheck(GLEXT_glBindFramebuffer(GLEXT_GL_READ_FRAMEBUFFER, sourceFrameBuffer));
        glCheck(GLEXT_glFramebufferTexture2D(GLEXT_GL_READ_FRAMEBUFFER,
                                             GLEXT_GL_COLOR_ATTACHMENT0,
                                             GL_TEXTURE_2D,
                                             glSource.m_texture,
                                             0));

        // Link the destination texture to the destination frame buffer
        glCheck(GLEXT_glBindFramebuffer(GLEXT_GL_DRAW_FRAMEBUFFER, destFrameBuffer));
        glCheck(
            GLEXT_glFramebufferTexture2D(GLEXT_GL_DRAW_FRAMEBUFFER, GLEXT_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0));

        // A final check, just to be sure...
        const GLenum sourceStatus = glCheck(GLEXT_glCheckFramebufferStatus(GLEXT_GL_READ_FRAMEBUFFER));

        const GLenum destStatus = glCheck(GLEXT_glCheckFramebufferStatus(GLEXT_GL_DRAW_FRAMEBUFFER));

        if ((sourceStatus == GLEXT_GL_FRAMEBUFFER_COMPLETE) && (destStatus == GLEXT_GL_FRAMEBUFFER_COMPLETE))
        {
            // Scissor testing affects framebuffer blits as well
            // Since we don't want scissor testing to interfere with our copying, we temporarily disable it for the blit if it is enabled
            GLboolean scissorEnabled = GL_FALSE;
            glCheck(glGetBooleanv(GL_SCISSOR_TEST, &scissorEnabled));

            if (scissorEnabled == GL_TRUE)
                glCheck(glDisable(GL_SCISSOR_TEST));

            // Blit the texture contents from the source to the destination texture
            glCheck(GLEXT_glBlitFramebuffer(0,
                                            sourcePixelsFlipped ? static_cast<GLint>(sourceSize.y) : 0,
                                            static_cast<GLint>(sourceSize.x),
                                            sourcePixelsFlipped ? 0 : static_cast<GLint>(sourceSize.y), // Source rectangle, flip y if source is flipped
                                            static_cast<GLint>(dest.x),
                                            static_cast<GLint>(dest.y),
                                            static_cast<GLint>(dest.x + sourceSize.x),
                                            static_cast<GLint>(dest.y + sourceSize.y), // Destination rectangle
                                            GL_COLOR_BUFFER_BIT,
                                            GL_NEAREST));

            // Re-enable scissor testing if it was previously enabled
            if (scissorEnabled == GL_TRUE)
                glCheck(glEnable(GL_SCISSOR_TEST));
        }
        else
        {
            err() << "Cannot copy texture, failed to link texture to frame buffer" << std::endl;
        }

        // Restore previously bound framebuffers
        glCheck(GLEXT_glBindFramebuffer(GLEXT_GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer)));
        glCheck(GLEXT_glBindFramebuffer(GLEXT_GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer)));

        // Delete the framebuffers
        glCheck(GLEXT_glDeleteFramebuffers(1, &sourceFrameBuffer));
        glCheck(GLEXT_glDeleteFramebuffers(1, &destFrameBuffer));

        // Make sure that the current texture binding will be preserved
        const TextureSaver save;

        // Set the parameters of this texture
        glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST));

        // Force an OpenGL flush, so that the texture data will appear updated
        // in all contexts immediately (solves problems in multi-threaded apps)
        glCheck(glFlush());

        return UpdateResult::Updated;
    }

#endif // SFML_OPENGL_ES

    return UpdateResult::Unsupported;
}


////////////////////////////////////////////////////////////
void GlTextureImpl::update(const Image& image, const IntRect& rectangle, bool smooth)
{
    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const TextureSaver save;

    const auto size = Vector2i(image.getSize());

    // Copy the pixels to the texture, row by row
    const std::uint8_t* pixels = image.getPixelsPtr() + 4 * (rectangle.position.x + (size.x * rectangle.position.y));
    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    for (int i = 0; i < rectangle.size.y; ++i)
    {
        glCheck(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, i, rectangle.size.x, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
        pixels += 4 * size.x;
    }

    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST));

    // Force an OpenGL flush, so that the texture will appear updated
    // in all contexts immediately (solves problems in multi-threaded apps)
    glCheck(glFlush());
}


////////////////////////////////////////////////////////////
bool GlTextureImpl::update(const Window& window, Vector2u dest, bool smooth, bool& pixelsFlipped)
{
    if (!window.setActive(true))
    {
        return false;
    }

    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const TextureSaver save;

    // Copy pixels from the back-buffer to the texture
    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glCopyTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                static_cast<GLint>(dest.x),
                                static_cast<GLint>(dest.y),
                                0,
                                0,
                                static_cast<GLsizei>(window.getSize().x),
                                static_cast<GLsizei>(window.getSize().y)));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST));

    // Force an OpenGL flush, so that the texture will appear updated
    // in all contexts immediately (solves problems in multi-threaded apps)
    glCheck(glFlush());

    // glCopyTexSubImage2D copies the back buffer bottom-up
    pixelsFlipped = true;

    return true;
}


////////////////////////////////////////////////////////////
Image GlTextureImpl::copyToImage(Vector2u size, Vector2u actualSize, bool pixelsFlipped) const
{
    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const TextureSaver save;

    // Create an array of pixels
    std::vector<std::uint8_t> pixels(size.x * size.y * 4);

#ifdef SFML_OPENGL_ES

    // OpenGL ES doesn't have the glGetTexImage function, the only way to read
    // from a texture is to bind it to a FBO and use glReadPixels
    GLuint frameBuffer = 0;
    glCheck(GLEXT_glGenFramebuffers(1, &frameBuffer));
    if (frameBuffer)
    {
        GLint previousFrameBuffer = 0;
        glCheck(glGetIntegerv(GLEXT_GL_FRAMEBUFFER_BINDING, &previousFrameBuffer));

        glCheck(GLEXT_glBindFramebuffer(GLEXT_GL_FRAMEBUFFER, frameBuffer));
        glCheck(GLEXT_glFramebufferTexture2D(GLEXT_GL_FRAMEBUFFER, GLEXT_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0));
        glCheck(glReadPixels(0,
                             0,
                             static_cast<GLsizei>(size.x),
                             static_cast<GLsizei>(size.y),
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             pixels.data()));
        glCheck(GLEXT_glDeleteFramebuffers(1, &frameBuffer));

        glCheck(GLEXT_glBindFramebuffer(GLEXT_GL_FRAMEBUFFER, static_cast<GLuint>(previousFrameBuffer)));

        if (pixelsFlipped)
        {
            // Flip the texture vertically
            const auto stride             = static_cast<std::ptrdiff_t>(size.x * 4);
            auto       currentRowIterator = pixels.begin();
            auto       nextRowIterator    = pixels.begin() + stride;
            auto       reverseRowIterator = pixels.begin() + (stride * static_cast<std::ptrdiff_t>(size.y - 1));
            for (unsigned int i = 0; i < size.y / 2; ++i)
            {
                std::swap_ranges(currentRowIterator, nextRowIterator, reverseRowIterator);
                currentRowIterator = nextRowIterator;
                nextRowIterator += stride;
                reverseRowIterator -= stride;
            }
        }
    }

#else

    if ((size == actualSize) && !pixelsFlipped)
    {
        // Texture is not padded nor flipped, we can use a direct copy
        glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
        glCheck(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data()));
    }
    else
    {
        // Texture is either padded or flipped, we have to use a slower algorithm

        // All the pixels will first be copied to a temporary array
        std::vector<std::uint8_t> allPixels(actualSize.x * actualSize.y * 4);
        glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
        glCheck(glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, allPixels.data()));

        // Then we copy the useful pixels from the temporary array to the final one
        const std::uint8_t* src      = allPixels.data();
        std::uint8_t*       dst      = pixels.data();
        int                 srcPitch = static_cast<int>(actualSize.x * 4);
        const unsigned int  dstPitch = size.x * 4;

        // Handle the case where source pixels are flipped vertically
        if (pixelsFlipped)
        {
            src += static_cast<unsigned int>(srcPitch * static_cast<int>(size.y - 1));
            srcPitch = -srcPitch;
        }

        for (unsigned int i = 0; i < size.y; ++i)
        {
            std::memcpy(dst, src, dstPitch);
            src += srcPitch;
            dst += dstPitch;
        }
    }

#endif // SFML_OPENGL_ES

    return {size, pixels.data()};
}


////////////////////////////////////////////////////////////
void GlTextureImpl::setSmooth(bool smooth, bool hasMipmap)
{
    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const TextureSaver save;

    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST));

    if (hasMipmap)
    {
        glCheck(glTexParameteri(GL_TEXTURE_2D,
                                GL_TEXTURE_MIN_FILTER,
                                smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR));
    }
    else
    {
        glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST));
    }
}


////////////////////////////////////////////////////////////
void GlTextureImpl::setRepeated(bool repeated)
{
    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const TextureSaver save;

    static const bool textureEdgeClamp = GLEXT_texture_edge_clamp;

    if (!repeated && !textureEdgeClamp)
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "OpenGL extension SGIS_texture_edge_clamp unavailable" << '\n'
                  << "Artifacts may occur along texture edges" << '\n'
                  << "Ensure that hardware acceleration is enabled if available" << std::endl;

            warned = true;
        }
    }

#ifndef SFML_OPENGL_ES
    const GLint textureWrapParam = repeated ? GL_REPEAT : (textureEdgeClamp ? GLEXT_GL_CLAMP_TO_EDGE : GLEXT_GL_CLAMP);
#else
    const GLint textureWrapParam = repeated ? GL_REPEAT : GLEXT_GL_CLAMP_TO_EDGE;
#endif

    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, textureWrapParam));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, textureWrapParam));
}


////////////////////////////////////////////////////////////
bool GlTextureImpl::generateMipmap(bool smooth)
{
    const TransientContextLock lock;

    // Make sure that extensions are initialized
    ensureExtensionsInit();

    if (!GLEXT_framebuffer_object)
        return false;

    // Make sure that the current texture binding will be preserved
    const TextureSaver save;

    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(GLEXT_glGenerateMipmap(GL_TEXTURE_2D));
    glCheck(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR));

    return true;
}


////////////////////////////////////////////////////////////
void GlTextureImpl::invalidateMipmap(bool smooth)
{
    const TransientContextLock lock;

    // Make sure that the current texture binding will be preserved
    const TextureSaver save;

    glCheck(glBindTexture(GL_TEXTURE_2D, m_texture));
    glCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST));
}


////////////////////////////////////////////////////////////
unsigned int GlTextureImpl::getNativeHandle() const
{
    return m_texture;
}


////////////////////////////////////////////////////////////
void GlTextureImpl::bind(const Texture* texture, CoordinateType coordinateType)
{
    const TransientContextLock lock;

    const auto* impl = texture ? static_cast<const GlTextureImpl*>(getImpl(*texture)) : nullptr;

    if (impl && impl->m_texture)
    {
        // When debugging, ensure that the texture name is valid
        assert((glIsTexture(impl->m_texture) == GL_TRUE) &&
               "Texture to be bound is invalid, check if the texture is still being used after it has been destroyed");

        // Bind the texture
        glCheck(glBindTexture(GL_TEXTURE_2D, impl->m_texture));

        const Vector2u size          = texture->getSize();
        const Vector2u actualSize    = getActualSize(*texture);
        const bool     pixelsFlipped = arePixelsFlipped(*texture);

        // Check if we need to define a special texture matrix
        if ((coordinateType == CoordinateType::Pixels) || pixelsFlipped ||
            ((coordinateType == CoordinateType::Normalized) && (size != actualSize)))
        {
            // clang-format off
            std::array matrix = {1.f, 0.f, 0.f, 0.f,
                                 0.f, 1.f, 0.f, 0.f,
                                 0.f, 0.f, 1.f, 0.f,
                                 0.f, 0.f, 0.f, 1.f};
            // clang-format on

            // If non-normalized coordinates (= pixels) are requested, we need to
            // setup scale factors that convert the range [0 .. size] to [0 .. 1]
            if (coordinateType == CoordinateType::Pixels)
            {
                matrix[0] = 1.f / static_cast<float>(actualSize.x);
                matrix[5] = 1.f / static_cast<float>(actualSize.y);
            }

            // If normalized coordinates are used when NPOT textures aren't supported,
            // then we need to setup scale factors to make the coordinates relative to the actual POT size
            if ((coordinateType == CoordinateType::Normalized) && (size != actualSize))
            {
                matrix[0] = static_cast<float>(size.x) / static_cast<float>(actualSize.x);
                matrix[5] = static_cast<float>(size.y) / static_cast<float>(actualSize.y);
            }

            // If pixels are flipped we must invert the Y axis
            if (pixelsFlipped)
            {
                matrix[5]  = -matrix[5];
                matrix[13] = static_cast<float>(size.y) / static_cast<float>(actualSize.y);
            }

            // Load the matrix
            glCheck(glMatrixMode(GL_TEXTURE));
            glCheck(glLoadMatrixf(matrix.data()));
        }
        else
        {
            // Reset the texture matrix
            glCheck(glMatrixMode(GL_TEXTURE));
            glCheck(glLoadIdentity());
        }

        // Go back to model-view mode (sf::RenderTarget relies on it)
        glCheck(glMatrixMode(GL_MODELVIEW));
    }
    else
    {
        // Bind no texture
        glCheck(glBindTexture(GL_TEXTURE_2D, 0));

        // Reset the texture matrix
        glCheck(glMatrixMode(GL_TEXTURE));
        glCheck(glLoadIdentity());

        // Go back to model-view mode (sf::RenderTarget relies on it)
        glCheck(glMatrixMode(GL_MODELVIEW));
    }
}


////////////////////////////////////////////////////////////
unsigned int GlTextureImpl::getMaximumSize()
{
    static const unsigned int size = []
    {
        const TransientContextLock transientLock;

        GLint value = 0;

        // Make sure that extensions are initialized
        ensureExtensionsInit();

        glCheck(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value));

        return static_cast<unsigned int>(value);
    }();

    return size;
}


////////////////////////////////////////////////////////////
unsigned int GlTextureImpl::getValidSize(unsigned int size)
{
    if (GLEXT_texture_non_power_of_two)
    {
        // If hardware supports NPOT textures, then just return the unmodified size
        return size;
    }

    // If hardware doesn't support NPOT textures, we calculate the nearest power of two
    unsigned int powerOfTwo = 1;
    while (powerOfTwo < size)
        powerOfTwo *= 2;

    return powerOfTwo;
}

} // namespace sf::priv
