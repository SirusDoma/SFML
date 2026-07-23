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
#include <SFML/Graphics/ShaderImpl.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Metal implementation of the shader
///
/// Compiles MSL sources whose single `vertex`/`fragment`
/// function is found by inspection, its name is free. Uniforms
/// are matched by name against the members of the constant
/// buffer structs found through pipeline reflection; vertex
/// buffer indices 0 and 1 are reserved for the vertex data and
/// the SFML matrices. Textures are matched by name, a texture
/// the shader declares without assigning one samples the draw's
/// texture, as does the CurrentTexture special uniform. Samplers
/// take the sampler of the texture bound at the same index.
///
////////////////////////////////////////////////////////////
class MetalShaderImpl : public ShaderImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param device Device this shader is created on
    ///
    ////////////////////////////////////////////////////////////
    explicit MetalShaderImpl(MetalGraphicsDevice& device);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor, drops the shader's cached pipeline states
    ///
    ////////////////////////////////////////////////////////////
    ~MetalShaderImpl() override;

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
    /// \brief Specify value for \p float2 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the vec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Vec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p float3 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the vec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p float4 uniform
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
    /// \brief Specify value for \p int2 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the ivec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Ivec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p int3 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the ivec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p int4 uniform
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
    /// \brief Specify value for \p bool2 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the bvec2 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Bvec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bool3 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the bvec3 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bool4 uniform
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param vector Value of the bvec4 vector
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p float3x3 matrix
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param matrix Value of the mat3 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat3& matrix) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p float4x4 matrix
    ///
    /// \param name   Name of the uniform variable in the shader
    /// \param matrix Value of the mat4 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat4& matrix) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify a texture as \p texture2d uniform
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
    /// \brief Specify current texture as \p texture2d uniform
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
    /// \brief Specify values for \p float2[] array uniform
    ///
    /// \param name        Name of the uniform variable in the shader
    /// \param vectorArray pointer to array of \p vec2 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p float3[] array uniform
    ///
    /// \param name        Name of the uniform variable in the shader
    /// \param vectorArray pointer to array of \p vec3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p float4[] array uniform
    ///
    /// \param name        Name of the uniform variable in the shader
    /// \param vectorArray pointer to array of \p vec4 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p float3x3[] array uniform
    ///
    /// \param name        Name of the uniform variable in the shader
    /// \param matrixArray pointer to array of \p mat3 values
    /// \param length      Number of elements in the array
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p float4x4[] array uniform
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
    /// This sets the shader's pipeline state on the open encoder
    /// and binds its uniform data and textures, including the
    /// current texture.
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
    /// \brief Get a stamp identifying the bindable state of the shader
    ///
    /// The stamp changes whenever a uniform is set or a texture
    /// used by the shader changes; a shader bound with `bind`
    /// stays bound correctly for as long as the stamp is unchanged.
    /// Stamps are never reused across shaders.
    ///
    /// \return Stamp of the shader's current bindable state
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::uint64_t getPipelineStateId() const;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether textures were assigned to the shader
    ///
    /// \return `true` if binding the shader binds textures other than the draw's texture
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool bindsExternalTextures() const;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Uniform bookkeeping of one compiled stage
    ///
    ////////////////////////////////////////////////////////////
    struct Stage
    {
        struct Variable
        {
            std::size_t   block{};        //!< Index into the stage's buffer blocks
            std::size_t   offset{};       //!< Byte offset inside the block
            std::size_t   arrayStride{};  //!< Byte stride between array elements, 0 for non-arrays
            std::size_t   elementCount{1}; //!< Number of array elements the shader declared
            std::uint32_t dataType{};     //!< Raw MTLDataType of one element
        };

        struct Block
        {
            std::uint32_t          index{}; //!< Buffer bind index
            std::vector<std::byte> shadow;  //!< CPU copy of the buffer contents
        };

        NSPtr<MetalLibraryPtr>  library;  //!< Library the stage was compiled into
        NSPtr<MetalFunctionPtr> function; //!< Entry function of the stage

        std::vector<Block>                             blocks;       //!< Constant buffer blocks of the stage
        std::unordered_map<std::string, Variable>      variables;    //!< Uniforms by name
        std::unordered_map<std::string, std::uint32_t> textureSlots; //!< Texture bind indices by name
        std::vector<std::uint32_t>                     samplerSlots; //!< Sampler bind indices
    };

    ////////////////////////////////////////////////////////////
    /// \brief Compile one stage and find its entry function
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool compileStage(std::string_view code, bool fragment, Stage& stage);

    ////////////////////////////////////////////////////////////
    /// \brief Set up the uniform bookkeeping from the pipeline reflection
    ///
    /// \param reflectionObject The MTLRenderPipelineReflection of the compiled pair
    ///
    ////////////////////////////////////////////////////////////
    void reflect(void* reflectionObject);

    ////////////////////////////////////////////////////////////
    /// \brief Write elements to a uniform in every stage that has it
    ///
    /// Elements are placed at the stride the shader declared, a
    /// missing uniform is reported once.
    ///
    /// \param elementSize Byte size of one source element
    ///
    ////////////////////////////////////////////////////////////
    void writeUniform(const std::string& name, const void* data, std::size_t elementSize, std::size_t elementCount);

    ////////////////////////////////////////////////////////////
    /// \brief Bind the uniform data and textures of one stage
    ///
    ////////////////////////////////////////////////////////////
    void bindStage(const Stage& stage, bool fragment) const;

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    MetalGraphicsDevice& m_device; //!< Device this shader is created on

    Stage m_vertexStage;   //!< Vertex stage, function null to use the built-in one
    Stage m_fragmentStage; //!< Fragment stage, function null to use the built-in one

    std::unordered_map<std::string, const Texture*> m_textures;           //!< Textures by uniform name
    std::string                                     m_currentTextureName; //!< Uniform resolved to the draw's texture
    std::unordered_set<std::string>                 m_warnedUniforms;     //!< Names already reported as not found
    std::uint64_t                                   m_revision;           //!< Stamp renewed on every uniform change
    std::uint32_t                                   m_shaderId{}; //!< Identity of the compiled pair in the pipeline cache
};

} // namespace sf::priv
