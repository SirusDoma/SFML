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
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cstddef>
#include <cstdint>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Vulkan implementation of the shader
///
/// Consumes SPIR-V bytecode compiled from HLSL with the `main`
/// entry point; HLSL source is compiled at run time when the
/// library was built with the runtime shader compiler.
///
/// Resources are reflected from the SPIR-V: constant buffer
/// members are matched by name, `Texture2D` bindings by name and
/// their `SamplerState` by register index. Register `bN` maps to
/// descriptor binding N, `tN` to 16 + N and `sN` to 32 + N, with
/// `b0` reserved for the `SFMLMatrices` constant buffer and
/// `t0`/`s0` doubling as the draw's texture when the built-in
/// stages are kept. Push constants are reserved by the backend
/// and rejected in user shaders.
///
/// Stages that are not provided keep using the built-in
/// pipeline: a pixel-only shader is combined with the built-in
/// vertex shader, whose output signature (SV_POSITION, COLOR0,
/// TEXCOORD0) the pixel shader input must match.
///
/// The vertex input of a user vertex shader is fed from
/// `sf::Vertex`. SPIR-V has no semantic names, so the inputs are
/// matched by location, which the compiler assigns in
/// declaration order: the input struct has to declare position,
/// color and texture coordinates in that order, whatever
/// semantics they carry.
///
////////////////////////////////////////////////////////////
class VulkanShaderImpl : public ShaderImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param device Device this shader is created on
    ///
    ////////////////////////////////////////////////////////////
    explicit VulkanShaderImpl(VulkanGraphicsDevice& device);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~VulkanShaderImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Compile the shader(s) and create the program
    ///
    /// If one of the arguments is empty, the corresponding shader
    /// is not created.
    ///
    /// \param vertexShaderCode   Source code or SPIR-V of the vertex shader
    /// \param geometryShaderCode Source code or SPIR-V of the geometry shader
    /// \param fragmentShaderCode Source code or SPIR-V of the fragment shader
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
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, float x) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec2 uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Vec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec3 uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p vec4 uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Vec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p int uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, int x) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec2 uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Ivec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec3 uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p ivec4 uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Ivec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bool uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, bool x) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec2 uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, Glsl::Bvec2 vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec3 uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec3& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p bvec4 uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Bvec4& vector) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat3 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat3& matrix) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify value for \p mat4 matrix
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Glsl::Mat4& matrix) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify a texture as \p Texture2D uniform
    ///
    /// The texture must remain alive as long as the shader
    /// uses it, no copy is made internally.
    ///
    ////////////////////////////////////////////////////////////
    void setUniform(const std::string& name, const Texture& texture) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify current texture as \p Texture2D uniform
    ///
    ////////////////////////////////////////////////////////////
    void setCurrentTextureUniform(const std::string& name) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p float[] array uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const float* scalarArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec2[] array uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec3[] array uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p vec4[] array uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat3[] array uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Specify values for \p mat4[] array uniform
    ///
    ////////////////////////////////////////////////////////////
    void setUniformArray(const std::string& name, const Glsl::Mat4* matrixArray, std::size_t length) override;

    ////////////////////////////////////////////////////////////
    /// \brief Bring the assigned textures into their sampling layout
    ///
    /// Has to be called before the draw's render pass opens,
    /// layout transitions are illegal inside one.
    ///
    ////////////////////////////////////////////////////////////
    void prepareTextures();

    ////////////////////////////////////////////////////////////
    /// \brief Bind the shader program for rendering
    ///
    /// Updates and binds the descriptor set covering the shader's
    /// resources and registers the shader modules for the pipeline
    /// of the next draw.
    ///
    ////////////////////////////////////////////////////////////
    void bind() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Bind the shader for the draw being submitted
    ///
    /// The pipeline layout of a user shader comes with its own
    /// descriptor set, so a draw whose set could not be written
    /// has to be dropped rather than submitted against the set
    /// of whatever was bound before.
    ///
    /// \return `true` if the shader is bound and ready to draw
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool bindForDraw() const;

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
    /// \brief One reflected constant buffer backed by CPU memory
    ///
    ////////////////////////////////////////////////////////////
    struct UniformBlock
    {
        struct Variable
        {
            std::uint32_t offset{}; //!< Byte offset in the block
            std::uint32_t size{};   //!< Byte size in the block
        };

        std::string                               name;      //!< Name the block was declared with
        std::uint32_t                             binding{}; //!< Descriptor binding of the block
        VkShaderStageFlags                        stages{};  //!< Stages the block is visible to
        std::vector<std::byte>                    shadow;    //!< CPU copy of the block contents
        std::unordered_map<std::string, Variable> variables; //!< Members by name
    };

    ////////////////////////////////////////////////////////////
    /// \brief One reflected texture binding
    ///
    ////////////////////////////////////////////////////////////
    struct TextureBinding
    {
        std::uint32_t      binding{};       //!< Descriptor binding of the texture
        std::uint32_t      registerIndex{}; //!< Register index the texture was declared with
        VkShaderStageFlags stages{};        //!< Stages the texture is visible to
        bool               combined{};      //!< Whether the binding is a combined image sampler
        bool               builtin{};       //!< Whether the slot serves the built-in stage, not user code
        std::string        name;            //!< Name the texture was declared with
    };

    ////////////////////////////////////////////////////////////
    /// \brief One reflected sampler binding
    ///
    ////////////////////////////////////////////////////////////
    struct SamplerBinding
    {
        std::uint32_t      binding{};       //!< Descriptor binding of the sampler
        std::uint32_t      registerIndex{}; //!< Register index the sampler was declared with
        VkShaderStageFlags stages{};        //!< Stages the sampler is visible to
        bool               builtin{};       //!< Whether the slot serves the built-in stage, not user code
    };

    ////////////////////////////////////////////////////////////
    /// \brief Turn stage input into SPIR-V, compiling HLSL when supported
    ///
    /// \param code  Source code or SPIR-V bytecode of the stage
    /// \param stage Stage the code belongs to
    /// \param spirv Filled with the SPIR-V of the stage
    ///
    /// \return `true` on success
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool toSpirv(std::string_view code, VkShaderStageFlagBits stage, std::vector<std::uint32_t>& spirv);

    ////////////////////////////////////////////////////////////
    /// \brief Reflect a stage and merge its resources, remapping bindings
    ///
    /// \param spirv Stage bytecode, patched in place with the final bindings
    /// \param stage Stage the bytecode belongs to
    ///
    /// \return `true` on success
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool reflectStage(std::vector<std::uint32_t>& spirv, VkShaderStageFlagBits stage);

    ////////////////////////////////////////////////////////////
    /// \brief Create the descriptor set layout and pipeline layout
    ///
    /// \return `true` on success
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool createLayouts();

    ////////////////////////////////////////////////////////////
    /// \brief Destroy the modules and layouts of the shader
    ///
    ////////////////////////////////////////////////////////////
    void destroy();

    ////////////////////////////////////////////////////////////
    /// \brief Write raw data to a uniform in every block that has it
    ///
    /// \param elementSize   Byte size of one source element
    /// \param elementStride Byte stride of one element in the block
    ///
    ////////////////////////////////////////////////////////////
    void writeUniform(const std::string& name,
                      const void*        data,
                      std::size_t        elementSize,
                      std::size_t        elementStride,
                      std::size_t        elementCount);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    VulkanGraphicsDevice& m_device; //!< Device this shader is created on

    VkShaderModule m_vertexModule{};      //!< User vertex module, null to use the built-in one
    VkShaderModule m_pointVertexModule{}; //!< Vertex module variant writing PointSize, for point pipelines
    VkShaderModule m_geometryModule{};    //!< User geometry module
    VkShaderModule m_fragmentModule{};    //!< User fragment module, null to use the built-in one

    VkDescriptorSetLayout m_setLayout{};      //!< Descriptor set layout covering the shader's resources
    VkPipelineLayout      m_pipelineLayout{}; //!< Pipeline layout of the shader

    std::vector<UniformBlock>   m_blocks;                  //!< Constant buffers settable by name
    std::vector<TextureBinding> m_textureSlots;            //!< Reflected texture bindings
    std::vector<SamplerBinding> m_samplerSlots;            //!< Reflected sampler bindings
    std::uint32_t m_matricesBinding{VK_ATTACHMENT_UNUSED}; //!< Binding of the SFMLMatrices block, unused when absent
    VkShaderStageFlags m_matricesStages{};                 //!< Stages the SFMLMatrices block is visible to
    bool               m_hasUserFragmentStage{};           //!< Whether a user fragment module replaces the built-in one

    std::unordered_map<std::string, const Texture*> m_textures;           //!< Textures by uniform name
    std::string                                     m_currentTextureName; //!< Uniform resolved to the draw's texture
    std::unordered_set<std::string>                 m_warnedUniforms;     //!< Names already reported as not found
    std::uint64_t                                   m_revision;           //!< Stamp renewed on every uniform change
    std::uint32_t                                   m_shaderId{};         //!< Identity of the module set, never reused
};

} // namespace sf::priv
