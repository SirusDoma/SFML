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
#include <SFML/Graphics/Metal/MetalGraphicsDevice.hpp>
#include <SFML/Graphics/RenderTextureImpl.hpp>

#include <SFML/System/Vector2.hpp>


namespace sf::priv
{
class MetalTextureImpl;

////////////////////////////////////////////////////////////
/// \brief Metal implementation of the render texture
///
/// Rendering happens directly into the target texture, or into
/// a multisampled color buffer that is resolved into the target
/// texture on display. The target texture is re-fetched every
/// pass, mipmap generation may have replaced it.
///
////////////////////////////////////////////////////////////
class MetalRenderTextureImpl : public RenderTextureImpl, public MetalRenderSurface
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor
    ///
    /// \param device Graphics device the render texture lives on
    ///
    ////////////////////////////////////////////////////////////
    explicit MetalRenderTextureImpl(MetalGraphicsDevice& device);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~MetalRenderTextureImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Create the render texture implementation
    ///
    /// \param size     Width and height of the texture to render to
    /// \param texture  Target texture implementation
    /// \param settings Context settings to create render-texture with
    ///
    /// \return `true` if creation has been successful
    ///
    ////////////////////////////////////////////////////////////
    bool create(Vector2u size, TextureImpl& texture, const ContextSettings& settings) override;

    ////////////////////////////////////////////////////////////
    /// \brief Activate or deactivate the render texture for rendering
    ///
    /// \param active `true` to activate, `false` to deactivate
    ///
    /// \return `true` on success, `false` on failure
    ///
    ////////////////////////////////////////////////////////////
    bool activate(bool active) override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell if the render-texture will use sRGB encoding when drawing on it
    ///
    /// \return `true` if the render-texture use sRGB encoding, `false` otherwise
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isSrgb() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Update the pixels of the target texture
    ///
    /// \param texture Target texture implementation
    ///
    ////////////////////////////////////////////////////////////
    void updateTexture(TextureImpl& texture) override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the rendered pixels end up flipped vertically in the target texture
    ///
    /// \return `true` if the target texture's pixels are flipped
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool arePixelsFlipped() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether displaying requires a full activation of the render texture
    ///
    /// \return `true` if display() must fully activate the render texture
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool needsFullActivationForDisplay() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the target texture is attached to the render surface
    ///
    /// \return `true` if the target texture is rendered to directly (e.g. a framebuffer attachment)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isTextureAttachment() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Provide the attachments of the next render pass
    ///
    /// \param attachments Attachments to fill in
    ///
    /// \return `true` if the attachments are ready to be drawn into
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool prepareAttachments(MetalSurfaceAttachments& attachments) override;

    ////////////////////////////////////////////////////////////
    /// \brief Get a texture holding the render texture's rendered content
    ///
    /// \return The target texture, with multisampled content resolved into it
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalTexturePtr acquireReadableColorTexture() override;

private:
    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    MetalGraphicsDevice&   m_device;              //!< Graphics device the render texture lives on
    MetalTextureImpl*      m_targetTexture{};     //!< Texture rendering is directed to, not owned
    NSPtr<MetalTexturePtr> m_multisampleTexture;  //!< Multisampled color buffer, null when not multisampled
    NSPtr<MetalTexturePtr> m_depthStencilTexture; //!< Depth-stencil buffer, can be null
    std::uint32_t m_depthStencilFormat{}; //!< Raw MTLPixelFormat of the depth-stencil buffer, 0 when none
    unsigned int  m_sampleCount{1};       //!< Samples per pixel of the color buffer
    Vector2u      m_size;                 //!< Size of the render texture
    bool          m_sRgb{};               //!< Whether the target texture uses sRGB encoding
};

} // namespace sf::priv
