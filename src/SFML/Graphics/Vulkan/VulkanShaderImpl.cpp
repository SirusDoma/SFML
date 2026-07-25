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
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>
#include <SFML/Graphics/Vulkan/VulkanShaderImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanTextureImpl.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <ostream>
#include <spirv-reflect/spirv_reflect.h>

#include <cstring>

#ifdef SFML_VULKAN_RUNTIME_SHADER_COMPILER
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <SPIRV/GlslangToSpv.h>
#include <mutex>
#endif


namespace
{
// Monotonic stamp handed out to shaders on creation and on every change, values
// are never reused so a (shader, stamp) pair identifies one exact bindable state
// even when a destroyed shader's address is reused by a new one
std::uint64_t nextShaderRevision()
{
    static std::atomic<std::uint64_t> revision{0};
    return ++revision;
}

// Monotonic identity for compiled module sets, keying the pipeline cache
std::uint32_t nextShaderId()
{
    static std::atomic<std::uint32_t> id{0};
    return ++id;
}

// The first word of every SPIR-V module
constexpr std::uint32_t spirvMagic = 0x07230203;

// Tell whether the bytes are SPIR-V bytecode rather than source text
bool isSpirv(std::string_view code)
{
    if ((code.size() < 4) || (code.size() % 4 != 0))
        return false;

    std::uint32_t magic = 0;
    std::memcpy(&magic, code.data(), sizeof(magic));

    return magic == spirvMagic;
}

// Descriptor binding ranges of the backend's register convention
constexpr std::uint32_t bufferBindingBase  = 0;
constexpr std::uint32_t textureBindingBase = 16;
constexpr std::uint32_t samplerBindingBase = 32;
constexpr std::uint32_t bindingRangeSize   = 16;

// SPIR-V opcodes and enumerant values used by the PointSize injection below
constexpr std::uint32_t spvOpEntryPoint         = 15;
constexpr std::uint32_t spvOpTypeFloat          = 22;
constexpr std::uint32_t spvOpTypePointer        = 32;
constexpr std::uint32_t spvOpConstant           = 43;
constexpr std::uint32_t spvOpFunction           = 54;
constexpr std::uint32_t spvOpFunctionEnd        = 56;
constexpr std::uint32_t spvOpVariable           = 59;
constexpr std::uint32_t spvOpStore              = 62;
constexpr std::uint32_t spvOpDecorate           = 71;
constexpr std::uint32_t spvOpMemberDecorate     = 72;
constexpr std::uint32_t spvOpLabel              = 248;
constexpr std::uint32_t spvExecutionModelVertex = 0;
constexpr std::uint32_t spvStorageClassOutput   = 3;
constexpr std::uint32_t spvDecorationBuiltIn    = 11;
constexpr std::uint32_t spvBuiltInPosition      = 0;
constexpr std::uint32_t spvBuiltInPointSize     = 1;
constexpr std::uint32_t spvFloatOneBits         = 0x3F800000;

// Vulkan wants point draws to receive a PointSize from the vertex shader, but
// HLSL shared with Direct3D has no way to declare one. This rewrites a vertex
// shader to store a constant 1.0 into a new PointSize output, the fixed point
// size of Direct3D; devices with the maintenance5 default never need it.
// Returns an empty vector when the module already provides PointSize (or uses
// a form the rewrite does not cover, like block-style built-in outputs).
//
// Only the vertex stage needs this. A geometry stage emitting points follows
// the inverse rule: without the shaderTessellationAndGeometryPointSize feature
// (which this backend does not enable, as Direct3D has no equivalent) writing
// PointSize there is forbidden and the fixed size of 1.0 applies on its own.
std::vector<std::uint32_t> injectPointSize(const std::vector<std::uint32_t>& spirv)
{
    if ((spirv.size() < 5) || (spirv[0] != spirvMagic))
        return {};

    // First pass: find the vertex entry point, the section positions to insert
    // into, ids that can be reused, and the conditions that rule the patch out
    std::size_t   entryPointOffset = 0; //!< Offset of the vertex OpEntryPoint
    std::uint32_t entryPointId     = 0;
    std::size_t   annotationInsert = 0; //!< End of the last decoration
    std::size_t   firstTypeOffset  = 0; //!< Start of the type/constant section
    std::size_t   typeInsert       = 0; //!< Before the first function
    std::size_t   storeInsert      = 0; //!< After the leading variables of the entry block
    std::uint32_t floatTypeId      = 0;
    std::uint32_t pointerTypeId    = 0;
    std::uint32_t constantOneId    = 0;

    bool inEntryFunction = false;

    for (std::size_t offset = 5; offset < spirv.size();)
    {
        const std::uint32_t wordCount = spirv[offset] >> 16;
        const std::uint32_t opcode    = spirv[offset] & 0xFFFFu;
        if ((wordCount == 0) || (offset + wordCount > spirv.size()))
            return {};

        // The type, constant and global variable declarations form one section
        if (!firstTypeOffset && (((opcode >= 19) && (opcode <= 39)) || ((opcode >= 41) && (opcode <= 46)) ||
                                 ((opcode == spvOpVariable) && !typeInsert)))
            firstTypeOffset = offset;

        switch (opcode)
        {
            case spvOpEntryPoint:
                if (!entryPointOffset && (spirv[offset + 1] == spvExecutionModelVertex))
                {
                    entryPointOffset = offset;
                    entryPointId     = spirv[offset + 2];
                }
                break;

            case spvOpDecorate:
                annotationInsert = offset + wordCount;
                if ((wordCount >= 4) && (spirv[offset + 2] == spvDecorationBuiltIn) &&
                    (spirv[offset + 3] == spvBuiltInPointSize))
                    return {};
                break;

            case spvOpMemberDecorate:
                annotationInsert = offset + wordCount;
                // A block-style built-in output cannot take a standalone
                // PointSize beside it
                if ((wordCount >= 5) && (spirv[offset + 3] == spvDecorationBuiltIn) &&
                    ((spirv[offset + 4] == spvBuiltInPointSize) || (spirv[offset + 4] == spvBuiltInPosition)))
                    return {};
                break;

            case spvOpTypeFloat:
                if (spirv[offset + 2] == 32)
                    floatTypeId = spirv[offset + 1];
                break;

            case spvOpTypePointer:
                if (floatTypeId && (spirv[offset + 2] == spvStorageClassOutput) && (spirv[offset + 3] == floatTypeId))
                    pointerTypeId = spirv[offset + 1];
                break;

            case spvOpConstant:
                if (floatTypeId && (wordCount == 4) && (spirv[offset + 1] == floatTypeId) &&
                    (spirv[offset + 3] == spvFloatOneBits))
                    constantOneId = spirv[offset + 2];
                break;

            case spvOpFunction:
                if (!typeInsert)
                    typeInsert = offset;
                inEntryFunction = entryPointId && (spirv[offset + 2] == entryPointId);
                break;

            case spvOpFunctionEnd:
                inEntryFunction = false;
                break;

            case spvOpLabel:
                if (inEntryFunction && !storeInsert)
                    storeInsert = offset + wordCount;
                break;

            case spvOpVariable:
                // Function-scope variables lead their block, the store goes after them
                if (inEntryFunction && (offset == storeInsert))
                    storeInsert = offset + wordCount;
                break;

            default:
                break;
        }

        offset += wordCount;
    }

    if (!entryPointOffset || !firstTypeOffset || !typeInsert || !storeInsert)
        return {};

    // A module can legitimately carry no decorations at all
    if (!annotationInsert)
        annotationInsert = firstTypeOffset;

    // Reuse existing declarations where possible, mint ids for the rest
    std::uint32_t bound = spirv[3];

    const bool createFloat    = (floatTypeId == 0);
    const bool createPointer  = (pointerTypeId == 0);
    const bool createConstant = (constantOneId == 0);

    if (createFloat)
        floatTypeId = bound++;
    if (createPointer)
        pointerTypeId = bound++;
    if (createConstant)
        constantOneId = bound++;
    const std::uint32_t variableId = bound++;

    std::vector<std::uint32_t> patched;
    patched.reserve(spirv.size() + 16);

    std::size_t cursor = 0;

    const auto copyUpTo = [&](std::size_t target)
    {
        while (cursor < target)
            patched.push_back(spirv[cursor++]);
    };

    copyUpTo(5);
    patched[3] = bound;

    // The new output joins the entry point's interface
    copyUpTo(entryPointOffset);
    const std::uint32_t entryWordCount = spirv[entryPointOffset] >> 16;
    patched.push_back(((entryWordCount + 1) << 16) | spvOpEntryPoint);
    for (std::size_t i = 1; i < entryWordCount; ++i)
        patched.push_back(spirv[entryPointOffset + i]);
    patched.push_back(variableId);
    cursor = entryPointOffset + entryWordCount;

    copyUpTo(annotationInsert);
    patched.insert(patched.end(), {(4u << 16) | spvOpDecorate, variableId, spvDecorationBuiltIn, spvBuiltInPointSize});

    copyUpTo(typeInsert);
    if (createFloat)
        patched.insert(patched.end(), {(3u << 16) | spvOpTypeFloat, floatTypeId, 32u});
    if (createPointer)
        patched.insert(patched.end(), {(4u << 16) | spvOpTypePointer, pointerTypeId, spvStorageClassOutput, floatTypeId});
    if (createConstant)
        patched.insert(patched.end(), {(4u << 16) | spvOpConstant, floatTypeId, constantOneId, spvFloatOneBits});
    patched.insert(patched.end(), {(4u << 16) | spvOpVariable, pointerTypeId, variableId, spvStorageClassOutput});

    copyUpTo(storeInsert);
    patched.insert(patched.end(), {(3u << 16) | spvOpStore, variableId, constantOneId});

    copyUpTo(spirv.size());

    return patched;
}

#ifdef SFML_VULKAN_RUNTIME_SHADER_COMPILER
// glslang has process-wide state that has to be initialized once
void ensureGlslangInitialized()
{
    static std::once_flag flag;
    std::call_once(flag, [] { glslang::InitializeProcess(); });
}
#endif
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
VulkanShaderImpl::VulkanShaderImpl(VulkanGraphicsDevice& device) : m_device(device), m_revision(nextShaderRevision())
{
}


////////////////////////////////////////////////////////////
VulkanShaderImpl::~VulkanShaderImpl()
{
    destroy();
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::destroy()
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    VkDevice device = m_device.getDevice();
    if (!device)
        return;

    if (m_shaderId != 0)
        m_device.clearShaderPipelines(m_shaderId);

    if (m_vertexModule)
        m_device.fn().vkDestroyShaderModule(device, m_vertexModule, nullptr);
    if (m_pointVertexModule)
        m_device.fn().vkDestroyShaderModule(device, m_pointVertexModule, nullptr);
    if (m_geometryModule)
        m_device.fn().vkDestroyShaderModule(device, m_geometryModule, nullptr);
    if (m_fragmentModule)
        m_device.fn().vkDestroyShaderModule(device, m_fragmentModule, nullptr);

    // Modules are only needed while pipelines are built, but recorded commands
    // still reference the layouts they were bound through, so those follow the
    // pipelines into the frame's deferred destruction
    m_device.deferDestroyShaderLayouts(m_pipelineLayout, m_setLayout);

    m_vertexModule      = VK_NULL_HANDLE;
    m_pointVertexModule = VK_NULL_HANDLE;
    m_geometryModule    = VK_NULL_HANDLE;
    m_fragmentModule    = VK_NULL_HANDLE;
    m_pipelineLayout    = VK_NULL_HANDLE;
    m_setLayout         = VK_NULL_HANDLE;
    m_shaderId          = 0;

    m_blocks.clear();
    m_textureSlots.clear();
    m_samplerSlots.clear();
    m_matricesBinding      = VK_ATTACHMENT_UNUSED;
    m_matricesStages       = 0;
    m_hasUserFragmentStage = false;
}


////////////////////////////////////////////////////////////
// The stage only selects the language of the runtime compiler, bytecode needs none
bool VulkanShaderImpl::toSpirv(std::string_view                       code,
                               [[maybe_unused]] VkShaderStageFlagBits stage,
                               std::vector<std::uint32_t>&            spirv)
{
    if (isSpirv(code))
    {
        spirv.resize(code.size() / 4);
        std::memcpy(spirv.data(), code.data(), code.size());
        return true;
    }

#ifdef SFML_VULKAN_RUNTIME_SHADER_COMPILER
    ensureGlslangInitialized();

    EShLanguage language = EShLangVertex;
    if (stage == VK_SHADER_STAGE_GEOMETRY_BIT)
        language = EShLangGeometry;
    else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        language = EShLangFragment;

    glslang::TShader shader(language);

    const char* source = code.data();
    const int   length = static_cast<int>(code.size());
    shader.setStringsWithLengths(&source, &length, 1);
    shader.setEntryPoint("main");
    shader.setSourceEntryPoint("main");
    shader.setEnvInput(glslang::EShSourceHlsl, language, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    shader.setAutoMapBindings(true);
    shader.setAutoMapLocations(true);

    const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl);

    if (!shader.parse(GetDefaultResources(), 110, false, messages))
    {
        err() << "Failed to compile shader:" << '\n' << shader.getInfoLog() << std::endl;
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages) || !program.mapIO())
    {
        err() << "Failed to link shader:" << '\n' << program.getInfoLog() << std::endl;
        return false;
    }

    spv::SpvBuildLogger logger;
    glslang::SpvOptions options;
    options.disableOptimizer = false;
    glslang::GlslangToSpv(*program.getIntermediate(language), spirv, &logger, &options);

    return !spirv.empty();
#else
    err() << "Failed to load shader: the Vulkan backend was built without the runtime shader compiler, "
             "provide SPIR-V bytecode compiled offline (for example with dxc -spirv) or rebuild SFML "
             "with SFML_VULKAN_RUNTIME_SHADER_COMPILER=ON"
          << std::endl;
    return false;
#endif
}


////////////////////////////////////////////////////////////
bool VulkanShaderImpl::reflectStage(std::vector<std::uint32_t>& spirv, VkShaderStageFlagBits stage)
{
    SpvReflectShaderModule module{};
    if (spvReflectCreateShaderModule(spirv.size() * sizeof(std::uint32_t), spirv.data(), &module) !=
        SPV_REFLECT_RESULT_SUCCESS)
    {
        err() << "Failed to reflect the shader bytecode" << std::endl;
        return false;
    }

    // The whole push constant block is reserved for the built-in matrices, and
    // the guaranteed 128 byte minimum leaves no room to relocate a user one
    std::uint32_t pushConstantCount = 0;
    spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, nullptr);

    if (pushConstantCount > 0)
    {
        err() << "Failed to load shader: push constants are reserved by the Vulkan backend, "
                 "declare the data as a constant buffer instead"
              << std::endl;
        spvReflectDestroyShaderModule(&module);
        return false;
    }

    std::uint32_t bindingCount = 0;
    spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr);

    std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
    spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data());

    bool ok = true;

    // Reserved bindings: b0 always belongs to SFMLMatrices; t0/s0 belong to the
    // draw's texture while the built-in fragment stage is in use
    const auto assignBinding =
        [&](std::uint32_t base, std::uint32_t registerIndex, bool reserveFirst, auto&& isTaken) -> std::uint32_t
    {
        std::uint32_t slot = registerIndex;
        while (slot < bindingRangeSize)
        {
            const std::uint32_t candidate = base + slot;
            if ((!reserveFirst || (slot != 0)) && !isTaken(candidate))
                return candidate;
            ++slot;
        }
        return VK_ATTACHMENT_UNUSED;
    };

    for (SpvReflectDescriptorBinding* binding : bindings)
    {
        if (!binding)
            continue;

        const char* variableName = binding->name;
        const char* typeName     = binding->type_description ? binding->type_description->type_name : nullptr;
        std::string name = (variableName && variableName[0] != '\0') ? variableName : (typeName ? typeName : "");

        // Every resource is declared and written as a single descriptor, and
        // the uniform interface addresses one resource per name, so an arrayed
        // binding could only produce a layout its own bytecode disagrees with
        if ((binding->array.dims_count > 0) || (binding->count > 1))
        {
            err() << "Failed to load shader: arrays of resources are not supported, '" << name
                  << "' has to be declared as separate resources" << std::endl;
            ok = false;
            break;
        }

        std::uint32_t newBinding = VK_ATTACHMENT_UNUSED;

        switch (binding->descriptor_type)
        {
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            {
                // The SFML matrices block keeps its reserved binding and is fed by the backend
                if ((name == "SFMLMatrices") || (typeName && (std::string_view(typeName) == "SFMLMatrices")))
                {
                    // The backend uploads a block of a fixed layout; a larger
                    // one would have the shader read past what was written
                    constexpr std::uint32_t matricesSize = 48 * sizeof(float);
                    const std::uint32_t declaredSize = std::max(binding->block.padded_size, binding->block.size);

                    if (declaredSize > matricesSize)
                    {
                        err() << "Failed to load shader: the SFMLMatrices block is " << declaredSize
                              << " bytes, the renderer provides at most " << matricesSize << std::endl;
                        ok = false;
                        break;
                    }

                    newBinding        = bufferBindingBase;
                    m_matricesBinding = newBinding;
                    m_matricesStages |= static_cast<VkShaderStageFlags>(stage);
                    break;
                }

                // Constant buffers are per-stage resources like on Direct3D:
                // every stage's block gets its own buffer, even under the same
                // name, and members are fed by name into each of them. This
                // matters for the compiler-generated $Globals blocks, whose
                // contents are unrelated between the stages.
                const auto bufferTaken = [&](std::uint32_t candidate)
                {
                    if (candidate == m_matricesBinding)
                        return true;
                    return std::any_of(m_blocks.begin(),
                                       m_blocks.end(),
                                       [&](const UniformBlock& block) { return block.binding == candidate; });
                };

                newBinding = assignBinding(bufferBindingBase, binding->binding, true, bufferTaken);
                if (newBinding == VK_ATTACHMENT_UNUSED)
                {
                    err() << "Too many constant buffers in shader" << std::endl;
                    ok = false;
                    break;
                }

                UniformBlock block;
                block.name    = name;
                block.binding = newBinding;
                block.stages  = static_cast<VkShaderStageFlags>(stage);
                block.shadow.assign(std::max<std::uint32_t>(binding->block.padded_size, binding->block.size), std::byte{0});

                for (std::uint32_t i = 0; i < binding->block.member_count; ++i)
                {
                    const SpvReflectBlockVariable& member = binding->block.members[i];
                    if (member.name)
                        block.variables.emplace(member.name,
                                                UniformBlock::Variable{member.offset,
                                                                       std::max(member.size, member.padded_size)});
                }

                m_blocks.push_back(std::move(block));
                break;
            }

            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            {
                auto it = std::find_if(m_textureSlots.begin(),
                                       m_textureSlots.end(),
                                       [&](const TextureBinding& slot) { return slot.name == name && !name.empty(); });
                if (it != m_textureSlots.end())
                {
                    it->stages |= static_cast<VkShaderStageFlags>(stage);
                    newBinding = it->binding;
                    break;
                }

                const auto textureTaken = [&](std::uint32_t candidate)
                {
                    return std::any_of(m_textureSlots.begin(),
                                       m_textureSlots.end(),
                                       [&](const TextureBinding& slot) { return slot.binding == candidate; });
                };

                newBinding = assignBinding(textureBindingBase, binding->binding, !m_hasUserFragmentStage, textureTaken);
                if (newBinding == VK_ATTACHMENT_UNUSED)
                {
                    err() << "Too many textures in shader" << std::endl;
                    ok = false;
                    break;
                }

                TextureBinding slot;
                slot.name          = name;
                slot.binding       = newBinding;
                slot.registerIndex = binding->binding;
                slot.stages        = static_cast<VkShaderStageFlags>(stage);
                slot.combined      = (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                m_textureSlots.push_back(std::move(slot));
                break;
            }

            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            {
                auto it = std::find_if(m_samplerSlots.begin(),
                                       m_samplerSlots.end(),
                                       [&](const SamplerBinding& slot) { return slot.registerIndex == binding->binding; });
                if (it != m_samplerSlots.end())
                {
                    it->stages |= static_cast<VkShaderStageFlags>(stage);
                    newBinding = it->binding;
                    break;
                }

                const auto samplerTaken = [&](std::uint32_t candidate)
                {
                    return std::any_of(m_samplerSlots.begin(),
                                       m_samplerSlots.end(),
                                       [&](const SamplerBinding& slot) { return slot.binding == candidate; });
                };

                newBinding = assignBinding(samplerBindingBase, binding->binding, !m_hasUserFragmentStage, samplerTaken);
                if (newBinding == VK_ATTACHMENT_UNUSED)
                {
                    err() << "Too many samplers in shader" << std::endl;
                    ok = false;
                    break;
                }

                SamplerBinding slot;
                slot.binding       = newBinding;
                slot.registerIndex = binding->binding;
                slot.stages        = static_cast<VkShaderStageFlags>(stage);
                m_samplerSlots.push_back(slot);
                break;
            }

            default:
                err() << "Unsupported resource type in shader, only constant buffers, textures and samplers are "
                         "supported"
                      << std::endl;
                ok = false;
                break;
        }

        if (ok && (newBinding != VK_ATTACHMENT_UNUSED))
        {
            if (spvReflectChangeDescriptorBindingNumbers(&module, binding, newBinding, 0) != SPV_REFLECT_RESULT_SUCCESS)
            {
                err() << "Failed to relocate a shader resource binding" << std::endl;
                ok = false;
            }
        }

        if (!ok)
            break;
    }

    if (ok)
    {
        // The module holds the bytecode with the final binding numbers
        const std::size_t codeSize = spvReflectGetCodeSize(&module);
        spirv.resize(codeSize / sizeof(std::uint32_t));
        std::memcpy(spirv.data(), spvReflectGetCode(&module), codeSize);
    }

    spvReflectDestroyShaderModule(&module);

    return ok;
}


////////////////////////////////////////////////////////////
bool VulkanShaderImpl::createLayouts()
{
    VkDevice device = m_device.getDevice();

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;

    // The fragment stage kept from the built-in pipeline brings its fixed
    // resources: the draw's texture pair at t0/s0. The built-in vertex stage
    // needs nothing here, it reads the matrices from the push constants.
    if (!m_hasUserFragmentStage)
    {
        TextureBinding textureSlot;
        textureSlot.binding       = textureBindingBase;
        textureSlot.registerIndex = 0;
        textureSlot.stages        = VK_SHADER_STAGE_FRAGMENT_BIT;
        textureSlot.builtin       = true;
        m_textureSlots.push_back(std::move(textureSlot));

        SamplerBinding samplerSlot;
        samplerSlot.binding       = samplerBindingBase;
        samplerSlot.registerIndex = 0;
        samplerSlot.stages        = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerSlot.builtin       = true;
        m_samplerSlots.push_back(samplerSlot);
    }

    if (m_matricesBinding != VK_ATTACHMENT_UNUSED)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding         = m_matricesBinding;
        binding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags      = m_matricesStages;
        layoutBindings.push_back(binding);
    }

    for (const UniformBlock& block : m_blocks)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding         = block.binding;
        binding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags      = block.stages;
        layoutBindings.push_back(binding);
    }

    for (const TextureBinding& slot : m_textureSlots)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = slot.binding;
        binding.descriptorType = slot.combined ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        binding.descriptorCount = 1;
        binding.stageFlags      = slot.stages;
        layoutBindings.push_back(binding);
    }

    for (const SamplerBinding& slot : m_samplerSlots)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding         = slot.binding;
        binding.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags      = slot.stages;
        layoutBindings.push_back(binding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(layoutBindings.size());
    layoutInfo.pBindings    = layoutBindings.data();

    if (!vkCheck(m_device.fn().vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_setLayout)))
        return false;

    // A shader that only replaces the fragment stage is drawn with the built-in
    // vertex stage, which reads its matrices from push constants, so every user
    // layout has to declare that range too
    const VkPushConstantRange matricesRange = VulkanGraphicsDevice::builtinMatricesRange();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &m_setLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &matricesRange;

    return vkCheck(m_device.fn().vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));
}


////////////////////////////////////////////////////////////
bool VulkanShaderImpl::compile(std::string_view vertexShaderCode,
                               std::string_view geometryShaderCode,
                               std::string_view fragmentShaderCode)
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    VkDevice device = m_device.getDevice();
    if (!device)
        return false;

    destroy();

    m_hasUserFragmentStage = !fragmentShaderCode.empty();

    struct StageBuild
    {
        std::string_view      code;
        VkShaderStageFlagBits stage;
        VkShaderModule*       module;
    };

    const std::array<StageBuild, 3> stages = {{{vertexShaderCode, VK_SHADER_STAGE_VERTEX_BIT, &m_vertexModule},
                                               {geometryShaderCode, VK_SHADER_STAGE_GEOMETRY_BIT, &m_geometryModule},
                                               {fragmentShaderCode, VK_SHADER_STAGE_FRAGMENT_BIT, &m_fragmentModule}}};

    for (const StageBuild& build : stages)
    {
        if (build.code.empty())
            continue;

        std::vector<std::uint32_t> spirv;
        if (!toSpirv(build.code, build.stage, spirv) || !reflectStage(spirv, build.stage))
        {
            destroy();
            return false;
        }

        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = spirv.size() * sizeof(std::uint32_t);
        moduleInfo.pCode    = spirv.data();

        if (!vkCheck(m_device.fn().vkCreateShaderModule(device, &moduleInfo, nullptr, build.module)))
        {
            destroy();
            return false;
        }

        // Without the maintenance5 point size default, point pipelines need a
        // vertex module variant that writes the PointSize builtin themselves.
        // A vertex stage is only the last one before rasterization when no
        // geometry stage follows, so the patch lives in a separate module the
        // pipeline picks for point draws
        if ((build.stage == VK_SHADER_STAGE_VERTEX_BIT) && !m_device.hasMaintenance5())
        {
            const std::vector<std::uint32_t> patched = injectPointSize(spirv);
            if (!patched.empty())
            {
                moduleInfo.codeSize = patched.size() * sizeof(std::uint32_t);
                moduleInfo.pCode    = patched.data();

                if (!vkCheck(m_device.fn().vkCreateShaderModule(device, &moduleInfo, nullptr, &m_pointVertexModule)))
                {
                    destroy();
                    return false;
                }
            }
        }
    }

    if (!createLayouts())
    {
        destroy();
        return false;
    }

    m_shaderId = nextShaderId();
    m_revision = nextShaderRevision();

    return true;
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::writeUniform(const std::string& name,
                                    const void*        data,
                                    std::size_t        elementSize,
                                    std::size_t        elementStride,
                                    std::size_t        elementCount)
{
    bool found = false;

    for (UniformBlock& block : m_blocks)
    {
        const auto it = block.variables.find(name);
        if (it == block.variables.end())
            continue;

        found = true;

        const auto* source      = static_cast<const std::byte*>(data);
        std::size_t destination = it->second.offset;

        for (std::size_t i = 0; i < elementCount; ++i)
        {
            // Never write past the reflected size of the variable
            if (destination + elementSize > it->second.offset + it->second.size)
                break;

            std::memcpy(block.shadow.data() + destination, source, elementSize);
            source += elementSize;
            destination += elementStride;
        }
    }

    m_revision = nextShaderRevision();

    if (!found && m_warnedUniforms.emplace(name).second)
        err() << "Uniform \"" << name << "\" not found in shader" << std::endl;
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, float x)
{
    writeUniform(name, &x, sizeof(float), sizeof(float), 1);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, Glsl::Vec2 vector)
{
    const std::array data = {vector.x, vector.y};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, const Glsl::Vec3& vector)
{
    const std::array data = {vector.x, vector.y, vector.z};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, const Glsl::Vec4& vector)
{
    const std::array data = {vector.x, vector.y, vector.z, vector.w};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, int x)
{
    writeUniform(name, &x, sizeof(int), sizeof(int), 1);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, Glsl::Ivec2 vector)
{
    const std::array data = {vector.x, vector.y};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, const Glsl::Ivec3& vector)
{
    const std::array data = {vector.x, vector.y, vector.z};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, const Glsl::Ivec4& vector)
{
    const std::array data = {vector.x, vector.y, vector.z, vector.w};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, bool x)
{
    // Booleans are stored as 32 bit values in HLSL constant buffers
    setUniform(name, x ? 1 : 0);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, Glsl::Bvec2 vector)
{
    setUniform(name, Glsl::Ivec2(vector.x ? 1 : 0, vector.y ? 1 : 0));
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, const Glsl::Bvec3& vector)
{
    setUniform(name, Glsl::Ivec3(vector.x ? 1 : 0, vector.y ? 1 : 0, vector.z ? 1 : 0));
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, const Glsl::Bvec4& vector)
{
    setUniform(name, Glsl::Ivec4(vector.x ? 1 : 0, vector.y ? 1 : 0, vector.z ? 1 : 0, vector.w ? 1 : 0));
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, const Glsl::Mat3& matrix)
{
    // float3x3 columns are padded to 16 bytes in HLSL constant buffers
    writeUniform(name, matrix.array.data(), sizeof(float) * 3, sizeof(float) * 4, 3);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, const Glsl::Mat4& matrix)
{
    writeUniform(name, matrix.array.data(), sizeof(float) * 16, sizeof(float) * 16, 1);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniform(const std::string& name, const Texture& texture)
{
    m_textures[name] = &texture;
    m_revision       = nextShaderRevision();
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setCurrentTextureUniform(const std::string& name)
{
    m_currentTextureName = name;
    m_revision           = nextShaderRevision();
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniformArray(const std::string& name, const float* scalarArray, std::size_t length)
{
    // Array elements are aligned to 16 bytes in HLSL constant buffers
    writeUniform(name, scalarArray, sizeof(float), sizeof(float) * 4, length);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length)
{
    writeUniform(name, vectorArray, sizeof(Glsl::Vec2), sizeof(float) * 4, length);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length)
{
    writeUniform(name, vectorArray, sizeof(Glsl::Vec3), sizeof(float) * 4, length);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length)
{
    writeUniform(name, vectorArray, sizeof(float) * 4, sizeof(float) * 4, length);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length)
{
    bool found = false;

    for (UniformBlock& block : m_blocks)
    {
        const auto variableIt = block.variables.find(name);
        if (variableIt == block.variables.end())
            continue;

        found = true;

        // float3x3 array elements are 3 columns padded to 16 bytes each
        constexpr std::size_t matrixStride = sizeof(float) * 12;

        for (std::size_t i = 0; i < length; ++i)
        {
            for (std::size_t column = 0; column < 3; ++column)
            {
                const std::size_t destination = variableIt->second.offset + i * matrixStride + column * sizeof(float) * 4;
                if (destination + sizeof(float) * 3 > variableIt->second.offset + variableIt->second.size)
                    break;

                std::memcpy(block.shadow.data() + destination, matrixArray[i].array.data() + column * 3, sizeof(float) * 3);
            }
        }
    }

    m_revision = nextShaderRevision();

    if (!found && m_warnedUniforms.emplace(name).second)
        err() << "Uniform \"" << name << "\" not found in shader" << std::endl;
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::setUniformArray(const std::string& name, const Glsl::Mat4* matrixArray, std::size_t length)
{
    writeUniform(name, matrixArray, sizeof(float) * 16, sizeof(float) * 16, length);
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::prepareTextures()
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    // Bring the assigned textures into their sampling layout before the draw's
    // render pass opens, transitions are illegal inside one
    for (const auto& [name, texture] : m_textures)
    {
        if (auto* impl = static_cast<VulkanTextureImpl*>(getTextureImpl(*texture)))
            impl->prepareForSampling();
    }
}


////////////////////////////////////////////////////////////
void VulkanShaderImpl::bind() const
{
    // The draw path uses bindForDraw, which reports whether the shader ended
    // up bound; this override exists for the neutral interface
    static_cast<void>(bindForDraw());
}


////////////////////////////////////////////////////////////
bool VulkanShaderImpl::bindForDraw() const
{
    const VulkanGraphicsDevice::ContextLock lock(m_device);

    VkCommandBuffer commandBuffer = m_device.getRenderCommandBuffer();
    if (!commandBuffer || !m_pipelineLayout)
        return false;

    // Register the modules for the pipeline of the draw
    m_device.setPendingUserPipelineSource(m_vertexModule,
                                          m_pointVertexModule,
                                          m_geometryModule,
                                          m_fragmentModule,
                                          m_pipelineLayout,
                                          m_shaderId);

    // A fresh descriptor set is written on every bind: binds only happen when
    // the shader's state stamp changed or the program switched, and sets are
    // recycled with their frame either way
    VkDescriptorSet set = m_device.allocateDescriptorSet(m_setLayout);
    if (!set)
        return false;

    std::vector<VkWriteDescriptorSet>   writes;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo>  imageInfos;
    writes.reserve(2 + m_blocks.size() + m_textureSlots.size() + m_samplerSlots.size());
    bufferInfos.reserve(1 + m_blocks.size());
    imageInfos.reserve(m_textureSlots.size() + m_samplerSlots.size());

    // The SFML matrices come from the render target's pending constants
    if (m_matricesBinding != VK_ATTACHMENT_UNUSED)
    {
        VulkanTransientAllocation constants;
        if (!m_device.uploadPendingConstants(constants))
            return false;

        bufferInfos.push_back({constants.buffer, constants.offset, sizeof(float) * 48});

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = set;
        write.dstBinding      = m_matricesBinding;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo     = &bufferInfos.back();
        writes.push_back(write);
    }

    // Upload the shadow copies of the constant buffers
    for (const UniformBlock& block : m_blocks)
    {
        VulkanTransientAllocation allocation;
        if (!m_device.allocateTransient(block.shadow.data(), block.shadow.size(), m_device.getUniformBufferAlignment(), allocation))
            return false;

        bufferInfos.push_back({allocation.buffer, allocation.offset, block.shadow.size()});

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = set;
        write.dstBinding      = block.binding;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo     = &bufferInfos.back();
        writes.push_back(write);
    }

    // Resolve the texture of a slot: an assigned texture by name, everything
    // else samples the draw's texture like an unset GLSL sampler does
    const auto resolveTexture = [&](const TextureBinding& slot) -> const Texture*
    {
        if (slot.name.empty() || (slot.name == m_currentTextureName))
            return nullptr;

        const auto it = m_textures.find(slot.name);
        return (it != m_textures.end()) ? it->second : nullptr;
    };

    for (const TextureBinding& slot : m_textureSlots)
    {
        VkImageView   view    = VK_NULL_HANDLE;
        VkImageLayout layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkSampler     sampler = VK_NULL_HANDLE;

        if (const Texture* texture = resolveTexture(slot))
        {
            if (const auto* impl = static_cast<const VulkanTextureImpl*>(getTextureImpl(*texture)))
            {
                view   = impl->getImageView();
                layout = impl->getSampleLayout();
            }
            sampler = m_device.getSamplerState(texture->isSmooth(), texture->isRepeated(), textureHasMipmap(*texture));
        }
        else
        {
            view    = m_device.getCurrentTextureView();
            layout  = m_device.getCurrentTextureLayout();
            sampler = m_device.getCurrentTextureSampler();
        }

        if (!view)
        {
            view   = m_device.getWhiteTextureView();
            layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        if (!sampler)
            sampler = m_device.getSamplerState(false, false);

        imageInfos.push_back({slot.combined ? sampler : VK_NULL_HANDLE, view, layout});

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = set;
        write.dstBinding      = slot.binding;
        write.descriptorCount = 1;
        write.descriptorType = slot.combined ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &imageInfos.back();
        writes.push_back(write);
    }

    // Samplers pair with the texture of the same register index. The built-in
    // pair is matched on its own: a user texture can carry register index 0
    // too, and would otherwise lend its filter states to the draw's texture.
    for (const SamplerBinding& slot : m_samplerSlots)
    {
        VkSampler sampler = VK_NULL_HANDLE;

        const auto pairedIt = std::find_if(m_textureSlots.begin(),
                                           m_textureSlots.end(),
                                           [&](const TextureBinding& texture) {
                                               return (texture.builtin == slot.builtin) &&
                                                      (texture.registerIndex == slot.registerIndex);
                                           });
        if (pairedIt != m_textureSlots.end())
        {
            if (const Texture* texture = resolveTexture(*pairedIt))
                sampler = m_device.getSamplerState(texture->isSmooth(), texture->isRepeated(), textureHasMipmap(*texture));
        }

        if (!sampler)
            sampler = m_device.getCurrentTextureSampler();
        if (!sampler)
            sampler = m_device.getSamplerState(false, false);

        imageInfos.push_back({sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = set;
        write.dstBinding      = slot.binding;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo      = &imageInfos.back();
        writes.push_back(write);
    }

    m_device.fn().vkUpdateDescriptorSets(m_device.getDevice(), static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);

    m_device.fn().vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &set, 0, nullptr);

    return true;
}


////////////////////////////////////////////////////////////
unsigned int VulkanShaderImpl::getNativeHandle() const
{
    return 0;
}


////////////////////////////////////////////////////////////
std::uint64_t VulkanShaderImpl::getPipelineStateId() const
{
    // Combined with the revision, the texture identities catch changes the
    // revision cannot see, like mipmaps being generated or filtering being toggled
    std::uint64_t id = m_revision;
    for (const auto& texture : m_textures)
        id = (id * 31) + (getTextureCacheId(*texture.second) * 4) + (texture.second->isSmooth() ? 2u : 0u) +
             (texture.second->isRepeated() ? 1u : 0u);

    return id;
}


////////////////////////////////////////////////////////////
bool VulkanShaderImpl::bindsExternalTextures() const
{
    return !m_textures.empty();
}

} // namespace sf::priv
