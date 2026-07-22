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
#include <SFML/Graphics/GraphicsDevice.hpp>

#include <SFML/Window/GlResource.hpp>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief OpenGL implementation of the graphics device
///
/// Deriving from GlResource keeps the shared OpenGL context
/// alive for the lifetime of the device.
///
////////////////////////////////////////////////////////////
class GlGraphicsDevice : public GraphicsDevice, GlResource
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    GlGraphicsDevice() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Create a render target implementation for this backend
    ///
    /// \return New render target implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<RenderTargetImpl> createRenderTargetImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a render texture implementation for this backend
    ///
    /// The most capable implementation available is selected.
    ///
    /// \return New render texture implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<RenderTextureImpl> createRenderTextureImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a presentation surface for a render window
    ///
    /// The OpenGL backend presents through the context owned by
    /// `sf::Window`, so a null pointer is returned.
    ///
    /// \param handle       Native handle of the window to present to
    /// \param settings     Requested settings for the surface
    /// \param bitsPerPixel Pixel depth of the window, in bits per pixel
    ///
    /// \return Always a null pointer for the legacy path
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<RenderWindowImpl> createRenderWindowImpl(WindowHandle           handle,
                                                                           const ContextSettings& settings,
                                                                           unsigned int bitsPerPixel) override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a shader implementation for this backend
    ///
    /// \return New shader implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<ShaderImpl> createShaderImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a texture implementation for this backend
    ///
    /// \return New texture implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<TextureImpl> createTextureImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create a vertex buffer implementation for this backend
    ///
    /// \return New vertex buffer implementation
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] std::unique_ptr<VertexBufferImpl> createVertexBufferImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the maximum anti-aliasing level supported for render textures
    ///
    /// \return The maximum anti-aliasing level supported
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] unsigned int getMaximumAntiAliasingLevel() override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the maximum texture size supported by this backend
    ///
    /// \return Maximum size allowed for textures, in pixels
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] unsigned int getMaximumTextureSize() override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether this backend supports shaders
    ///
    /// \return `true` if shaders are supported
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isShaderAvailable() override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether this backend supports geometry shaders
    ///
    /// \return `true` if geometry shaders are supported
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isGeometryShaderAvailable() override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether this backend supports vertex buffers
    ///
    /// \return `true` if vertex buffers are supported
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isVertexBufferAvailable() override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the renderer this device renders through
    ///
    /// \return The renderer
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] Renderer getRenderer() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the shading language this backend consumes
    ///
    /// \return The shading language
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] ShadingLanguage getShadingLanguage() const override;
};

} // namespace sf::priv
