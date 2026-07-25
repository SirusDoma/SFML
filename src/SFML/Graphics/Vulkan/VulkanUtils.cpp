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
#include <SFML/Graphics/Vulkan/VulkanUtils.hpp>

#include <SFML/Window/Vulkan.hpp>


namespace sf::priv
{
////////////////////////////////////////////////////////////
bool VulkanFunctions::loadLoaderEntryPoints()
{
    // The window module owns the loader library, reuse it so both
    // modules are guaranteed to talk to the same Vulkan runtime
    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(sf::Vulkan::getFunction("vkGetInstanceProcAddr"));
    if (!vkGetInstanceProcAddr)
        return false;

    const auto load = [&](auto& entryPoint, const char* name)
    { entryPoint = reinterpret_cast<std::decay_t<decltype(entryPoint)>>(vkGetInstanceProcAddr(nullptr, name)); };

    load(vkEnumerateInstanceVersion, "vkEnumerateInstanceVersion");
    load(vkEnumerateInstanceExtensionProperties, "vkEnumerateInstanceExtensionProperties");
    load(vkCreateInstance, "vkCreateInstance");

    // vkEnumerateInstanceVersion missing means a Vulkan 1.0 loader, too old for this backend
    return vkEnumerateInstanceVersion && vkEnumerateInstanceExtensionProperties && vkCreateInstance;
}


////////////////////////////////////////////////////////////
bool VulkanFunctions::loadInstanceEntryPoints(VkInstance instance)
{
    bool ok = true;

    const auto load = [&](auto& entryPoint, const char* name)
    {
        entryPoint = reinterpret_cast<std::decay_t<decltype(entryPoint)>>(vkGetInstanceProcAddr(instance, name));
        if (!entryPoint)
        {
            err() << "Failed to resolve Vulkan entry point " << name << std::endl;
            ok = false;
        }
    };

    load(vkDestroyInstance, "vkDestroyInstance");
    load(vkEnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
    load(vkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
    load(vkGetPhysicalDeviceFeatures, "vkGetPhysicalDeviceFeatures");
    load(vkGetPhysicalDeviceFeatures2, "vkGetPhysicalDeviceFeatures2");
    load(vkGetPhysicalDeviceQueueFamilyProperties, "vkGetPhysicalDeviceQueueFamilyProperties");
    load(vkGetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties");
    load(vkGetPhysicalDeviceFormatProperties, "vkGetPhysicalDeviceFormatProperties");
    load(vkGetPhysicalDeviceImageFormatProperties, "vkGetPhysicalDeviceImageFormatProperties");
    load(vkEnumerateDeviceExtensionProperties, "vkEnumerateDeviceExtensionProperties");
    load(vkCreateDevice, "vkCreateDevice");
    load(vkGetDeviceProcAddr, "vkGetDeviceProcAddr");
    load(vkDestroySurfaceKHR, "vkDestroySurfaceKHR");
    load(vkGetPhysicalDeviceSurfaceSupportKHR, "vkGetPhysicalDeviceSurfaceSupportKHR");
    load(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    load(vkGetPhysicalDeviceSurfaceFormatsKHR, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    load(vkGetPhysicalDeviceSurfacePresentModesKHR, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    load(vkGetPhysicalDeviceMemoryProperties2, "vkGetPhysicalDeviceMemoryProperties2");

    return ok;
}


////////////////////////////////////////////////////////////
bool VulkanFunctions::loadDeviceEntryPoints(VkDevice device)
{
    bool ok = true;

    const auto load = [&](auto& entryPoint, const char* name)
    {
        entryPoint = reinterpret_cast<std::decay_t<decltype(entryPoint)>>(vkGetDeviceProcAddr(device, name));
        if (!entryPoint)
        {
            err() << "Failed to resolve Vulkan entry point " << name << std::endl;
            ok = false;
        }
    };

    load(vkDestroyDevice, "vkDestroyDevice");
    load(vkGetDeviceQueue, "vkGetDeviceQueue");
    load(vkQueueSubmit, "vkQueueSubmit");
    load(vkQueueWaitIdle, "vkQueueWaitIdle");
    load(vkDeviceWaitIdle, "vkDeviceWaitIdle");
    load(vkCreateCommandPool, "vkCreateCommandPool");
    load(vkDestroyCommandPool, "vkDestroyCommandPool");
    load(vkResetCommandPool, "vkResetCommandPool");
    load(vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
    load(vkBeginCommandBuffer, "vkBeginCommandBuffer");
    load(vkEndCommandBuffer, "vkEndCommandBuffer");
    load(vkCreateFence, "vkCreateFence");
    load(vkDestroyFence, "vkDestroyFence");
    load(vkWaitForFences, "vkWaitForFences");
    load(vkResetFences, "vkResetFences");
    load(vkGetFenceStatus, "vkGetFenceStatus");
    load(vkCreateSemaphore, "vkCreateSemaphore");
    load(vkDestroySemaphore, "vkDestroySemaphore");
    load(vkCreateRenderPass, "vkCreateRenderPass");
    load(vkDestroyRenderPass, "vkDestroyRenderPass");
    load(vkCreateFramebuffer, "vkCreateFramebuffer");
    load(vkDestroyFramebuffer, "vkDestroyFramebuffer");
    load(vkCreateImage, "vkCreateImage");
    load(vkDestroyImage, "vkDestroyImage");
    load(vkCreateImageView, "vkCreateImageView");
    load(vkDestroyImageView, "vkDestroyImageView");
    load(vkCreateBuffer, "vkCreateBuffer");
    load(vkDestroyBuffer, "vkDestroyBuffer");
    load(vkCreateSampler, "vkCreateSampler");
    load(vkDestroySampler, "vkDestroySampler");
    load(vkCreateShaderModule, "vkCreateShaderModule");
    load(vkDestroyShaderModule, "vkDestroyShaderModule");
    load(vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
    load(vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
    load(vkCreateDescriptorPool, "vkCreateDescriptorPool");
    load(vkDestroyDescriptorPool, "vkDestroyDescriptorPool");
    load(vkResetDescriptorPool, "vkResetDescriptorPool");
    load(vkFreeDescriptorSets, "vkFreeDescriptorSets");
    load(vkAllocateDescriptorSets, "vkAllocateDescriptorSets");
    load(vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
    load(vkCreatePipelineLayout, "vkCreatePipelineLayout");
    load(vkDestroyPipelineLayout, "vkDestroyPipelineLayout");
    load(vkCreateGraphicsPipelines, "vkCreateGraphicsPipelines");
    load(vkDestroyPipeline, "vkDestroyPipeline");
    load(vkCmdBeginRenderPass, "vkCmdBeginRenderPass");
    load(vkCmdEndRenderPass, "vkCmdEndRenderPass");
    load(vkCmdBindPipeline, "vkCmdBindPipeline");
    load(vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
    load(vkCmdBindVertexBuffers, "vkCmdBindVertexBuffers");
    load(vkCmdBindIndexBuffer, "vkCmdBindIndexBuffer");
    load(vkCmdSetViewport, "vkCmdSetViewport");
    load(vkCmdSetScissor, "vkCmdSetScissor");
    load(vkCmdSetStencilReference, "vkCmdSetStencilReference");
    load(vkCmdSetStencilCompareMask, "vkCmdSetStencilCompareMask");
    load(vkCmdSetStencilWriteMask, "vkCmdSetStencilWriteMask");
    load(vkCmdPushConstants, "vkCmdPushConstants");
    load(vkCmdDraw, "vkCmdDraw");
    load(vkCmdDrawIndexed, "vkCmdDrawIndexed");
    load(vkCmdClearAttachments, "vkCmdClearAttachments");
    load(vkCmdPipelineBarrier, "vkCmdPipelineBarrier");
    load(vkCmdCopyBuffer, "vkCmdCopyBuffer");
    load(vkCmdCopyImage, "vkCmdCopyImage");
    load(vkCmdBlitImage, "vkCmdBlitImage");
    load(vkCmdCopyBufferToImage, "vkCmdCopyBufferToImage");
    load(vkCmdCopyImageToBuffer, "vkCmdCopyImageToBuffer");
    load(vkCmdResolveImage, "vkCmdResolveImage");
    load(vkCreateSwapchainKHR, "vkCreateSwapchainKHR");
    load(vkDestroySwapchainKHR, "vkDestroySwapchainKHR");
    load(vkGetSwapchainImagesKHR, "vkGetSwapchainImagesKHR");
    load(vkAcquireNextImageKHR, "vkAcquireNextImageKHR");
    load(vkQueuePresentKHR, "vkQueuePresentKHR");

    // Optional, only present when the device enabled VK_KHR_present_wait
    vkWaitForPresentKHR = reinterpret_cast<PFN_vkWaitForPresentKHR>(vkGetDeviceProcAddr(device, "vkWaitForPresentKHR"));
    load(vkAllocateMemory, "vkAllocateMemory");
    load(vkFreeMemory, "vkFreeMemory");
    load(vkMapMemory, "vkMapMemory");
    load(vkUnmapMemory, "vkUnmapMemory");
    load(vkFlushMappedMemoryRanges, "vkFlushMappedMemoryRanges");
    load(vkInvalidateMappedMemoryRanges, "vkInvalidateMappedMemoryRanges");
    load(vkBindBufferMemory, "vkBindBufferMemory");
    load(vkBindImageMemory, "vkBindImageMemory");
    load(vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
    load(vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements");
    load(vkGetBufferMemoryRequirements2, "vkGetBufferMemoryRequirements2");
    load(vkGetImageMemoryRequirements2, "vkGetImageMemoryRequirements2");
    load(vkBindBufferMemory2, "vkBindBufferMemory2");
    load(vkBindImageMemory2, "vkBindImageMemory2");

    return ok;
}


////////////////////////////////////////////////////////////
void vulkanImageBarrier(const VulkanFunctions& fn,
                        VkCommandBuffer        commandBuffer,
                        VkImage                image,
                        VkImageAspectFlags     aspect,
                        VkImageLayout          oldLayout,
                        VkImageLayout          newLayout)
{
    // Conservative masks covering every access the layout allows
    const auto masksForLayout = [](VkImageLayout layout, VkAccessFlags& access, VkPipelineStageFlags& stages)
    {
        switch (layout)
        {
            case VK_IMAGE_LAYOUT_UNDEFINED:
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                access = 0;
                stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                access = VK_ACCESS_TRANSFER_READ_BIT;
                stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                access = VK_ACCESS_TRANSFER_WRITE_BIT;
                stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                access = VK_ACCESS_SHADER_READ_BIT;
                stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                break;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                access = 0;
                stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;
            case VK_IMAGE_LAYOUT_GENERAL:
            default:
                access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT |
                         VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
        }
    };

    VkImageMemoryBarrier barrier{};
    barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                   = oldLayout;
    barrier.newLayout                   = newLayout;
    barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                       = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkPipelineStageFlags sourceStages      = 0;
    VkPipelineStageFlags destinationStages = 0;
    masksForLayout(oldLayout, barrier.srcAccessMask, sourceStages);
    masksForLayout(newLayout, barrier.dstAccessMask, destinationStages);

    fn.vkCmdPipelineBarrier(commandBuffer, sourceStages, destinationStages, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}


////////////////////////////////////////////////////////////
const char* vulkanResultToString(VkResult result)
{
    // clang-format off
    switch (result)
    {
        case VK_SUCCESS:                        return "VK_SUCCESS";
        case VK_NOT_READY:                      return "VK_NOT_READY";
        case VK_TIMEOUT:                        return "VK_TIMEOUT";
        case VK_INCOMPLETE:                     return "VK_INCOMPLETE";
        case VK_SUBOPTIMAL_KHR:                 return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_HOST_MEMORY:       return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:    return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:              return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:        return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:        return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:    return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:      return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:      return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:         return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:     return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL:          return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_OUT_OF_POOL_MEMORY:       return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:  return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_SURFACE_LOST_KHR:         return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:          return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:    return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_UNKNOWN:                  return "VK_ERROR_UNKNOWN";
        default:                                return "unknown VkResult";
    }
    // clang-format on
}

} // namespace sf::priv
