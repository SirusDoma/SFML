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
#include <SFML/Graphics/GraphicsDevice.hpp>
#include <SFML/Graphics/OpenGL/GlGraphicsDevice.hpp>
#include <SFML/Graphics/Renderer.hpp>

#ifdef SFML_ENABLE_D3D11
#include <SFML/Graphics/D3D11/D3D11GraphicsDevice.hpp>
#endif

#ifdef SFML_ENABLE_METAL
#include <SFML/Graphics/Metal/MetalGraphicsDevice.hpp>
#endif

#include <SFML/System/Err.hpp>

#include <algorithm>
#include <memory>
#include <mutex>
#include <ostream>

#include <cassert>


namespace
{
// Mutex to protect device creation, lookup and renderer selection
std::mutex& getGraphicsDeviceMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::weak_ptr<sf::priv::GraphicsDevice>& getWeakGraphicsDevice()
{
    static std::weak_ptr<sf::priv::GraphicsDevice> weakDevice;
    return weakDevice;
}

// Renderer used to create the device, changeable until a device exists
sf::Renderer& getPendingRenderer()
{
    static sf::Renderer renderer = sf::Renderer::OpenGL;
    return renderer;
}
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
std::shared_ptr<GraphicsDevice> ensureGraphicsDevice()
{
    const std::lock_guard lock(getGraphicsDeviceMutex());

    auto& weakDevice = getWeakGraphicsDevice();
    auto  device     = weakDevice.lock();

    if (!device)
    {
        switch (getPendingRenderer())
        {
            case Renderer::OpenGL:
                device = std::make_shared<GlGraphicsDevice>();
                break;
            case Renderer::Direct3D11:
#ifdef SFML_ENABLE_D3D11
                device = std::make_shared<D3D11GraphicsDevice>();
#else
                // Unreachable, setRenderer only accepts compiled-in backends
                assert(false && "The Direct3D 11 backend is not compiled in");
#endif
                break;
            case Renderer::Metal:
#ifdef SFML_ENABLE_METAL
                device = std::make_shared<MetalGraphicsDevice>();
#else
                // Unreachable, setRenderer only accepts compiled-in backends
                assert(false && "The Metal backend is not compiled in");
#endif
                break;
        }

        weakDevice = device;
    }

    return device;
}


////////////////////////////////////////////////////////////
GraphicsDevice* getGraphicsDevice()
{
    const std::lock_guard lock(getGraphicsDeviceMutex());

    return getWeakGraphicsDevice().lock().get();
}

} // namespace sf::priv


namespace sf
{
////////////////////////////////////////////////////////////
void setRenderer(Renderer renderer)
{
    if (!isRendererAvailable(renderer))
    {
        err() << "Failed to select renderer, it is not available on this system" << std::endl;
        return;
    }

    const std::lock_guard lock(getGraphicsDeviceMutex());

    if (const auto device = getWeakGraphicsDevice().lock())
    {
        if (device->getRenderer() != renderer)
            err() << "Failed to select renderer, another renderer is already in use" << std::endl;
        return;
    }

    getPendingRenderer() = renderer;
}


////////////////////////////////////////////////////////////
Renderer getRenderer()
{
    const std::lock_guard lock(getGraphicsDeviceMutex());

    if (const auto device = getWeakGraphicsDevice().lock())
        return device->getRenderer();

    return getPendingRenderer();
}


////////////////////////////////////////////////////////////
std::vector<Renderer> getAvailableRenderers()
{
    return {
        Renderer::OpenGL,
#ifdef SFML_ENABLE_D3D11
        Renderer::Direct3D11,
#endif
#ifdef SFML_ENABLE_METAL
        Renderer::Metal,
#endif
    };
}


////////////////////////////////////////////////////////////
bool isRendererAvailable(Renderer renderer)
{
    const auto available = getAvailableRenderers();

    return std::find(available.begin(), available.end(), renderer) != available.end();
}


////////////////////////////////////////////////////////////
ShadingLanguage getShadingLanguage()
{
    const std::lock_guard lock(getGraphicsDeviceMutex());

    if (const auto device = getWeakGraphicsDevice().lock())
        return device->getShadingLanguage();

    switch (getPendingRenderer())
    {
        case Renderer::Direct3D11:
            return ShadingLanguage::Hlsl;
        case Renderer::Metal:
            return ShadingLanguage::Msl;
        default:
            return ShadingLanguage::Glsl;
    }
}

} // namespace sf
