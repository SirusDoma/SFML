#include <SFML/Graphics/Renderer.hpp>

// Other 1st party headers
#include <SFML/Graphics/Texture.hpp>

#include <catch2/catch_test_macros.hpp>

#include <WindowUtil.hpp>
#include <algorithm>

TEST_CASE("[Graphics] sf::Renderer", runDisplayTests())
{
    SECTION("Available renderers")
    {
        CHECK(sf::isRendererAvailable(sf::Renderer::OpenGL));

        const auto renderers = sf::getAvailableRenderers();
        CHECK(std::find(renderers.begin(), renderers.end(), sf::Renderer::OpenGL) != renderers.end());

#ifdef SFML_TEST_METAL_AVAILABLE
        CHECK(sf::isRendererAvailable(sf::Renderer::Metal));
        CHECK(std::find(renderers.begin(), renderers.end(), sf::Renderer::Metal) != renderers.end());
#endif
    }

    SECTION("Default renderer")
    {
        CHECK(sf::getRenderer() == sf::Renderer::OpenGL);
        CHECK(sf::getShadingLanguage() == sf::ShadingLanguage::Glsl);
    }

    SECTION("Selection")
    {
        // Selecting the renderer already in use never changes it
        sf::setRenderer(sf::Renderer::OpenGL);
        CHECK(sf::getRenderer() == sf::Renderer::OpenGL);

        // Creating any graphics resource locks the renderer in
        const sf::Texture texture;
        sf::setRenderer(sf::Renderer::Direct3D11);
        CHECK(sf::getRenderer() == sf::Renderer::OpenGL);

#ifdef SFML_TEST_METAL_AVAILABLE
        // Metal is available here, selecting it must still fail while locked in
        sf::setRenderer(sf::Renderer::Metal);
        CHECK(sf::getRenderer() == sf::Renderer::OpenGL);
#endif
    }
}
