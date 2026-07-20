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
#include <SFML/Graphics/D3D11/D3D11Utils.hpp>
#include <SFML/Graphics/ShaderImpl.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cstddef>
#include <cstdint>


namespace sf::priv
{
class D3D11GraphicsDevice;

////////////////////////////////////////////////////////////
/// \brief Direct3D 11 implementation of the shader
///
/// Compiles HLSL sources with the `main` entry point against
/// shader model 4. Uniforms are matched by name against the
/// global constant buffer of each compiled stage using shader
/// reflection; textures are matched against `Texture2D`
/// bindings, their sampler is bound at the same slot.
///
/// Stages that are not provided keep using the built-in
/// pipeline: a pixel-only shader is combined with the built-in
/// vertex shader, whose output signature (SV_POSITION, COLOR0,
/// TEXCOORD0) the pixel shader input must match.
///
////////////////////////////////////////////////////////////
class D3D11ShaderImpl : public ShaderImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param device Device this shader is created on
    ///
    ////////////////////////////////////////////////////////////
    explicit D3D11ShaderImpl(D3D11GraphicsDevice& device);

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
    /// \param name Name of the uniform variable in the shader
    /// \param x    Value of the float scalar
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, float x) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec2 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the vec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Vec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec3 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the vec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec4 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the vec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p int uniform
    ///
    /// \param name Name of the uniform variable in the shader
    /// \param x    Value of the int scalar
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, int x) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec2 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the ivec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Ivec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec3 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the ivec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec4 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the ivec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bool uniform
    ///
    /// \param name Name of the uniform variable in the shader
    /// \param x    Value of the bool scalar
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, bool x) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec2 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the bvec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Bvec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec3 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the bvec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec4 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the bvec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat3 matrix
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param matrix Value of the mat3 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat3& matrix) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat4 matrix
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param matrix Value of the mat4 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat4& matrix) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify a texture as \p Texture2D uniform
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
    /// \brief Specify current texture as \p Texture2D uniform
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
    /// \param name        Name of the uniform variable in the shader
    /// \param scalarArray pointer to array of \p float values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const float* scalarArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec2[] array uniform
    ///
    /// \param name        Name of the uniform variable in the shader
    /// \param vectorArray pointer to array of \p vec2 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec3[] array uniform
    ///
    /// \param name        Name of the uniform variable in the shader
    /// \param vectorArray pointer to array of \p vec3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec4[] array uniform
    ///
    /// \param name        Name of the uniform variable in the shader
    /// \param vectorArray pointer to array of \p vec4 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat3[] array uniform
    ///
    /// \param name        Name of the uniform variable in the shader
    /// \param matrixArray pointer to array of \p mat3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat4[] array uniform
    ///
    /// \param name        Name of the uniform variable in the shader
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

private:
    ////////////////////////////////////////////////////////////
    /// \brief Uniform bookkeeping of one compiled stage
    ///
    ////////////////////////////////////////////////////////////
    struct Stage
    {
        struct Variable
        {
            std::uint32_t offset{}; //!< Byte offset in the global constant buffer
            std::uint32_t size{};   //!< Byte size in the global constant buffer
        };

        ComPtr<ID3D11Buffer>   constantBuffer;       //!< Global constant buffer of the stage
        std::uint32_t          constantBufferSlot{}; //!< Bind slot of the global constant buffer
        std::vector<std::byte> shadow;               //!< CPU copy of the constant buffer contents
        mutable bool           dirty{};              //!< Does the shadow need to be uploaded?

        std::unordered_map<std::string, Variable>      variables;    //!< Uniforms by name
        std::unordered_map<std::string, std::uint32_t> textureSlots; //!< Texture bind slots by name
    };

    ////////////////////////////////////////////////////////////
    /// \brief Compile one stage and set up its reflection data
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool compileStage(std::string_view code, const char* profile, Stage& stage, ComPtr<ID3DBlob>& bytecode);

    ////////////////////////////////////////////////////////////
    /// \brief Write raw data to a uniform in every stage that has it
    ///
    /// \param elementSize   Byte size of one source element
    /// \param elementStride Byte stride of one element in the constant buffer
    ///
    ////////////////////////////////////////////////////////////
    void writeUniform(const std::string& name,
                      const void*        data,
                      std::size_t        elementSize,
                      std::size_t        elementStride,
                      std::size_t        elementCount);

    ////////////////////////////////////////////////////////////
    /// \brief Shader stages a resource can be bound to
    ///
    ////////////////////////////////////////////////////////////
    enum class StageKind
    {
        Vertex,
        Geometry,
        Pixel
    };

    ////////////////////////////////////////////////////////////
    /// \brief Bind the textures of one stage
    ///
    ////////////////////////////////////////////////////////////
    void bindTextures(const Stage& stage, StageKind kind) const;

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    D3D11GraphicsDevice& m_device; //!< Device this shader is created on

    ComPtr<ID3D11VertexShader>   m_vertexShader;   //!< User vertex shader, null to use the built-in one
    ComPtr<ID3D11GeometryShader> m_geometryShader; //!< User geometry shader
    ComPtr<ID3D11PixelShader>    m_pixelShader;    //!< User pixel shader, null to use the built-in one
    ComPtr<ID3D11InputLayout>    m_inputLayout;    //!< Input layout matching the user vertex shader

    Stage m_vertexStage;   //!< Uniform bookkeeping of the vertex stage
    Stage m_geometryStage; //!< Uniform bookkeeping of the geometry stage
    Stage m_pixelStage;    //!< Uniform bookkeeping of the pixel stage

    std::unordered_map<std::string, const Texture*> m_textures;           //!< Textures by uniform name
    std::string                                     m_currentTextureName; //!< Uniform resolved to the draw's texture
    std::unordered_set<std::string>                 m_warnedUniforms;     //!< Names already reported as not found
};

} // namespace sf::priv
