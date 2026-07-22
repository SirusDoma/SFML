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


#ifdef __OBJC__
@protocol MTLBuffer;
@protocol MTLCommandBuffer;
@protocol MTLCommandQueue;
@protocol MTLDevice;
@protocol MTLTexture;
@class CAMetalLayer;
#endif


namespace sf
{
class RenderTarget;
class Texture;
class VertexBuffer;

////////////////////////////////////////////////////////////
/// \brief Interoperability with raw Metal code
///
/// The functions in this namespace allow mixing SFML drawing
/// with direct Metal calls. They are only meaningful while the
/// Metal renderer is active and return null pointers or do
/// nothing on other renderers.
///
////////////////////////////////////////////////////////////
namespace Metal
{
////////////////////////////////////////////////////////////
// Handles to Metal objects, opaque pointers outside of
// Objective-C++ so the header stays includable from plain C++
////////////////////////////////////////////////////////////
#ifdef __OBJC__
using DevicePtr        = id<MTLDevice>;
using CommandQueuePtr  = id<MTLCommandQueue>;
using CommandBufferPtr = id<MTLCommandBuffer>;
using TexturePtr       = id<MTLTexture>;
using BufferPtr        = id<MTLBuffer>;
using LayerPtr         = CAMetalLayer*;
#else
using DevicePtr        = void*;
using CommandQueuePtr  = void*;
using CommandBufferPtr = void*;
using TexturePtr       = void*;
using BufferPtr        = void*;
using LayerPtr         = void*;
#endif

////////////////////////////////////////////////////////////
/// \brief Get the Metal device of the active renderer
///
/// The returned object is owned by SFML and stays valid as
/// long as at least one SFML graphics resource exists.
///
/// \return The device, or a null pointer if the Metal renderer is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API DevicePtr getDevice();

////////////////////////////////////////////////////////////
/// \brief Get the command queue of the active renderer
///
/// All SFML rendering is submitted through this queue.
///
/// \return The command queue, or a null pointer if the Metal renderer is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API CommandQueuePtr getCommandQueue();

////////////////////////////////////////////////////////////
/// \brief Get the command buffer SFML is currently recording into
///
/// Encode your own passes into this command buffer to keep them
/// ordered with the SFML rendering of the frame; call `flush`
/// first so everything drawn through SFML so far is encoded
/// ahead of yours. The command buffer is committed when the
/// frame is presented or read back.
///
/// \return The command buffer, or a null pointer if the Metal renderer is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API CommandBufferPtr getCommandBuffer();

////////////////////////////////////////////////////////////
/// \brief Get the color texture of the surface currently bound for rendering
///
/// This is the texture SFML render passes draw into: the
/// drawable of a render window, the target texture of a render
/// texture, or their multisampled color buffer when
/// anti-aliasing is enabled. A window's drawable is acquired on
/// first use each frame.
///
/// \return The bound color texture, or a null pointer if no surface is bound
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API TexturePtr getRenderTargetTexture();

////////////////////////////////////////////////////////////
/// \brief Get the depth-stencil texture of the surface currently bound for rendering
///
/// A depth-stencil texture only exists when the render target
/// was created with depth or stencil bits in its
/// `ContextSettings`; a depth plane only when depth bits were
/// requested.
///
/// \return The bound depth-stencil texture, or a null pointer if there is none
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API TexturePtr getDepthStencilTexture();

////////////////////////////////////////////////////////////
/// \brief Get the layer of the render window active for rendering
///
/// Use it to tune presentation properties SFML does not manage
/// itself. The layer is owned by the window.
///
/// \return The layer, or a null pointer if no render window is active
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API LayerPtr getLayer();

////////////////////////////////////////////////////////////
/// \brief Get the native texture of an `sf::Texture`
///
/// This function is not part of the graphics API, it must be
/// used only if you mix `sf::Texture` with Metal code. The
/// returned object is owned by the texture, and is replaced
/// when a mipmap is generated for the texture the first time.
///
/// \param texture Texture to access
///
/// \return The native texture, or a null pointer if there is none
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API TexturePtr getTexture(const Texture& texture);

////////////////////////////////////////////////////////////
/// \brief Get the native buffer of an `sf::VertexBuffer`
///
/// The buffer contains `sf::Vertex` data: a 20 byte stride of
/// position (2 floats), color (4 normalized bytes) and texture
/// coordinates (2 floats).
///
/// \param vertexBuffer Vertex buffer to access
///
/// \return The native buffer, or a null pointer if there is none
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API BufferPtr getBuffer(const VertexBuffer& vertexBuffer);

////////////////////////////////////////////////////////////
/// \brief Encode any draws SFML has not encoded yet and end the open render pass
///
/// SFML merges consecutive compatible draws and encodes them
/// when needed, and keeps a render pass open across draws. Call
/// this function before encoding your own passes so everything
/// drawn through SFML so far is encoded ahead of yours, and no
/// SFML encoder is left open:
/// \code
/// window.draw(background);
/// sf::Metal::flush();
/// // encode your own passes into sf::Metal::getCommandBuffer()...
/// sf::Metal::resetStates(window);
/// \endcode
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void flush();

////////////////////////////////////////////////////////////
/// \brief Reset the internal states so that the target is ready for drawing
///
/// Metal has no state stack to save and restore: encode your own
/// rendering in your own passes, and call this function
/// afterwards so SFML re-applies its pipeline before the next
/// `draw()` call.
///
/// Example:
/// \code
/// // Metal code here...
/// sf::Metal::resetStates(window);
/// window.draw(...);
/// window.draw(...);
/// \endcode
///
/// \param target Render target whose states to reset
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void resetStates(RenderTarget& target);

} // namespace Metal

} // namespace sf


////////////////////////////////////////////////////////////
/// \namespace sf::Metal
/// \ingroup graphics
///
/// All interoperability with raw Metal code lives in this
/// namespace: accessing the device, command queue and command
/// buffer SFML renders through, the native objects behind SFML
/// resources, and handing the pipeline back to SFML after raw
/// Metal passes.
///
/// Unlike OpenGL, Metal state is not ambient: encode your own
/// rendering into passes on the command buffer SFML is
/// recording, after calling `flush` so the SFML draws of the
/// frame stay ordered ahead of yours, and call `resetStates`
/// before drawing with SFML again.
///
/// These functions only have an effect while the Metal renderer
/// is active (see `sf::setRenderer`).
///
////////////////////////////////////////////////////////////
