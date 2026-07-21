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
#include <SFML/Graphics/Glsl.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <string>
#include <string_view>

#include <cstddef>
#include <cstdint>


namespace sf
{
class Texture;

namespace priv
{
////////////////////////////////////////////////////////////
/// \brief Abstract base class for shader implementations
///
/// The shader implementation owns the backend shader program
/// and the uniform bookkeeping that goes with it. Source code
/// loading and dispatching is done by `sf::Shader`.
///
////////////////////////////////////////////////////////////
class ShaderImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    ShaderImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~ShaderImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy constructor
    ///
    ////////////////////////////////////////////////////////////
    ShaderImpl(const ShaderImpl&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy assignment
    ///
    ////////////////////////////////////////////////////////////
    ShaderImpl& operator=(const ShaderImpl&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Compile the shader(s) and create the program
    ///
    /// If one of the arguments is empty, the corresponding shader
    /// is not created.
    ///
    /// \param vertexShaderCode   Source code of the vertex shader
    /// \param geometryShaderCode Source code of the geometry shader
    /// \param fragmentShaderCode Source code of the fragment shader
    ///
    /// \return `true` on success, `false` if any error happened
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual bool compile(std::string_view vertexShaderCode,
                                       std::string_view geometryShaderCode,
                                       std::string_view fragmentShaderCode) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p float uniform
    ///
    /// \param name Name of the uniform variable in GLSL
    /// \param x    Value of the float scalar
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, float x) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec2 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the vec2 vector
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, Glsl::Vec2 vector) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec3 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the vec3 vector
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, const Glsl::Vec3& vector) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec4 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the vec4 vector
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, const Glsl::Vec4& vector) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p int uniform
    ///
    /// \param name Name of the uniform variable in GLSL
    /// \param x    Value of the int scalar
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, int x) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec2 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the ivec2 vector
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, Glsl::Ivec2 vector) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec3 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the ivec3 vector
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, const Glsl::Ivec3& vector) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec4 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the ivec4 vector
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, const Glsl::Ivec4& vector) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bool uniform
    ///
    /// \param name Name of the uniform variable in GLSL
    /// \param x    Value of the bool scalar
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, bool x) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec2 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the bvec2 vector
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, Glsl::Bvec2 vector) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec3 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the bvec3 vector
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, const Glsl::Bvec3& vector) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec4 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the bvec4 vector
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, const Glsl::Bvec4& vector) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat3 matrix
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param matrix Value of the mat3 matrix
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, const Glsl::Mat3& matrix) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat4 matrix
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param matrix Value of the mat4 matrix
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, const Glsl::Mat4& matrix) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify a texture as \p sampler2D uniform
    ///
    /// The texture must remain alive as long as the shader
    /// uses it, no copy is made internally.
    ///
    /// \param name    Name of the texture in the shader
    /// \param texture Texture to assign
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniform(const std::string& name, const Texture& texture) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify current texture as \p sampler2D uniform
    ///
    /// This maps a shader texture variable to the texture of
    /// the object being drawn, which cannot be known in advance.
    ///
    /// \param name Name of the texture in the shader
    ///
    ////////////////////////////////////////////////////////////
    virtual void setCurrentTextureUniform(const std::string& name) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p float[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param scalarArray pointer to array of \p float values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniformArray(const std::string& name, const float* scalarArray, std::size_t length) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec2[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param vectorArray pointer to array of \p vec2 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec3[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param vectorArray pointer to array of \p vec3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec4[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param vectorArray pointer to array of \p vec4 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat3[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param matrixArray pointer to array of \p mat3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat4[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param matrixArray pointer to array of \p mat4 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    virtual void setUniformArray(const std::string& name, const Glsl::Mat4* matrixArray, std::size_t length) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Bind the shader program for rendering
    ///
    /// This binds the program object as well as all the textures
    /// used by the shader, including the current texture.
    ///
    ////////////////////////////////////////////////////////////
    virtual void bind() const = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Get the underlying native handle of the shader
    ///
    /// \return Native handle of the shader, 0 if the backend has none
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual unsigned int getNativeHandle() const = 0;

protected:
    ////////////////////////////////////////////////////////////
    /// \brief Get the implementation of a shader
    ///
    /// \param shader Shader to access
    ///
    /// \return The shader's implementation, can be a null pointer
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static ShaderImpl* getImpl(const Shader& shader)
    {
        return shader.m_impl.get();
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get the implementation of a texture used by the shader
    ///
    /// \param texture Texture to access
    ///
    /// \return The texture's implementation, can be a null pointer
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static TextureImpl* getTextureImpl(const Texture& texture)
    {
        return texture.m_impl.get();
    }

    ////////////////////////////////////////////////////////////
    /// \brief Check whether a texture used by the shader has a mipmap
    ///
    /// \param texture Texture to access
    ///
    /// \return `true` if the texture currently has a mipmap
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static bool textureHasMipmap(const Texture& texture)
    {
        return texture.m_hasMipmap;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get the cache id of a texture used by the shader
    ///
    /// \param texture Texture to access
    ///
    /// \return Unique number identifying the texture to the shader's cache
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static std::uint64_t getTextureCacheId(const Texture& texture)
    {
        return texture.m_cacheId;
    }
};

} // namespace priv

} // namespace sf
