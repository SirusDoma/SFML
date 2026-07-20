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
#include <SFML/Graphics/OpenGL/GLCheck.hpp>
#include <SFML/Graphics/OpenGL/GLExtensions.hpp>
#include <SFML/Graphics/OpenGL/GlRenderTargetImpl.hpp>
#include <SFML/Graphics/OpenGL/GlShaderImpl.hpp>
#include <SFML/Graphics/OpenGL/GlTextureImpl.hpp>
#include <SFML/Graphics/OpenGL/GlVertexBufferImpl.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>

#include <SFML/Window/Context.hpp>

#include <SFML/System/EnumArray.hpp>
#include <SFML/System/Err.hpp>

#include <mutex>
#include <ostream>
#include <unordered_map>

#include <cassert>
#include <cstddef>
#include <cstdint>


namespace
{
// Mutex to protect our context-RenderTarget-map
std::recursive_mutex& getContextRenderTargetMapMutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}

// Map to help us detect whether a different RenderTarget
// has been activated within a single context
using ContextRenderTargetMap = std::unordered_map<std::uint64_t, std::uint64_t>;
ContextRenderTargetMap& getContextRenderTargetMap()
{
    static ContextRenderTargetMap contextRenderTargetMap;
    return contextRenderTargetMap;
}

// Check if a RenderTarget with the given ID is active in the current context
bool isActiveInCurrentContext(std::uint64_t id)
{
    const auto it = getContextRenderTargetMap().find(sf::Context::getActiveContextId());
    return (it != getContextRenderTargetMap().end()) && (it->second == id);
}

// Convert an sf::BlendMode::Factor constant to the corresponding OpenGL constant.
std::uint32_t factorToGlConstant(sf::BlendMode::Factor blendFactor)
{
    // clang-format off
    switch (blendFactor)
    {
        case sf::BlendMode::Factor::Zero:             return GL_ZERO;
        case sf::BlendMode::Factor::One:              return GL_ONE;
        case sf::BlendMode::Factor::SrcColor:         return GL_SRC_COLOR;
        case sf::BlendMode::Factor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case sf::BlendMode::Factor::DstColor:         return GL_DST_COLOR;
        case sf::BlendMode::Factor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
        case sf::BlendMode::Factor::SrcAlpha:         return GL_SRC_ALPHA;
        case sf::BlendMode::Factor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case sf::BlendMode::Factor::DstAlpha:         return GL_DST_ALPHA;
        case sf::BlendMode::Factor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
    }
    // clang-format on

    sf::err() << "Invalid value for sf::BlendMode::Factor! Fallback to sf::BlendMode::Factor::Zero." << std::endl;
    assert(false);
    return GL_ZERO;
}


// Convert an sf::BlendMode::Equation constant to the corresponding OpenGL constant.
std::uint32_t equationToGlConstant(sf::BlendMode::Equation blendEquation)
{
    switch (blendEquation)
    {
        case sf::BlendMode::Equation::Add:
            return GLEXT_GL_FUNC_ADD;
        case sf::BlendMode::Equation::Subtract:
            if (GLEXT_blend_subtract)
                return GLEXT_GL_FUNC_SUBTRACT;
            break;
        case sf::BlendMode::Equation::ReverseSubtract:
            if (GLEXT_blend_subtract)
                return GLEXT_GL_FUNC_REVERSE_SUBTRACT;
            break;
        case sf::BlendMode::Equation::Min:
            if (GLEXT_blend_minmax)
                return GLEXT_GL_MIN;
            break;
        case sf::BlendMode::Equation::Max:
            if (GLEXT_blend_minmax)
                return GLEXT_GL_MAX;
            break;
    }

    static bool warned = false;
    if (!warned)
    {
        sf::err() << "OpenGL extension EXT_blend_minmax or EXT_blend_subtract unavailable" << '\n'
                  << "Some blending equations will fallback to sf::BlendMode::Equation::Add" << '\n'
                  << "Ensure that hardware acceleration is enabled if available" << std::endl;

        warned = true;
    }

    return GLEXT_GL_FUNC_ADD;
}


// Convert an UpdateOperation constant to the corresponding OpenGL constant.
std::uint32_t stencilOperationToGlConstant(sf::StencilUpdateOperation operation)
{
    // clang-format off
    switch (operation)
    {
        case sf::StencilUpdateOperation::Keep:      return GL_KEEP;
        case sf::StencilUpdateOperation::Zero:      return GL_ZERO;
        case sf::StencilUpdateOperation::Replace:   return GL_REPLACE;
        case sf::StencilUpdateOperation::Increment: return GL_INCR;
        case sf::StencilUpdateOperation::Decrement: return GL_DECR;
        case sf::StencilUpdateOperation::Invert:    return GL_INVERT;
    }
    // clang-format on

    sf::err() << "Invalid value for sf::StencilUpdateOperation! Fallback to sf::StencilMode::Keep." << std::endl;
    assert(false);
    return GL_KEEP;
}


// Convert a Comparison constant to the corresponding OpenGL constant.
std::uint32_t stencilFunctionToGlConstant(sf::StencilComparison comparison)
{
    // clang-format off
    switch (comparison)
    {
        case sf::StencilComparison::Never:        return GL_NEVER;
        case sf::StencilComparison::Less:         return GL_LESS;
        case sf::StencilComparison::LessEqual:    return GL_LEQUAL;
        case sf::StencilComparison::Greater:      return GL_GREATER;
        case sf::StencilComparison::GreaterEqual: return GL_GEQUAL;
        case sf::StencilComparison::Equal:        return GL_EQUAL;
        case sf::StencilComparison::NotEqual:     return GL_NOTEQUAL;
        case sf::StencilComparison::Always:       return GL_ALWAYS;
    }
    // clang-format on

    sf::err() << "Invalid value for sf::StencilComparison! Fallback to sf::StencilMode::Always." << std::endl;
    assert(false);
    return GL_ALWAYS;
}
} // namespace


namespace sf::priv
{
////////////////////////////////////////////////////////////
bool GlRenderTargetImpl::isActive(std::uint64_t id) const
{
    return isActiveInCurrentContext(id);
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::activate(RenderTarget& target, std::uint64_t id, bool active)
{
    // Mark this RenderTarget as active or no longer active in the tracking map
    const std::lock_guard lock(getContextRenderTargetMapMutex());

    const std::uint64_t contextId = Context::getActiveContextId();

    auto& cache = getCache(target);

    auto&      contextRenderTargetMap = getContextRenderTargetMap();
    const auto it                     = contextRenderTargetMap.find(contextId);

    if (active)
    {
        if (it == contextRenderTargetMap.end())
        {
            contextRenderTargetMap[contextId] = id;

            cache.glStatesSet = false;
            cache.enable      = false;
        }
        else if (it->second != id)
        {
            it->second = id;

            cache.enable = false;
        }
    }
    else
    {
        if (it != contextRenderTargetMap.end())
            contextRenderTargetMap.erase(it);

        cache.enable = false;
    }
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::clear(RenderTarget& target, Color color)
{
    // Unbind texture to fix RenderTexture preventing clear
    applyTexture(target, nullptr);

    // Apply the view (scissor testing can affect clearing)
    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    glCheck(glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
    glCheck(glClear(GL_COLOR_BUFFER_BIT));
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::clearStencil(RenderTarget& target, StencilValue stencilValue)
{
    // Unbind texture to fix RenderTexture preventing clear
    applyTexture(target, nullptr);

    // Apply the view (scissor testing can affect clearing)
    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    glCheck(glClearStencil(static_cast<int>(stencilValue.value)));
    glCheck(glClear(GL_STENCIL_BUFFER_BIT));
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::clear(RenderTarget& target, Color color, StencilValue stencilValue)
{
    // Unbind texture to fix RenderTexture preventing clear
    applyTexture(target, nullptr);

    // Apply the view (scissor testing can affect clearing)
    const auto& cache = getCache(target);
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    glCheck(glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
    glCheck(glClearStencil(static_cast<int>(stencilValue.value)));
    glCheck(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::draw(RenderTarget&       target,
                              const Vertex*       vertices,
                              std::size_t         vertexCount,
                              PrimitiveType       type,
                              const RenderStates& states,
                              bool                useVertexCache)
{
    auto& cache = getCache(target);

    setupDraw(target, useVertexCache, states);

    // Check if texture coordinates array is needed, and update client state accordingly
    const bool enableTexCoordsArray = (states.texture || states.shader);
    if (!cache.enable || (enableTexCoordsArray != cache.texCoordsArrayEnabled))
    {
        if (enableTexCoordsArray)
            glCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
        else
            glCheck(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
    }

    // If we switch between non-cache and cache mode or enable texture
    // coordinates we need to set up the pointers to the vertices' components
    if (!cache.enable || !useVertexCache || !cache.useVertexCache)
    {
        const auto* data = reinterpret_cast<const std::byte*>(vertices);

        // If we pre-transform the vertices, we must use our internal vertex cache
        if (useVertexCache)
            data = reinterpret_cast<const std::byte*>(cache.vertexCache.data());

        glCheck(glVertexPointer(2, GL_FLOAT, sizeof(Vertex), data + 0));
        glCheck(glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), data + 8));
        if (enableTexCoordsArray)
            glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), data + 12));
    }
    else if (enableTexCoordsArray && !cache.texCoordsArrayEnabled)
    {
        // If we enter this block, we are already using our internal vertex cache
        const auto* data = reinterpret_cast<const std::byte*>(cache.vertexCache.data());

        glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), data + 12));
    }

    drawPrimitives(type, 0, vertexCount);
    cleanupDraw(target, states);

    // Update the cache
    cache.useVertexCache        = useVertexCache;
    cache.texCoordsArrayEnabled = enableTexCoordsArray;
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::draw(RenderTarget&       target,
                              const VertexBuffer& vertexBuffer,
                              std::size_t         firstVertex,
                              std::size_t         vertexCount,
                              const RenderStates& states)
{
    if (!vertexBuffer.getNativeHandle())
        return;

    auto& cache = getCache(target);

    setupDraw(target, false, states);

    // Bind vertex buffer
    GlVertexBufferImpl::bind(&vertexBuffer);

    // Always enable texture coordinates
    if (!cache.enable || !cache.texCoordsArrayEnabled)
        glCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));

    glCheck(glVertexPointer(2, GL_FLOAT, sizeof(Vertex), reinterpret_cast<const void*>(0)));
    glCheck(glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), reinterpret_cast<const void*>(8)));
    glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), reinterpret_cast<const void*>(12)));

    drawPrimitives(vertexBuffer.getPrimitiveType(), firstVertex, vertexCount);

    // Unbind vertex buffer
    GlVertexBufferImpl::bind(nullptr);

    cleanupDraw(target, states);

    // Update the cache
    cache.useVertexCache        = false;
    cache.texCoordsArrayEnabled = true;
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::pushStates([[maybe_unused]] RenderTarget& target)
{
#ifdef SFML_DEBUG
    // make sure that the user didn't leave an unchecked OpenGL error
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        err() << "OpenGL error (" << error << ") detected in user code, "
              << "you should check for errors with glGetError()" << std::endl;
    }
#endif

#ifndef SFML_OPENGL_ES
    glCheck(glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS));
    glCheck(glPushAttrib(GL_ALL_ATTRIB_BITS));
#endif
    glCheck(glMatrixMode(GL_MODELVIEW));
    glCheck(glPushMatrix());
    glCheck(glMatrixMode(GL_PROJECTION));
    glCheck(glPushMatrix());
    glCheck(glMatrixMode(GL_TEXTURE));
    glCheck(glPushMatrix());
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::popStates([[maybe_unused]] RenderTarget& target)
{
    glCheck(glMatrixMode(GL_PROJECTION));
    glCheck(glPopMatrix());
    glCheck(glMatrixMode(GL_MODELVIEW));
    glCheck(glPopMatrix());
    glCheck(glMatrixMode(GL_TEXTURE));
    glCheck(glPopMatrix());
#ifndef SFML_OPENGL_ES
    glCheck(glPopClientAttrib());
    glCheck(glPopAttrib());
#endif
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::resetStates(RenderTarget& target, std::uint64_t id)
{
    // Check here to make sure a context change does not happen after activate(true)
    const bool shaderAvailable       = GlShaderImpl::isAvailable();
    const bool vertexBufferAvailable = GlVertexBufferImpl::isAvailable();

// Workaround for states not being properly reset on
// macOS unless a context switch really takes place
#if defined(SFML_SYSTEM_MACOS)
    if (!target.setActive(false))
    {
        err() << "Failed to set render target inactive" << std::endl;
    }
#endif

    if (isActive(id) || target.setActive(true))
    {
        auto& cache = getCache(target);

        // Make sure that extensions are initialized
        ensureExtensionsInit();

        // Make sure that the texture unit which is active is the number 0
        if (GLEXT_multitexture)
        {
            glCheck(GLEXT_glClientActiveTexture(GLEXT_GL_TEXTURE0));
            glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0));
        }

        // Define the default OpenGL states
        glCheck(glDisable(GL_CULL_FACE));
        glCheck(glDisable(GL_LIGHTING));
        glCheck(glDisable(GL_STENCIL_TEST));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glDisable(GL_ALPHA_TEST));
        glCheck(glDisable(GL_SCISSOR_TEST));
        glCheck(glEnable(GL_TEXTURE_2D));
        glCheck(glEnable(GL_BLEND));
        glCheck(glMatrixMode(GL_MODELVIEW));
        glCheck(glLoadIdentity());
        glCheck(glEnableClientState(GL_VERTEX_ARRAY));
        glCheck(glEnableClientState(GL_COLOR_ARRAY));
        glCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
        glCheck(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
        cache.scissorEnabled = false;
        cache.stencilEnabled = false;
        cache.glStatesSet    = true;

        // Apply the default SFML states
        applyBlendMode(target, BlendAlpha);
        applyStencilMode(target, StencilMode());
        applyTexture(target, nullptr);
        if (shaderAvailable)
            applyShader(nullptr);

        if (vertexBufferAvailable)
            glCheck(GlVertexBufferImpl::bind(nullptr));

        cache.texCoordsArrayEnabled = true;

        cache.useVertexCache = false;

        // Set the default view
        target.setView(target.getView());

        cache.enable = true;
    }
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::applyCurrentView(RenderTarget& target)
{
    auto&       cache = getCache(target);
    const View& view  = target.getView();

    // Set the viewport
    const IntRect viewport    = target.getViewport(view);
    const int     viewportTop = static_cast<int>(target.getSize().y) - (viewport.position.y + viewport.size.y);
    glCheck(glViewport(viewport.position.x, viewportTop, viewport.size.x, viewport.size.y));

    // Set the scissor rectangle and enable/disable scissor testing
    if (view.getScissor() == FloatRect({0, 0}, {1, 1}))
    {
        if (!cache.enable || cache.scissorEnabled)
        {
            glCheck(glDisable(GL_SCISSOR_TEST));
            cache.scissorEnabled = false;
        }
    }
    else
    {
        const IntRect pixelScissor = target.getScissor(view);
        const int scissorTop = static_cast<int>(target.getSize().y) - (pixelScissor.position.y + pixelScissor.size.y);
        glCheck(glScissor(pixelScissor.position.x, scissorTop, pixelScissor.size.x, pixelScissor.size.y));

        if (!cache.enable || !cache.scissorEnabled)
        {
            glCheck(glEnable(GL_SCISSOR_TEST));
            cache.scissorEnabled = true;
        }
    }

    // Set the projection matrix
    glCheck(glMatrixMode(GL_PROJECTION));
    glCheck(glLoadMatrixf(view.getTransform().getMatrix()));

    // Go back to model-view mode
    glCheck(glMatrixMode(GL_MODELVIEW));

    cache.viewChanged = false;
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::applyBlendMode(RenderTarget& target, const BlendMode& mode)
{
    // Apply the blend mode, falling back to the non-separate versions if necessary
    if (GLEXT_blend_func_separate)
    {
        glCheck(GLEXT_glBlendFuncSeparate(factorToGlConstant(mode.colorSrcFactor),
                                          factorToGlConstant(mode.colorDstFactor),
                                          factorToGlConstant(mode.alphaSrcFactor),
                                          factorToGlConstant(mode.alphaDstFactor)));
    }
    else
    {
        glCheck(glBlendFunc(factorToGlConstant(mode.colorSrcFactor), factorToGlConstant(mode.colorDstFactor)));
    }

    if (GLEXT_blend_minmax || GLEXT_blend_subtract)
    {
        if (GLEXT_blend_equation_separate)
        {
            glCheck(GLEXT_glBlendEquationSeparate(equationToGlConstant(mode.colorEquation),
                                                  equationToGlConstant(mode.alphaEquation)));
        }
        else
        {
            glCheck(GLEXT_glBlendEquation(equationToGlConstant(mode.colorEquation)));
        }
    }
    else if ((mode.colorEquation != BlendMode::Equation::Add) || (mode.alphaEquation != BlendMode::Equation::Add))
    {
        static bool warned = false;

        if (!warned)
        {
#ifdef SFML_OPENGL_ES
            err() << "OpenGL ES extension OES_blend_subtract unavailable" << std::endl;
#else
            err() << "OpenGL extension EXT_blend_minmax and EXT_blend_subtract unavailable" << std::endl;
#endif
            err() << "Selecting a blend equation not possible" << '\n'
                  << "Ensure that hardware acceleration is enabled if available" << std::endl;

            warned = true;
        }
    }

    getCache(target).lastBlendMode = mode;
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::applyStencilMode(RenderTarget& target, const StencilMode& mode)
{
    auto& cache = getCache(target);

    // Fast path if we have a default (disabled) stencil mode
    if (mode == StencilMode())
    {
        if (!cache.enable || cache.stencilEnabled)
        {
            glCheck(glDisable(GL_STENCIL_TEST));
            glCheck(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

            cache.stencilEnabled = false;
        }
    }
    else
    {
        // Apply the stencil mode
        if (!cache.enable || !cache.stencilEnabled)
            glCheck(glEnable(GL_STENCIL_TEST));

        glCheck(glStencilOp(GL_KEEP,
                            stencilOperationToGlConstant(mode.stencilUpdateOperation),
                            stencilOperationToGlConstant(mode.stencilUpdateOperation)));
        glCheck(glStencilFunc(stencilFunctionToGlConstant(mode.stencilComparison),
                              static_cast<int>(mode.stencilReference.value),
                              mode.stencilMask.value));

        cache.stencilEnabled = true;
    }

    cache.lastStencilMode = mode;
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::applyTransform(const Transform& transform)
{
    // No need to call glMatrixMode(GL_MODELVIEW), it is always the
    // current mode (for optimization purpose, since it's the most used)
    if (transform == Transform::Identity)
        glCheck(glLoadIdentity());
    else
        glCheck(glLoadMatrixf(transform.getMatrix()));
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::applyTexture(RenderTarget& target, const Texture* texture, CoordinateType coordinateType)
{
    GlTextureImpl::bind(texture, coordinateType);

    auto& cache              = getCache(target);
    cache.lastTextureId      = texture ? getTextureCacheId(*texture) : 0;
    cache.lastCoordinateType = coordinateType;
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::applyShader(const Shader* shader)
{
    GlShaderImpl::bind(shader);
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::setupDraw(RenderTarget& target, bool useVertexCache, const RenderStates& states)
{
    auto& cache = getCache(target);

    // GL_FRAMEBUFFER_SRGB is not available on OpenGL ES
    // If a framebuffer supports sRGB, it will always be enabled on OpenGL ES
#ifndef SFML_OPENGL_ES
    // Enable or disable sRGB encoding
    // This is needed for drivers that do not check the format of the surface drawn to before applying sRGB conversion
    if (!cache.enable)
    {
        if (target.isSrgb())
            glCheck(glEnable(GL_FRAMEBUFFER_SRGB));
        else if (GLEXT_framebuffer_sRGB)
            glCheck(glDisable(GL_FRAMEBUFFER_SRGB));
    }
#endif

    // First set the persistent OpenGL states if it's the very first call
    if (!cache.glStatesSet)
        resetStates(target, getId(target));

    if (useVertexCache)
    {
        // Since vertices are transformed, we must use an identity transform to render them
        if (!cache.enable || !cache.useVertexCache)
            glCheck(glLoadIdentity());
    }
    else
    {
        applyTransform(states.transform);
    }

    // Apply the view
    if (!cache.enable || cache.viewChanged)
        applyCurrentView(target);

    // Apply the blend mode
    if (!cache.enable || (states.blendMode != cache.lastBlendMode))
        applyBlendMode(target, states.blendMode);

    // Apply the stencil mode
    if (!cache.enable || (states.stencilMode != cache.lastStencilMode))
        applyStencilMode(target, states.stencilMode);

    // Mask the color buffer off if necessary
    if (states.stencilMode.stencilOnly)
        glCheck(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));

    // Apply the texture
    if (!cache.enable || (states.texture && isTextureFboAttachment(*states.texture)))
    {
        // If the texture is an FBO attachment, always rebind it
        // in order to inform the OpenGL driver that we want changes
        // made to it in other contexts to be visible here as well
        // This saves us from having to call glFlush() in
        // RenderTextureImplFBO which can be quite costly
        // See: https://www.khronos.org/opengl/wiki/Memory_Model
        applyTexture(target, states.texture, states.coordinateType);
    }
    else
    {
        const std::uint64_t textureId = states.texture ? getTextureCacheId(*states.texture) : 0;
        if (textureId != cache.lastTextureId || states.coordinateType != cache.lastCoordinateType)
            applyTexture(target, states.texture, states.coordinateType);
    }

    // Apply the shader
    if (states.shader)
        applyShader(states.shader);
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::drawPrimitives(PrimitiveType type, std::size_t firstVertex, std::size_t vertexCount)
{
    // Find the OpenGL primitive type
    static constexpr EnumArray<PrimitiveType, GLenum, 6> modes =
        {GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
    const GLenum mode = modes[type];

    // Draw the primitives
    glCheck(glDrawArrays(mode, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount)));
}


////////////////////////////////////////////////////////////
void GlRenderTargetImpl::cleanupDraw(RenderTarget& target, const RenderStates& states)
{
    // Unbind the shader, if any
    if (states.shader)
        applyShader(nullptr);

    // If the texture we used to draw belonged to a RenderTexture, then forcibly unbind that texture.
    // This prevents a bug where some drivers do not clear RenderTextures properly.
    if (states.texture && isTextureFboAttachment(*states.texture))
        applyTexture(target, nullptr);

    // Mask the color buffer back on if necessary
    if (states.stencilMode.stencilOnly)
        glCheck(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

    // Re-enable the cache at the end of the draw if it was disabled
    getCache(target).enable = true;
}

} // namespace sf::priv
