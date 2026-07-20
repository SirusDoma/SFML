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
#include <SFML/Graphics/OpenGL/GlShaderImpl.hpp>
#include <SFML/Graphics/OpenGL/GlTextureImpl.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <SFML/System/Err.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>

#include <array>
#include <iomanip>
#include <ostream>
#include <vector>

#include <cstddef>

#ifndef SFML_OPENGL_ES

#if defined(SFML_SYSTEM_MACOS) || defined(SFML_SYSTEM_IOS)

#define castToGlHandle(x)   reinterpret_cast<GLEXT_GLhandle>(std::ptrdiff_t{x})
#define castFromGlHandle(x) static_cast<unsigned int>(reinterpret_cast<std::ptrdiff_t>(x))

#else

#define castToGlHandle(x)   (x)
#define castFromGlHandle(x) (x)

#endif

namespace
{
// Retrieve the maximum number of texture units available
std::size_t getMaxTextureUnits()
{
    static const GLint maxUnits = []
    {
        GLint value = 0;
        glCheck(glGetIntegerv(GLEXT_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value));

        return value;
    }();

    return static_cast<std::size_t>(maxUnits);
}

// Transforms an array of 2D vectors into a contiguous array of scalars
std::vector<float> flatten(const sf::Vector2f* vectorArray, std::size_t length)
{
    const std::size_t vectorSize = 2;

    std::vector<float> contiguous(vectorSize * length);
    for (std::size_t i = 0; i < length; ++i)
    {
        contiguous[vectorSize * i]     = vectorArray[i].x;
        contiguous[vectorSize * i + 1] = vectorArray[i].y;
    }

    return contiguous;
}

// Transforms an array of 3D vectors into a contiguous array of scalars
std::vector<float> flatten(const sf::Vector3f* vectorArray, std::size_t length)
{
    const std::size_t vectorSize = 3;

    std::vector<float> contiguous(vectorSize * length);
    for (std::size_t i = 0; i < length; ++i)
    {
        contiguous[vectorSize * i]     = vectorArray[i].x;
        contiguous[vectorSize * i + 1] = vectorArray[i].y;
        contiguous[vectorSize * i + 2] = vectorArray[i].z;
    }

    return contiguous;
}

// Transforms an array of 4D vectors into a contiguous array of scalars
std::vector<float> flatten(const sf::Glsl::Vec4* vectorArray, std::size_t length)
{
    const std::size_t vectorSize = 4;

    std::vector<float> contiguous(vectorSize * length);
    for (std::size_t i = 0; i < length; ++i)
    {
        contiguous[vectorSize * i]     = vectorArray[i].x;
        contiguous[vectorSize * i + 1] = vectorArray[i].y;
        contiguous[vectorSize * i + 2] = vectorArray[i].z;
        contiguous[vectorSize * i + 3] = vectorArray[i].w;
    }

    return contiguous;
}
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
struct GlShaderImpl::UniformBinder
{
    ////////////////////////////////////////////////////////////
    /// \brief Constructor: set up state before uniform is set
    ///
    ////////////////////////////////////////////////////////////
    UniformBinder(GlShaderImpl& shader, const std::string& name) :
        currentProgram(castToGlHandle(shader.m_shaderProgram))
    {
        if (currentProgram)
        {
            // Enable program object
            savedProgram = glCheck(GLEXT_glGetHandle(GLEXT_GL_PROGRAM_OBJECT));
            if (currentProgram != savedProgram)
                glCheck(GLEXT_glUseProgramObject(currentProgram));

            // Store uniform location for further use outside constructor
            location = shader.getUniformLocation(name);
        }
    }

    ////////////////////////////////////////////////////////////
    /// \brief Destructor: restore state after uniform is set
    ///
    ////////////////////////////////////////////////////////////
    ~UniformBinder()
    {
        // Disable program object
        if (currentProgram && (currentProgram != savedProgram))
            glCheck(GLEXT_glUseProgramObject(savedProgram));
    }

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy constructor
    ///
    ////////////////////////////////////////////////////////////
    UniformBinder(const UniformBinder&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy assignment
    ///
    ////////////////////////////////////////////////////////////
    UniformBinder& operator=(const UniformBinder&) = delete;

    TransientContextLock lock;           //!< Lock to keep context active while uniform is bound
    GLEXT_GLhandle       savedProgram{}; //!< Handle to the previously active program object
    GLEXT_GLhandle       currentProgram; //!< Handle to the program object of the modified sf::Shader instance
    GLint                location{-1};   //!< Uniform location, used by the surrounding sf::Shader code
};


////////////////////////////////////////////////////////////
GlShaderImpl::~GlShaderImpl()
{
    const TransientContextLock lock;

    // Destroy effect program
    if (m_shaderProgram)
        glCheck(GLEXT_glDeleteObject(castToGlHandle(m_shaderProgram)));
}


////////////////////////////////////////////////////////////
bool GlShaderImpl::compile(std::string_view vertexShaderCode, std::string_view geometryShaderCode, std::string_view fragmentShaderCode)
{
    const TransientContextLock lock;

    // First make sure that we can use shaders
    if (!isAvailable())
    {
        err() << "Failed to create a shader: your system doesn't support shaders "
              << "(you should test Shader::isAvailable() before trying to use the Shader class)" << std::endl;
        return false;
    }

    // Make sure we can use geometry shaders
    if (!geometryShaderCode.empty() && !isGeometryAvailable())
    {
        err() << "Failed to create a shader: your system doesn't support geometry shaders "
              << "(you should test Shader::isGeometryAvailable() before trying to use geometry shaders)" << std::endl;
        return false;
    }

    // Create the program
    const GLEXT_GLhandle shaderProgram = glCheck(GLEXT_glCreateProgramObject());

    // Helper function for shader creation
    const auto createAndAttachShader =
        [shaderProgram](GLenum shaderType, const char* shaderTypeStr, std::string_view shaderCode)
    {
        // Create and compile the shader
        const GLEXT_GLhandle shader           = glCheck(GLEXT_glCreateShaderObject(shaderType));
        const GLcharARB*     sourceCode       = shaderCode.data();
        const auto           sourceCodeLength = static_cast<GLint>(shaderCode.length());
        glCheck(GLEXT_glShaderSource(shader, 1, &sourceCode, &sourceCodeLength));
        glCheck(GLEXT_glCompileShader(shader));

        // Check the compile log
        GLint success = 0;
        glCheck(GLEXT_glGetObjectParameteriv(shader, GLEXT_GL_OBJECT_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            std::array<char, 1024> log{};
            glCheck(GLEXT_glGetInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data()));
            err() << "Failed to compile " << shaderTypeStr << " shader:" << '\n' << log.data() << std::endl;
            glCheck(GLEXT_glDeleteObject(shader));
            glCheck(GLEXT_glDeleteObject(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(GLEXT_glAttachObject(shaderProgram, shader));
        glCheck(GLEXT_glDeleteObject(shader));
        return true;
    };

    // Create the vertex shader if needed
    if (!vertexShaderCode.empty())
        if (!createAndAttachShader(GLEXT_GL_VERTEX_SHADER, "vertex", vertexShaderCode))
            return false;

    // Create the geometry shader if needed
    if (!geometryShaderCode.empty())
        if (!createAndAttachShader(GLEXT_GL_GEOMETRY_SHADER, "geometry", geometryShaderCode))
            return false;

    // Create the fragment shader if needed
    if (!fragmentShaderCode.empty())
        if (!createAndAttachShader(GLEXT_GL_FRAGMENT_SHADER, "fragment", fragmentShaderCode))
            return false;

    // Link the program
    glCheck(GLEXT_glLinkProgram(shaderProgram));

    // Check the link log
    GLint success = 0;
    glCheck(GLEXT_glGetObjectParameteriv(shaderProgram, GLEXT_GL_OBJECT_LINK_STATUS, &success));
    if (success == GL_FALSE)
    {
        std::array<char, 1024> log{};
        glCheck(GLEXT_glGetInfoLog(shaderProgram, static_cast<GLsizei>(log.size()), nullptr, log.data()));
        err() << "Failed to link shader:" << '\n' << log.data() << std::endl;
        glCheck(GLEXT_glDeleteObject(shaderProgram));
        return false;
    }

    // Destroy the shader if it was already created
    if (m_shaderProgram)
    {
        glCheck(GLEXT_glDeleteObject(castToGlHandle(m_shaderProgram)));
        m_shaderProgram = 0;
    }

    // Reset the internal state
    m_currentTexture = -1;
    m_textures.clear();
    m_uniforms.clear();

    m_shaderProgram = castFromGlHandle(shaderProgram);

    // Force an OpenGL flush, so that the shader will appear updated
    // in all contexts immediately (solves problems in multi-threaded apps)
    glCheck(glFlush());

    return true;
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, float x)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform1f(binder.location, x));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, Glsl::Vec2 v)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform2f(binder.location, v.x, v.y));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, const Glsl::Vec3& v)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform3f(binder.location, v.x, v.y, v.z));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, const Glsl::Vec4& v)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform4f(binder.location, v.x, v.y, v.z, v.w));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, int x)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform1i(binder.location, x));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, Glsl::Ivec2 v)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform2i(binder.location, v.x, v.y));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, const Glsl::Ivec3& v)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform3i(binder.location, v.x, v.y, v.z));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, const Glsl::Ivec4& v)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform4i(binder.location, v.x, v.y, v.z, v.w));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, bool x)
{
    setUniform(name, static_cast<int>(x));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, Glsl::Bvec2 v)
{
    setUniform(name, Glsl::Ivec2(v));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, const Glsl::Bvec3& v)
{
    setUniform(name, Glsl::Ivec3(v));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, const Glsl::Bvec4& v)
{
    setUniform(name, Glsl::Ivec4(v));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, const Glsl::Mat3& matrix)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniformMatrix3fv(binder.location, 1, GL_FALSE, matrix.array.data()));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, const Glsl::Mat4& matrix)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniformMatrix4fv(binder.location, 1, GL_FALSE, matrix.array.data()));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& name, const Texture& texture)
{
    if (!m_shaderProgram)
        return;

    const TransientContextLock lock;

    // Find the location of the variable in the shader
    const int location = getUniformLocation(name);
    if (location != -1)
    {
        // Store the location -> texture mapping
        const auto it = m_textures.find(location);
        if (it == m_textures.end())
        {
            // New entry, make sure there are enough texture units
            if (m_textures.size() + 1 >= getMaxTextureUnits())
            {
                err() << "Impossible to use texture " << std::quoted(name)
                      << " for shader: all available texture units are used" << std::endl;
                return;
            }

            m_textures[location] = &texture;
        }
        else
        {
            // Location already used, just replace the texture
            it->second = &texture;
        }
    }
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setCurrentTextureUniform(const std::string& name)
{
    if (!m_shaderProgram)
        return;

    const TransientContextLock lock;

    // Find the location of the variable in the shader
    m_currentTexture = getUniformLocation(name);
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& name, const float* scalarArray, std::size_t length)
{
    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform1fv(binder.location, static_cast<GLsizei>(length), scalarArray));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length)
{
    std::vector<float> contiguous = flatten(vectorArray, length);

    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform2fv(binder.location, static_cast<GLsizei>(length), contiguous.data()));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length)
{
    std::vector<float> contiguous = flatten(vectorArray, length);

    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform3fv(binder.location, static_cast<GLsizei>(length), contiguous.data()));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length)
{
    std::vector<float> contiguous = flatten(vectorArray, length);

    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform4fv(binder.location, static_cast<GLsizei>(length), contiguous.data()));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length)
{
    static const std::size_t matrixSize = matrixArray[0].array.size();

    std::vector<float> contiguous(matrixSize * length);
    for (std::size_t i = 0; i < length; ++i)
        priv::copyMatrix(matrixArray[i].array.data(), matrixSize, &contiguous[matrixSize * i]);

    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniformMatrix3fv(binder.location, static_cast<GLsizei>(length), GL_FALSE, contiguous.data()));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& name, const Glsl::Mat4* matrixArray, std::size_t length)
{
    static const std::size_t matrixSize = matrixArray[0].array.size();

    std::vector<float> contiguous(matrixSize * length);
    for (std::size_t i = 0; i < length; ++i)
        priv::copyMatrix(matrixArray[i].array.data(), matrixSize, &contiguous[matrixSize * i]);

    const UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniformMatrix4fv(binder.location, static_cast<GLsizei>(length), GL_FALSE, contiguous.data()));
}


////////////////////////////////////////////////////////////
void GlShaderImpl::bind() const
{
    // Enable the program
    glCheck(GLEXT_glUseProgramObject(castToGlHandle(m_shaderProgram)));

    // Bind the textures
    bindTextures();

    // Bind the current texture
    if (m_currentTexture != -1)
        glCheck(GLEXT_glUniform1i(m_currentTexture, 0));
}


////////////////////////////////////////////////////////////
unsigned int GlShaderImpl::getNativeHandle() const
{
    return m_shaderProgram;
}


////////////////////////////////////////////////////////////
void GlShaderImpl::bind(const Shader* shader)
{
    const TransientContextLock lock;

    // Make sure that we can use shaders
    if (!isAvailable())
    {
        err() << "Failed to bind or unbind shader: your system doesn't support shaders "
              << "(you should test Shader::isAvailable() before trying to use the Shader class)" << std::endl;
        return;
    }

    const auto* impl = shader ? static_cast<const GlShaderImpl*>(shader->m_impl.get()) : nullptr;

    if (impl && impl->m_shaderProgram)
    {
        impl->bind();
    }
    else
    {
        // Bind no shader
        glCheck(GLEXT_glUseProgramObject({}));
    }
}


////////////////////////////////////////////////////////////
bool GlShaderImpl::isAvailable()
{
    static const bool available = []
    {
        const TransientContextLock contextLock;

        // Make sure that extensions are initialized
        ensureExtensionsInit();

        return GLEXT_multitexture && GLEXT_shading_language_100 && GLEXT_shader_objects && GLEXT_vertex_shader &&
               GLEXT_fragment_shader;
    }();

    return available;
}


////////////////////////////////////////////////////////////
bool GlShaderImpl::isGeometryAvailable()
{
    static const bool available = []
    {
        const TransientContextLock contextLock;

        // Make sure that extensions are initialized
        ensureExtensionsInit();

        return isAvailable() && (GLEXT_geometry_shader4 || GLEXT_GL_VERSION_3_2);
    }();

    return available;
}


////////////////////////////////////////////////////////////
void GlShaderImpl::bindTextures() const
{
    auto it = m_textures.begin();
    for (std::size_t i = 0; i < m_textures.size(); ++i)
    {
        const auto index = static_cast<GLsizei>(i + 1);
        glCheck(GLEXT_glUniform1i(it->first, index));
        glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0 + static_cast<GLenum>(index)));
        GlTextureImpl::bind(it->second, CoordinateType::Normalized);
        ++it;
    }

    // Make sure that the texture unit which is left active is the number 0
    glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0));
}


////////////////////////////////////////////////////////////
int GlShaderImpl::getUniformLocation(const std::string& name)
{
    // Check the cache
    if (const auto it = m_uniforms.find(name); it != m_uniforms.end())
    {
        // Already in cache, return it
        return it->second;
    }

    // Not in cache, request the location from OpenGL
    const int location = GLEXT_glGetUniformLocation(castToGlHandle(m_shaderProgram), name.c_str());
    m_uniforms.try_emplace(name, location);

    if (location == -1)
        err() << "Uniform " << std::quoted(name) << " not found in shader" << std::endl;

    return location;
}

} // namespace sf::priv

#else // SFML_OPENGL_ES

// OpenGL ES 1 doesn't support GLSL shaders at all, we have to provide an empty implementation

namespace sf::priv
{
////////////////////////////////////////////////////////////
GlShaderImpl::~GlShaderImpl() = default;


////////////////////////////////////////////////////////////
bool GlShaderImpl::compile(std::string_view /* vertexShaderCode */,
                           std::string_view /* geometryShaderCode */,
                           std::string_view /* fragmentShaderCode */)
{
    return false;
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, float)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, Glsl::Vec2)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, const Glsl::Vec3&)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, const Glsl::Vec4&)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, int)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, Glsl::Ivec2)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, const Glsl::Ivec3&)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, const Glsl::Ivec4&)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, bool)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, Glsl::Bvec2)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, const Glsl::Bvec3&)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, const Glsl::Bvec4&)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, const Glsl::Mat3& /* matrix */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, const Glsl::Mat4& /* matrix */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniform(const std::string& /* name */, const Texture& /* texture */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setCurrentTextureUniform(const std::string& /* name */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& /* name */, const float* /* scalarArray */, std::size_t /* length */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& /* name */, const Glsl::Vec2* /* vectorArray */, std::size_t /* length */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& /* name */, const Glsl::Vec3* /* vectorArray */, std::size_t /* length */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& /* name */, const Glsl::Vec4* /* vectorArray */, std::size_t /* length */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& /* name */, const Glsl::Mat3* /* matrixArray */, std::size_t /* length */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::setUniformArray(const std::string& /* name */, const Glsl::Mat4* /* matrixArray */, std::size_t /* length */)
{
}


////////////////////////////////////////////////////////////
void GlShaderImpl::bind() const
{
}


////////////////////////////////////////////////////////////
unsigned int GlShaderImpl::getNativeHandle() const
{
    return 0;
}


////////////////////////////////////////////////////////////
void GlShaderImpl::bind(const Shader* /* shader */)
{
}


////////////////////////////////////////////////////////////
bool GlShaderImpl::isAvailable()
{
    return false;
}


////////////////////////////////////////////////////////////
bool GlShaderImpl::isGeometryAvailable()
{
    return false;
}

} // namespace sf::priv

#endif // SFML_OPENGL_ES
