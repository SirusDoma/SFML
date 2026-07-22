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
#include <SFML/Graphics/RenderWindowImpl.hpp>

#include <SFML/Window/WindowHandle.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Metal implementation of the render window presentation surface
///
/// Presents through a Core Animation Metal layer hosted on the
/// window's content view. The drawable is acquired lazily on the
/// first render pass of a frame and handed back on present.
///
////////////////////////////////////////////////////////////
class MetalRenderWindowImpl : public RenderWindowImpl, public MetalRenderSurface
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor, attaches the layer to the window
    ///
    /// \param device       Graphics device presenting to the window
    /// \param handle       Handle of the window to present to
    /// \param settings     Requested settings of the presentation surface
    /// \param bitsPerPixel Requested pixel depth, unused, the layer format is fixed
    ///
    ////////////////////////////////////////////////////////////
    MetalRenderWindowImpl(MetalGraphicsDevice&   device,
                          WindowHandle           handle,
                          const ContextSettings& settings,
                          unsigned int           bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~MetalRenderWindowImpl() override;

    ////////////////////////////////////////////////////////////
    /// \brief Present the rendered frame on screen
    ///
    ////////////////////////////////////////////////////////////
    void present() override;

    ////////////////////////////////////////////////////////////
    /// \brief Enable or disable vertical synchronization
    ///
    /// \param enabled `true` to enable v-sync, `false` to deactivate it
    ///
    ////////////////////////////////////////////////////////////
    void setVerticalSyncEnabled(bool enabled) override;

    ////////////////////////////////////////////////////////////
    /// \brief Resize the presentation surface
    ///
    /// \param size New size of the window, in pixels
    ///
    ////////////////////////////////////////////////////////////
    void resize(Vector2u size) override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the settings of the presentation surface
    ///
    /// \return Settings actually used by the surface
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] const ContextSettings& getSettings() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Tell whether the surface uses sRGB encoding
    ///
    /// \return `true` if the surface uses sRGB encoding
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool isSrgb() const override;

    ////////////////////////////////////////////////////////////
    /// \brief Activate or deactivate the surface as the current render target
    ///
    /// \param active `true` to activate, `false` to deactivate
    ///
    /// \return `true` on success, `false` on failure
    ///
    ////////////////////////////////////////////////////////////
    bool activate(bool active) override;

    ////////////////////////////////////////////////////////////
    /// \brief Provide the attachments of the next render pass
    ///
    /// Acquires the drawable of the frame on first use. Fails
    /// when no drawable is available, like when the window is
    /// occluded, dropping the frame.
    ///
    /// \param attachments Attachments to fill in
    ///
    /// \return `true` if the attachments are ready to be drawn into
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool prepareAttachments(MetalSurfaceAttachments& attachments) override;

    ////////////////////////////////////////////////////////////
    /// \brief Get a texture holding the window's rendered content
    ///
    /// Fails when the presentation intent made the drawables
    /// write-only, or when nothing was rendered this frame.
    ///
    /// \return Texture with the rendered content, null when the window cannot be read
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalTexturePtr acquireReadableColorTexture() override;

    ////////////////////////////////////////////////////////////
    /// \brief Get the layer presenting to the window
    ///
    /// \return The layer, null if surface creation failed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] MetalLayerPtr getLayer() const;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Create the multisampled color and depth-stencil textures
    ///
    /// \param size Size of the textures, in pixels
    ///
    ////////////////////////////////////////////////////////////
    void createAncillaryTextures(Vector2u size);

    ////////////////////////////////////////////////////////////
    /// \brief Frames in flight shared with the completion handlers
    ///
    /// The application is paced by bounding the command buffers
    /// that have been presented but not yet executed, the moral
    /// equivalent of the frame latency waitable of Direct3D.
    ///
    ////////////////////////////////////////////////////////////
    struct FramePacer
    {
        std::mutex              mutex;     //!< Guards the pending count
        std::condition_variable condition; //!< Signaled when a frame completes
        unsigned int            pending{}; //!< Presented command buffers not yet executed
    };

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    MetalGraphicsDevice&        m_device;              //!< Graphics device presenting to the window
    NSPtr<void*>                m_view;                //!< View hosting the layer
    NSPtr<MetalLayerPtr>        m_layer;               //!< Layer presenting to the window
    NSPtr<MetalDrawablePtr>     m_drawable;            //!< Drawable of the current frame, null between frames
    NSPtr<MetalTexturePtr>      m_multisampleTexture;  //!< Multisampled color buffer, null when not multisampled
    NSPtr<MetalTexturePtr>      m_depthStencilTexture; //!< Depth-stencil buffer, can be null
    std::shared_ptr<FramePacer> m_pacer;               //!< Frames in flight, shared with the completion handlers
    ContextSettings             m_settings;            //!< Settings actually used by the surface
    std::uint32_t               m_depthStencilFormat{}; //!< Raw MTLPixelFormat of the depth-stencil buffer, 0 when none
    unsigned int                m_sampleCount{1};       //!< Samples per pixel of the color buffer
    bool                        m_sRgb{};               //!< Whether the layer uses sRGB encoding
};

} // namespace sf::priv
