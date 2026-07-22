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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#ifdef __OBJC__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#endif


namespace sf::priv
{
////////////////////////////////////////////////////////////
// Handles to Metal objects. All Objective-C handles are plain
// pointers under the hood, outside of Objective-C++ they decay
// to opaque pointers so the backend headers remain includable
// from C++ translation units.
////////////////////////////////////////////////////////////
#ifdef __OBJC__
using MetalDevicePtr            = id<MTLDevice>;
using MetalCommandQueuePtr      = id<MTLCommandQueue>;
using MetalCommandBufferPtr     = id<MTLCommandBuffer>;
using MetalRenderEncoderPtr     = id<MTLRenderCommandEncoder>;
using MetalTexturePtr           = id<MTLTexture>;
using MetalBufferPtr            = id<MTLBuffer>;
using MetalLibraryPtr           = id<MTLLibrary>;
using MetalFunctionPtr          = id<MTLFunction>;
using MetalRenderPipelinePtr    = id<MTLRenderPipelineState>;
using MetalDepthStencilStatePtr = id<MTLDepthStencilState>;
using MetalSamplerStatePtr      = id<MTLSamplerState>;
using MetalDrawablePtr          = id<CAMetalDrawable>;
using MetalLayerPtr             = CAMetalLayer*;
using MetalVertexDescriptorPtr  = MTLVertexDescriptor*;
#else
using MetalDevicePtr            = void*;
using MetalCommandQueuePtr      = void*;
using MetalCommandBufferPtr     = void*;
using MetalRenderEncoderPtr     = void*;
using MetalTexturePtr           = void*;
using MetalBufferPtr            = void*;
using MetalLibraryPtr           = void*;
using MetalFunctionPtr          = void*;
using MetalRenderPipelinePtr    = void*;
using MetalDepthStencilStatePtr = void*;
using MetalSamplerStatePtr      = void*;
using MetalDrawablePtr          = void*;
using MetalLayerPtr             = void*;
using MetalVertexDescriptorPtr  = void*;
#endif

////////////////////////////////////////////////////////////
/// \brief Retain an Objective-C object
///
/// \param object Object to retain, can be a null pointer
///
/// \return The retained object
///
////////////////////////////////////////////////////////////
[[nodiscard]] void* metalRetain(void* object);

////////////////////////////////////////////////////////////
/// \brief Release an Objective-C object
///
/// \param object Object to release, can be a null pointer
///
////////////////////////////////////////////////////////////
void metalRelease(void* object);

////////////////////////////////////////////////////////////
/// \brief Smart pointer owning one reference to an Objective-C object
///
/// The backend is compiled without ARC like the rest of the
/// Cocoa code, this is the moral equivalent of the ComPtr the
/// Direct3D 11 backend uses.
///
////////////////////////////////////////////////////////////
template <typename T>
class NSPtr
{
public:
    NSPtr() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Adopt ownership of an already-retained object
    ///
    ////////////////////////////////////////////////////////////
    explicit NSPtr(T object) : m_object(object)
    {
    }

    ~NSPtr()
    {
        metalRelease(m_object);
    }

    NSPtr(const NSPtr& other) : m_object(static_cast<T>(metalRetain(other.m_object)))
    {
    }

    NSPtr& operator=(const NSPtr& other)
    {
        if (this != &other)
            reset(static_cast<T>(metalRetain(other.m_object)));
        return *this;
    }

    NSPtr(NSPtr&& other) noexcept : m_object(other.m_object)
    {
        other.m_object = nullptr;
    }

    NSPtr& operator=(NSPtr&& other) noexcept
    {
        if (this != &other)
        {
            reset(other.m_object);
            other.m_object = nullptr;
        }
        return *this;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Replace the owned object, adopting ownership of the new one
    ///
    ////////////////////////////////////////////////////////////
    void reset(T object = nullptr)
    {
        metalRelease(m_object);
        m_object = object;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Get the owned object without affecting ownership
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] T get() const
    {
        return m_object;
    }

    explicit operator bool() const
    {
        return m_object != nullptr;
    }

private:
    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    T m_object{}; //!< The owned object, holds one reference
};

} // namespace sf::priv
