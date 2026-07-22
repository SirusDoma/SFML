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
#include <SFML/Graphics/Metal/MetalVertexBufferImpl.hpp>

#include <SFML/System/Err.hpp>

#include <ostream>

#include <cstring>


namespace sf::priv
{
////////////////////////////////////////////////////////////
MetalVertexBufferImpl::MetalVertexBufferImpl(MetalGraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
bool MetalVertexBufferImpl::create(std::size_t vertexCount, VertexBuffer::Usage usage)
{
    id<MTLDevice> device = m_device.getDevice();
    if (!device)
    {
        err() << "Failed to create vertex buffer, no Metal device available" << std::endl;
        return false;
    }

    m_dynamic = (usage != VertexBuffer::Usage::Static);

    // Frequently rewritten buffers live in shared memory the CPU writes directly,
    // static buffers in private GPU memory filled through staging blits
    const MTLResourceOptions options = m_dynamic
                                           ? (MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined)
                                           : MTLResourceStorageModePrivate;

    m_buffer.reset([device newBufferWithLength:sizeof(Vertex) * vertexCount options:options]);

    return m_buffer.get() != nullptr;
}


////////////////////////////////////////////////////////////
bool MetalVertexBufferImpl::update(const Vertex*       vertices,
                                   std::size_t         vertexCount,
                                   unsigned int        offset,
                                   std::size_t&        size,
                                   VertexBuffer::Usage usage)
{
    if (!m_buffer)
        return false;

    // Grow the buffer if needed, discarding the old contents like the OpenGL orphaning path does
    if (vertexCount >= size)
    {
        if (!create(vertexCount, usage))
            return false;

        size = vertexCount;
    }

    const MetalGraphicsDevice::ContextLock lock(m_device);

    // Draws collected so far must read the buffer before it changes
    m_device.flushPendingDraws();

    // Shared memory can be written directly while the GPU holds no work referencing it
    if (m_dynamic && m_device.isGpuIdle())
    {
        std::memcpy(static_cast<std::byte*>([m_buffer.get() contents]) + sizeof(Vertex) * offset,
                    vertices,
                    sizeof(Vertex) * vertexCount);
        return true;
    }

    // Otherwise the update is encoded as a blit so it lands behind the recorded draws
    m_device.endEncoding(false);

    id<MTLDevice>        device        = m_device.getDevice();
    id<MTLCommandBuffer> commandBuffer = m_device.currentCommandBuffer();
    if (!device || !commandBuffer)
        return false;

    id<MTLBuffer> staging = [device newBufferWithBytes:vertices
                                                length:sizeof(Vertex) * vertexCount
                                               options:MTLResourceStorageModeShared];
    if (!staging)
        return false;

    @autoreleasepool
    {
        id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
        [blit copyFromBuffer:staging
                sourceOffset:0
                    toBuffer:m_buffer.get()
           destinationOffset:sizeof(Vertex) * offset
                        size:sizeof(Vertex) * vertexCount];
        [blit endEncoding];
    }

    // The command buffer keeps the staging memory alive until the copy executed
    [staging release];

    return true;
}


////////////////////////////////////////////////////////////
bool MetalVertexBufferImpl::update(const VertexBufferImpl& other, std::size_t otherSize, [[maybe_unused]] VertexBuffer::Usage usage)
{
    const auto& metalOther = static_cast<const MetalVertexBufferImpl&>(other);

    if (!m_buffer || !metalOther.m_buffer)
        return false;

    if (sizeof(Vertex) * otherSize > [m_buffer.get() length])
        return false;

    const MetalGraphicsDevice::ContextLock lock(m_device);

    m_device.flushPendingDraws();
    m_device.endEncoding(false);

    id<MTLCommandBuffer> commandBuffer = m_device.currentCommandBuffer();
    if (!commandBuffer)
        return false;

    @autoreleasepool
    {
        id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
        [blit copyFromBuffer:metalOther.m_buffer.get()
                sourceOffset:0
                    toBuffer:m_buffer.get()
           destinationOffset:0
                        size:sizeof(Vertex) * otherSize];
        [blit endEncoding];
    }

    return true;
}


////////////////////////////////////////////////////////////
unsigned int MetalVertexBufferImpl::getNativeHandle() const
{
    // Metal resources have no GL-style integer handle
    return 0;
}


////////////////////////////////////////////////////////////
MetalBufferPtr MetalVertexBufferImpl::getBuffer() const
{
    return m_buffer.get();
}

} // namespace sf::priv
