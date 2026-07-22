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

#include <chrono>
#include <dxgi1_3.h>


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
    /// \brief Watch whether v-synced flip presentation is actually paced
    ///
    /// A v-synced present cycle can never legitimately complete
    /// faster than the display's refresh period. Broken driver
    /// states exist that execute v-synced flip-model presents
    /// immediately; when sustained impossibly fast cycles are
    /// observed, the window falls back to the blit model, whose
    /// pacing the compositor enforces.
    ///
    /// \param presentResult Result of the present call of this cycle
    ///
    ////////////////////////////////////////////////////////////
    void monitorPresentationPacing(HRESULT presentResult);

    ////////////////////////////////////////////////////////////
    /// \brief Replace the flip-model swap chain with a blit-model one
    ///
    ////////////////////////////////////////////////////////////
    void fallBackToBlitPresentation();

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    D3D11GraphicsDevice&    m_device;                  //!< Graphics device presenting to the window
    WindowHandle            m_handle{};                //!< Window the swap chain presents to
    ComPtr<IDXGISwapChain1> m_swapChain;               //!< Swap chain presenting to the window
    ComPtr<IDXGISwapChain2> m_swapChain2;              //!< 1.3 view of the swap chain, null without a waitable object
    HANDLE                  m_frameLatencyWaitable{};  //!< Signaled when the presentation queue has room, can be null
    ComPtr<ID3D11RenderTargetView> m_renderTargetView; //!< View of the back buffer
    ComPtr<ID3D11Texture2D>        m_depthStencilTexture; //!< Depth-stencil buffer, can be null
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;    //!< View of the depth-stencil buffer, can be null
    ContextSettings                m_settings;            //!< Settings actually used by the surface
    UINT                           m_syncInterval{};      //!< Present sync interval, 1 when v-sync is enabled
    bool                           m_sRgb{};              //!< Whether the back buffer uses sRGB encoding
    bool                           m_flipModel{};         //!< Whether the swap chain presents through the flip model
    UINT                           m_swapChainFlags{};    //!< Flags the swap chain was created with

    std::chrono::steady_clock::time_point m_lastPresentTime; //!< When the previous present cycle completed
    unsigned int m_unsyncedPresentStreak{}; //!< Consecutive v-synced cycles that completed impossibly fast
};

} // namespace sf::priv
