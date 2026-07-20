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
#include <SFML/Graphics/D3D11/D3D11GraphicsDevice.hpp>
#include <SFML/Graphics/RenderWindowImpl.hpp>

#include <SFML/Window/WindowHandle.hpp>

#include <dxgi1_2.h>


namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Direct3D 11 implementation of the render window presentation surface
///
////////////////////////////////////////////////////////////
class D3D11RenderWindowImpl : public RenderWindowImpl
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Constructor, creates the swap chain for a window
    ///
    /// \param device       Graphics device presenting to the window
    /// \param handle       Handle of the window to present to
    /// \param settings     Requested settings of the presentation surface
    /// \param bitsPerPixel Requested pixel depth, unused, the swap chain format is fixed
    ///
    ////////////////////////////////////////////////////////////
    D3D11RenderWindowImpl(D3D11GraphicsDevice&   device,
                          WindowHandle           handle,
                          const ContextSettings& settings,
                          unsigned int           bitsPerPixel);

    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~D3D11RenderWindowImpl() override;

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

private:
    ////////////////////////////////////////////////////////////
    /// \brief Create the views of the back buffer
    ///
    /// Has to be called again after every ResizeBuffers, the old
    /// views keep the old buffers alive.
    ///
    /// \param depthStencil Whether a depth-stencil buffer is wanted
    ///
    /// \return `true` if all wanted views were created
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] bool createViews(bool depthStencil);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    D3D11GraphicsDevice&           m_device;              //!< Graphics device presenting to the window
    ComPtr<IDXGISwapChain1>        m_swapChain;           //!< Swap chain presenting to the window
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;    //!< View of the back buffer
    ComPtr<ID3D11Texture2D>        m_depthStencilTexture; //!< Depth-stencil buffer, can be null
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;    //!< View of the depth-stencil buffer, can be null
    ContextSettings                m_settings;            //!< Settings actually used by the surface
    UINT                           m_syncInterval{};      //!< Present sync interval, 1 when v-sync is enabled
    bool                           m_sRgb{};              //!< Whether the back buffer uses sRGB encoding
};

} // namespace sf::priv
