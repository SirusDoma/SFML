#include <SFML/Graphics/GraphicsBackend.hpp>

// Other 1st party headers
#include <SFML/Graphics/Texture.hpp>

#include <catch2/catch_test_macros.hpp>

#include <WindowUtil.hpp>

TEST_CASE("[Graphics] sf::GraphicsBackend", runDisplayTests())
{
    SECTION("Availability")
    {
        CHECK(sf::isGraphicsBackendAvailable(sf::GraphicsBackend::OpenGL));
    }

    SECTION("Default backend")
    {
        CHECK(sf::getGraphicsBackend() == sf::GraphicsBackend::OpenGL);
        CHECK(sf::getShadingLanguage() == sf::ShadingLanguage::Glsl);
    }

    SECTION("Selection")
    {
        // Selecting the backend already in use always succeeds
        CHECK(sf::setGraphicsBackend(sf::GraphicsBackend::OpenGL));

        // Creating any graphics resource locks the backend in
        const sf::Texture texture;
        CHECK(sf::setGraphicsBackend(sf::GraphicsBackend::OpenGL));
        CHECK_FALSE(sf::setGraphicsBackend(sf::GraphicsBackend::Direct3D11));
        CHECK(sf::getGraphicsBackend() == sf::GraphicsBackend::OpenGL);
    }
}
