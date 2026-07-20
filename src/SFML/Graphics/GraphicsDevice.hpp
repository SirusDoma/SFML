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
#include <SFML/Graphics/GraphicsBackend.hpp>

#include <SFML/Window/WindowHandle.hpp>

#include <memory>


namespace sf
{
struct ContextSettings;

namespace priv
{
class RenderTargetImpl;
class RenderTextureImpl;
class RenderWindowImpl;
class ShaderImpl;
class TextureImpl;
class VertexBufferImpl;

////////////////////////////////////////////////////////////
/// \brief Abstract base class for graphics backend devices
///
/// A graphics device represents one rendering backend (OpenGL,
/// Direct3D 11, ...). It acts as the factory for all
/// backend-specific resource implementations and answers
/// backend capability queries.
///
////////////////////////////////////////////////////////////
class GraphicsDevice
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    GraphicsDevice() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    virtual ~GraphicsDevice() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy constructor
    ///
    ////////////////////////////////////////////////////////////
    GraphicsDevice(const GraphicsDevice&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy assignment
    ///
    ////////////////////////////////////////////////////////////
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Create a render target implementation for this backend
    ///
    /// \return New render target implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual std::unique_ptr<RenderTargetImpl> createRenderTargetImpl() = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Create a render texture implementation for this backend
    ///
    /// The most capable implementation available is selected.
    ///
    /// \return New render texture implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual std::unique_ptr<RenderTextureImpl> createRenderTextureImpl() = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Create a presentation surface for a render window
    ///
    /// Backends that present through the OpenGL context owned by
    /// `sf::Window` return a null pointer.
    ///
    /// \param handle       Native handle of the window to present to
    /// \param settings     Requested settings for the surface
    /// \param bitsPerPixel Pixel depth of the window, in bits per pixel
    ///
    /// \return New presentation surface, or a null pointer for the legacy path
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual std::unique_ptr<RenderWindowImpl> createRenderWindowImpl(WindowHandle           handle,
                                                                                   const ContextSettings& settings,
                                                                                   unsigned int bitsPerPixel) = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Create a shader implementation for this backend
    ///
    /// \return New shader implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual std::unique_ptr<ShaderImpl> createShaderImpl() = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Create a texture implementation for this backend
    ///
    /// \return New texture implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual std::unique_ptr<TextureImpl> createTextureImpl() = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Create a vertex buffer implementation for this backend
    ///
    /// \return New vertex buffer implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual std::unique_ptr<VertexBufferImpl> createVertexBufferImpl() = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Get the maximum anti-aliasing level supported for render textures
    ///
    /// \return The maximum anti-aliasing level supported
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual unsigned int getMaximumAntiAliasingLevel() = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Get the backend this device renders through
    ///
    /// \return The graphics backend
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual GraphicsBackend getBackend() const = 0;

    ////////////////////////////////////////////////////////////
    /// \brief Get the shading language this backend consumes
    ///
    /// \return The shading language
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual ShadingLanguage getShadingLanguage() const = 0;
};

////////////////////////////////////////////////////////////
/// \brief Get the graphics device, creating it if none is alive
///
/// The backend used for the lifetime of the returned device is
/// determined on creation. Resources keep the device alive by
/// holding on to the returned shared_ptr; once the last holder
/// releases it, the device is destroyed and a subsequent call
/// creates a new one.
///
/// \return shared_ptr to the graphics device
///
////////////////////////////////////////////////////////////
[[nodiscard]] std::shared_ptr<GraphicsDevice> ensureGraphicsDevice();

////////////////////////////////////////////////////////////
/// \brief Get the currently alive graphics device, if any
///
/// The returned pointer is only guaranteed to stay valid while
/// the caller also holds a shared_ptr obtained from
/// ensureGraphicsDevice().
///
/// \return Pointer to the graphics device, or `nullptr` if no device is alive
///
////////////////////////////////////////////////////////////
[[nodiscard]] GraphicsDevice* getGraphicsDevice();

} // namespace priv

} // namespace sf
