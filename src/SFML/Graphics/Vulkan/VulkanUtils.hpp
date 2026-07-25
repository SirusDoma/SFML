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
#include <SFML/System/Err.hpp>

#include <ostream>

#include <cstddef>
#include <cstdint>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Entry points of the Vulkan loader used by the backend
///
/// The backend never links against the Vulkan loader, every
/// entry point is resolved at run time: the exported loader
/// functions through `sf::Vulkan::getFunction`, instance-level
/// functions through `vkGetInstanceProcAddr` and device-level
/// functions through `vkGetDeviceProcAddr`.
///
////////////////////////////////////////////////////////////
struct VulkanFunctions
{
    // Loader-level entry points
    PFN_vkGetInstanceProcAddr                  vkGetInstanceProcAddr{};
    PFN_vkEnumerateInstanceVersion             vkEnumerateInstanceVersion{};
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties{};
    PFN_vkCreateInstance                       vkCreateInstance{};

    // Instance-level entry points
    PFN_vkDestroyInstance                         vkDestroyInstance{};
    PFN_vkEnumeratePhysicalDevices                vkEnumeratePhysicalDevices{};
    PFN_vkGetPhysicalDeviceProperties             vkGetPhysicalDeviceProperties{};
    PFN_vkGetPhysicalDeviceFeatures               vkGetPhysicalDeviceFeatures{};
    PFN_vkGetPhysicalDeviceFeatures2              vkGetPhysicalDeviceFeatures2{};
    PFN_vkGetPhysicalDeviceQueueFamilyProperties  vkGetPhysicalDeviceQueueFamilyProperties{};
    PFN_vkGetPhysicalDeviceMemoryProperties       vkGetPhysicalDeviceMemoryProperties{};
    PFN_vkGetPhysicalDeviceFormatProperties       vkGetPhysicalDeviceFormatProperties{};
    PFN_vkGetPhysicalDeviceImageFormatProperties  vkGetPhysicalDeviceImageFormatProperties{};
    PFN_vkEnumerateDeviceExtensionProperties      vkEnumerateDeviceExtensionProperties{};
    PFN_vkCreateDevice                            vkCreateDevice{};
    PFN_vkGetDeviceProcAddr                       vkGetDeviceProcAddr{};
    PFN_vkDestroySurfaceKHR                       vkDestroySurfaceKHR{};
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR      vkGetPhysicalDeviceSurfaceSupportKHR{};
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR{};
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      vkGetPhysicalDeviceSurfaceFormatsKHR{};
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR{};

    // Device-level entry points
    PFN_vkDestroyDevice              vkDestroyDevice{};
    PFN_vkGetDeviceQueue             vkGetDeviceQueue{};
    PFN_vkQueueSubmit                vkQueueSubmit{};
    PFN_vkQueueWaitIdle              vkQueueWaitIdle{};
    PFN_vkDeviceWaitIdle             vkDeviceWaitIdle{};
    PFN_vkCreateCommandPool          vkCreateCommandPool{};
    PFN_vkDestroyCommandPool         vkDestroyCommandPool{};
    PFN_vkResetCommandPool           vkResetCommandPool{};
    PFN_vkAllocateCommandBuffers     vkAllocateCommandBuffers{};
    PFN_vkBeginCommandBuffer         vkBeginCommandBuffer{};
    PFN_vkEndCommandBuffer           vkEndCommandBuffer{};
    PFN_vkCreateFence                vkCreateFence{};
    PFN_vkDestroyFence               vkDestroyFence{};
    PFN_vkWaitForFences              vkWaitForFences{};
    PFN_vkResetFences                vkResetFences{};
    PFN_vkGetFenceStatus             vkGetFenceStatus{};
    PFN_vkCreateSemaphore            vkCreateSemaphore{};
    PFN_vkDestroySemaphore           vkDestroySemaphore{};
    PFN_vkCreateRenderPass           vkCreateRenderPass{};
    PFN_vkDestroyRenderPass          vkDestroyRenderPass{};
    PFN_vkCreateFramebuffer          vkCreateFramebuffer{};
    PFN_vkDestroyFramebuffer         vkDestroyFramebuffer{};
    PFN_vkCreateImage                vkCreateImage{};
    PFN_vkDestroyImage               vkDestroyImage{};
    PFN_vkCreateImageView            vkCreateImageView{};
    PFN_vkDestroyImageView           vkDestroyImageView{};
    PFN_vkCreateBuffer               vkCreateBuffer{};
    PFN_vkDestroyBuffer              vkDestroyBuffer{};
    PFN_vkCreateSampler              vkCreateSampler{};
    PFN_vkDestroySampler             vkDestroySampler{};
    PFN_vkCreateShaderModule         vkCreateShaderModule{};
    PFN_vkDestroyShaderModule        vkDestroyShaderModule{};
    PFN_vkCreateDescriptorSetLayout  vkCreateDescriptorSetLayout{};
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout{};
    PFN_vkCreateDescriptorPool       vkCreateDescriptorPool{};
    PFN_vkDestroyDescriptorPool      vkDestroyDescriptorPool{};
    PFN_vkResetDescriptorPool        vkResetDescriptorPool{};
    PFN_vkFreeDescriptorSets         vkFreeDescriptorSets{};
    PFN_vkAllocateDescriptorSets     vkAllocateDescriptorSets{};
    PFN_vkUpdateDescriptorSets       vkUpdateDescriptorSets{};
    PFN_vkCreatePipelineLayout       vkCreatePipelineLayout{};
    PFN_vkDestroyPipelineLayout      vkDestroyPipelineLayout{};
    PFN_vkCreateGraphicsPipelines    vkCreateGraphicsPipelines{};
    PFN_vkDestroyPipeline            vkDestroyPipeline{};
    PFN_vkCmdBeginRenderPass         vkCmdBeginRenderPass{};
    PFN_vkCmdEndRenderPass           vkCmdEndRenderPass{};
    PFN_vkCmdBindPipeline            vkCmdBindPipeline{};
    PFN_vkCmdBindDescriptorSets      vkCmdBindDescriptorSets{};
    PFN_vkCmdBindVertexBuffers       vkCmdBindVertexBuffers{};
    PFN_vkCmdBindIndexBuffer         vkCmdBindIndexBuffer{};
    PFN_vkCmdSetViewport             vkCmdSetViewport{};
    PFN_vkCmdSetScissor              vkCmdSetScissor{};
    PFN_vkCmdSetStencilReference     vkCmdSetStencilReference{};
    PFN_vkCmdSetStencilCompareMask   vkCmdSetStencilCompareMask{};
    PFN_vkCmdSetStencilWriteMask     vkCmdSetStencilWriteMask{};
    PFN_vkCmdPushConstants           vkCmdPushConstants{};
    PFN_vkCmdDraw                    vkCmdDraw{};
    PFN_vkCmdDrawIndexed             vkCmdDrawIndexed{};
    PFN_vkCmdClearAttachments        vkCmdClearAttachments{};
    PFN_vkCmdPipelineBarrier         vkCmdPipelineBarrier{};
    PFN_vkCmdCopyBuffer              vkCmdCopyBuffer{};
    PFN_vkCmdCopyImage               vkCmdCopyImage{};
    PFN_vkCmdBlitImage               vkCmdBlitImage{};
    PFN_vkCmdCopyBufferToImage       vkCmdCopyBufferToImage{};
    PFN_vkCmdCopyImageToBuffer       vkCmdCopyImageToBuffer{};
    PFN_vkCmdResolveImage            vkCmdResolveImage{};

    // VK_KHR_swapchain entry points
    PFN_vkCreateSwapchainKHR    vkCreateSwapchainKHR{};
    PFN_vkDestroySwapchainKHR   vkDestroySwapchainKHR{};
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR{};
    PFN_vkAcquireNextImageKHR   vkAcquireNextImageKHR{};
    PFN_vkQueuePresentKHR       vkQueuePresentKHR{};

    // VK_KHR_present_wait entry point, null when the device lacks the extension
    PFN_vkWaitForPresentKHR vkWaitForPresentKHR{};

    // Functions VMA needs beyond the ones above
    PFN_vkAllocateMemory                     vkAllocateMemory{};
    PFN_vkFreeMemory                         vkFreeMemory{};
    PFN_vkMapMemory                          vkMapMemory{};
    PFN_vkUnmapMemory                        vkUnmapMemory{};
    PFN_vkFlushMappedMemoryRanges            vkFlushMappedMemoryRanges{};
    PFN_vkInvalidateMappedMemoryRanges       vkInvalidateMappedMemoryRanges{};
    PFN_vkBindBufferMemory                   vkBindBufferMemory{};
    PFN_vkBindImageMemory                    vkBindImageMemory{};
    PFN_vkGetBufferMemoryRequirements        vkGetBufferMemoryRequirements{};
    PFN_vkGetImageMemoryRequirements         vkGetImageMemoryRequirements{};
    PFN_vkGetBufferMemoryRequirements2       vkGetBufferMemoryRequirements2{};
    PFN_vkGetImageMemoryRequirements2        vkGetImageMemoryRequirements2{};
    PFN_vkBindBufferMemory2                  vkBindBufferMemory2{};
    PFN_vkBindImageMemory2                   vkBindImageMemory2{};
    PFN_vkGetPhysicalDeviceMemoryProperties2 vkGetPhysicalDeviceMemoryProperties2{};

    ////////////////////////////////////////////////////////////
    /// \brief Resolve the loader-level entry points
    ///
    /// \return `true` if every entry point was resolved
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool loadLoaderEntryPoints();

    ////////////////////////////////////////////////////////////
    /// \brief Resolve the instance-level entry points
    ///
    /// \param instance Instance to resolve against
    ///
    /// \return `true` if every entry point was resolved
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool loadInstanceEntryPoints(VkInstance instance);

    ////////////////////////////////////////////////////////////
    /// \brief Resolve the device-level entry points
    ///
    /// \param device Device to resolve against
    ///
    /// \return `true` if every entry point was resolved
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool loadDeviceEntryPoints(VkDevice device);
};

////////////////////////////////////////////////////////////
/// \brief Get the name of a Vulkan result code
///
/// \param result Result code to name
///
/// \return Name of the result code, or a generic fallback
///
////////////////////////////////////////////////////////////
[[nodiscard]] const char* vulkanResultToString(VkResult result);

////////////////////////////////////////////////////////////
/// \brief Record an image layout transition
///
/// The access and stage masks are derived from the layouts,
/// conservatively covering every use the layouts allow.
///
/// \param fn            Entry points to record through
/// \param commandBuffer Command buffer to record into, must not be inside a render pass
/// \param image         Image to transition
/// \param aspect        Aspect of the image to transition
/// \param oldLayout     Layout the image is in
/// \param newLayout     Layout to transition the image to
///
////////////////////////////////////////////////////////////
void vulkanImageBarrier(const VulkanFunctions& fn,
                        VkCommandBuffer        commandBuffer,
                        VkImage                image,
                        VkImageAspectFlags     aspect,
                        VkImageLayout          oldLayout,
                        VkImageLayout          newLayout);

////////////////////////////////////////////////////////////
/// \brief Check the result of a Vulkan call and log failures
///
/// \param result Result of the call
/// \param call   Textual representation of the call
///
/// \return `true` if the call succeeded
///
////////////////////////////////////////////////////////////
inline bool vulkanCheckResult(VkResult result, const char* call)
{
    if (result >= VK_SUCCESS)
        return true;

    err() << call << " failed (" << vulkanResultToString(result) << ")" << std::endl;

    return false;
}

////////////////////////////////////////////////////////////
/// \brief Get the numeric value behind a Vulkan handle
///
/// Non-dispatchable handles are pointers on 64 bit builds and
/// plain 64 bit integers on 32 bit ones, so neither cast alone
/// compiles everywhere. Used to fold handles into hashes.
///
/// \param handle Handle to read
///
/// \return The value of the handle
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard]] inline std::uint64_t vulkanHandleValue(T handle)
{
#if defined(VK_USE_64_BIT_PTR_DEFINES) && (VK_USE_64_BIT_PTR_DEFINES == 1)
    return reinterpret_cast<std::uint64_t>(handle);
#else
    return static_cast<std::uint64_t>(handle);
#endif
}

////////////////////////////////////////////////////////////
/// \brief Reduce a Vulkan handle to a hash value
///
/// \param handle Handle to hash
///
/// \return Hash of the handle, folded to the platform's width
///
////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard]] inline std::size_t vulkanHandleHash(T handle)
{
    const std::uint64_t value = vulkanHandleValue(handle);

    // Folding keeps the high half significant where size_t is 32 bits
    return static_cast<std::size_t>(value ^ (value >> 32));
}

} // namespace sf::priv

////////////////////////////////////////////////////////////
/// Check the result of a Vulkan call, logging the failed
/// expression like glCheck does for OpenGL. Unlike glCheck this
/// stays active in release builds, VkResults are real failure
/// paths and not an error queue that has to be polled.
/// Success codes other than VK_SUCCESS (like VK_SUBOPTIMAL_KHR)
/// pass the check, they have to be handled by the caller.
////////////////////////////////////////////////////////////
#define vkCheck(...) sf::priv::vulkanCheckResult((__VA_ARGS__), #__VA_ARGS__)
