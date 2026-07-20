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
#include <SFML/Graphics/ShaderImpl.hpp>

#include <SFML/Window/GlResource.hpp>

#include <unordered_map>


namespace sf
{
class Shader;
class Texture;

namespace priv
{
////////////////////////////////////////////////////////////
/// \brief OpenGL implementation of the shader
///
////////////////////////////////////////////////////////////
class GlShaderImpl : public ShaderImpl, GlResource
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    GlShaderImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~GlShaderImpl() override;

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
    [[nodiscard]] bool compile(std::string_view vertexShaderCode,
                               std::string_view geometryShaderCode,
                               std::string_view fragmentShaderCode) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p float uniform
    ///
    /// \param name Name of the uniform variable in GLSL
    /// \param x    Value of the float scalar
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, float x) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec2 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the vec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Vec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec3 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the vec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec4 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the vec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p int uniform
    ///
    /// \param name Name of the uniform variable in GLSL
    /// \param x    Value of the int scalar
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, int x) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec2 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the ivec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Ivec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec3 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the ivec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec4 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the ivec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bool uniform
    ///
    /// \param name Name of the uniform variable in GLSL
    /// \param x    Value of the bool scalar
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, bool x) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec2 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the bvec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Bvec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec3 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the bvec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec4 uniform
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param vector Value of the bvec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat3 matrix
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param matrix Value of the mat3 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat3& matrix) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat4 matrix
    ///
    /// \param name   Name of the uniform variable in GLSL
    /// \param matrix Value of the mat4 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat4& matrix) override;

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
    void setUniform(const std::string& name, const Texture& texture) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify current texture as \p sampler2D uniform
    ///
    /// This maps a shader texture variable to the texture of
    /// the object being drawn, which cannot be known in advance.
    ///
    /// \param name Name of the texture in the shader
    ///
    ////////////////////////////////////////////////////////////
    void setCurrentTextureUniform(const std::string& name) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p float[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param scalarArray pointer to array of \p float values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const float* scalarArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec2[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param vectorArray pointer to array of \p vec2 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec3[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param vectorArray pointer to array of \p vec3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec4[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param vectorArray pointer to array of \p vec4 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat3[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param matrixArray pointer to array of \p mat3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat4[] array uniform
    ///
    /// \param name        Name of the uniform variable in GLSL
    /// \param matrixArray pointer to array of \p mat4 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Mat4* matrixArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Bind the shader program for rendering
    ///
    /// This binds the program object as well as all the textures
    /// used by the shader, including the current texture.
    ///
    ////////////////////////////////////////////////////////////
    void bind() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the underlying native handle of the shader
    ///
    /// \return Native handle of the shader, 0 if the backend has none
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] unsigned int getNativeHandle() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Bind a shader for rendering
    ///
    /// This is the OpenGL backing of `sf::Shader::bind`.
    ///
    /// \param shader Pointer to the shader to bind, can be null to use no shader
    ///
    ////////////////////////////////////////////////////////////
    static void bind(const Shader* shader);

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether or not the system supports shaders
    ///
    /// \return `true` if shaders are supported, `false` otherwise
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static bool isAvailable();

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether or not the system supports geometry shaders
    ///
    /// \return `true` if geometry shaders are supported, `false` otherwise
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static bool isGeometryAvailable();

private:
    ////////////////////////////////////////////////////////////
    /// \brief Bind all the textures used by the shader
    ///
    /// This function each texture to a different unit, and
    /// updates the corresponding variables in the shader accordingly.
    ///
    ////////////////////////////////////////////////////////////
    void bindTextures() const;

    ////////////////////////////////////////////////////////////
    /// \brief Get the location ID of a shader uniform
    ///
    /// \param name Name of the uniform variable to search
    ///
    /// \return Location ID of the uniform, or -1 if not found
    ///
    ////////////////////////////////////////////////////////////
    int getUniformLocation(const std::string& name);

    ////////////////////////////////////////////////////////////
    /// \brief RAII object to save and restore the program
    ///        binding while uniforms are being set
    ///
    /// Implementation is private in the .cpp file.
    ///
    ////////////////////////////////////////////////////////////
    struct UniformBinder;

    ////////////////////////////////////////////////////////////
    // Types
    ////////////////////////////////////////////////////////////
    using TextureTable = std::unordered_map<int, const Texture*>;
    using UniformTable = std::unordered_map<std::string, int>;

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    unsigned int m_shaderProgram{};    //!< OpenGL identifier for the program
    int          m_currentTexture{-1}; //!< Location of the current texture in the shader
    TextureTable m_textures;           //!< Texture variables in the shader, mapped to their location
    UniformTable m_uniforms;           //!< Parameters location cache
};

} // namespace priv

} // namespace sf
