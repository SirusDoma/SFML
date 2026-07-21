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
#include <SFML/Graphics/D3D11/D3D11TextureImpl.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <ostream>
#include <vector>

#include <cassert>
#include <cstring>


namespace sf::priv
{
////////////////////////////////////////////////////////////
D3D11TextureImpl::D3D11TextureImpl(D3D11GraphicsDevice& device) : m_device(device)
{
}


////////////////////////////////////////////////////////////
bool D3D11TextureImpl::create(Vector2u size, bool& sRgb, [[maybe_unused]] bool smooth, [[maybe_unused]] bool repeated, Vector2u& actualSize)
{
    // No padding is required on Direct3D
    actualSize = size;

    // Check the maximum texture size
    const unsigned int maxSize = m_device.getMaximumTextureSize();
    if ((actualSize.x > maxSize) || (actualSize.y > maxSize))
    {
        err() << "Failed to create texture, its internal size is too high "
              << "(" << actualSize.x << "x" << actualSize.y << ", "
              << "maximum is " << maxSize << "x" << maxSize << ")" << std::endl;
        return false;
    }

    ID3D11Device* device = m_device.getDevice();
    if (!device)
    {
        err() << "Failed to create texture, no Direct3D device available" << std::endl;
        return false;
    }

    m_texture.Reset();
    m_shaderResourceView.Reset();

    // The render-target bind flag allows render textures to attach and GPU copies to resolve into the texture.
    // A full mip chain is allocated up front so generateMipmap can work, samplers with a zero MaxLOD
    // keep the unused levels invisible as long as no mipmap was generated.
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = size.x;
    desc.Height           = size.y;
    desc.MipLevels        = 0;
    desc.ArraySize        = 1;
    desc.Format           = sRgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags        = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    if (!d3dCheck(device->CreateTexture2D(&desc, nullptr, &m_texture)))
        return false;

    if (!d3dCheck(device->CreateShaderResourceView(m_texture.Get(), nullptr, &m_shaderResourceView)))
    {
        m_texture.Reset();
        return false;
    }

    return true;
}


////////////////////////////////////////////////////////////
void D3D11TextureImpl::update(const std::uint8_t* pixels, Vector2u size, Vector2u dest, [[maybe_unused]] bool smooth)
{
    ID3D11DeviceContext* context = m_device.getContext();
    if (!m_texture || !context)
        return;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    // Draws collected so far must sample the texture before it changes
    m_device.flushPendingDraws();

    const D3D11_BOX box{dest.x, dest.y, 0, dest.x + size.x, dest.y + size.y, 1};
    context->UpdateSubresource(m_texture.Get(), 0, &box, pixels, size.x * 4, 0);
}


////////////////////////////////////////////////////////////
TextureImpl::UpdateResult D3D11TextureImpl::update(
    const TextureImpl&    source,
    Vector2u              sourceSize,
    [[maybe_unused]] bool sourcePixelsFlipped,
    Vector2u              dest,
    [[maybe_unused]] bool smooth)
{
    // Pixels are never flipped on this backend
    assert(!sourcePixelsFlipped && "Flipped source pixels are not produced by the Direct3D 11 backend");

    const auto& d3dSource = static_cast<const D3D11TextureImpl&>(source);

    ID3D11DeviceContext* context = m_device.getContext();
    if (!m_texture || !d3dSource.m_texture || !context)
        return UpdateResult::Failed;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    m_device.flushPendingDraws();

    const D3D11_BOX box{0, 0, 0, sourceSize.x, sourceSize.y, 1};
    context->CopySubresourceRegion(m_texture.Get(), 0, dest.x, dest.y, 0, d3dSource.m_texture.Get(), 0, &box);

    return UpdateResult::Updated;
}


////////////////////////////////////////////////////////////
void D3D11TextureImpl::update(const Image& image, const IntRect& rectangle, [[maybe_unused]] bool smooth)
{
    ID3D11DeviceContext* context = m_device.getContext();
    if (!m_texture || !context)
        return;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    m_device.flushPendingDraws();

    const auto imageSize = Vector2i(image.getSize());

    // The source row pitch covers the full image, no row-by-row copy is needed for the sub-rectangle
    const std::uint8_t* pixels = image.getPixelsPtr() + 4 * (rectangle.position.x + (imageSize.x * rectangle.position.y));

    const D3D11_BOX box{0, 0, 0, static_cast<UINT>(rectangle.size.x), static_cast<UINT>(rectangle.size.y), 1};
    context->UpdateSubresource(m_texture.Get(), 0, &box, pixels, static_cast<UINT>(imageSize.x) * 4, 0);
}


////////////////////////////////////////////////////////////
bool D3D11TextureImpl::update(const Window& window, Vector2u dest, [[maybe_unused]] bool smooth, bool& pixelsFlipped)
{
    ID3D11Device*        device  = m_device.getDevice();
    ID3D11DeviceContext* context = m_device.getContext();
    if (!m_texture || !device || !context)
        return false;

    // Only render windows have a Direct3D surface to copy from
    const auto* renderWindow = dynamic_cast<const RenderWindow*>(&window);
    if (!renderWindow)
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "Updating a texture is only supported from render windows on the Direct3D 11 backend" << std::endl;

            warned = true;
        }

        return false;
    }

    // Activate the window so its surface is bound to the device.
    // RenderWindow::setActive is not const like the Window::setActive the OpenGL backend calls.
    if (!const_cast<RenderWindow*>(renderWindow)->setActive(true))
        return false;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    m_device.flushPendingDraws();

    auto* renderTargetView = m_device.getCurrentRenderTargetView();
    if (!renderTargetView)
        return false;

    ComPtr<ID3D11Resource> resource;
    renderTargetView->GetResource(&resource);

    ComPtr<ID3D11Texture2D> backBuffer;
    if (!d3dCheck(resource.As(&backBuffer)))
        return false;

    D3D11_TEXTURE2D_DESC backBufferDesc{};
    backBuffer->GetDesc(&backBufferDesc);

    // Multisampled back buffers have to be resolved before they can be copied from
    ComPtr<ID3D11Texture2D> source = backBuffer;
    if (backBufferDesc.SampleDesc.Count > 1)
    {
        D3D11_TEXTURE2D_DESC resolveDesc = backBufferDesc;
        resolveDesc.SampleDesc.Count     = 1;
        resolveDesc.SampleDesc.Quality   = 0;
        resolveDesc.BindFlags            = D3D11_BIND_RENDER_TARGET;
        resolveDesc.MiscFlags            = 0;

        if (!d3dCheck(device->CreateTexture2D(&resolveDesc, nullptr, source.ReleaseAndGetAddressOf())))
            return false;

        context->ResolveSubresource(source.Get(), 0, backBuffer.Get(), 0, backBufferDesc.Format);
    }

    const Vector2u size(std::min(window.getSize().x, backBufferDesc.Width),
                        std::min(window.getSize().y, backBufferDesc.Height));

    const D3D11_BOX box{0, 0, 0, size.x, size.y, 1};
    context->CopySubresourceRegion(m_texture.Get(), 0, dest.x, dest.y, 0, source.Get(), 0, &box);

    // Direct3D back buffers are stored top-down, matching the texture's pixel order
    pixelsFlipped = false;

    return true;
}


////////////////////////////////////////////////////////////
Image D3D11TextureImpl::copyToImage(Vector2u size, [[maybe_unused]] Vector2u actualSize, [[maybe_unused]] bool pixelsFlipped) const
{
    // Create an array of pixels
    std::vector<std::uint8_t> pixels(size.x * size.y * 4);

    ID3D11Device*        device  = m_device.getDevice();
    ID3D11DeviceContext* context = m_device.getContext();
    if (m_texture && device && context)
    {
        // The texture has to be copied to a staging texture before the CPU can read it
        D3D11_TEXTURE2D_DESC desc{};
        m_texture->GetDesc(&desc);
        desc.Usage          = D3D11_USAGE_STAGING;
        desc.BindFlags      = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags      = 0;

        ComPtr<ID3D11Texture2D> stagingTexture;
        if (d3dCheck(device->CreateTexture2D(&desc, nullptr, &stagingTexture)))
        {
            const D3D11GraphicsDevice::ContextLock lock(m_device);

            // Draws collected so far may render into this texture
            m_device.flushPendingDraws();

            context->CopyResource(stagingTexture.Get(), m_texture.Get());

            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (d3dCheck(context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
            {
                // Copy row by row, the mapped row pitch can be larger than the row size
                const auto*       src     = static_cast<const std::uint8_t*>(mapped.pData);
                std::uint8_t*     dst     = pixels.data();
                const std::size_t rowSize = size.x * 4;

                for (unsigned int i = 0; i < size.y; ++i)
                {
                    std::memcpy(dst, src, rowSize);
                    src += mapped.RowPitch;
                    dst += rowSize;
                }

                context->Unmap(stagingTexture.Get(), 0);
            }
        }
    }

    return {size, pixels.data()};
}


////////////////////////////////////////////////////////////
void D3D11TextureImpl::setSmooth([[maybe_unused]] bool smooth, [[maybe_unused]] bool hasMipmap)
{
    // Sampler state is applied per draw by the render pipeline
}


////////////////////////////////////////////////////////////
void D3D11TextureImpl::setRepeated([[maybe_unused]] bool repeated)
{
    // Sampler state is applied per draw by the render pipeline
}


////////////////////////////////////////////////////////////
bool D3D11TextureImpl::generateMipmap([[maybe_unused]] bool smooth)
{
    ID3D11DeviceContext* context = m_device.getContext();
    if (!m_texture || !m_shaderResourceView || !context)
        return false;

    const D3D11GraphicsDevice::ContextLock lock(m_device);

    m_device.flushPendingDraws();

    context->GenerateMips(m_shaderResourceView.Get());

    return true;
}


////////////////////////////////////////////////////////////
void D3D11TextureImpl::invalidateMipmap([[maybe_unused]] bool smooth)
{
}


////////////////////////////////////////////////////////////
unsigned int D3D11TextureImpl::getNativeHandle() const
{
    // Direct3D resources have no GL-style integer handle
    return 0;
}


////////////////////////////////////////////////////////////
ID3D11Texture2D* D3D11TextureImpl::getTexture() const
{
    return m_texture.Get();
}


////////////////////////////////////////////////////////////
ID3D11ShaderResourceView* D3D11TextureImpl::getShaderResourceView() const
{
    return m_shaderResourceView.Get();
}

} // namespace sf::priv
