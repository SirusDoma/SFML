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

#include <vector>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Renderers the graphics module can use
///
/// The availability of a renderer depends on the platform and
/// the options SFML was built with, query it with
/// `sf::getAvailableRenderers`.
///
////////////////////////////////////////////////////////////
enum class Renderer
{
    OpenGL,     //!< OpenGL renderer, available on all platforms
    Direct3D11, //!< Direct3D 11 renderer, only available on Windows
    Metal       //!< Metal renderer, only available on macOS
};

////////////////////////////////////////////////////////////
/// \brief Shading languages consumed by `sf::Shader`
///
////////////////////////////////////////////////////////////
enum class ShadingLanguage
{
    Glsl, //!< OpenGL Shading Language, consumed by the OpenGL renderer
    Hlsl, //!< High-Level Shading Language, consumed by the Direct3D 11 renderer
    Msl   //!< Metal Shading Language, consumed by the Metal renderer
};

////////////////////////////////////////////////////////////
/// \brief Select the renderer used by the graphics module
///
/// The renderer must be selected before the first graphics
/// resource (window, texture, shader, ...) is created. Once a
/// resource exists the renderer is locked in and can no longer
/// be changed.
///
/// If the requested renderer is unavailable or another renderer
/// is already locked in, an error is written to `sf::err()` and
/// the selection is left unchanged; check `getRenderer` to see
/// which renderer is in use.
///
/// The default renderer is `sf::Renderer::OpenGL`.
///
/// \param renderer Renderer to use for all subsequent rendering
///
/// \see `getRenderer`, `getAvailableRenderers`
///
////////////////////////////////////////////////////////////
SFML_GRAPHICS_API void setRenderer(Renderer renderer);

////////////////////////////////////////////////////////////
/// \brief Get the renderer the graphics module uses
///
/// Returns the active renderer, or the pending selection if no
/// graphics resource has been created yet.
///
/// \return The renderer
///
/// \see `setRenderer`
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API Renderer getRenderer();

////////////////////////////////////////////////////////////
/// \brief Get the renderers available on this system
///
/// \return All renderers that can be selected, in no particular order
///
/// \see `setRenderer`, `isRendererAvailable`
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API std::vector<Renderer> getAvailableRenderers();

////////////////////////////////////////////////////////////
/// \brief Check whether a renderer is available on this system
///
/// \param renderer Renderer to check
///
/// \return `true` if the renderer can be selected with `setRenderer`
///
/// \see `setRenderer`, `getAvailableRenderers`
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API bool isRendererAvailable(Renderer renderer);

////////////////////////////////////////////////////////////
/// \brief Get the shading language consumed by `sf::Shader`
///
/// Use this to decide which shader sources to load when
/// supporting multiple renderers.
///
/// \return The shading language of the renderer
///
/// \see `setRenderer`
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API ShadingLanguage getShadingLanguage();

} // namespace sf


////////////////////////////////////////////////////////////
/// \file
/// \ingroup graphics
///
/// The graphics module can render through different renderers.
/// OpenGL is the default and is always available; additional
/// renderers depend on the platform and build options:
/// Direct3D 11 on Windows and Metal on macOS.
///
/// The renderer has to be chosen up front, before any graphics
/// resource is created:
/// \code
/// sf::setRenderer(sf::Renderer::Direct3D11);
/// if (sf::getRenderer() != sf::Renderer::Direct3D11)
/// {
///     // Direct3D 11 not available, OpenGL remains selected
/// }
///
/// sf::RenderWindow window(sf::VideoMode({640, 480}), "Renderer demo");
/// \endcode
///
/// Shaders are written in the shading language of the active
/// renderer, query it with `sf::getShadingLanguage`. Metal
/// shader sources contain a single `vertex` or `fragment`
/// function whose name is free; uniforms live in constant
/// buffer structs matched by member name, with vertex buffer
/// indices 0 and 1 reserved for the vertex data and the SFML
/// matrices.
///
/// Note that the raw OpenGL interoperability functions in the
/// `sf::OpenGL` namespace (saving and restoring states, binding
/// resources for direct OpenGL use) only have an effect when
/// the OpenGL renderer is active; their counterparts for the
/// other renderers live in the `sf::D3D11` and `sf::Metal`
/// namespaces.
///
////////////////////////////////////////////////////////////
