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

#include <algorithm>


namespace sf::priv
{
////////////////////////////////////////////////////////////
D3D11RenderWindowImpl::D3D11RenderWindowImpl(D3D11GraphicsDevice&          device,
                                             WindowHandle                  handle,
                                             const ContextSettings&        settings,
                                             [[maybe_unused]] unsigned int bitsPerPixel) :
    m_device(device)
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

    const DXGI_FORMAT  format  = settings.sRgbCapable ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    const unsigned int samples = m_device.clampAntiAliasingLevel(std::max(settings.antiAliasingLevel, 1u), format);

    // The blit presentation model is used because it supports multisampled and sRGB back buffers directly.
    // Width and height are left zero so the buffers are sized from the window.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Format           = format;
    swapChainDesc.SampleDesc.Count = samples;
    swapChainDesc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount      = 1;
    swapChainDesc.SwapEffect       = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Scaling          = DXGI_SCALING_STRETCH;

    if (!d3dCheck(factory->CreateSwapChainForHwnd(d3dDevice, handle, &swapChainDesc, nullptr, nullptr, &m_swapChain)))
        return;

    // WindowImplWin32 owns fullscreen switching, keep DXGI away from Alt+Enter
    d3dCheck(factory->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER));

    if (!createViews(settings.depthBits > 0 || settings.stencilBits > 0))
        return;

    m_sRgb = (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    // Report what was actually created
    m_settings.depthBits         = m_depthStencilView ? 24 : 0;
    m_settings.stencilBits       = m_depthStencilView ? 8 : 0;
    m_settings.antiAliasingLevel = samples > 1 ? samples : 0;
    m_settings.majorVersion      = 11;
    m_settings.minorVersion      = 0;
    m_settings.attributeFlags    = ContextSettings::Default;
    m_settings.sRgbCapable       = m_sRgb;
}


////////////////////////////////////////////////////////////
D3D11RenderWindowImpl::~D3D11RenderWindowImpl()
{
    m_device.unbindSurface(m_renderTargetView.Get());
}


////////////////////////////////////////////////////////////
void D3D11RenderWindowImpl::present()
{
    if (!m_swapChain)
        return;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    d3dCheck(m_swapChain->Present(m_syncInterval, 0));
}


////////////////////////////////////////////////////////////
void D3D11RenderWindowImpl::setVerticalSyncEnabled(bool enabled)
{
    m_syncInterval = enabled ? 1 : 0;
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

    if (!d3dCheck(m_swapChain->ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, 0)))
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
    if (!d3dCheck(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) ||
        !d3dCheck(device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView)))
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
