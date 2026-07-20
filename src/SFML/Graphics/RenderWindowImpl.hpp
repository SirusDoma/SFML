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
#include <SFML/Window/ContextSettings.hpp>

#include <SFML/System/Vector2.hpp>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Abstract base class for render window presentation surfaces
///
/// Only used by backends that do not present through an OpenGL
/// context (the OpenGL backend keeps presenting through the
/// context owned by `sf::Window`).
///
////////////////////////////////////////////////////////////
class RenderWindowImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    RenderWindowImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~RenderWindowImpl() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy constructor
    ///
    ////////////////////////////////////////////////////////////
    RenderWindowImpl(const RenderWindowImpl&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy assignment
    ///
    ////////////////////////////////////////////////////////////
    RenderWindowImpl& operator=(const RenderWindowImpl&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Present the rendered frame on screen
    ///
    ////////////////////////////////////////////////////////////
    virtual void present() = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable vertical synchronization
    ///
    /// \param enabled `true` to enable v-sync, `false` to deactivate it
    ///
    ////////////////////////////////////////////////////////////
    virtual void setVerticalSyncEnabled(bool enabled) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Resize the presentation surface
    ///
    /// \param size New size of the window, in pixels
    ///
    ////////////////////////////////////////////////////////////
    virtual void resize(Vector2u size) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Get the settings of the presentation surface
    ///
    /// \return Settings actually used by the surface
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual const ContextSettings& getSettings() const = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the surface uses sRGB encoding
    ///
    /// \return `true` if the surface uses sRGB encoding
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual bool isSrgb() const = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Activate or deactivate the surface as the current render target
    ///
    /// \param active `true` to activate, `false` to deactivate
    ///
    /// \return `true` on success, `false` on failure
    ///
    ////////////////////////////////////////////////////////////
    virtual bool activate(bool active) = 0;
};

} // namespace sf::priv
