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
#include <SFML/Graphics/GraphicsDevice.hpp>
#include <SFML/Graphics/RenderTargetImpl.hpp>
#include <SFML/Graphics/Renderer.hpp>
#include <SFML/Graphics/VulkanInterop.hpp>

#ifdef SFML_ENABLE_VULKAN
#include <SFML/Graphics/Vulkan/VulkanGraphicsDevice.hpp>
#include <SFML/Graphics/Vulkan/VulkanTextureImpl.hpp>
#include <SFML/Graphics/Vulkan/VulkanVertexBufferImpl.hpp>
#endif


namespace
{
// Never instantiated, re-exposes the protected resource accessors of the implementation base
struct VulkanImplAccess : sf::priv::RenderTargetImpl
{
    using RenderTargetImpl::getId;
    using RenderTargetImpl::getImpl;
    using RenderTargetImpl::getTextureImpl;
    using RenderTargetImpl::getVertexBufferImpl;
};

#ifdef SFML_ENABLE_VULKAN
// The Vulkan device, null unless the Vulkan backend is the one alive
sf::priv::VulkanGraphicsDevice* getVulkanDevice()
{
    auto* device = sf::priv::getGraphicsDevice();
    if (!device || (device->getRenderer() != sf::Renderer::Vulkan))
        return nullptr;

    return static_cast<sf::priv::VulkanGraphicsDevice*>(device);
}
#endif
} // namespace


namespace sf::Vulkan
{
////////////////////////////////////////////////////////////
VkInstance getInstance()
{
#ifdef SFML_ENABLE_VULKAN
    if (auto* device = getVulkanDevice())
        return device->getInstance();
#endif

    return {};
}


////////////////////////////////////////////////////////////
VkPhysicalDevice getPhysicalDevice()
{
#ifdef SFML_ENABLE_VULKAN
    if (auto* device = getVulkanDevice())
        return device->getPhysicalDevice();
#endif

    return {};
}


////////////////////////////////////////////////////////////
VkDevice getDevice()
{
#ifdef SFML_ENABLE_VULKAN
    if (auto* device = getVulkanDevice())
        return device->getDevice();
#endif

    return {};
}


////////////////////////////////////////////////////////////
VkQueue getGraphicsQueue()
{
#ifdef SFML_ENABLE_VULKAN
    if (auto* device = getVulkanDevice())
        return device->getQueue();
#endif

    return {};
}


////////////////////////////////////////////////////////////
std::uint32_t getGraphicsQueueFamilyIndex()
{
#ifdef SFML_ENABLE_VULKAN
    if (auto* device = getVulkanDevice())
        return device->getQueueFamilyIndex();
#endif

    return 0;
}


////////////////////////////////////////////////////////////
VkCommandBuffer getCommandBuffer()
{
#ifdef SFML_ENABLE_VULKAN
    if (auto* device = getVulkanDevice())
        return device->currentCommandBuffer();
#endif

    return {};
}


////////////////////////////////////////////////////////////
VkRenderPass getRenderPass()
{
#ifdef SFML_ENABLE_VULKAN
    if (auto* device = getVulkanDevice())
    {
        // Make sure a pass is open on the current target so raw draws can be
        // recorded into it right away
        if (device->applyPendingState())
            return device->getOpenRenderPass();
    }
#endif

    return {};
}


////////////////////////////////////////////////////////////
VkImage getTexture([[maybe_unused]] const Texture& texture)
{
#ifdef SFML_ENABLE_VULKAN
    if (getVulkanDevice())
    {
        if (auto* impl = VulkanImplAccess::getTextureImpl(texture))
            return static_cast<priv::VulkanTextureImpl*>(impl)->getImage();
    }
#endif

    return {};
}


////////////////////////////////////////////////////////////
VkImageView getImageView([[maybe_unused]] const Texture& texture)
{
#ifdef SFML_ENABLE_VULKAN
    if (getVulkanDevice())
    {
        if (auto* impl = VulkanImplAccess::getTextureImpl(texture))
            return static_cast<priv::VulkanTextureImpl*>(impl)->getImageView();
    }
#endif

    return {};
}


////////////////////////////////////////////////////////////
VkBuffer getBuffer([[maybe_unused]] const VertexBuffer& vertexBuffer)
{
#ifdef SFML_ENABLE_VULKAN
    if (getVulkanDevice())
    {
        if (auto* impl = VulkanImplAccess::getVertexBufferImpl(vertexBuffer))
            return static_cast<priv::VulkanVertexBufferImpl*>(impl)->getBuffer();
    }
#endif

    return {};
}


////////////////////////////////////////////////////////////
void flush()
{
#ifdef SFML_ENABLE_VULKAN
    if (auto* device = getVulkanDevice())
        device->flushPendingDraws();
#endif
}


////////////////////////////////////////////////////////////
void resetStates(RenderTarget& target)
{
    if (auto* impl = VulkanImplAccess::getImpl(target))
        impl->resetStates(target, VulkanImplAccess::getId(target));
}

} // namespace sf::Vulkan
