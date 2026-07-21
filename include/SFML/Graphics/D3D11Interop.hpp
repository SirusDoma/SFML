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


struct ID3D11Buffer;
struct ID3D11DepthStencilView;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;


namespace sf
{
class RenderTarget;
class Texture;
class VertexBuffer;

////////////////////////////////////////////////////////////
/// \brief Interoperability with raw Direct3D 11 code
///
/// The functions in this namespace allow mixing SFML drawing
/// with direct Direct3D 11 calls. They are only meaningful
/// while the Direct3D 11 backend is active and return null
/// pointers or do nothing on other backends.
///
////////////////////////////////////////////////////////////
namespace D3D11
{
////////////////////////////////////////////////////////////
/// \brief Get the Direct3D device of the active backend
///
/// The returned pointer is owned by SFML and stays valid as
/// long as at least one SFML graphics resource exists.
///
/// \return The device, or a null pointer if the Direct3D 11 backend is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API ID3D11Device* getDevice();

////////////////////////////////////////////////////////////
/// \brief Get the immediate context of the active backend
///
/// All SFML rendering goes through this context. It is not
/// thread-safe: raw calls are only safe from the thread that
/// also performs the SFML rendering.
///
/// \return The immediate context, or a null pointer if the Direct3D 11 backend is not in use
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API ID3D11DeviceContext* getContext();

////////////////////////////////////////////////////////////
/// \brief Get the render target view of the surface currently bound for rendering
///
/// \return The bound color output view, or a null pointer if no surface is bound
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API ID3D11RenderTargetView* getRenderTargetView();

////////////////////////////////////////////////////////////
/// \brief Get the depth-stencil view of the surface currently bound for rendering
///
/// Use it to clear the depth buffer before your own 3D rendering:
/// \code
/// context->ClearDepthStencilView(sf::D3D11::getDepthStencilView(), D3D11_CLEAR_DEPTH, 1.0f, 0);
/// \endcode
///
/// A depth-stencil view only exists when the render target was
/// created with depth or stencil bits in its `ContextSettings`.
///
/// \return The bound depth-stencil view, or a null pointer if there is none
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API ID3D11DepthStencilView* getDepthStencilView();

////////////////////////////////////////////////////////////
/// \brief Get the native texture of an `sf::Texture`
///
/// This function is not part of the graphics API, it must be
/// used only if you mix `sf::Texture` with Direct3D code. The
/// returned pointer is owned by the texture.
///
/// \param texture Texture to access
///
/// \return The native texture, or a null pointer if there is none
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API ID3D11Texture2D* getTexture(const Texture& texture);

////////////////////////////////////////////////////////////
/// \brief Get the shader resource view of an `sf::Texture`
///
/// Bind this view to sample the texture from your own shaders:
/// \code
/// ID3D11ShaderResourceView* view = sf::D3D11::getShaderResourceView(texture);
/// context->PSSetShaderResources(0, 1, &view);
/// \endcode
///
/// \param texture Texture to access
///
/// \return The shader resource view, or a null pointer if there is none
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API ID3D11ShaderResourceView* getShaderResourceView(const Texture& texture);

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
[[nodiscard]] SFML_GRAPHICS_API ID3D11Buffer* getBuffer(const VertexBuffer& vertexBuffer);

////////////////////////////////////////////////////////////
/// \brief Submit any draws SFML has not sent to the device yet
///
/// SFML merges consecutive compatible draws into a single draw
/// call and submits them when needed. Raw Direct3D calls bypass
/// this mechanism, so call this function before issuing your own
/// rendering to guarantee that everything drawn through SFML so
/// far is rendered first:
/// \code
/// window.draw(background);
/// sf::D3D11::flush();
/// // Direct3D code here...
/// sf::D3D11::resetStates(window);
/// \endcode
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void flush();

////////////////////////////////////////////////////////////
/// \brief Reset the internal states so that the target is ready for drawing
///
/// Direct3D has no state stack to save and restore: set the
/// states your own rendering relies on yourself, and call this
/// function afterwards so SFML rebinds its pipeline before the
/// next `draw()` call.
///
/// Example:
/// \code
/// // Direct3D code here...
/// sf::D3D11::resetStates(window);
/// window.draw(...);
/// window.draw(...);
/// \endcode
///
/// \param target Render target whose states to reset
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void resetStates(RenderTarget& target);

} // namespace D3D11

} // namespace sf


////////////////////////////////////////////////////////////
/// \namespace sf::D3D11
/// \ingroup graphics
///
/// All interoperability with raw Direct3D 11 code lives in
/// this namespace: accessing the device and immediate context
/// SFML renders through, the native objects behind SFML
/// resources, and handing the pipeline back to SFML after raw
/// Direct3D calls.
///
/// Unlike OpenGL, Direct3D state is not ambient: raw rendering
/// works against the same immediate context SFML uses, so set
/// all pipeline states your code relies on before drawing, and
/// call `resetStates` before drawing with SFML again.
///
/// These functions only have an effect while the Direct3D 11
/// backend is active (see `sf::setGraphicsBackend`).
///
////////////////////////////////////////////////////////////
