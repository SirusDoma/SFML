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
#include <SFML/Graphics/Export.hpp>

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/CoordinateType.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/StencilMode.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <array>

#include <cstddef>
#include <cstdint>


namespace sf
{
class Shader;
class Texture;
class Transform;
class VertexBuffer;

////////////////////////////////////////////////////////////
/// \brief OpenGL-based implementation of `sf::RenderTarget`
///
////////////////////////////////////////////////////////////
class SFML_GRAPHICS_API GlRenderTarget : public RenderTarget
{
public:
    ////////////////////////////////////////////////////////////
    /// \brief Destructor
    ///
    ////////////////////////////////////////////////////////////
    ~GlRenderTarget() override = default;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy constructor
    ///
    ////////////////////////////////////////////////////////////
    GlRenderTarget(const GlRenderTarget&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Deleted copy assignment
    ///
    ////////////////////////////////////////////////////////////
    GlRenderTarget& operator=(const GlRenderTarget&) = delete;

    ////////////////////////////////////////////////////////////
    /// \brief Move constructor
    ///
    ////////////////////////////////////////////////////////////
    GlRenderTarget(GlRenderTarget&&) noexcept = default;

    ////////////////////////////////////////////////////////////
    /// \brief Move assignment
    ///
    ////////////////////////////////////////////////////////////
    GlRenderTarget& operator=(GlRenderTarget&&) noexcept = default;


    ////////////////////////////////////////////////////////////
    /// \brief Clear the entire target with a single color
    ///
    /// This function is usually called once every frame,
    /// to clear the previous contents of the target.
    ///
    /// \param color Fill color to use to clear the render target
    ///
    ////////////////////////////////////////////////////////////
    void clear(Color color = Color::Black) override;

    ////////////////////////////////////////////////////////////
    /// \brief Clear the stencil buffer to a specific value
    ///
    /// The specified value is truncated to the bit width of
    /// the current stencil buffer.
    ///
    /// \param stencilValue Stencil value to clear to
    ///
    ////////////////////////////////////////////////////////////
    void clearStencil(StencilValue stencilValue) override;

    ////////////////////////////////////////////////////////////
    /// \brief Clear the entire target with a single color and stencil value
    ///
    /// The specified stencil value is truncated to the bit
    /// width of the current stencil buffer.
    ///
    /// \param color        Fill color to use to clear the render target
    /// \param stencilValue Stencil value to clear to
    ///
    ////////////////////////////////////////////////////////////
    void clear(Color color, StencilValue stencilValue) override;

    ////////////////////////////////////////////////////////////
    /// \brief Change the current active view
    ///
    /// \param view New view to use
    ///
    /// \see `getView`, `getDefaultView`
    ///
    ////////////////////////////////////////////////////////////
    void setView(const View& view) override;

    ////////////////////////////////////////////////////////////
    /// \brief Draw a drawable object to the render target
    ///
    /// \param drawable Object to draw
    /// \param states   Render states to use for drawing
    ///
    ////////////////////////////////////////////////////////////
    void draw(const Drawable& drawable, const RenderStates& states = RenderStates::Default) override;

    ////////////////////////////////////////////////////////////
    /// \brief Draw primitives defined by an array of vertices
    ///
    /// \param vertices    Pointer to the vertices
    /// \param vertexCount Number of vertices in the array
    /// \param type        Type of primitives to draw
    /// \param states      Render states to use for drawing
    ///
    ////////////////////////////////////////////////////////////
    void draw(const Vertex*       vertices,
              std::size_t         vertexCount,
              PrimitiveType       type,
              const RenderStates& states = RenderStates::Default) override;

    ////////////////////////////////////////////////////////////
    /// \brief Draw primitives defined by a vertex buffer
    ///
    /// \param vertexBuffer Vertex buffer
    /// \param states       Render states to use for drawing
    ///
    ////////////////////////////////////////////////////////////
    void draw(const VertexBuffer& vertexBuffer, const RenderStates& states = RenderStates::Default) override;

    ////////////////////////////////////////////////////////////
    /// \brief Draw primitives defined by a vertex buffer
    ///
    /// \param vertexBuffer Vertex buffer
    /// \param firstVertex  Index of the first vertex to render
    /// \param vertexCount  Number of vertices to render
    /// \param states       Render states to use for drawing
    ///
    ////////////////////////////////////////////////////////////
    void draw(const VertexBuffer& vertexBuffer,
              std::size_t         firstVertex,
              std::size_t         vertexCount,
              const RenderStates& states = RenderStates::Default) override;

    ////////////////////////////////////////////////////////////
    /// \brief Activate or deactivate the render target for rendering
    ///
    /// This function makes the render target's context current for
    /// future OpenGL rendering operations (so you shouldn't care
    /// about it if you're not doing direct OpenGL stuff).
    /// A render target's context is active only on the current thread,
    /// if you want to make it active on another thread you have
    /// to deactivate it on the previous thread first if it was active.
    /// Only one context can be current in a thread, so if you
    /// want to draw OpenGL geometry to another render target
    /// don't forget to activate it again. Activating a render
    /// target will automatically deactivate the previously active
    /// context (if any).
    ///
    /// \param active `true` to activate, `false` to deactivate
    ///
    /// \return `true` if operation was successful, `false` otherwise
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] virtual bool setActive(bool active = true);

    ////////////////////////////////////////////////////////////
    /// \brief Save the current OpenGL render states and matrices
    ///
    /// This function can be used when you mix SFML drawing
    /// and direct OpenGL rendering. Combined with popGLStates,
    /// it ensures that:
    /// \li SFML's internal states are not messed up by your OpenGL code
    /// \li your OpenGL states are not modified by a call to a SFML function
    ///
    /// More specifically, it must be used around code that
    /// calls `draw` functions. Example:
    /// \code
    /// // OpenGL code here...
    /// window.pushGLStates();
    /// window.draw(...);
    /// window.draw(...);
    /// window.popGLStates();
    /// // OpenGL code here...
    /// \endcode
    ///
    /// Note that this function is quite expensive: it saves all the
    /// possible OpenGL states and matrices, even the ones you
    /// don't care about. Therefore it should be used wisely.
    /// It is provided for convenience, but the best results will
    /// be achieved if you handle OpenGL states yourself (because
    /// you know which states have really changed, and need to be
    /// saved and restored). Take a look at the resetGLStates
    /// function if you do so.
    ///
    /// \see `popGLStates`
    ///
    ////////////////////////////////////////////////////////////
    void pushGLStates();

    ////////////////////////////////////////////////////////////
    /// \brief Restore the previously saved OpenGL render states and matrices
    ///
    /// See the description of `pushGLStates` to get a detailed
    /// description of these functions.
    ///
    /// \see `pushGLStates`
    ///
    ////////////////////////////////////////////////////////////
    void popGLStates();

    ////////////////////////////////////////////////////////////
    /// \brief Reset the internal OpenGL states so that the target is ready for drawing
    ///
    /// This function can be used when you mix SFML drawing
    /// and direct OpenGL rendering, if you choose not to use
    /// `pushGLStates`/`popGLStates`. It makes sure that all OpenGL
    /// states needed by SFML are set, so that subsequent `draw()`
    /// calls will work as expected.
    ///
    /// Example:
    /// \code
    /// // OpenGL code here...
    /// glPushAttrib(...);
    /// window.resetGLStates();
    /// window.draw(...);
    /// window.draw(...);
    /// glPopAttrib(...);
    /// // OpenGL code here...
    /// \endcode
    ///
    ////////////////////////////////////////////////////////////
    void resetGLStates();

protected:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    ////////////////////////////////////////////////////////////
    GlRenderTarget() = default;

    ////////////////////////////////////////////////////////////
    /// \brief Performs the common initialization step after creation
    ///
    /// The derived classes must call this function after the
    /// target is created and ready for drawing.
    ///
    ////////////////////////////////////////////////////////////
    void initialize() override;

private:
    ////////////////////////////////////////////////////////////
    /// \brief Apply the current view
    ///
    ////////////////////////////////////////////////////////////
    void applyCurrentView();

    ////////////////////////////////////////////////////////////
    /// \brief Apply a new blending mode
    ///
    /// \param mode Blending mode to apply
    ///
    ////////////////////////////////////////////////////////////
    void applyBlendMode(const BlendMode& mode);

    ////////////////////////////////////////////////////////////
    /// \brief Apply a new stencil mode
    ///
    /// \param mode Stencil mode to apply
    ///
    ////////////////////////////////////////////////////////////
    void applyStencilMode(const StencilMode& mode);

    ////////////////////////////////////////////////////////////
    /// \brief Apply a new transform
    ///
    /// \param transform Transform to apply
    ///
    ////////////////////////////////////////////////////////////
    void applyTransform(const Transform& transform);

    ////////////////////////////////////////////////////////////
    /// \brief Apply a new texture
    ///
    /// \param texture        Texture to apply
    /// \param coordinateType The texture coordinate type to use
    ///
    ////////////////////////////////////////////////////////////
    void applyTexture(const Texture* texture, CoordinateType coordinateType = CoordinateType::Pixels);

    ////////////////////////////////////////////////////////////
    /// \brief Apply a new shader
    ///
    /// \param shader Shader to apply
    ///
    ////////////////////////////////////////////////////////////
    void applyShader(const Shader* shader);

    ////////////////////////////////////////////////////////////
    /// \brief Setup environment for drawing
    ///
    /// \param useVertexCache Are we going to use the vertex cache?
    /// \param states         Render states to use for drawing
    ///
    ////////////////////////////////////////////////////////////
    void setupDraw(bool useVertexCache, const RenderStates& states);

    ////////////////////////////////////////////////////////////
    /// \brief Draw the primitives
    ///
    /// \param type        Type of primitives to draw
    /// \param firstVertex Index of the first vertex to use when drawing
    /// \param vertexCount Number of vertices to use when drawing
    ///
    ////////////////////////////////////////////////////////////
    void drawPrimitives(PrimitiveType type, std::size_t firstVertex, std::size_t vertexCount);

    ////////////////////////////////////////////////////////////
    /// \brief Clean up environment after drawing
    ///
    /// \param states Render states used for drawing
    ///
    ////////////////////////////////////////////////////////////
    void cleanupDraw(const RenderStates& states);

    ////////////////////////////////////////////////////////////
    /// \brief Render states cache
    ///
    ////////////////////////////////////////////////////////////
    struct StatesCache
    {
        bool                  enable{};                //!< Is the cache enabled?
        bool                  glStatesSet{};           //!< Are our internal GL states set yet?
        bool                  viewChanged{};           //!< Has the current view changed since last draw?
        bool                  scissorEnabled{};        //!< Is scissor testing enabled?
        bool                  stencilEnabled{};        //!< Is stencil testing enabled?
        BlendMode             lastBlendMode;           //!< Cached blending mode
        StencilMode           lastStencilMode;         //!< Cached stencil
        std::uint64_t         lastTextureId{};         //!< Cached texture
        CoordinateType        lastCoordinateType{};    //!< Texture coordinate type
        bool                  texCoordsArrayEnabled{}; //!< Is `GL_TEXTURE_COORD_ARRAY` client state enabled?
        bool                  useVertexCache{};        //!< Did we previously use the vertex cache?
        std::array<Vertex, 4> vertexCache{};           //!< Pre-transformed vertices cache
    };

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    StatesCache   m_cache{}; //!< Render states cache
    std::uint64_t m_id{};    //!< Unique number that identifies the RenderTarget
};

} // namespace sf


////////////////////////////////////////////////////////////
/// \class sf::GlRenderTarget
/// \ingroup graphics
///
/// `sf::GlRenderTarget` is an OpenGL-based implementation of
/// `sf::RenderTarget`. It provides the actual rendering
/// functionality using OpenGL, including state caching,
/// vertex array management, and shader/texture binding.
///
/// Both `sf::RenderWindow` and `sf::RenderTexture` inherit
/// from this class to get OpenGL rendering capabilities.
///
/// If you need to create a custom render target that does not
/// involve OpenGL directly (such as a sprite batcher or a
/// draw call counter), inherit from `sf::RenderTarget` instead.
///
/// \see `sf::RenderTarget`, `sf::RenderWindow`, `sf::RenderTexture`
///
////////////////////////////////////////////////////////////
