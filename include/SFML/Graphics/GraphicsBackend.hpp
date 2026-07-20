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


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Renderer backends the graphics module can use
///
/// The availability of a backend depends on the platform and
/// the options SFML was built with, query it with
/// `sf::isGraphicsBackendAvailable`.
///
////////////////////////////////////////////////////////////
enum class GraphicsBackend
{
    OpenGL,    //!< OpenGL renderer, available on all platforms
    Direct3D11 //!< Direct3D 11 renderer, only available on Windows
};

////////////////////////////////////////////////////////////
/// \brief Shading languages consumed by `sf::Shader`
///
////////////////////////////////////////////////////////////
enum class ShadingLanguage
{
    Glsl, //!< OpenGL Shading Language, consumed by the OpenGL backend
    Hlsl  //!< High-Level Shading Language, consumed by the Direct3D 11 backend
};

////////////////////////////////////////////////////////////
/// \brief Select the renderer backend used by the graphics module
///
/// The backend must be selected before the first graphics
/// resource (window, texture, shader, ...) is created. Once a
/// resource exists the backend is locked in and can no longer
/// be changed; calling this function then fails, unless the
/// requested backend is the one already in use.
///
/// The default backend is `sf::GraphicsBackend::OpenGL`.
///
/// \param backend Backend to use for all subsequent rendering
///
/// \return `true` if the backend was selected, `false` if it is
///         unavailable or another backend is already in use
///
/// \see `getGraphicsBackend`, `isGraphicsBackendAvailable`
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API bool setGraphicsBackend(GraphicsBackend backend);

////////////////////////////////////////////////////////////
/// \brief Get the renderer backend the graphics module uses
///
/// Returns the active backend, or the pending selection if no
/// graphics resource has been created yet.
///
/// \return The graphics backend
///
/// \see `setGraphicsBackend`
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API GraphicsBackend getGraphicsBackend();

////////////////////////////////////////////////////////////
/// \brief Check whether a renderer backend is available
///
/// \param backend Backend to check
///
/// \return `true` if the backend can be selected on this system
///
/// \see `setGraphicsBackend`
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API bool isGraphicsBackendAvailable(GraphicsBackend backend);

////////////////////////////////////////////////////////////
/// \brief Get the shading language consumed by `sf::Shader`
///
/// Use this to decide which shader sources to load when
/// supporting multiple backends.
///
/// \return The shading language of the graphics backend
///
/// \see `setGraphicsBackend`
///
////////////////////////////////////////////////////////////
[[nodiscard]] SFML_GRAPHICS_API ShadingLanguage getShadingLanguage();

} // namespace sf


////////////////////////////////////////////////////////////
/// \file
/// \ingroup graphics
///
/// The graphics module can render through different backends.
/// OpenGL is the default and is always available; additional
/// backends depend on the platform and build options.
///
/// The backend has to be chosen up front, before any graphics
/// resource is created:
/// \code
/// if (!sf::setGraphicsBackend(sf::GraphicsBackend::Direct3D11))
/// {
///     // Direct3D 11 not available, OpenGL remains selected
/// }
///
/// sf::RenderWindow window(sf::VideoMode({640, 480}), "Backend demo");
/// \endcode
///
/// Note that the raw OpenGL interoperability functions in the
/// `sf::OpenGL` namespace (saving and restoring states, binding
/// resources for direct OpenGL use) only have an effect when
/// the OpenGL backend is active.
///
////////////////////////////////////////////////////////////
