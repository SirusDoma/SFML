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
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>
#include <SFML/Graphics/Vulkan/VulkanVertexBufferImpl.hpp>

#include <SFML/System/Err.hpp>

#include <ostream>
#include <vma/vk_mem_alloc.h>


namespace
{
// Record a barrier ordering vertex fetches against a transfer into the buffer
void bufferTransferBarrier(const sf::priv::VulkanFunctions& fn, VkCommandBuffer commandBuffer, VkBuffer buffer, bool beforeTransfer)
{
    VkBufferMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer              = buffer;
    barrier.size                = VK_WHOLE_SIZE;

    if (beforeTransfer)
    {
        barrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        fn.vkCmdPipelineBarrier(commandBuffer,
                                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0,
                                0,
                                nullptr,
                                1,
                                &barrier,
                                0,
                                nullptr);
    }
    else
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;

        fn.vkCmdPipelineBarrier(commandBuffer,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0,
                                0,
                                nullptr,
                                1,
                                &barrier,
                                0,
                                nullptr);
    }
}
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
VulkanVertexBufferImpl::VulkanVertexBufferImpl(VulkanGraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
VulkanVertexBufferImpl::~VulkanVertexBufferImpl()
{
    if (m_buffer)
    {
        const VulkanGraphicsDevice::ContextLock lock(m_device);

        m_device.deferDestroyBuffer(m_buffer, m_allocation);
    }
}


////////////////////////////////////////////////////////////
bool VulkanVertexBufferImpl::create(std::size_t vertexCount, [[maybe_unused]] VertexBuffer::Usage usage)
{
    if (!m_device.getDevice())
    {
        err() << "Failed to create vertex buffer, no Vulkan device available" << std::endl;
        return false;
    }

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    if (m_buffer)
    {
        m_device.flushPendingDraws();
        m_device.deferDestroyBuffer(m_buffer, m_allocation);
        m_buffer     = VK_NULL_HANDLE;
        m_allocation = nullptr;
        m_capacity   = 0;
    }

    // Zero-size buffers are invalid; an empty one is left without storage and
    // every update bounds-checks itself against the capacity anyway
    if (vertexCount == 0)
        return true;

    // The buffer lives in device memory for every usage; updates are recorded
    // transfers, so no CPU-visible storage or shadow copy is needed
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = sizeof(Vertex) * vertexCount;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (!vkCheck(vmaCreateBuffer(m_device.getAllocator(), &bufferInfo, &allocationInfo, &m_buffer, &m_allocation, nullptr)))
    {
        m_buffer   = VK_NULL_HANDLE;
        m_capacity = 0;
        return false;
    }

    m_capacity = vertexCount;

    return true;
}


////////////////////////////////////////////////////////////
VkCommandBuffer VulkanVertexBufferImpl::beginTransfer()
{
    // Draws collected so far must fetch the old contents before they change,
    // and transfer commands are illegal inside a render pass
    m_device.flushPendingDraws();
    m_device.materializePendingClears();
    m_device.endEncoding();

    return m_device.currentCommandBuffer();
}


////////////////////////////////////////////////////////////
bool VulkanVertexBufferImpl::update(const Vertex*       vertices,
                                    std::size_t         vertexCount,
                                    unsigned int        offset,
                                    std::size_t&        size,
                                    VertexBuffer::Usage usage)
{
    if (!m_buffer || !vertices)
        return false;

    // Nothing to copy, and a zero-size transfer is not a legal command
    if (vertexCount == 0)
        return true;

    // Grow the buffer if needed, discarding the old contents like the OpenGL orphaning path does
    if (vertexCount >= size)
    {
        if (!create(vertexCount, usage))
            return false;

        size   = vertexCount;
        offset = 0;
    }

    // The copy has to stay inside the allocation, whatever the caller's own
    // bookkeeping says
    if ((vertexCount > m_capacity) || (offset > m_capacity - vertexCount))
        return false;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    VkCommandBuffer commandBuffer = beginTransfer();
    if (!commandBuffer)
        return false;

    VulkanTransientAllocation staging;
    if (!m_device.allocateTransient(vertices, sizeof(Vertex) * vertexCount, 4, staging))
        return false;

    bufferTransferBarrier(m_device.fn(), commandBuffer, m_buffer, true);

    VkBufferCopy region{};
    region.srcOffset = staging.offset;
    region.dstOffset = sizeof(Vertex) * offset;
    region.size      = sizeof(Vertex) * vertexCount;

    m_device.fn().vkCmdCopyBuffer(commandBuffer, staging.buffer, m_buffer, 1, &region);

    bufferTransferBarrier(m_device.fn(), commandBuffer, m_buffer, false);

    return true;
}


////////////////////////////////////////////////////////////
bool VulkanVertexBufferImpl::update(const VertexBufferImpl& other, std::size_t otherSize, [[maybe_unused]] VertexBuffer::Usage usage)
{
    const auto& vulkanOther = static_cast<const VulkanVertexBufferImpl&>(other);

    if (!m_buffer || !vulkanOther.m_buffer)
        return false;

    // A buffer already holds its own contents, and a copy whose regions fully
    // overlap is undefined
    if (vulkanOther.m_buffer == m_buffer)
        return true;

    // Nothing to copy, and a zero-size transfer is not a legal command
    if (otherSize == 0)
        return true;

    // Neither side may be overrun: this buffer is the copy's destination and
    // the other one is where the vertices come from
    if ((otherSize > m_capacity) || (otherSize > vulkanOther.m_capacity))
        return false;

    const VulkanGraphicsDevice::ContextLock lock(m_device);

    VkCommandBuffer commandBuffer = beginTransfer();
    if (!commandBuffer)
        return false;

    bufferTransferBarrier(m_device.fn(), commandBuffer, m_buffer, true);

    VkBufferCopy region{};
    region.size = sizeof(Vertex) * otherSize;

    m_device.fn().vkCmdCopyBuffer(commandBuffer, vulkanOther.m_buffer, m_buffer, 1, &region);

    bufferTransferBarrier(m_device.fn(), commandBuffer, m_buffer, false);

    return true;
}


////////////////////////////////////////////////////////////
unsigned int VulkanVertexBufferImpl::getNativeHandle() const
{
    // Vulkan resources have no GL-style integer handle
    return 0;
}


////////////////////////////////////////////////////////////
VkBuffer VulkanVertexBufferImpl::getBuffer() const
{
    return m_buffer;
}

} // namespace sf::priv
