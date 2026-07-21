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
#include <SFML/Graphics/D3D11/D3D11GraphicsDevice.hpp>
#include <SFML/Graphics/D3D11/D3D11ShaderImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11TextureImpl.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <array>
#include <d3dcompiler.h>
#include <ostream>

#include <cstring>


namespace sf::priv
{
////////////////////////////////////////////////////////////
D3D11ShaderImpl::D3D11ShaderImpl(D3D11GraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
bool D3D11ShaderImpl::compile(std::string_view vertexShaderCode,
                              std::string_view geometryShaderCode,
                              std::string_view fragmentShaderCode)
{
    auto* device = m_device.getDevice();
    if (!device)
        return false;

    ComPtr<ID3DBlob> vertexBytecode;

    if (!vertexShaderCode.empty())
    {
        if (!compileStage(vertexShaderCode, "vs_4_0", m_vertexStage, vertexBytecode))
            return false;

        if (!d3dCheck(device->CreateVertexShader(vertexBytecode->GetBufferPointer(),
                                                 vertexBytecode->GetBufferSize(),
                                                 nullptr,
                                                 &m_vertexShader)))
            return false;

        // The user vertex shader must consume the sf::Vertex signature
        constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 3> layout = {
            {{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
             {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
             {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}}};

        if (!d3dCheck(device->CreateInputLayout(layout.data(),
                                                static_cast<UINT>(layout.size()),
                                                vertexBytecode->GetBufferPointer(),
                                                vertexBytecode->GetBufferSize(),
                                                &m_inputLayout)))
        {
            err() << "Failed to create the input layout for the vertex shader" << '\n'
                  << "Its input signature must be (float2 POSITION, float4 COLOR0, float2 TEXCOORD0)" << std::endl;
            return false;
        }
    }

    if (!geometryShaderCode.empty())
    {
        ComPtr<ID3DBlob> bytecode;
        if (!compileStage(geometryShaderCode, "gs_4_0", m_geometryStage, bytecode))
            return false;

        if (!d3dCheck(
                device->CreateGeometryShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &m_geometryShader)))
            return false;
    }

    if (!fragmentShaderCode.empty())
    {
        ComPtr<ID3DBlob> bytecode;
        if (!compileStage(fragmentShaderCode, "ps_4_0", m_pixelStage, bytecode))
            return false;

        if (!d3dCheck(
                device->CreatePixelShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &m_pixelShader)))
            return false;
    }

    return true;
}


////////////////////////////////////////////////////////////
bool D3D11ShaderImpl::compileStage(std::string_view code, const char* profile, Stage& stage, ComPtr<ID3DBlob>& bytecode)
{
    ComPtr<ID3DBlob> errors;

    if (FAILED(D3DCompile(code.data(),
                          code.size(),
                          nullptr,
                          nullptr,
                          D3D_COMPILE_STANDARD_FILE_INCLUDE,
                          "main",
                          profile,
                          D3DCOMPILE_OPTIMIZATION_LEVEL3,
                          0,
                          &bytecode,
                          &errors)))
    {
        err() << "Failed to compile shader:" << '\n'
              << (errors ? static_cast<const char*>(errors->GetBufferPointer()) : "unknown error") << std::endl;
        return false;
    }

    // Reflect the global constant buffer and the texture bindings
    ComPtr<ID3D11ShaderReflection> reflection;
    if (!d3dCheck(
            D3DReflect(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), IID_PPV_ARGS(reflection.GetAddressOf()))))
        return false;

    D3D11_SHADER_DESC shaderDesc{};
    if (!d3dCheck(reflection->GetDesc(&shaderDesc)))
        return false;

    for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
    {
        D3D11_SHADER_INPUT_BIND_DESC bindDesc{};
        if (FAILED(reflection->GetResourceBindingDesc(i, &bindDesc)))
            continue;

        if (bindDesc.Type == D3D_SIT_TEXTURE)
            stage.textureSlots.emplace(bindDesc.Name, bindDesc.BindPoint);
        else if ((bindDesc.Type == D3D_SIT_CBUFFER) && (std::strcmp(bindDesc.Name, "$Globals") == 0))
            stage.constantBufferSlot = bindDesc.BindPoint;
    }

    if (auto* globals = reflection->GetConstantBufferByName("$Globals"))
    {
        D3D11_SHADER_BUFFER_DESC bufferDesc{};
        if (SUCCEEDED(globals->GetDesc(&bufferDesc)) && (bufferDesc.Variables > 0))
        {
            for (UINT i = 0; i < bufferDesc.Variables; ++i)
            {
                auto* variable = globals->GetVariableByIndex(i);

                D3D11_SHADER_VARIABLE_DESC variableDesc{};
                if (SUCCEEDED(variable->GetDesc(&variableDesc)))
                    stage.variables.emplace(variableDesc.Name, Stage::Variable{variableDesc.StartOffset, variableDesc.Size});
            }

            // Constant buffer sizes must be multiples of 16
            stage.shadow.assign((bufferDesc.Size + 15u) & ~15u, std::byte{0});

            D3D11_BUFFER_DESC constantBufferDesc{};
            constantBufferDesc.ByteWidth      = static_cast<UINT>(stage.shadow.size());
            constantBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
            constantBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            if (!d3dCheck(m_device.getDevice()->CreateBuffer(&constantBufferDesc, nullptr, &stage.constantBuffer)))
                return false;
        }
    }

    return true;
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::writeUniform(const std::string& name,
                                   const void*        data,
                                   std::size_t        elementSize,
                                   std::size_t        elementStride,
                                   std::size_t        elementCount)
{
    bool found = false;

    for (auto* stage : {&m_vertexStage, &m_geometryStage, &m_pixelStage})
    {
        const auto it = stage->variables.find(name);
        if (it == stage->variables.end())
            continue;

        found = true;

        const auto* source      = static_cast<const std::byte*>(data);
        std::size_t destination = it->second.offset;

        for (std::size_t i = 0; i < elementCount; ++i)
        {
            // Never write past the reflected size of the variable
            if (destination + elementSize > it->second.offset + it->second.size)
                break;

            std::memcpy(stage->shadow.data() + destination, source, elementSize);
            source += elementSize;
            destination += elementStride;
        }

        stage->dirty = true;
    }

    if (!found && m_warnedUniforms.emplace(name).second)
        err() << "Uniform \"" << name << "\" not found in shader" << std::endl;
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, float x)
{
    writeUniform(name, &x, sizeof(float), sizeof(float), 1);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, Glsl::Vec2 vector)
{
    const std::array data = {vector.x, vector.y};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, const Glsl::Vec3& vector)
{
    const std::array data = {vector.x, vector.y, vector.z};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, const Glsl::Vec4& vector)
{
    const std::array data = {vector.x, vector.y, vector.z, vector.w};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, int x)
{
    writeUniform(name, &x, sizeof(int), sizeof(int), 1);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, Glsl::Ivec2 vector)
{
    const std::array data = {vector.x, vector.y};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, const Glsl::Ivec3& vector)
{
    const std::array data = {vector.x, vector.y, vector.z};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, const Glsl::Ivec4& vector)
{
    const std::array data = {vector.x, vector.y, vector.z, vector.w};
    writeUniform(name, data.data(), sizeof(data), sizeof(data), 1);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, bool x)
{
    // Booleans are stored as 32 bit values in HLSL constant buffers
    setUniform(name, x ? 1 : 0);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, Glsl::Bvec2 vector)
{
    setUniform(name, Glsl::Ivec2(vector.x ? 1 : 0, vector.y ? 1 : 0));
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, const Glsl::Bvec3& vector)
{
    setUniform(name, Glsl::Ivec3(vector.x ? 1 : 0, vector.y ? 1 : 0, vector.z ? 1 : 0));
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, const Glsl::Bvec4& vector)
{
    setUniform(name, Glsl::Ivec4(vector.x ? 1 : 0, vector.y ? 1 : 0, vector.z ? 1 : 0, vector.w ? 1 : 0));
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, const Glsl::Mat3& matrix)
{
    // float3x3 columns are padded to 16 bytes in HLSL constant buffers
    writeUniform(name, matrix.array.data(), sizeof(float) * 3, sizeof(float) * 4, 3);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, const Glsl::Mat4& matrix)
{
    writeUniform(name, matrix.array.data(), sizeof(float) * 16, sizeof(float) * 16, 1);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniform(const std::string& name, const Texture& texture)
{
    m_textures[name] = &texture;
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setCurrentTextureUniform(const std::string& name)
{
    m_currentTextureName = name;
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniformArray(const std::string& name, const float* scalarArray, std::size_t length)
{
    // Array elements are aligned to 16 bytes in HLSL constant buffers
    writeUniform(name, scalarArray, sizeof(float), sizeof(float) * 4, length);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length)
{
    writeUniform(name, vectorArray, sizeof(Glsl::Vec2), sizeof(float) * 4, length);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length)
{
    writeUniform(name, vectorArray, sizeof(Glsl::Vec3), sizeof(float) * 4, length);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length)
{
    writeUniform(name, vectorArray, sizeof(float) * 4, sizeof(float) * 4, length);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length)
{
    bool found = false;

    for (auto* stage : {&m_vertexStage, &m_geometryStage, &m_pixelStage})
    {
        const auto variableIt = stage->variables.find(name);
        if (variableIt == stage->variables.end())
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

                std::memcpy(stage->shadow.data() + destination, matrixArray[i].array.data() + column * 3, sizeof(float) * 3);
            }
        }

        stage->dirty = true;
    }

    if (!found && m_warnedUniforms.emplace(name).second)
        err() << "Uniform \"" << name << "\" not found in shader" << std::endl;
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::setUniformArray(const std::string& name, const Glsl::Mat4* matrixArray, std::size_t length)
{
    writeUniform(name, matrixArray, sizeof(float) * 16, sizeof(float) * 16, length);
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::bindTextures(const Stage& stage, StageKind kind) const
{
    auto* context = m_device.getContext();

    for (const auto& [name, slot] : stage.textureSlots)
    {
        ID3D11ShaderResourceView* view    = nullptr;
        ID3D11SamplerState*       sampler = nullptr;

        const auto it = (name != m_currentTextureName) ? m_textures.find(name) : m_textures.end();
        if (it != m_textures.end())
        {
            if (const auto* impl = static_cast<const D3D11TextureImpl*>(getTextureImpl(*it->second)))
                view = impl->getShaderResourceView();

            sampler = m_device.getSamplerState(it->second->isSmooth(), it->second->isRepeated(), textureHasMipmap(*it->second));
        }
        else
        {
            // The CurrentTexture uniform and textures never assigned one resolve to the
            // draw's texture, like unset GLSL samplers defaulting to texture unit 0
            view    = m_device.getCurrentTextureView();
            sampler = m_device.getCurrentTextureSampler();
        }

        if (!sampler)
            sampler = m_device.getSamplerState(false, false);

        switch (kind)
        {
            case StageKind::Vertex:
                context->VSSetShaderResources(slot, 1, &view);
                context->VSSetSamplers(slot, 1, &sampler);
                break;
            case StageKind::Geometry:
                context->GSSetShaderResources(slot, 1, &view);
                context->GSSetSamplers(slot, 1, &sampler);
                break;
            case StageKind::Pixel:
                context->PSSetShaderResources(slot, 1, &view);
                context->PSSetSamplers(slot, 1, &sampler);
                break;
        }
    }
}


////////////////////////////////////////////////////////////
void D3D11ShaderImpl::bind() const
{
    const D3D11GraphicsDevice::ContextLock lock(m_device);

    auto* context = m_device.getContext();
    if (!context)
        return;

    // Upload the constant buffers that changed
    for (const auto* stage : {&m_vertexStage, &m_geometryStage, &m_pixelStage})
    {
        if (!stage->constantBuffer || !stage->dirty)
            continue;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (d3dCheck(context->Map(stage->constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            std::memcpy(mapped.pData, stage->shadow.data(), stage->shadow.size());
            context->Unmap(stage->constantBuffer.Get(), 0);
        }

        stage->dirty = false;
    }

    // Bind the stages, keeping the built-in ones where the user didn't provide a replacement
    if (m_vertexShader)
    {
        context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        context->IASetInputLayout(m_inputLayout.Get());

        if (m_vertexStage.constantBuffer)
        {
            auto* buffer = m_vertexStage.constantBuffer.Get();
            context->VSSetConstantBuffers(m_vertexStage.constantBufferSlot, 1, &buffer);
        }

        bindTextures(m_vertexStage, StageKind::Vertex);
    }

    context->GSSetShader(m_geometryShader.Get(), nullptr, 0);

    if (m_geometryShader)
    {
        if (m_geometryStage.constantBuffer)
        {
            auto* buffer = m_geometryStage.constantBuffer.Get();
            context->GSSetConstantBuffers(m_geometryStage.constantBufferSlot, 1, &buffer);
        }

        bindTextures(m_geometryStage, StageKind::Geometry);
    }

    if (m_pixelShader)
    {
        context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

        if (m_pixelStage.constantBuffer)
        {
            auto* buffer = m_pixelStage.constantBuffer.Get();
            context->PSSetConstantBuffers(m_pixelStage.constantBufferSlot, 1, &buffer);
        }

        bindTextures(m_pixelStage, StageKind::Pixel);
    }
}


////////////////////////////////////////////////////////////
unsigned int D3D11ShaderImpl::getNativeHandle() const
{
    return 0;
}

} // namespace sf::priv
