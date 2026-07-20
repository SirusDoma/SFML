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
#include <SFML/Graphics/D3D11/D3D11VertexBufferImpl.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <SFML/System/Err.hpp>

#include <ostream>

#include <cstddef>


namespace sf::priv
{
////////////////////////////////////////////////////////////
D3D11VertexBufferImpl::D3D11VertexBufferImpl(D3D11GraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
bool D3D11VertexBufferImpl::create(std::size_t vertexCount, [[maybe_unused]] VertexBuffer::Usage usage)
{
    ID3D11Device* device = m_device.getDevice();
    if (!device)
    {
        err() << "Failed to create vertex buffer, no Direct3D device available" << std::endl;
        return false;
    }

    m_buffer.Reset();

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertexCount);
    desc.Usage     = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    return d3dCheck(device->CreateBuffer(&desc, nullptr, &m_buffer));
}


////////////////////////////////////////////////////////////
bool D3D11VertexBufferImpl::update(const Vertex*       vertices,
                                   std::size_t         vertexCount,
                                   unsigned int        offset,
                                   std::size_t&        size,
                                   VertexBuffer::Usage usage)
{
    ID3D11DeviceContext* context = m_device.getContext();
    if (!m_buffer || !context)
        return false;

    // Grow the buffer if needed, discarding the old contents like the OpenGL orphaning path does
    if (vertexCount >= size)
    {
        if (!create(vertexCount, usage))
            return false;

        size = vertexCount;
    }

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    const D3D11_BOX box{static_cast<UINT>(sizeof(Vertex) * offset),
                        0,
                        0,
                        static_cast<UINT>(sizeof(Vertex) * (offset + vertexCount)),
                        1,
                        1};
    context->UpdateSubresource(m_buffer.Get(), 0, &box, vertices, 0, 0);

    return true;
}


////////////////////////////////////////////////////////////
bool D3D11VertexBufferImpl::update(const VertexBufferImpl& other, std::size_t otherSize, [[maybe_unused]] VertexBuffer::Usage usage)
{
    const auto& d3dOther = static_cast<const D3D11VertexBufferImpl&>(other);

    ID3D11DeviceContext* context = m_device.getContext();
    if (!m_buffer || !d3dOther.m_buffer || !context)
        return false;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    const D3D11_BOX box{0, 0, 0, static_cast<UINT>(sizeof(Vertex) * otherSize), 1, 1};
    context->CopySubresourceRegion(m_buffer.Get(), 0, 0, 0, 0, d3dOther.m_buffer.Get(), 0, &box);

    return true;
}


////////////////////////////////////////////////////////////
unsigned int D3D11VertexBufferImpl::getNativeHandle() const
{
    // Direct3D resources have no GL-style integer handle
    return 0;
}


////////////////////////////////////////////////////////////
ID3D11Buffer* D3D11VertexBufferImpl::getBuffer() const
{
    return m_buffer.Get();
}

} // namespace sf::priv
