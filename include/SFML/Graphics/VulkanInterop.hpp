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
#include <SFML/Graphics/Export.hpp>

#include <cstdint>


////////////////////////////////////////////////////////////
// Vulkan handle declarations mirroring <vulkan/vulkan.h>, so
// this header can be used without the Vulkan headers installed
////////////////////////////////////////////////////////////
using VkInstance       = struct VkInstance_T*;
using VkPhysicalDevice = struct VkPhysicalDevice_T*;
using VkDevice         = struct VkDevice_T*;
using VkQueue          = struct VkQueue_T*;
using VkCommandBuffer  = struct VkCommandBuffer_T*;

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)

using VkRenderPass = struct VkRenderPass_T*;
using VkImage      = struct VkImage_T*;
using VkImageView  = struct VkImageView_T*;
using VkBuffer     = struct VkBuffer_T*;

#else

using VkRenderPass = std::uint64_t;
using VkImage      = std::uint64_t;
using VkImageView  = std::uint64_t;
using VkBuffer     = std::uint64_t;

#endif


namespace sf
{
class RenderTarget;
class Texture;
class VertexBuffer;

////////////////////////////////////////////////////////////
/// \brief Interoperability with raw Vulkan code
///
/// This extends the `sf::Vulkan` namespace of the window module
/// (`SFML/Window/Vulkan.hpp`) with the resources of the Vulkan
/// renderer of the graphics module. The functions here allow
/// mixing SFML drawing with direct Vulkan calls. They are only
/// meaningful while the Vulkan renderer is active and return
/// null handles or do nothing on other renderers.
///
////////////////////////////////////////////////////////////
namespace Vulkan
{
////////////////////////////////////////////////////////////
/// \brief Get the Vulkan instance of the active renderer
///
/// The returned handle is owned by SFML and stays valid as
/// long as at least one SFML graphics resource exists.
///
/// \return The instance, or a null handle if the Vulkan renderer is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API VkInstance getInstance();

////////////////////////////////////////////////////////////
/// \brief Get the physical device the renderer runs on
///
/// \return The physical device, or a null handle if the Vulkan renderer is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API VkPhysicalDevice getPhysicalDevice();

////////////////////////////////////////////////////////////
/// \brief Get the logical device of the active renderer
///
/// \return The device, or a null handle if the Vulkan renderer is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API VkDevice getDevice();

////////////////////////////////////////////////////////////
/// \brief Get the graphics queue all SFML rendering is submitted through
///
/// The queue is not thread-safe: raw submissions are only safe
/// from the thread that also performs the SFML rendering.
///
/// \return The queue, or a null handle if the Vulkan renderer is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API VkQueue getGraphicsQueue();

////////////////////////////////////////////////////////////
/// \brief Get the family index of the graphics queue
///
/// \return The family index, 0 if the Vulkan renderer is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API std::uint32_t getGraphicsQueueFamilyIndex();

////////////////////////////////////////////////////////////
/// \brief Get the command buffer SFML is currently recording into
///
/// A command buffer is started if none is recording. It is
/// submitted by SFML when the frame is presented or a readback
/// requires it; raw commands recorded into it execute in order
/// with the SFML rendering around them.
///
/// \return The command buffer, or a null handle if the Vulkan renderer is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API VkCommandBuffer getCommandBuffer();

////////////////////////////////////////////////////////////
/// \brief Get the render pass of the active render target and open it
///
/// Guarantees a render pass is open on the current render
/// target and returns a handle compatible with it, ready to
/// create raw pipelines against:
/// \code
/// window.draw(background);
/// sf::Vulkan::flush();
/// VkRenderPass pass = sf::Vulkan::getRenderPass();
/// // record raw draws into sf::Vulkan::getCommandBuffer()...
/// sf::Vulkan::resetStates(window);
/// \endcode
///
/// \return The render pass, or a null handle if no render target is active
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API VkRenderPass getRenderPass();

////////////////////////////////////////////////////////////
/// \brief Get the native image of an `sf::Texture`
///
/// This function is not part of the graphics API, it must be
/// used only if you mix `sf::Texture` with Vulkan code. The
/// returned handle is owned by the texture.
///
/// \param texture Texture to access
///
/// \return The image, or a null handle if there is none
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API VkImage getTexture(const Texture& texture);

////////////////////////////////////////////////////////////
/// \brief Get the image view of an `sf::Texture`
///
/// Reference this view from your own descriptor sets to sample
/// the texture in raw shaders.
///
/// \param texture Texture to access
///
/// \return The image view, or a null handle if there is none
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API VkImageView getImageView(const Texture& texture);

////////////////////////////////////////////////////////////
/// \brief Get the native buffer of an `sf::VertexBuffer`
///
/// The buffer contains `sf::Vertex` data: a 20 byte stride of
/// position (2 floats), color (4 normalized bytes) and texture
/// coordinates (2 floats).
///
/// \param vertexBuffer Vertex buffer to access
///
/// \return The buffer, or a null handle if there is none
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API VkBuffer getBuffer(const VertexBuffer& vertexBuffer);

////////////////////////////////////////////////////////////
/// \brief Submit any draws SFML has not recorded yet
///
/// SFML merges consecutive compatible draws into a single draw
/// call and records them when needed. Raw Vulkan commands bypass
/// this mechanism, so call this function before recording your
/// own commands to guarantee that everything drawn through SFML
/// so far is recorded first.
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void flush();

////////////////////////////////////////////////////////////
/// \brief Reset the internal states so that the target is ready for drawing
///
/// Vulkan has no state stack to save and restore: record the
/// state your own rendering relies on yourself, and call this
/// function afterwards so SFML re-records its pipeline before
/// the next `draw()` call.
///
/// Example:
/// \code
/// // raw Vulkan commands here...
/// sf::Vulkan::resetStates(window);
/// window.draw(...);
/// window.draw(...);
/// \endcode
///
/// \param target Render target whose states to reset
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void resetStates(RenderTarget& target);

} // namespace Vulkan

} // namespace sf


////////////////////////////////////////////////////////////
/// \namespace sf::Vulkan
/// \ingroup graphics
///
/// The `sf::Vulkan` namespace spans two modules: the window
/// module part (`SFML/Window/Vulkan.hpp`) helps writing your
/// own Vulkan renderer on top of `sf::Window`, while this
/// graphics module part interoperates with the Vulkan renderer
/// of the graphics module: accessing the instance, device and
/// queue SFML renders through, the native objects behind SFML
/// resources, and handing the pipeline back to SFML after raw
/// Vulkan commands.
///
/// Unlike OpenGL, Vulkan state is not ambient: raw rendering
/// records into the same command buffer SFML uses, so bind all
/// pipeline state your code relies on before drawing, and call
/// `resetStates` before drawing with SFML again.
///
/// These functions only have an effect while the Vulkan
/// renderer is active (see `sf::setRenderer`).
///
////////////////////////////////////////////////////////////
