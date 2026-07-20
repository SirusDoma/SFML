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
#include <SFML/Graphics/OpenGL/GLCheck.hpp>
#include <SFML/Graphics/OpenGL/GLExtensions.hpp>
#include <SFML/Graphics/OpenGL/GlVertexBufferImpl.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <SFML/System/Err.hpp>

#include <ostream>

#include <cstring>


namespace
{
// A nested named namespace is used here to allow unity builds of SFML.
namespace GlVertexBufferImplImpl
{
GLenum usageToGlEnum(sf::VertexBuffer::Usage usage)
{
    switch (usage)
    {
        case sf::VertexBuffer::Usage::Static:
            return GLEXT_GL_STATIC_DRAW;
        case sf::VertexBuffer::Usage::Dynamic:
            return GLEXT_GL_DYNAMIC_DRAW;
        default:
            return GLEXT_GL_STREAM_DRAW;
    }
}
} // namespace GlVertexBufferImplImpl
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
GlVertexBufferImpl::~GlVertexBufferImpl()
{
    if (m_buffer)
    {
        const TransientContextLock contextLock;

        glCheck(GLEXT_glDeleteBuffers(1, &m_buffer));
    }
}


////////////////////////////////////////////////////////////
bool GlVertexBufferImpl::create(std::size_t vertexCount, VertexBuffer::Usage usage)
{
    const TransientContextLock contextLock;

    if (!m_buffer)
        glCheck(GLEXT_glGenBuffers(1, &m_buffer));

    if (!m_buffer)
    {
        err() << "Could not create vertex buffer, generation failed" << std::endl;
        return false;
    }

    glCheck(GLEXT_glBindBuffer(GLEXT_GL_ARRAY_BUFFER, m_buffer));
    glCheck(GLEXT_glBufferData(GLEXT_GL_ARRAY_BUFFER,
                               static_cast<GLsizeiptrARB>(sizeof(Vertex) * vertexCount),
                               nullptr,
                               GlVertexBufferImplImpl::usageToGlEnum(usage)));
    glCheck(GLEXT_glBindBuffer(GLEXT_GL_ARRAY_BUFFER, 0));

    return true;
}


////////////////////////////////////////////////////////////
bool GlVertexBufferImpl::update(const Vertex*       vertices,
                                std::size_t         vertexCount,
                                unsigned int        offset,
                                std::size_t&        size,
                                VertexBuffer::Usage usage)
{
    const TransientContextLock contextLock;

    glCheck(GLEXT_glBindBuffer(GLEXT_GL_ARRAY_BUFFER, m_buffer));

    // Check if we need to resize or orphan the buffer
    if (vertexCount >= size)
    {
        glCheck(GLEXT_glBufferData(GLEXT_GL_ARRAY_BUFFER,
                                   static_cast<GLsizeiptrARB>(sizeof(Vertex) * vertexCount),
                                   nullptr,
                                   GlVertexBufferImplImpl::usageToGlEnum(usage)));

        size = vertexCount;
    }

    glCheck(GLEXT_glBufferSubData(GLEXT_GL_ARRAY_BUFFER,
                                  static_cast<GLintptrARB>(sizeof(Vertex) * offset),
                                  static_cast<GLsizeiptrARB>(sizeof(Vertex) * vertexCount),
                                  vertices));

    glCheck(GLEXT_glBindBuffer(GLEXT_GL_ARRAY_BUFFER, 0));

    return true;
}


////////////////////////////////////////////////////////////
bool GlVertexBufferImpl::update([[maybe_unused]] const VertexBufferImpl& other,
                                [[maybe_unused]] std::size_t             otherSize,
                                [[maybe_unused]] VertexBuffer::Usage     usage)
{
#ifdef SFML_OPENGL_ES

    return false;

#else

    const auto& glOther = static_cast<const GlVertexBufferImpl&>(other);

    const TransientContextLock contextLock;

    // Make sure that extensions are initialized
    ensureExtensionsInit();

    if (GLEXT_copy_buffer)
    {
        glCheck(GLEXT_glBindBuffer(GLEXT_GL_COPY_READ_BUFFER, glOther.m_buffer));
        glCheck(GLEXT_glBindBuffer(GLEXT_GL_COPY_WRITE_BUFFER, m_buffer));

        glCheck(GLEXT_glCopyBufferSubData(GLEXT_GL_COPY_READ_BUFFER,
                                          GLEXT_GL_COPY_WRITE_BUFFER,
                                          0,
                                          0,
                                          static_cast<GLsizeiptr>(sizeof(Vertex) * otherSize)));

        glCheck(GLEXT_glBindBuffer(GLEXT_GL_COPY_WRITE_BUFFER, 0));
        glCheck(GLEXT_glBindBuffer(GLEXT_GL_COPY_READ_BUFFER, 0));

        return true;
    }

    glCheck(GLEXT_glBindBuffer(GLEXT_GL_ARRAY_BUFFER, m_buffer));
    glCheck(GLEXT_glBufferData(GLEXT_GL_ARRAY_BUFFER,
                               static_cast<GLsizeiptrARB>(sizeof(Vertex) * otherSize),
                               nullptr,
                               GlVertexBufferImplImpl::usageToGlEnum(usage)));

    void* const destination = glCheck(GLEXT_glMapBuffer(GLEXT_GL_ARRAY_BUFFER, GLEXT_GL_WRITE_ONLY));

    glCheck(GLEXT_glBindBuffer(GLEXT_GL_ARRAY_BUFFER, glOther.m_buffer));

    const void* const source = glCheck(GLEXT_glMapBuffer(GLEXT_GL_ARRAY_BUFFER, GLEXT_GL_READ_ONLY));

    std::memcpy(destination, source, sizeof(Vertex) * otherSize);

    const GLboolean sourceResult = glCheck(GLEXT_glUnmapBuffer(GLEXT_GL_ARRAY_BUFFER));

    glCheck(GLEXT_glBindBuffer(GLEXT_GL_ARRAY_BUFFER, m_buffer));

    const GLboolean destinationResult = glCheck(GLEXT_glUnmapBuffer(GLEXT_GL_ARRAY_BUFFER));

    glCheck(GLEXT_glBindBuffer(GLEXT_GL_ARRAY_BUFFER, 0));

    return (sourceResult == GL_TRUE) && (destinationResult == GL_TRUE);

#endif // SFML_OPENGL_ES
}


////////////////////////////////////////////////////////////
unsigned int GlVertexBufferImpl::getNativeHandle() const
{
    return m_buffer;
}


////////////////////////////////////////////////////////////
void GlVertexBufferImpl::bind(const VertexBuffer* vertexBuffer)
{
    if (!isAvailable())
        return;

    const TransientContextLock lock;

    glCheck(GLEXT_glBindBuffer(GLEXT_GL_ARRAY_BUFFER, vertexBuffer ? vertexBuffer->getNativeHandle() : 0));
}


////////////////////////////////////////////////////////////
bool GlVertexBufferImpl::isAvailable()
{
    static const bool available = []
    {
        const TransientContextLock contextLock;

        // Make sure that extensions are initialized
        ensureExtensionsInit();

        return GLEXT_vertex_buffer_object != 0;
    }();

    return available;
}

} // namespace sf::priv
