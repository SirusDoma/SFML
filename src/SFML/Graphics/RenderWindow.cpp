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
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/OpenGL/GLCheck.hpp>
#include <SFML/Graphics/OpenGL/GLExtensions.hpp>
#include <SFML/Graphics/OpenGL/RenderTextureImplFBO.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderWindowImpl.hpp>
#include <SFML/Graphics/Renderer.hpp>

#include <SFML/Window/VideoMode.hpp>

#include <SFML/System/Err.hpp>

#include <ostream>


namespace sf
{
////////////////////////////////////////////////////////////
RenderWindow::RenderWindow() = default;


////////////////////////////////////////////////////////////
RenderWindow::RenderWindow(VideoMode mode, const String& title, std::uint32_t style, State state, const ContextSettings& settings)
{
    // Don't call the base class constructor because it contains virtual function calls
    create(mode, title, style, state, settings);
}


////////////////////////////////////////////////////////////
RenderWindow::RenderWindow(VideoMode mode, const String& title, State state, const ContextSettings& settings)
{
    // Don't call the base class constructor because it contains virtual function calls
    create(mode, title, sf::Style::Default, state, settings);
}


////////////////////////////////////////////////////////////
RenderWindow::RenderWindow(WindowHandle handle, const ContextSettings& settings)
{
    // Don't call the base class constructor because it contains virtual function calls
    create(handle, settings);
}


////////////////////////////////////////////////////////////
RenderWindow::~RenderWindow() = default;


////////////////////////////////////////////////////////////
RenderWindow::RenderWindow(RenderWindow&&) noexcept = default;


////////////////////////////////////////////////////////////
RenderWindow& RenderWindow::operator=(RenderWindow&&) noexcept = default;


////////////////////////////////////////////////////////////
void RenderWindow::create(VideoMode mode, const String& title, std::uint32_t style, State state, const ContextSettings& settings)
{
    // The RenderTarget constructor already locked the backend in
    if (getRenderer() == Renderer::OpenGL)
    {
        Window::create(mode, title, style, state, settings);
        return;
    }

    // Create the window without an OpenGL context, the surface is created in onCreate()
    m_surface.reset();
    m_requestedSettings     = settings;
    m_requestedBitsPerPixel = mode.bitsPerPixel;
    WindowBase::create(mode, title, style, state);
}


////////////////////////////////////////////////////////////
void RenderWindow::create(VideoMode mode, const String& title, State state, const ContextSettings& settings)
{
    create(mode, title, sf::Style::Default, state, settings);
}


////////////////////////////////////////////////////////////
void RenderWindow::create(WindowHandle handle, const ContextSettings& settings)
{
    if (getRenderer() == Renderer::OpenGL)
    {
        Window::create(handle, settings);
        return;
    }

    m_surface.reset();
    m_requestedSettings     = settings;
    m_requestedBitsPerPixel = VideoMode::getDesktopMode().bitsPerPixel;
    WindowBase::create(handle);
}


////////////////////////////////////////////////////////////
void RenderWindow::close()
{
    m_surface.reset();

    Window::close();
}


////////////////////////////////////////////////////////////
void RenderWindow::display()
{
    if (m_surface)
        m_surface->present();

    // Swaps the buffers of the OpenGL context and applies the framerate limit
    Window::display();
}


////////////////////////////////////////////////////////////
void RenderWindow::setVerticalSyncEnabled(bool enabled)
{
    if (m_surface)
        m_surface->setVerticalSyncEnabled(enabled);
    else
        Window::setVerticalSyncEnabled(enabled);
}


////////////////////////////////////////////////////////////
const ContextSettings& RenderWindow::getSettings() const
{
    if (m_surface)
        return m_surface->getSettings();

    return Window::getSettings();
}


////////////////////////////////////////////////////////////
Vector2u RenderWindow::getSize() const
{
    return Window::getSize();
}


////////////////////////////////////////////////////////////
void RenderWindow::setIcon(const Image& icon)
{
    setIcon(icon.getSize(), icon.getPixelsPtr());
}


////////////////////////////////////////////////////////////
bool RenderWindow::isSrgb() const
{
    if (m_surface)
        return m_surface->isSrgb();

    return getSettings().sRgbCapable;
}


////////////////////////////////////////////////////////////
bool RenderWindow::setActive(bool active)
{
    if (m_surface)
    {
        bool result = m_surface->activate(active);

        // Update RenderTarget tracking
        if (result)
            result = RenderTarget::setActive(active);

        return result;
    }

    bool result = Window::setActive(active);

    // Update RenderTarget tracking
    if (result)
        result = RenderTarget::setActive(active);

    // If FBOs are available, make sure none are bound when we
    // try to draw to the default framebuffer of the RenderWindow
    if (active && result && priv::RenderTextureImplFBO::isAvailable())
    {
        glCheck(GLEXT_glBindFramebuffer(GLEXT_GL_FRAMEBUFFER, m_defaultFrameBuffer));

        return true;
    }

    return result;
}


////////////////////////////////////////////////////////////
void RenderWindow::onCreate()
{
    if (getRenderer() == Renderer::OpenGL)
    {
        if (priv::RenderTextureImplFBO::isAvailable())
        {
            // Retrieve the framebuffer ID we have to bind when targeting the window for rendering
            // We assume that this window's context is still active at this point
            glCheck(glGetIntegerv(GLEXT_GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&m_defaultFrameBuffer)));
        }
    }
    else
    {
        // Create the presentation surface on the native window
        m_surface = priv::ensureGraphicsDevice()
                        ->createRenderWindowImpl(getNativeHandle(), m_requestedSettings, m_requestedBitsPerPixel);
    }

    // Just initialize the render target part
    RenderTarget::initialize();

    if (m_surface && !setActive(true))
        err() << "Failed to activate render window" << std::endl;
}


////////////////////////////////////////////////////////////
void RenderWindow::onResize()
{
    if (m_surface)
        m_surface->resize(getSize());

    // Update the current view (recompute the viewport, which is stored in relative coordinates)
    setView(getView());
}

} // namespace sf
