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
#include <SFML/Graphics/GraphicsBackend.hpp>
#include <SFML/Graphics/GraphicsDevice.hpp>
#include <SFML/Graphics/OpenGL/GlGraphicsDevice.hpp>

#include <SFML/System/Err.hpp>

#include <memory>
#include <mutex>
#include <ostream>

#include <cassert>


namespace
{
// A nested named namespace is used here to allow unity builds of SFML.
namespace GraphicsDeviceImpl
{
// Mutex to protect device creation, lookup and backend selection
std::mutex& getMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::weak_ptr<sf::priv::GraphicsDevice>& getWeakDevice()
{
    static std::weak_ptr<sf::priv::GraphicsDevice> weakDevice;
    return weakDevice;
}

// Backend used to create the device, changeable until a device exists
sf::GraphicsBackend& getPendingBackend()
{
    static sf::GraphicsBackend backend = sf::GraphicsBackend::OpenGL;
    return backend;
}
} // namespace GraphicsDeviceImpl
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
std::shared_ptr<GraphicsDevice> ensureGraphicsDevice()
{
    const std::lock_guard lock(GraphicsDeviceImpl::getMutex());

    auto& weakDevice = GraphicsDeviceImpl::getWeakDevice();
    auto  device     = weakDevice.lock();

    if (!device)
    {
        switch (GraphicsDeviceImpl::getPendingBackend())
        {
            case GraphicsBackend::OpenGL:
                device = std::make_shared<GlGraphicsDevice>();
                break;
            case GraphicsBackend::Direct3D11:
                // Unreachable, setGraphicsBackend only accepts compiled-in backends
                assert(false && "Direct3D 11 backend is not implemented");
                break;
        }

        weakDevice = device;
    }

    return device;
}


////////////////////////////////////////////////////////////
GraphicsDevice* getGraphicsDevice()
{
    const std::lock_guard lock(GraphicsDeviceImpl::getMutex());

    return GraphicsDeviceImpl::getWeakDevice().lock().get();
}

} // namespace sf::priv


namespace sf
{
////////////////////////////////////////////////////////////
bool setGraphicsBackend(GraphicsBackend backend)
{
    if (!isGraphicsBackendAvailable(backend))
    {
        err() << "Failed to select graphics backend, it is not available on this system" << std::endl;
        return false;
    }

    const std::lock_guard lock(GraphicsDeviceImpl::getMutex());

    if (const auto device = GraphicsDeviceImpl::getWeakDevice().lock())
    {
        if (device->getBackend() == backend)
            return true;

        err() << "Failed to select graphics backend, another backend is already in use" << std::endl;
        return false;
    }

    GraphicsDeviceImpl::getPendingBackend() = backend;

    return true;
}


////////////////////////////////////////////////////////////
GraphicsBackend getGraphicsBackend()
{
    const std::lock_guard lock(GraphicsDeviceImpl::getMutex());

    if (const auto device = GraphicsDeviceImpl::getWeakDevice().lock())
        return device->getBackend();

    return GraphicsDeviceImpl::getPendingBackend();
}


////////////////////////////////////////////////////////////
bool isGraphicsBackendAvailable(GraphicsBackend backend)
{
    switch (backend)
    {
        case GraphicsBackend::OpenGL:
            return true;
        case GraphicsBackend::Direct3D11:
            return false;
    }

    return false;
}


////////////////////////////////////////////////////////////
ShadingLanguage getShadingLanguage()
{
    const std::lock_guard lock(GraphicsDeviceImpl::getMutex());

    if (const auto device = GraphicsDeviceImpl::getWeakDevice().lock())
        return device->getShadingLanguage();

    return GraphicsDeviceImpl::getPendingBackend() == GraphicsBackend::OpenGL ? ShadingLanguage::Glsl : ShadingLanguage::Hlsl;
}

} // namespace sf
