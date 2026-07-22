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

#include <SFML/Graphics/CoordinateType.hpp>

#include <type_traits>


namespace sf
{
class RenderTarget;
class Shader;
class Texture;
class VertexBuffer;

////////////////////////////////////////////////////////////
/// \brief Interoperability with raw OpenGL code
///
/// The functions in this namespace allow mixing SFML drawing
/// with direct OpenGL calls. They are only meaningful while the
/// OpenGL backend is active and do nothing on other backends.
///
////////////////////////////////////////////////////////////
namespace OpenGL
{
////////////////////////////////////////////////////////////
/// \brief Save the current OpenGL render states and matrices
///
/// This function can be used when you mix SFML drawing
/// and direct OpenGL rendering. Combined with `popStates`,
/// it ensures that:
/// \li SFML's internal states are not messed up by your OpenGL code
/// \li your OpenGL states are not modified by a call to an SFML function
///
/// More specifically, it must be used around code that
/// calls draw functions. Example:
/// \code
/// // OpenGL code here...
/// sf::OpenGL::pushStates(window);
/// window.draw(...);
/// window.draw(...);
/// sf::OpenGL::popStates(window);
/// // OpenGL code here...
/// \endcode
///
/// Note that this function is quite expensive: it saves all the
/// possible OpenGL states and matrices, even the ones you
/// don't care about. Therefore it should be used wisely.
/// It is provided for convenience, but the best results will
/// be achieved if you handle OpenGL states yourself (because
/// you know which states have really changed, and need to be
/// saved and restored). Take a look at the `resetStates`
/// function if you do so.
///
/// \param target Render target whose states to save
///
/// \see `popStates`
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void pushStates(RenderTarget& target);

////////////////////////////////////////////////////////////
/// \brief Restore the previously saved OpenGL render states and matrices
///
/// See the description of `pushStates` to get a detailed
/// description of these functions.
///
/// \param target Render target whose states to restore
///
/// \see `pushStates`
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void popStates(RenderTarget& target);

////////////////////////////////////////////////////////////
/// \brief Reset the internal OpenGL states so that the target is ready for drawing
///
/// This function can be used when you mix SFML drawing
/// and direct OpenGL rendering, if you choose not to use
/// `pushStates`/`popStates`. It makes sure that all OpenGL
/// states needed by SFML are set, so that subsequent draw
/// calls will work as expected.
///
/// Example:
/// \code
/// // OpenGL code here...
/// glPushAttrib(...);
/// sf::OpenGL::resetStates(window);
/// window.draw(...);
/// window.draw(...);
/// glPopAttrib(...);
/// // OpenGL code here...
/// \endcode
///
/// \param target Render target whose states to reset
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void resetStates(RenderTarget& target);

////////////////////////////////////////////////////////////
/// \brief Bind a texture for rendering with raw OpenGL
///
/// This function is not part of the graphics API, it mustn't be
/// used when drawing SFML entities. It must be used only if you
/// mix `sf::Texture` with OpenGL code.
///
/// \code
/// sf::Texture t1, t2;
/// ...
/// sf::OpenGL::bindTexture(&t1);
/// // draw OpenGL stuff that use t1...
/// sf::OpenGL::bindTexture(&t2);
/// // draw OpenGL stuff that use t2...
/// sf::OpenGL::bindTexture(nullptr);
/// // draw OpenGL stuff that use no texture...
/// \endcode
///
/// The `coordinateType` argument controls how texture
/// coordinates will be interpreted. If Normalized (the default), they
/// must be in range [0 .. 1], which is the default way of handling
/// texture coordinates with OpenGL. If Pixels, they must be given
/// in pixels (range [0 .. size]). This mode is used internally by
/// the graphics classes of SFML, it makes the definition of texture
/// coordinates more intuitive for the high-level API, users don't need
/// to compute normalized values.
///
/// \param texture        Pointer to the texture to bind, can be null to use no texture
/// \param coordinateType Type of texture coordinates to use
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void bindTexture(const Texture* texture, CoordinateType coordinateType = CoordinateType::Normalized);

////////////////////////////////////////////////////////////
/// \brief Bind a shader for rendering with raw OpenGL
///
/// This function is not part of the graphics API, it mustn't be
/// used when drawing SFML entities. It must be used only if you
/// mix `sf::Shader` with OpenGL code.
///
/// \code
/// sf::Shader s1, s2;
/// ...
/// sf::OpenGL::bindShader(&s1);
/// // draw OpenGL stuff that use s1...
/// sf::OpenGL::bindShader(&s2);
/// // draw OpenGL stuff that use s2...
/// sf::OpenGL::bindShader(nullptr);
/// // draw OpenGL stuff that use no shader...
/// \endcode
///
/// \param shader Shader to bind, can be null to use no shader
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void bindShader(const Shader* shader);

////////////////////////////////////////////////////////////
/// \brief Bind a vertex buffer for rendering with raw OpenGL
///
/// This function is not part of the graphics API, it mustn't be
/// used when drawing SFML entities. It must be used only if you
/// mix `sf::VertexBuffer` with OpenGL code.
///
/// \code
/// sf::VertexBuffer vb1, vb2;
/// ...
/// sf::OpenGL::bindVertexBuffer(&vb1);
/// // draw OpenGL stuff that use vb1...
/// sf::OpenGL::bindVertexBuffer(&vb2);
/// // draw OpenGL stuff that use vb2...
/// sf::OpenGL::bindVertexBuffer(nullptr);
/// // draw OpenGL stuff that use no vertex buffer...
/// \endcode
///
/// \param vertexBuffer Pointer to the vertex buffer to bind, can be null to use no vertex buffer
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void bindVertexBuffer(const VertexBuffer* vertexBuffer);

////////////////////////////////////////////////////////////
/// \brief Bind a resource for rendering with raw OpenGL
///
/// Generic form of `bindTexture`, `bindShader` and
/// `bindVertexBuffer`, the resource type selects the function
/// to dispatch to. Unbinding works without a cast by naming
/// the type explicitly:
/// \code
/// sf::OpenGL::bind(&texture);
/// // draw OpenGL stuff that use the texture...
/// sf::OpenGL::bind<sf::Texture>(nullptr);
/// // draw OpenGL stuff that use no texture...
/// \endcode
///
/// \param resource Pointer to the resource to bind, can be null to bind no resource
///
////////////////////////////////////////////////////////////
template <typename T>
void bind(const T* resource)
{
    static_assert(std::is_same_v<T, Texture> || std::is_same_v<T, Shader> || std::is_same_v<T, VertexBuffer>,
                  "sf::OpenGL::bind only accepts sf::Texture, sf::Shader or sf::VertexBuffer");

    if constexpr (std::is_same_v<T, Texture>)
        bindTexture(resource);
    else if constexpr (std::is_same_v<T, Shader>)
        bindShader(resource);
    else if constexpr (std::is_same_v<T, VertexBuffer>)
        bindVertexBuffer(resource);
}

} // namespace OpenGL

} // namespace sf


////////////////////////////////////////////////////////////
/// \namespace sf::OpenGL
/// \ingroup graphics
///
/// All interoperability with raw OpenGL code lives in this
/// namespace: saving and restoring the OpenGL states around
/// user OpenGL code, and binding SFML resources for use by
/// raw OpenGL calls.
///
/// These functions only have an effect while the OpenGL
/// renderer is active (see `sf::setRenderer`), on other
/// renderers they do nothing.
///
////////////////////////////////////////////////////////////
