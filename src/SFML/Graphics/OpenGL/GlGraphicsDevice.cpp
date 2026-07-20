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
#include <SFML/Graphics/OpenGL/GlGraphicsDevice.hpp>
#include <SFML/Graphics/OpenGL/GlRenderTargetImpl.hpp>
#include <SFML/Graphics/OpenGL/GlShaderImpl.hpp>
#include <SFML/Graphics/OpenGL/GlTextureImpl.hpp>
#include <SFML/Graphics/OpenGL/GlVertexBufferImpl.hpp>
#include <SFML/Graphics/OpenGL/RenderTextureImplDefault.hpp>
#include <SFML/Graphics/OpenGL/RenderTextureImplFBO.hpp>
#include <SFML/Graphics/RenderWindowImpl.hpp>

#include <memory>


namespace sf::priv
{
////////////////////////////////////////////////////////////
std::unique_ptr<RenderTargetImpl> GlGraphicsDevice::createRenderTargetImpl()
{
    return std::make_unique<GlRenderTargetImpl>();
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderTextureImpl> GlGraphicsDevice::createRenderTextureImpl()
{
    // Use frame-buffer object (FBO) if available, fall back to the default implementation otherwise
    if (RenderTextureImplFBO::isAvailable())
        return std::make_unique<RenderTextureImplFBO>();

    return std::make_unique<RenderTextureImplDefault>();
}


////////////////////////////////////////////////////////////
std::unique_ptr<RenderWindowImpl> GlGraphicsDevice::createRenderWindowImpl(WindowHandle /* handle */,
                                                                           const ContextSettings& /* settings */,
                                                                           unsigned int /* bitsPerPixel */)
{
    // The OpenGL backend presents through the context owned by sf::Window
    return nullptr;
}


////////////////////////////////////////////////////////////
std::unique_ptr<ShaderImpl> GlGraphicsDevice::createShaderImpl()
{
    return std::make_unique<GlShaderImpl>();
}


////////////////////////////////////////////////////////////
std::unique_ptr<TextureImpl> GlGraphicsDevice::createTextureImpl()
{
    return std::make_unique<GlTextureImpl>();
}


////////////////////////////////////////////////////////////
std::unique_ptr<VertexBufferImpl> GlGraphicsDevice::createVertexBufferImpl()
{
    return std::make_unique<GlVertexBufferImpl>();
}


////////////////////////////////////////////////////////////
unsigned int GlGraphicsDevice::getMaximumAntiAliasingLevel()
{
    if (RenderTextureImplFBO::isAvailable())
        return RenderTextureImplFBO::getMaximumAntiAliasingLevel();

    return RenderTextureImplDefault::getMaximumAntiAliasingLevel();
}


////////////////////////////////////////////////////////////
GraphicsBackend GlGraphicsDevice::getBackend() const
{
    return GraphicsBackend::OpenGL;
}


////////////////////////////////////////////////////////////
ShadingLanguage GlGraphicsDevice::getShadingLanguage() const
{
    return ShadingLanguage::Glsl;
}

} // namespace sf::priv
