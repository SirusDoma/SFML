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
#include <SFML/Graphics/D3D11/D3D11RenderTextureImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11TextureImpl.hpp>

#include <SFML/Window/ContextSettings.hpp>

#include <algorithm>


namespace sf::priv
{
////////////////////////////////////////////////////////////
D3D11RenderTextureImpl::D3D11RenderTextureImpl(D3D11GraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
D3D11RenderTextureImpl::~D3D11RenderTextureImpl()
{
    m_device.unbindSurface(m_renderTargetView.Get());
}


////////////////////////////////////////////////////////////
bool D3D11RenderTextureImpl::create(Vector2u size, TextureImpl& texture, const ContextSettings& settings)
{
    auto& d3dTexture = static_cast<D3D11TextureImpl&>(texture);

    ID3D11Device*    device        = m_device.getDevice();
    ID3D11Texture2D* targetTexture = d3dTexture.getTexture();
    if (!device || !targetTexture)
        return false;

    m_renderTargetView.Reset();
    m_multisampleTexture.Reset();
    m_depthStencilTexture.Reset();
    m_depthStencilView.Reset();

    D3D11_TEXTURE2D_DESC targetDesc{};
    targetTexture->GetDesc(&targetDesc);

    const unsigned int samples = m_device.clampAntiAliasingLevel(std::max(settings.antiAliasingLevel, 1u), targetDesc.Format);

    if (samples > 1)
    {
        // Render into a multisampled color buffer, resolved into the target texture on display
        D3D11_TEXTURE2D_DESC multisampleDesc{};
        multisampleDesc.Width            = size.x;
        multisampleDesc.Height           = size.y;
        multisampleDesc.MipLevels        = 1;
        multisampleDesc.ArraySize        = 1;
        multisampleDesc.Format           = targetDesc.Format;
        multisampleDesc.SampleDesc.Count = samples;
        multisampleDesc.Usage            = D3D11_USAGE_DEFAULT;
        multisampleDesc.BindFlags        = D3D11_BIND_RENDER_TARGET;

        if (!d3dCheck(device->CreateTexture2D(&multisampleDesc, nullptr, &m_multisampleTexture)) ||
            !d3dCheck(device->CreateRenderTargetView(m_multisampleTexture.Get(), nullptr, &m_renderTargetView)))
            return false;
    }
    else
    {
        // Render directly into the target texture
        if (!d3dCheck(device->CreateRenderTargetView(targetTexture, nullptr, &m_renderTargetView)))
            return false;
    }

    if (settings.depthBits > 0 || settings.stencilBits > 0)
    {
        D3D11_TEXTURE2D_DESC depthStencilDesc{};
        depthStencilDesc.Width            = size.x;
        depthStencilDesc.Height           = size.y;
        depthStencilDesc.MipLevels        = 1;
        depthStencilDesc.ArraySize        = 1;
        depthStencilDesc.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthStencilDesc.SampleDesc.Count = samples;
        depthStencilDesc.Usage            = D3D11_USAGE_DEFAULT;
        depthStencilDesc.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

        if (!d3dCheck(device->CreateTexture2D(&depthStencilDesc, nullptr, &m_depthStencilTexture)) ||
            !d3dCheck(device->CreateDepthStencilView(m_depthStencilTexture.Get(), nullptr, &m_depthStencilView)))
            return false;
    }

    m_size   = size;
    m_format = targetDesc.Format;
    m_sRgb   = (targetDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    return true;
}


////////////////////////////////////////////////////////////
bool D3D11RenderTextureImpl::activate(bool active)
{
    // Deactivation is a no-op, all surfaces share the single device
    if (!active)
        return true;

    m_device.bindSurface(m_renderTargetView.Get(), m_depthStencilView.Get());

    return m_renderTargetView != nullptr;
}


////////////////////////////////////////////////////////////
bool D3D11RenderTextureImpl::isSrgb() const
{
    return m_sRgb;
}


////////////////////////////////////////////////////////////
void D3D11RenderTextureImpl::updateTexture(TextureImpl& texture)
{
    // Without multisampling the rendering happened directly into the target texture
    if (!m_multisampleTexture)
        return;

    auto& d3dTexture = static_cast<D3D11TextureImpl&>(texture);

    ID3D11DeviceContext* context       = m_device.getContext();
    ID3D11Texture2D*     targetTexture = d3dTexture.getTexture();
    if (!context || !targetTexture)
        return;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    context->ResolveSubresource(targetTexture, 0, m_multisampleTexture.Get(), 0, m_format);
}


////////////////////////////////////////////////////////////
bool D3D11RenderTextureImpl::arePixelsFlipped() const
{
    // Direct3D renders top-down, matching sf::Texture's pixel order
    return false;
}


////////////////////////////////////////////////////////////
bool D3D11RenderTextureImpl::needsFullActivationForDisplay() const
{
    return false;
}


////////////////////////////////////////////////////////////
bool D3D11RenderTextureImpl::isTextureAttachment() const
{
    return true;
}

} // namespace sf::priv
