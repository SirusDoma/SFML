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
#include <SFML/Graphics/D3D11Interop.hpp>
#include <SFML/Graphics/GraphicsBackend.hpp>
#include <SFML/Graphics/GraphicsDevice.hpp>
#include <SFML/Graphics/RenderTargetImpl.hpp>

#ifdef SFML_ENABLE_D3D11
#include <SFML/Graphics/D3D11/D3D11GraphicsDevice.hpp>
#include <SFML/Graphics/D3D11/D3D11TextureImpl.hpp>
#include <SFML/Graphics/D3D11/D3D11VertexBufferImpl.hpp>
#endif


namespace
{
// Never instantiated, re-exposes the protected resource accessors of the implementation base
struct ImplAccess : sf::priv::RenderTargetImpl
{
    using RenderTargetImpl::getId;
    using RenderTargetImpl::getImpl;
    using RenderTargetImpl::getTextureImpl;
    using RenderTargetImpl::getVertexBufferImpl;
};

#ifdef SFML_ENABLE_D3D11
// The Direct3D device, null unless the Direct3D 11 backend is the one alive
sf::priv::D3D11GraphicsDevice* getD3d11Device()
{
    auto* device = sf::priv::getGraphicsDevice();
    if (!device || (device->getBackend() != sf::GraphicsBackend::Direct3D11))
        return nullptr;

    return static_cast<sf::priv::D3D11GraphicsDevice*>(device);
}
#endif
} // namespace


namespace sf::D3D11
{
////////////////////////////////////////////////////////////
ID3D11Device* getDevice()
{
#ifdef SFML_ENABLE_D3D11
    if (auto* device = getD3d11Device())
        return device->getDevice();
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
ID3D11DeviceContext* getContext()
{
#ifdef SFML_ENABLE_D3D11
    if (auto* device = getD3d11Device())
        return device->getContext();
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
ID3D11RenderTargetView* getRenderTargetView()
{
#ifdef SFML_ENABLE_D3D11
    if (auto* device = getD3d11Device())
        return device->getCurrentRenderTargetView();
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
ID3D11DepthStencilView* getDepthStencilView()
{
#ifdef SFML_ENABLE_D3D11
    if (auto* device = getD3d11Device())
        return device->getCurrentDepthStencilView();
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
ID3D11Texture2D* getTexture([[maybe_unused]] const Texture& texture)
{
#ifdef SFML_ENABLE_D3D11
    if (getD3d11Device())
    {
        if (auto* impl = ImplAccess::getTextureImpl(texture))
            return static_cast<priv::D3D11TextureImpl*>(impl)->getTexture();
    }
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
ID3D11ShaderResourceView* getShaderResourceView([[maybe_unused]] const Texture& texture)
{
#ifdef SFML_ENABLE_D3D11
    if (getD3d11Device())
    {
        if (auto* impl = ImplAccess::getTextureImpl(texture))
            return static_cast<priv::D3D11TextureImpl*>(impl)->getShaderResourceView();
    }
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
ID3D11Buffer* getBuffer([[maybe_unused]] const VertexBuffer& vertexBuffer)
{
#ifdef SFML_ENABLE_D3D11
    if (getD3d11Device())
    {
        if (auto* impl = ImplAccess::getVertexBufferImpl(vertexBuffer))
            return static_cast<priv::D3D11VertexBufferImpl*>(impl)->getBuffer();
    }
#endif

    return nullptr;
}


////////////////////////////////////////////////////////////
void resetStates(RenderTarget& target)
{
    if (auto* impl = ImplAccess::getImpl(target))
        impl->resetStates(target, ImplAccess::getId(target));
}

} // namespace sf::D3D11
