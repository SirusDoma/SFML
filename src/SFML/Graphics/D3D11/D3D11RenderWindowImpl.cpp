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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/D3D11/D3D11GraphicsDevice.hpp>
#include <SFML/Graphics/D3D11/D3D11RenderWindowImpl.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <array>
#include <dxgi1_5.h>
#include <ostream>


namespace sf::priv
{
////////////////////////////////////////////////////////////
D3D11RenderWindowImpl::D3D11RenderWindowImpl(D3D11GraphicsDevice&          device,
                                             WindowHandle                  handle,
                                             const ContextSettings&        settings,
                                             [[maybe_unused]] unsigned int bitsPerPixel) :
    m_device(device),
    m_handle(handle)
{
    ID3D11Device* d3dDevice = m_device.getDevice();
    if (!d3dDevice)
        return;

    // The swap chain has to be created by the factory that created the device
    ComPtr<IDXGIDevice>   dxgiDevice;
    ComPtr<IDXGIAdapter>  adapter;
    ComPtr<IDXGIFactory2> factory;
    if (!d3dCheck(d3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) ||
        !d3dCheck(dxgiDevice->GetAdapter(&adapter)) || !d3dCheck(adapter->GetParent(IID_PPV_ARGS(&factory))))
        return;

    const DXGI_FORMAT  blitFormat = settings.sRgbCapable ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    const unsigned int samples = m_device.clampAntiAliasingLevel(std::max(settings.antiAliasingLevel, 1u), blitFormat);

    // Tearing support is required to present uncapped with the flip model
    UINT                  tearingFlag = 0;
    ComPtr<IDXGIFactory5> factory5;
    BOOL                  allowTearing = FALSE;
    if (SUCCEEDED(factory.As(&factory5)) &&
        SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))) &&
        allowTearing)
        tearingFlag = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    // Flip-model presentation shares the buffers with the compositor instead of copying them,
    // giving frames a shorter path to the screen, but each present is a costlier transaction
    // and its buffers can be neither multisampled nor sRGB-formatted: multisampled windows
    // and windows asking for maximum uncapped throughput use the blit model, sRGB windows
    // render through an sRGB view of the linear buffer.
    // Newest supported model first, ending with the blit model every system supports.
    struct Attempt
    {
        DXGI_SWAP_EFFECT swapEffect;
        UINT             bufferCount;
        DXGI_FORMAT      format;
        UINT             flags;
    };

    // clang-format off
    constexpr std::size_t     blitAttempt = 2;
    const std::array<Attempt, 3> attempts = {{
        {DXGI_SWAP_EFFECT_FLIP_DISCARD,    3, DXGI_FORMAT_R8G8B8A8_UNORM,
         tearingFlag | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT},    // Windows 10
        {DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0}, // Windows 8
        {DXGI_SWAP_EFFECT_DISCARD,         1, blitFormat,                 0}, // Windows 7
    }};
    // clang-format on

    // The blit model behaves like classic swap chains everywhere, so it is the default;
    // the flip model is chosen by an explicit low-latency request, and broken driver
    // states that execute its v-synced presents without pacing are caught at runtime
    // by monitorPresentationPacing, which falls back to the blit model
    const bool forceBlit = (samples > 1) || (settings.presentation != ContextSettings::Presentation::LowLatency);

    // Width and height are left zero so the buffers are sized from the window
    for (std::size_t i = forceBlit ? blitAttempt : 0; (i < attempts.size()) && !m_swapChain; ++i)
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        swapChainDesc.Format           = attempts[i].format;
        swapChainDesc.SampleDesc.Count = (i == blitAttempt) ? samples : 1;
        swapChainDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount      = attempts[i].bufferCount;
        swapChainDesc.SwapEffect       = attempts[i].swapEffect;
        swapChainDesc.Scaling          = DXGI_SCALING_STRETCH;
        swapChainDesc.Flags            = attempts[i].flags;

        if (SUCCEEDED(factory->CreateSwapChainForHwnd(d3dDevice, handle, &swapChainDesc, nullptr, nullptr, &m_swapChain)))
        {
            m_flipModel      = attempts[i].swapEffect != DXGI_SWAP_EFFECT_DISCARD;
            m_swapChainFlags = attempts[i].flags;
        }
    }

    if (!m_swapChain)
    {
        err() << "Failed to create the swap chain" << std::endl;
        return;
    }

    // The waitable object paces the application to the presentation queue: waiting on it
    // before rendering keeps at most one frame queued, minimizing the input-to-display delay
    if (m_swapChainFlags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
    {
        if (SUCCEEDED(m_swapChain.As(&m_swapChain2)))
        {
            m_frameLatencyWaitable = m_swapChain2->GetFrameLatencyWaitableObject();

            // V-sync starts out disabled, allow the queue to run ahead until it is enabled
            d3dCheck(m_swapChain2->SetMaximumFrameLatency(3));
        }
    }

    // WindowImplWin32 owns fullscreen switching, keep DXGI away from Alt+Enter
    d3dCheck(factory->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER));

    m_sRgb = settings.sRgbCapable;

    if (!createViews(settings.depthBits > 0 || settings.stencilBits > 0))
        return;

    // Report what was actually created
    m_settings.depthBits         = m_depthStencilView ? 24 : 0;
    m_settings.stencilBits       = m_depthStencilView ? 8 : 0;
    m_settings.antiAliasingLevel = samples > 1 ? samples : 0;
    m_settings.majorVersion      = 11;
    m_settings.minorVersion      = 0;
    m_settings.attributeFlags    = ContextSettings::Default;
    m_settings.sRgbCapable       = m_sRgb;
    m_settings.presentation      = m_flipModel ? ContextSettings::Presentation::LowLatency
                                               : ContextSettings::Presentation::Throughput;
}


////////////////////////////////////////////////////////////
D3D11RenderWindowImpl::~D3D11RenderWindowImpl()
{
    if (m_frameLatencyWaitable)
        CloseHandle(m_frameLatencyWaitable);

    m_device.unbindSurface(m_renderTargetView.Get());
}


////////////////////////////////////////////////////////////
void D3D11RenderWindowImpl::present()
{
    if (!m_swapChain)
        return;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    m_device.flushPendingDraws();

    const bool tearing = m_flipModel && (m_syncInterval == 0) &&
                         ((m_swapChainFlags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0);
    const HRESULT presentResult = m_swapChain->Present(m_syncInterval, tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
    d3dCheck(presentResult);

    // Flip-model presentation unbinds the back buffer from the pipeline
    if (m_flipModel && (m_device.getCurrentRenderTargetView() == m_renderTargetView.Get()))
        m_device.bindSurface(m_renderTargetView.Get(), m_depthStencilView.Get());

    // With v-sync the application is paced here, before the next frame samples its input,
    // instead of inside a Present call issued after the frame was already rendered
    if (m_frameLatencyWaitable && (m_syncInterval > 0))
        WaitForSingleObjectEx(m_frameLatencyWaitable, 1000, FALSE);

    monitorPresentationPacing(presentResult);
}


////////////////////////////////////////////////////////////
void D3D11RenderWindowImpl::monitorPresentationPacing(HRESULT presentResult)
{
    const auto now      = std::chrono::steady_clock::now();
    const auto previous = m_lastPresentTime;
    m_lastPresentTime   = now;

    // Only fully v-synced, visible flip-model cycles are meaningful; occluded windows
    // present without pacing by design (Present reports them with a non-zero status)
    if (!m_flipModel || (m_syncInterval == 0) || (presentResult != S_OK) ||
        (previous == std::chrono::steady_clock::time_point{}))
    {
        m_unsyncedPresentStreak = 0;
        return;
    }

    // No display refreshes fast enough to legitimately complete v-synced cycles this
    // quickly, sustaining them means the driver is not pacing the presents
    constexpr std::chrono::microseconds pacingFloor{1000};
    if (now - previous >= pacingFloor)
    {
        m_unsyncedPresentStreak = 0;
        return;
    }

    constexpr unsigned int unsyncedStreakLimit = 30;
    if (++m_unsyncedPresentStreak < unsyncedStreakLimit)
        return;

    err() << "V-synced presentation is not being paced by the display driver, "
             "falling back to the throughput presentation path"
          << std::endl;
    fallBackToBlitPresentation();
}


////////////////////////////////////////////////////////////
void D3D11RenderWindowImpl::fallBackToBlitPresentation()
{
    ID3D11Device* d3dDevice = m_device.getDevice();
    if (!d3dDevice)
        return;

    ComPtr<IDXGIDevice>   dxgiDevice;
    ComPtr<IDXGIAdapter>  adapter;
    ComPtr<IDXGIFactory2> factory;
    if (!d3dCheck(d3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) ||
        !d3dCheck(dxgiDevice->GetAdapter(&adapter)) || !d3dCheck(adapter->GetParent(IID_PPV_ARGS(&factory))))
        return;

    // The old back buffer disappears with its swap chain
    const bool wasCurrent = m_renderTargetView && (m_device.getCurrentRenderTargetView() == m_renderTargetView.Get());
    if (wasCurrent)
        m_device.unbindSurface(m_renderTargetView.Get());

    const bool hadDepthStencil = m_depthStencilView != nullptr;

    if (m_frameLatencyWaitable)
    {
        CloseHandle(m_frameLatencyWaitable);
        m_frameLatencyWaitable = nullptr;
    }
    m_swapChain2.Reset();
    m_renderTargetView.Reset();
    m_depthStencilTexture.Reset();
    m_depthStencilView.Reset();
    m_swapChain.Reset();

    // The compositor paces blit-model presents itself, the broken pacing cannot recur.
    // Flip-model windows are never multisampled, so no sample count carries over.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Format           = m_sRgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount      = 1;
    swapChainDesc.SwapEffect       = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Scaling          = DXGI_SCALING_STRETCH;

    if (!d3dCheck(factory->CreateSwapChainForHwnd(d3dDevice, m_handle, &swapChainDesc, nullptr, nullptr, &m_swapChain)))
    {
        err() << "Failed to create the fallback swap chain" << std::endl;
        return;
    }

    m_flipModel      = false;
    m_swapChainFlags = 0;

    if (!createViews(hadDepthStencil))
        return;

    if (wasCurrent)
        m_device.bindSurface(m_renderTargetView.Get(), m_depthStencilView.Get());

    m_settings.presentation = ContextSettings::Presentation::Throughput;
}


////////////////////////////////////////////////////////////
void D3D11RenderWindowImpl::setVerticalSyncEnabled(bool enabled)
{
    m_syncInterval = enabled ? 1 : 0;

    // Keep at most one frame queued when v-sync paces the application, let the
    // queue run ahead when frames are presented as fast as possible
    if (m_swapChain2)
        d3dCheck(m_swapChain2->SetMaximumFrameLatency(enabled ? 1 : 3));

    // The waitable object is a semaphore: it starts signaled for the initial
    // latency, and every present retired while v-sync was off signals it with
    // no wait consuming the signal. The accumulated surplus would let that
    // many v-synced frames skip their pacing wait and pile up in the
    // presentation queue, so it is drained whenever the pacing starts.
    if (enabled && m_frameLatencyWaitable)
    {
        while (WaitForSingleObjectEx(m_frameLatencyWaitable, 0, FALSE) == WAIT_OBJECT_0)
        {
        }
    }
}


////////////////////////////////////////////////////////////
void D3D11RenderWindowImpl::resize(Vector2u size)
{
    if (!m_swapChain)
        return;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    // The back buffer cannot be resized while its views are alive or bound
    const bool wasCurrent = m_renderTargetView && (m_device.getCurrentRenderTargetView() == m_renderTargetView.Get());
    if (wasCurrent)
        m_device.unbindSurface(m_renderTargetView.Get());

    const bool hadDepthStencil = m_depthStencilView != nullptr;
    m_renderTargetView.Reset();
    m_depthStencilTexture.Reset();
    m_depthStencilView.Reset();

    if (!d3dCheck(m_swapChain->ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, m_swapChainFlags)))
        return;

    if (createViews(hadDepthStencil) && wasCurrent)
        m_device.bindSurface(m_renderTargetView.Get(), m_depthStencilView.Get());
}


////////////////////////////////////////////////////////////
const ContextSettings& D3D11RenderWindowImpl::getSettings() const
{
    return m_settings;
}


////////////////////////////////////////////////////////////
bool D3D11RenderWindowImpl::isSrgb() const
{
    return m_sRgb;
}


////////////////////////////////////////////////////////////
bool D3D11RenderWindowImpl::activate(bool active)
{
    // Deactivation is a no-op, all surfaces share the single device
    if (active)
        m_device.bindSurface(m_renderTargetView.Get(), m_depthStencilView.Get());

    return m_renderTargetView != nullptr;
}


////////////////////////////////////////////////////////////
bool D3D11RenderWindowImpl::createViews(bool depthStencil)
{
    ID3D11Device* device = m_device.getDevice();
    if (!device || !m_swapChain)
        return false;

    ComPtr<ID3D11Texture2D> backBuffer;
    if (!d3dCheck(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        return false;

    // Flip-model buffers cannot use an sRGB format, but they allow an sRGB view over the linear buffer
    D3D11_RENDER_TARGET_VIEW_DESC  viewDesc{};
    D3D11_RENDER_TARGET_VIEW_DESC* viewDescPtr = nullptr;
    if (m_flipModel && m_sRgb)
    {
        viewDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        viewDescPtr            = &viewDesc;
    }

    if (!d3dCheck(device->CreateRenderTargetView(backBuffer.Get(), viewDescPtr, &m_renderTargetView)))
        return false;

    if (depthStencil)
    {
        // The depth-stencil buffer has to match the size and sample count of the back buffer
        D3D11_TEXTURE2D_DESC backBufferDesc{};
        backBuffer->GetDesc(&backBufferDesc);

        D3D11_TEXTURE2D_DESC depthStencilDesc{};
        depthStencilDesc.Width      = backBufferDesc.Width;
        depthStencilDesc.Height     = backBufferDesc.Height;
        depthStencilDesc.MipLevels  = 1;
        depthStencilDesc.ArraySize  = 1;
        depthStencilDesc.Format     = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthStencilDesc.SampleDesc = backBufferDesc.SampleDesc;
        depthStencilDesc.Usage      = D3D11_USAGE_DEFAULT;
        depthStencilDesc.BindFlags  = D3D11_BIND_DEPTH_STENCIL;

        if (!d3dCheck(device->CreateTexture2D(&depthStencilDesc, nullptr, &m_depthStencilTexture)) ||
            !d3dCheck(device->CreateDepthStencilView(m_depthStencilTexture.Get(), nullptr, &m_depthStencilView)))
        {
            m_depthStencilTexture.Reset();
            m_depthStencilView.Reset();
            return false;
        }
    }

    return true;
}

} // namespace sf::priv
