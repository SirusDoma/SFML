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
#include <SFML/Graphics/Metal/MetalShaderImpl.hpp>
#include <SFML/Graphics/Metal/MetalTextureImpl.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <atomic>
#include <ostream>

#include <cstring>

// MTLBinding replaced MTLArgument in macOS 13, the deprecated API is used
// so shader reflection keeps working on the older systems SFML supports
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"


namespace
{
// Stamps identify shader state across all shaders and are never reused
std::atomic<std::uint64_t>& getRevisionCounter()
{
    static std::atomic<std::uint64_t> counter(1);
    return counter;
}

// Identifies a compiled function pair in the pipeline state cache
std::atomic<std::uint32_t>& getShaderIdCounter()
{
    static std::atomic<std::uint32_t> counter(1);
    return counter;
}
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
MetalShaderImpl::MetalShaderImpl(MetalGraphicsDevice& device) :
    m_device(device),
    m_revision(getRevisionCounter().fetch_add(1, std::memory_order_relaxed))
{
}


////////////////////////////////////////////////////////////
bool MetalShaderImpl::compile(std::string_view vertexShaderCode,
                              std::string_view geometryShaderCode,
                              std::string_view fragmentShaderCode)
{
    m_vertexStage   = {};
    m_fragmentStage = {};
    m_textures.clear();
    m_currentTextureName.clear();
    m_warnedUniforms.clear();
    m_shaderId = 0;

    if (!m_device.getDevice())
        return false;

    if (!geometryShaderCode.empty())
    {
        err() << "Failed to create shader: geometry shaders are not supported by the Metal backend" << std::endl;
        return false;
    }

    if (!vertexShaderCode.empty() && !compileStage(vertexShaderCode, false, m_vertexStage))
        return false;

    if (!fragmentShaderCode.empty() && !compileStage(fragmentShaderCode, true, m_fragmentStage))
        return false;

    // Reflect the pair against a canonical format; a stage the user did not provide
    // is filled in by the built-in pipeline, like at draw time
    @autoreleasepool
    {
        MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
        descriptor.vertexFunction   = m_vertexStage.function ? m_vertexStage.function.get()
                                                             : m_device.getDefaultVertexFunction();
        descriptor.fragmentFunction = m_fragmentStage.function ? m_fragmentStage.function.get()
                                                               : m_device.getDefaultFragmentFunction();
        descriptor.vertexDescriptor = m_device.makeVertexDescriptor();
        descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        MTLRenderPipelineReflection* reflection = nil;
        NSError*                     error      = nil;

        const NSPtr<MetalRenderPipelinePtr> pipeline(
            [m_device.getDevice() newRenderPipelineStateWithDescriptor:descriptor
                                                               options:MTLPipelineOptionArgumentInfo |
                                                                       MTLPipelineOptionBufferTypeInfo
                                                            reflection:&reflection
                                                                 error:&error]);
        [descriptor release];

        if (!pipeline)
        {
            err() << "Failed to create shader pipeline: "
                  << (error ? [[error localizedDescription] UTF8String] : "unknown error") << std::endl;
            return false;
        }

        reflect(reflection);
    }

    m_shaderId = getShaderIdCounter().fetch_add(1, std::memory_order_relaxed);
    m_revision = getRevisionCounter().fetch_add(1, std::memory_order_relaxed);

    return true;
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, float x)
{
    writeUniform(name, &x, sizeof(x), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, Glsl::Vec2 vector)
{
    writeUniform(name, &vector, sizeof(vector), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, const Glsl::Vec3& vector)
{
    writeUniform(name, &vector, sizeof(vector), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, const Glsl::Vec4& vector)
{
    writeUniform(name, &vector, sizeof(vector), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, int x)
{
    writeUniform(name, &x, sizeof(x), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, Glsl::Ivec2 vector)
{
    writeUniform(name, &vector, sizeof(vector), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, const Glsl::Ivec3& vector)
{
    writeUniform(name, &vector, sizeof(vector), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, const Glsl::Ivec4& vector)
{
    writeUniform(name, &vector, sizeof(vector), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, bool x)
{
    // MSL bools are single bytes
    const std::uint8_t value = x ? 1 : 0;
    writeUniform(name, &value, sizeof(value), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, Glsl::Bvec2 vector)
{
    const std::array<std::uint8_t, 2> value = {vector.x ? std::uint8_t{1} : std::uint8_t{0},
                                               vector.y ? std::uint8_t{1} : std::uint8_t{0}};
    writeUniform(name, value.data(), value.size(), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, const Glsl::Bvec3& vector)
{
    const std::array<std::uint8_t, 3> value = {vector.x ? std::uint8_t{1} : std::uint8_t{0},
                                               vector.y ? std::uint8_t{1} : std::uint8_t{0},
                                               vector.z ? std::uint8_t{1} : std::uint8_t{0}};
    writeUniform(name, value.data(), value.size(), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, const Glsl::Bvec4& vector)
{
    const std::array<std::uint8_t, 4> value = {vector.x ? std::uint8_t{1} : std::uint8_t{0},
                                               vector.y ? std::uint8_t{1} : std::uint8_t{0},
                                               vector.z ? std::uint8_t{1} : std::uint8_t{0},
                                               vector.w ? std::uint8_t{1} : std::uint8_t{0}};
    writeUniform(name, value.data(), value.size(), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, const Glsl::Mat3& matrix)
{
    setUniformArray(name, &matrix, 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, const Glsl::Mat4& matrix)
{
    writeUniform(name, matrix.array.data(), sizeof(matrix.array), 1);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniform(const std::string& name, const Texture& texture)
{
    m_textures[name] = &texture;
    m_revision       = getRevisionCounter().fetch_add(1, std::memory_order_relaxed);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setCurrentTextureUniform(const std::string& name)
{
    m_currentTextureName = name;
    m_revision           = getRevisionCounter().fetch_add(1, std::memory_order_relaxed);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniformArray(const std::string& name, const float* scalarArray, std::size_t length)
{
    writeUniform(name, scalarArray, sizeof(float), length);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length)
{
    writeUniform(name, vectorArray, sizeof(Glsl::Vec2), length);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length)
{
    writeUniform(name, vectorArray, sizeof(Glsl::Vec3), length);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length)
{
    writeUniform(name, vectorArray, sizeof(Glsl::Vec4), length);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length)
{
    // MSL float3x3 columns are 16-byte aligned, expand the packed source columns
    std::vector<float> expanded(length * 12);
    for (std::size_t element = 0; element < length; ++element)
        for (std::size_t column = 0; column < 3; ++column)
            std::memcpy(expanded.data() + element * 12 + column * 4,
                        matrixArray[element].array.data() + column * 3,
                        sizeof(float) * 3);

    writeUniform(name, expanded.data(), sizeof(float) * 12, length);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::setUniformArray(const std::string& name, const Glsl::Mat4* matrixArray, std::size_t length)
{
    writeUniform(name, matrixArray, sizeof(Glsl::Mat4), length);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::bind() const
{
    if (!m_device.applyUserShaderPipeline(m_vertexStage.function.get(), m_fragmentStage.function.get(), m_shaderId))
        return;

    bindStage(m_vertexStage, false);
    bindStage(m_fragmentStage, true);
}


////////////////////////////////////////////////////////////
unsigned int MetalShaderImpl::getNativeHandle() const
{
    return 0;
}


////////////////////////////////////////////////////////////
std::uint64_t MetalShaderImpl::getPipelineStateId() const
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
bool MetalShaderImpl::bindsExternalTextures() const
{
    return !m_textures.empty();
}


////////////////////////////////////////////////////////////
bool MetalShaderImpl::compileStage(std::string_view code, bool fragment, Stage& stage)
{
    @autoreleasepool
    {
        NSString* source = [[NSString alloc] initWithBytes:code.data()
                                                    length:code.size()
                                                  encoding:NSUTF8StringEncoding];

        NSError* error = nil;
        stage.library.reset([m_device.getDevice() newLibraryWithSource:source options:nil error:&error]);
        [source release];

        if (!stage.library)
        {
            err() << "Failed to compile " << (fragment ? "fragment" : "vertex") << " shader:" << '\n'
                  << (error ? [[error localizedDescription] UTF8String] : "unknown error") << std::endl;
            return false;
        }

        // The entry function is the source's single function of the stage's kind, its name is free
        const MTLFunctionType type = fragment ? MTLFunctionTypeFragment : MTLFunctionTypeVertex;
        for (NSString* name in [stage.library.get() functionNames])
        {
            id<MTLFunction> function = [stage.library.get() newFunctionWithName:name];
            if (function && ([function functionType] == type))
            {
                stage.function.reset(function);
                break;
            }
            [function release];
        }

        if (!stage.function)
        {
            err() << "Failed to create " << (fragment ? "fragment" : "vertex") << " shader: "
                  << "the source contains no " << (fragment ? "fragment" : "vertex") << " function" << std::endl;
            return false;
        }
    }

    return true;
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::reflect(void* reflectionObject)
{
    auto* reflection = static_cast<MTLRenderPipelineReflection*>(reflectionObject);

    const auto parse = [](Stage& stage, NSArray<MTLArgument*>* arguments, bool fragment)
    {
        for (MTLArgument* argument in arguments)
        {
            switch ([argument type])
            {
                case MTLArgumentTypeBuffer:
                {
                    // Vertex buffer indices 0 and 1 hold the vertex data and the SFML matrices
                    if (!fragment && ([argument index] <= 1))
                        break;

                    if (([argument bufferDataType] != MTLDataTypeStruct) || ![argument bufferStructType])
                        break;

                    Stage::Block block;
                    block.index = static_cast<std::uint32_t>([argument index]);
                    block.shadow.resize([argument bufferDataSize]);

                    for (MTLStructMember* member in [[argument bufferStructType] members])
                    {
                        Stage::Variable variable;
                        variable.block  = stage.blocks.size();
                        variable.offset = [member offset];

                        if ([member dataType] == MTLDataTypeArray)
                        {
                            MTLArrayType* array   = [member arrayType];
                            variable.arrayStride  = [array stride];
                            variable.elementCount = [array arrayLength];
                            variable.dataType     = static_cast<std::uint32_t>([array elementType]);
                        }
                        else
                        {
                            variable.dataType = static_cast<std::uint32_t>([member dataType]);
                        }

                        stage.variables.emplace([[member name] UTF8String], variable);
                    }

                    stage.blocks.push_back(std::move(block));
                    break;
                }
                case MTLArgumentTypeTexture:
                    stage.textureSlots.emplace([[argument name] UTF8String],
                                               static_cast<std::uint32_t>([argument index]));
                    break;
                case MTLArgumentTypeSampler:
                    stage.samplerSlots.push_back(static_cast<std::uint32_t>([argument index]));
                    break;
                default:
                    break;
            }
        }
    };

    // Only user-provided stages are reflected, the built-in fill-ins have no user uniforms
    if (m_vertexStage.function)
        parse(m_vertexStage, [reflection vertexArguments], false);
    if (m_fragmentStage.function)
        parse(m_fragmentStage, [reflection fragmentArguments], true);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::writeUniform(const std::string& name, const void* data, std::size_t elementSize, std::size_t elementCount)
{
    bool written = false;

    for (Stage* stage : {&m_vertexStage, &m_fragmentStage})
    {
        const auto it = stage->variables.find(name);
        if (it == stage->variables.end())
            continue;

        const Stage::Variable& variable = it->second;
        auto&                  shadow   = stage->blocks[variable.block].shadow;

        const std::size_t stride = std::max(variable.arrayStride, elementSize);

        // Excess elements are dropped, writing past the variable would corrupt its neighbors
        for (std::size_t element = 0; element < std::min(elementCount, variable.elementCount); ++element)
        {
            const std::size_t offset = variable.offset + element * stride;
            if (offset + elementSize > shadow.size())
                break;

            std::memcpy(shadow.data() + offset, static_cast<const std::byte*>(data) + element * elementSize, elementSize);
        }

        written = true;
    }

    if (!written)
    {
        if (m_warnedUniforms.emplace(name).second)
            err() << "Uniform \"" << name << "\" not found in shader" << std::endl;
        return;
    }

    m_revision = getRevisionCounter().fetch_add(1, std::memory_order_relaxed);
}


////////////////////////////////////////////////////////////
void MetalShaderImpl::bindStage(const Stage& stage, bool fragment) const
{
    id<MTLRenderCommandEncoder> encoder = m_device.getRenderEncoder();
    if (!encoder)
        return;

    for (const Stage::Block& block : stage.blocks)
    {
        // Small data travels inside the command buffer, large arrays through a transient buffer
        if (block.shadow.size() <= 4096)
        {
            if (fragment)
                [encoder setFragmentBytes:block.shadow.data() length:block.shadow.size() atIndex:block.index];
            else
                [encoder setVertexBytes:block.shadow.data() length:block.shadow.size() atIndex:block.index];
        }
        else
        {
            id<MTLBuffer> buffer = [m_device.getDevice() newBufferWithBytes:block.shadow.data()
                                                                     length:block.shadow.size()
                                                                    options:MTLResourceStorageModeShared];
            if (fragment)
                [encoder setFragmentBuffer:buffer offset:0 atIndex:block.index];
            else
                [encoder setVertexBuffer:buffer offset:0 atIndex:block.index];
            [buffer release];
        }
    }

    for (const auto& [name, slot] : stage.textureSlots)
    {
        // An assigned texture is bound with its own sampler; the CurrentTexture uniform and
        // textures the shader declares without assigning one sample the draw's texture.
        // The CurrentTexture designation takes precedence over an assignment to the same
        // name, matching the bind order of the OpenGL backend.
        id<MTLTexture>      texture = m_device.getCurrentTextureView();
        id<MTLSamplerState> sampler = m_device.getCurrentTextureSampler();

        if (const auto it = (name != m_currentTextureName) ? m_textures.find(name) : m_textures.end();
            it != m_textures.end())
        {
            if (auto* impl = static_cast<MetalTextureImpl*>(getTextureImpl(*it->second)); impl && impl->getTexture())
            {
                texture = impl->getTexture();
                sampler = m_device.getSamplerState(it->second->isSmooth(),
                                                   it->second->isRepeated(),
                                                   textureHasMipmap(*it->second));
            }
        }

        if (fragment)
            [encoder setFragmentTexture:texture atIndex:slot];
        else
            [encoder setVertexTexture:texture atIndex:slot];

        // A sampler at the texture's index samples that texture
        if (std::find(stage.samplerSlots.begin(), stage.samplerSlots.end(), slot) != stage.samplerSlots.end())
        {
            if (fragment)
                [encoder setFragmentSamplerState:sampler atIndex:slot];
            else
                [encoder setVertexSamplerState:sampler atIndex:slot];
        }
    }

    // Samplers without a texture at their index sample with the draw's sampler
    for (const std::uint32_t slot : stage.samplerSlots)
    {
        const bool paired = std::any_of(stage.textureSlots.begin(),
                                        stage.textureSlots.end(),
                                        [slot](const auto& entry) { return entry.second == slot; });
        if (paired)
            continue;

        if (fragment)
            [encoder setFragmentSamplerState:m_device.getCurrentTextureSampler() atIndex:slot];
        else
            [encoder setVertexSamplerState:m_device.getCurrentTextureSampler() atIndex:slot];
    }
}

} // namespace sf::priv

#pragma clang diagnostic pop
