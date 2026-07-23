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
#include <SFML/Graphics/Metal/MetalRenderWindowImpl.hpp>

#include <SFML/System/Err.hpp>

#import <AppKit/AppKit.h>

#include <chrono>
#include <ostream>


namespace sf::priv
{
////////////////////////////////////////////////////////////
MetalRenderWindowImpl::MetalRenderWindowImpl(MetalGraphicsDevice&   device,
                                             WindowHandle           handle,
                                             const ContextSettings& settings,
                                             [[maybe_unused]] unsigned int bitsPerPixel) :
    m_device(device),
    m_pacer(std::make_shared<FramePacer>()),
    m_visible(std::make_shared<std::atomic<bool>>(true)),
    m_sRgb(settings.sRgbCapable)
{
    // The handle is either the window or a view inside it
    id     nsHandle = static_cast<id>(handle);
    NSView* view    = nil;
    if ([nsHandle isKindOfClass:[NSWindow class]])
        view = [static_cast<NSWindow*>(nsHandle) contentView];
    else if ([nsHandle isKindOfClass:[NSView class]])
        view = static_cast<NSView*>(nsHandle);

    if (!view || !m_device.getDevice())
    {
        err() << "Failed to create the Metal presentation surface" << std::endl;
        return;
    }

    m_view.reset([view retain]);

    CAMetalLayer* layer = [[CAMetalLayer alloc] init];
    layer.device        = m_device.getDevice();
    layer.pixelFormat   = m_sRgb ? MTLPixelFormatBGRA8Unorm_sRGB : MTLPixelFormatBGRA8Unorm;

    // Map the presentation intent: the balanced default keeps the drawables readable so
    // the window contents can be captured, the explicit intents trade that for the
    // direct-to-display fast path. All intents keep the full drawable pool: pacing
    // v-synced presents any tighter, whether by a smaller pool or by counting
    // presented frames, was measured to miss vertical syncs on high-refresh
    // displays, the compositor paces windowed presents itself.
    const bool fastPath        = settings.presentation != ContextSettings::Presentation::Auto;
    layer.framebufferOnly      = fastPath ? YES : NO;
    layer.maximumDrawableCount = 3;

    // V-sync starts out disabled
    layer.displaySyncEnabled = NO;

    // One texel per SFML unit, matching how the other backends size their buffers
    layer.drawableSize = view.bounds.size;

    // The layer must be set before wantsLayer for the view to become layer-hosting
    [view setLayer:layer];
    [view setWantsLayer:YES];
    m_layer.reset(layer);

    // Track occlusion so hidden windows skip their frames instead of stalling in
    // nextDrawable waiting for drawables the compositor will not recycle. The
    // notification arrives on the main thread, the shared flag decouples it from
    // this surface's lifetime.
    if (NSWindow* nsWindow = [view window])
    {
        const std::shared_ptr<std::atomic<bool>> visible = m_visible;
        m_occlusionObserver.reset([[[NSNotificationCenter defaultCenter]
            addObserverForName:NSWindowDidChangeOcclusionStateNotification
                        object:nsWindow
                         queue:nil
                    usingBlock:^(NSNotification* notification) {
                        visible->store(([static_cast<NSWindow*>(notification.object) occlusionState] &
                                        NSWindowOcclusionStateVisible) != 0,
                                       std::memory_order_relaxed);
                    }] retain]);
    }

    // Pick the depth-stencil format: SFML itself only ever tests stencil, a depth
    // plane is allocated when the user asked for one for their own Metal rendering
    if (settings.depthBits > 0)
        m_depthStencilFormat = static_cast<std::uint32_t>(MTLPixelFormatDepth32Float_Stencil8);
    else if (settings.stencilBits > 0)
        m_depthStencilFormat = static_cast<std::uint32_t>(MTLPixelFormatStencil8);

    m_sampleCount = m_device.clampAntiAliasingLevel(std::max(settings.antiAliasingLevel, 1u));

    // Fill in the settings actually obtained
    m_settings.depthBits         = (settings.depthBits > 0) ? 32 : 0;
    m_settings.stencilBits       = (m_depthStencilFormat != 0) ? 8 : 0;
    m_settings.antiAliasingLevel = (m_sampleCount > 1) ? m_sampleCount : 0;
    m_settings.majorVersion      = 0;
    m_settings.minorVersion      = 0;
    m_settings.attributeFlags    = ContextSettings::Default;
    m_settings.sRgbCapable       = m_sRgb;
    m_settings.presentation      = (settings.presentation == ContextSettings::Presentation::LowLatency)
                                       ? ContextSettings::Presentation::LowLatency
                                       : ContextSettings::Presentation::Throughput;
}


////////////////////////////////////////////////////////////
MetalRenderWindowImpl::~MetalRenderWindowImpl()
{
    if (m_occlusionObserver)
        [[NSNotificationCenter defaultCenter] removeObserver:static_cast<id>(m_occlusionObserver.get())];

    m_device.unbindSurface(this);
}


////////////////////////////////////////////////////////////
void MetalRenderWindowImpl::present()
{
    bool presented = false;

    {
        const MetalGraphicsDevice::ContextLock lock(m_device);

        // Finish the frame: submit pending draws, materialize pending clears and
        // resolve multisampled content into the drawable
        if (m_device.getCurrentSurface() == this)
        {
            m_device.prepareForPresent(m_multisampleTexture.get() != nullptr);
        }
        else
        {
            m_device.flushPendingDraws();
            m_device.endEncoding(false);

            // Another surface is current, the multisampled content still has to
            // reach the drawable before it is presented
            if (m_multisampleTexture && m_drawable)
                m_device.resolvePass(m_multisampleTexture.get(), [m_drawable.get() texture]);
        }

        if (m_drawable)
        {
            if (id<MTLCommandBuffer> commandBuffer = m_device.currentCommandBuffer())
            {
                const std::shared_ptr<FramePacer> pacer = m_pacer;

                [commandBuffer presentDrawable:m_drawable.get()];

                {
                    const std::lock_guard pacerLock(m_pacer->mutex);
                    ++m_pacer->pending;
                }

                [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
                    {
                        const std::lock_guard pacerLock(pacer->mutex);
                        --pacer->pending;
                    }
                    pacer->condition.notify_all();
                }];

                m_device.commitCommandBuffer(false);
                presented = true;
            }

            m_drawable.reset();
        }
    }

    // Pace the application to the presentation queue: waiting here, before the next
    // frame samples its input, bounds the frames in flight and minimizes the delay
    // between rendering a frame and it reaching the screen
    if (presented)
    {
        std::unique_lock pacerLock(m_pacer->mutex);
        m_pacer->condition.wait_for(pacerLock, std::chrono::seconds(1), [this] { return m_pacer->pending < 2; });
    }
}


////////////////////////////////////////////////////////////
void MetalRenderWindowImpl::setVerticalSyncEnabled(bool enabled)
{
    if (m_layer)
        [m_layer.get() setDisplaySyncEnabled:enabled];
}


////////////////////////////////////////////////////////////
void MetalRenderWindowImpl::resize(Vector2u size)
{
    const MetalGraphicsDevice::ContextLock lock(m_device);

    if (m_layer)
        [m_layer.get() setDrawableSize:CGSizeMake(size.x, size.y)];

    // The ancillary textures are re-created to match on the next render pass
    m_multisampleTexture.reset();
    m_depthStencilTexture.reset();
}


////////////////////////////////////////////////////////////
const ContextSettings& MetalRenderWindowImpl::getSettings() const
{
    return m_settings;
}


////////////////////////////////////////////////////////////
bool MetalRenderWindowImpl::isSrgb() const
{
    return m_sRgb;
}


////////////////////////////////////////////////////////////
bool MetalRenderWindowImpl::activate(bool active)
{
    // Deactivation is a no-op, all surfaces share the single device
    if (active)
        m_device.bindSurface(this);

    return m_layer.get() != nullptr;
}


////////////////////////////////////////////////////////////
bool MetalRenderWindowImpl::prepareAttachments(MetalSurfaceAttachments& attachments)
{
    if (!m_layer)
        return false;

    // Hidden windows drop their frames, nextDrawable would stall waiting for
    // drawables the compositor will not recycle. The notification lags becoming
    // visible again by a few run loop turns, the state itself is fresh but only
    // safe to read on the main thread.
    if (!m_visible->load(std::memory_order_relaxed))
    {
        NSWindow* nsWindow = [NSThread isMainThread] ? [static_cast<NSView*>(m_view.get()) window] : nil;
        if (!nsWindow || !([nsWindow occlusionState] & NSWindowOcclusionStateVisible))
            return false;

        m_visible->store(true, std::memory_order_relaxed);
    }

    @autoreleasepool
    {
        if (!m_drawable)
            m_drawable.reset([[m_layer.get() nextDrawable] retain]);
    }

    if (!m_drawable)
        return false;

    id<MTLTexture> drawableTexture = [m_drawable.get() texture];
    const Vector2u size(static_cast<unsigned int>([drawableTexture width]),
                        static_cast<unsigned int>([drawableTexture height]));

    // Re-create the ancillary textures when the drawable size changed
    if (((m_sampleCount > 1) && !m_multisampleTexture) || ((m_depthStencilFormat != 0) && !m_depthStencilTexture) ||
        (m_depthStencilTexture &&
         ((Vector2u(static_cast<unsigned int>([m_depthStencilTexture.get() width]),
                    static_cast<unsigned int>([m_depthStencilTexture.get() height])) != size))) ||
        (m_multisampleTexture &&
         ((Vector2u(static_cast<unsigned int>([m_multisampleTexture.get() width]),
                    static_cast<unsigned int>([m_multisampleTexture.get() height])) != size))))
        createAncillaryTextures(size);

    attachments.color              = m_multisampleTexture ? m_multisampleTexture.get() : drawableTexture;
    attachments.resolve            = m_multisampleTexture ? drawableTexture : nullptr;
    attachments.depthStencil       = m_depthStencilTexture.get();
    attachments.colorFormat        = static_cast<std::uint32_t>([drawableTexture pixelFormat]);
    attachments.depthStencilFormat = m_depthStencilTexture ? m_depthStencilFormat : 0;
    attachments.sampleCount        = m_multisampleTexture ? m_sampleCount : 1;
    attachments.size               = size;

    return true;
}


////////////////////////////////////////////////////////////
MetalTexturePtr MetalRenderWindowImpl::acquireReadableColorTexture()
{
    const MetalGraphicsDevice::ContextLock lock(m_device);

    if (m_layer && [m_layer.get() framebufferOnly])
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "The window contents cannot be read, the presentation intent made the drawables write-only"
                  << std::endl;

            warned = true;
        }

        return nullptr;
    }

    // Submit the pending content of the frame, resolving multisampled draws into the drawable
    if (m_device.getCurrentSurface() == this)
    {
        m_device.prepareForPresent(m_multisampleTexture.get() != nullptr);
    }
    else
    {
        m_device.flushPendingDraws();
        m_device.endEncoding(false);

        // Another surface is current, the multisampled content still has to
        // reach the drawable before it can be read
        if (m_multisampleTexture && m_drawable)
            m_device.resolvePass(m_multisampleTexture.get(), [m_drawable.get() texture]);
    }

    // A frame that never rendered has no drawable whose contents could be read
    if (!m_drawable)
        return nullptr;

    return [m_drawable.get() texture];
}


////////////////////////////////////////////////////////////
MetalLayerPtr MetalRenderWindowImpl::getLayer() const
{
    return m_layer.get();
}


////////////////////////////////////////////////////////////
void MetalRenderWindowImpl::createAncillaryTextures(Vector2u size)
{
    m_multisampleTexture.reset();
    m_depthStencilTexture.reset();

    id<MTLDevice> device = m_device.getDevice();
    if (!device || (size.x == 0) || (size.y == 0))
        return;

    @autoreleasepool
    {
        if (m_sampleCount > 1)
        {
            MTLTextureDescriptor* descriptor =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:[m_layer.get() pixelFormat]
                                                                   width:size.x
                                                                  height:size.y
                                                               mipmapped:NO];
            descriptor.textureType = MTLTextureType2DMultisample;
            descriptor.sampleCount = m_sampleCount;
            descriptor.storageMode = MTLStorageModePrivate;
            descriptor.usage       = MTLTextureUsageRenderTarget;

            m_multisampleTexture.reset([device newTextureWithDescriptor:descriptor]);
            if (!m_multisampleTexture)
                m_sampleCount = 1;
        }

        if (m_depthStencilFormat != 0)
        {
            MTLTextureDescriptor* descriptor =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:static_cast<MTLPixelFormat>(m_depthStencilFormat)
                                                                   width:size.x
                                                                  height:size.y
                                                               mipmapped:NO];
            if (m_multisampleTexture)
            {
                descriptor.textureType = MTLTextureType2DMultisample;
                descriptor.sampleCount = m_sampleCount;
            }
            descriptor.storageMode = MTLStorageModePrivate;
            descriptor.usage       = MTLTextureUsageRenderTarget;

            m_depthStencilTexture.reset([device newTextureWithDescriptor:descriptor]);
        }
    }
}

} // namespace sf::priv
