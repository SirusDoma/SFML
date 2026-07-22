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
#include <SFML/Graphics/OpenGL/GlShaderImpl.hpp>
#include <SFML/Graphics/OpenGL/GlTextureImpl.hpp>
#include <SFML/Graphics/OpenGL/GlVertexBufferImpl.hpp>
#include <SFML/Graphics/OpenGLInterop.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTargetImpl.hpp>
#include <SFML/Graphics/Renderer.hpp>


namespace sf::OpenGL
{
////////////////////////////////////////////////////////////
void pushStates(RenderTarget& target)
{
    if (target.m_impl && (target.m_impl->isActive(target.m_id) || target.setActive(true)))
        target.m_impl->pushStates(target);

    resetStates(target);
}


////////////////////////////////////////////////////////////
void popStates(RenderTarget& target)
{
    if (target.m_impl && (target.m_impl->isActive(target.m_id) || target.setActive(true)))
        target.m_impl->popStates(target);
}


////////////////////////////////////////////////////////////
void resetStates(RenderTarget& target)
{
    if (target.m_impl)
        target.m_impl->resetStates(target, target.m_id);
}


////////////////////////////////////////////////////////////
void bindTexture(const Texture* texture, CoordinateType coordinateType)
{
    if (getRenderer() != Renderer::OpenGL)
        return;

    priv::GlTextureImpl::bind(texture, coordinateType);
}


////////////////////////////////////////////////////////////
void bindShader(const Shader* shader)
{
    if (getRenderer() != Renderer::OpenGL)
        return;

    priv::GlShaderImpl::bind(shader);
}


////////////////////////////////////////////////////////////
void bindVertexBuffer(const VertexBuffer* vertexBuffer)
{
    if (getRenderer() != Renderer::OpenGL)
        return;

    priv::GlVertexBufferImpl::bind(vertexBuffer);
}

} // namespace sf::OpenGL
