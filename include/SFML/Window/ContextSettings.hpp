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

#pragma once

#include <SFML/Config.hpp>

#include <cstdint>

namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Structure defining the settings of the OpenGL
///        context attached to a window
///
////////////////////////////////////////////////////////////
struct ContextSettings
{
    ////////////////////////////////////////////////////////////
    /// \brief Enumeration of the context attribute flags
    ///
    ////////////////////////////////////////////////////////////
    enum Attribute
    {
        Default = 0,      //!< Non-debug, compatibility context (this and the core attribute are mutually exclusive)
        Core    = 1 << 0, //!< Core attribute
        Debug   = 1 << 2  //!< Debug attribute
    };

    ////////////////////////////////////////////////////////////
    /// \brief Enumeration of the presentation intents
    ///
    ////////////////////////////////////////////////////////////
    enum class Presentation
    {
        Auto,       //!< Let the backend choose its preferred presentation path
        Throughput, //!< Maximize the uncapped frame rate
        LowLatency  //!< Minimize the delay until frames reach the screen
    };


    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    unsigned int  depthBits{};                        //!< Bits of the depth buffer
    unsigned int  stencilBits{};                      //!< Bits of the stencil buffer
    unsigned int  antiAliasingLevel{};                //!< Level of anti-aliasing
    unsigned int  majorVersion{1};                    //!< Major number of the context version to create
    unsigned int  minorVersion{1};                    //!< Minor number of the context version to create
    std::uint32_t attributeFlags{Attribute::Default}; //!< The attribute flags to create the context with
    bool          sRgbCapable{};                      //!< Whether the context framebuffer is sRGB capable
    Presentation  presentation{Presentation::Auto};   //!< How frames should be presented to the screen
};

} // namespace sf


////////////////////////////////////////////////////////////
/// \class sf::ContextSettings
/// \ingroup window
///
/// ContextSettings allows to define several advanced settings
/// of the OpenGL context attached to a window. All these
/// settings with the exception of the compatibility flag
/// and anti-aliasing level have no impact on the regular
/// SFML rendering (graphics module), so you may need to use
/// this structure only if you're using SFML as a windowing
/// system for custom OpenGL rendering.
///
/// The depthBits and stencilBits members define the number
/// of bits per pixel requested for the (respectively) depth
/// and stencil buffers.
///
/// antiAliasingLevel represents the requested number of
/// multisampling levels for anti-aliasing.
///
/// majorVersion and minorVersion define the version of the
/// OpenGL context that you want. Only versions greater or
/// equal to 3.0 are relevant; versions lesser than 3.0 are
/// all handled the same way (i.e. you can use any version
/// < 3.0 if you don't want an OpenGL 3 context).
///
/// When requesting a context with a version greater or equal
/// to 3.2, you have the option of specifying whether the
/// context should follow the core or compatibility profile
/// of all newer (>= 3.2) OpenGL specifications. For versions
/// 3.0 and 3.1 there is only the core profile. By default
/// a compatibility context is created. You only need to specify
/// the core flag if you want a core profile context to use with
/// your own OpenGL rendering.
/// <b>Warning: The graphics module will not function if you
/// request a core profile context. Make sure the attributes are
/// set to Default if you want to use the graphics module.</b>
///
/// Setting the debug attribute flag will request a context with
/// additional debugging features enabled. Depending on the
/// system, this might be required for advanced OpenGL debugging.
/// OpenGL debugging is disabled by default.
///
/// <b>Special Note for macOS:</b>
/// Apple only supports choosing between either a legacy context
/// (OpenGL 2.1) or a core context (OpenGL version depends on the
/// operating system version but is at least 3.2). Compatibility
/// contexts are not supported. Further information is available on the
/// <a href="https://developer.apple.com/opengl/capabilities/index.html">
/// OpenGL Capabilities Tables</a> page. macOS also currently does
/// not support debug contexts.
///
/// presentation expresses how frames should reach the screen,
/// independent of the graphics backend in use. `Throughput`
/// favors the highest possible uncapped frame rate, `LowLatency`
/// favors the shortest delay between rendering a frame and it
/// becoming visible, keeping the presentation queue as short as
/// the platform allows and trading the deeper queue that absorbs
/// frame time spikes for less input latency. Backends map the
/// intent to whatever their presentation path offers; the OpenGL
/// backend currently has a single presentation path and ignores
/// this setting. The settings of a created window report the
/// achieved path, which does not distinguish `Auto` from an
/// explicit `Throughput`.
///
/// <b>Special Note for Windows:</b>
/// On the Direct3D 11 renderer `Auto` and `Throughput` present
/// through the classic blit path, which behaves the same on
/// every system, while `LowLatency` opts into the flip model,
/// which hands frames to the display with less delay but is more
/// exposed to driver and display quirks. Multisampled windows
/// always use the blit path, and a window whose v-synced flip
/// presentation is not paced by the driver falls back to the
/// blit path on its own.
///
/// <b>Special Note for macOS:</b>
/// On the Metal renderer the explicit intents additionally make
/// the window's drawables write-only for the presentation fast
/// path, so `sf::Texture::update(const Window&)` fails on such
/// windows; request `Auto` when the window contents have to be
/// captured.
///
/// <b>Special Note for Vulkan:</b>
/// On the Vulkan renderer v-synced windows always present through
/// the FIFO mode, the only paced mode every driver provides. With
/// v-sync off, `Auto` and `Throughput` prefer the immediate mode
/// (uncapped, may tear) and `LowLatency` prefers the mailbox mode,
/// which replaces the queued frame with the newest one instead of
/// lining up behind it; either falls back to the other and finally
/// to FIFO when the driver does not offer it. `LowLatency`
/// additionally allows only a single frame in flight and requests
/// the shortest swapchain the surface supports, trading the deeper
/// queue that absorbs frame time spikes for less input latency,
/// and when the driver provides `VK_KHR_present_wait` its v-synced
/// frames additionally pace to the moment the previous frame
/// reached the screen, collapsing the presentation queue drivers
/// keep between the present call and the display. Note that on
/// current Windows drivers windowed swapchains commonly present
/// through the desktop compositor while uncapped, adding about one
/// compositor cycle of display latency regardless of the intent;
/// v-synced windows present through hardware flip. Toggling v-sync
/// re-creates the swapchain, which makes `setVerticalSyncEnabled`
/// a heavier call than on the other renderers.
///
/// Please note that these values are only a hint.
/// No failure will be reported if one or more of these values
/// are not supported by the system; instead, SFML will try to
/// find the closest valid match. You can then retrieve the
/// settings that the window actually used to create its context,
/// with `Window::getSettings()`.
///
////////////////////////////////////////////////////////////
