#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Renderer.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/StencilMode.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>

#include <catch2/catch_test_macros.hpp>

#include <GraphicsUtil.hpp>
#include <WindowUtil.hpp>
#include <array>
#include <string_view>

namespace
{
////////////////////////////////////////////////////////////
// The same rendering checks run on every backend: the default
// one, and Direct3D 11 when this file is compiled into the
// test-sfml-graphics-d3d11 target.
////////////////////////////////////////////////////////////
bool selectBackend()
{
#ifdef SFML_TEST_BACKEND_D3D11
    sf::setRenderer(sf::Renderer::Direct3D11);
    return sf::getRenderer() == sf::Renderer::Direct3D11;
#else
    return true;
#endif
}

constexpr std::string_view tintFragmentGlsl = R"(
void main()
{
    gl_FragColor = gl_Color * vec4(0.0, 1.0, 0.0, 1.0);
}
)";

constexpr std::string_view tintFragmentHlsl = R"(
struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    return input.color * float4(0.0f, 1.0f, 0.0f, 1.0f);
}
)";

constexpr std::string_view matricesVertexGlsl = R"(
void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    gl_FrontColor = gl_Color;
}
)";

constexpr std::string_view matricesVertexHlsl = R"(
cbuffer SFMLMatrices : register(b0)
{
    column_major float4x4 sfmlModelView;
    column_major float4x4 sfmlProjection;
    column_major float4x4 sfmlTextureMatrix;
};

struct VSInput  { float2 position : POSITION; float4 color : COLOR0; float2 texCoords : TEXCOORD0; };
struct PSInput  { float4 position : SV_POSITION; float4 color : COLOR0; };

PSInput main(VSInput input)
{
    PSInput output;
    output.position = mul(sfmlProjection, mul(sfmlModelView, float4(input.position, 0.0f, 1.0f)));
    output.color    = input.color;
    return output;
}
)";

constexpr std::string_view flatFragmentGlsl = R"(
void main()
{
    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
)";

constexpr std::string_view flatFragmentHlsl = R"(
struct PSInput { float4 position : SV_POSITION; float4 color : COLOR0; };

float4 main(PSInput input) : SV_TARGET
{
    return float4(0.0f, 1.0f, 0.0f, 1.0f);
}
)";

std::array<sf::Vertex, 6> makeQuad(sf::Vector2f position, sf::Vector2f size, sf::Color color)
{
    const sf::Vector2f corner = position + size;
    return {{{position, color},
             {{corner.x, position.y}, color},
             {corner, color},
             {position, color},
             {corner, color},
             {{position.x, corner.y}, color}}};
}

sf::Image render(sf::RenderTexture& target)
{
    target.display();
    return target.getTexture().copyToImage();
}
} // namespace

TEST_CASE("[Graphics] Backend rendering parity", runDisplayTests())
{
    if (!selectBackend())
        SKIP("The Direct3D 11 backend is not available");

    const bool hlsl = sf::getShadingLanguage() == sf::ShadingLanguage::Hlsl;

    sf::RenderTexture target(sf::Vector2u(100, 100));

    SECTION("Clear")
    {
        target.clear(sf::Color::Red);
        const sf::Image image = render(target);
        CHECK(image.getPixel({0, 0}) == sf::Color::Red);
        CHECK(image.getPixel({50, 50}) == sf::Color::Red);
        CHECK(image.getPixel({99, 99}) == sf::Color::Red);
    }

    SECTION("Vertex array quad")
    {
        target.clear(sf::Color::Blue);
        const auto quad = makeQuad({10, 10}, {80, 80}, sf::Color::Red);
        target.draw(quad.data(), quad.size(), sf::PrimitiveType::Triangles);
        const sf::Image image = render(target);
        CHECK(image.getPixel({50, 50}) == sf::Color::Red);
        CHECK(image.getPixel({5, 5}) == sf::Color::Blue);
    }

    SECTION("Triangle fan")
    {
        target.clear(sf::Color::Blue);
        sf::CircleShape circle(40.f);
        circle.setPosition({10, 10});
        circle.setFillColor(sf::Color::Yellow);
        target.draw(circle);
        const sf::Image image = render(target);
        CHECK(image.getPixel({50, 50}) == sf::Color::Yellow);
        CHECK(image.getPixel({12, 12}) == sf::Color::Blue);
    }

    SECTION("Sprite texels with pixel coordinates")
    {
        sf::Image source(sf::Vector2u(2, 2));
        source.setPixel({0, 0}, sf::Color::Red);
        source.setPixel({1, 0}, sf::Color::Green);
        source.setPixel({0, 1}, sf::Color::Blue);
        source.setPixel({1, 1}, sf::Color::White);
        const sf::Texture texture(source);

        sf::Sprite sprite(texture);
        sprite.setScale({50.f, 50.f});

        target.clear(sf::Color::Black);
        target.draw(sprite);
        const sf::Image image = render(target);
        CHECK(image.getPixel({25, 25}) == sf::Color::Red);
        CHECK(image.getPixel({75, 25}) == sf::Color::Green);
        CHECK(image.getPixel({25, 75}) == sf::Color::Blue);
        CHECK(image.getPixel({75, 75}) == sf::Color::White);
    }

    SECTION("Draw order across merged and unmerged draws")
    {
        sf::Image         whiteImage(sf::Vector2u(4, 4), sf::Color::White);
        const sf::Texture texture(whiteImage);

        target.clear(sf::Color::Black);

        // Consecutive same-state quads followed by state changes, the last draw must win
        const auto red = makeQuad({10, 10}, {80, 80}, sf::Color::Red);
        target.draw(red.data(), red.size(), sf::PrimitiveType::Triangles);

        sf::Sprite sprite(texture);
        sprite.setPosition({20, 20});
        sprite.setScale({15.f, 15.f});
        sprite.setColor(sf::Color::Green);
        target.draw(sprite);

        const auto blue = makeQuad({30, 30}, {40, 40}, sf::Color::Blue);
        target.draw(blue.data(), blue.size(), sf::PrimitiveType::Triangles);

        const sf::Image image = render(target);
        CHECK(image.getPixel({15, 15}) == sf::Color::Red);
        CHECK(image.getPixel({25, 25}) == sf::Color::Green);
        CHECK(image.getPixel({50, 50}) == sf::Color::Blue);
    }

    SECTION("Scissored clear")
    {
        target.clear(sf::Color::Red);

        sf::View view = target.getDefaultView();
        view.setScissor(sf::FloatRect({0, 0}, {0.5f, 1}));
        target.setView(view);
        target.clear(sf::Color::Green);
        target.setView(target.getDefaultView());

        const sf::Image image = render(target);
        CHECK(image.getPixel({25, 50}) == sf::Color::Green);
        CHECK(image.getPixel({75, 50}) == sf::Color::Red);
    }

    SECTION("Stencil mask")
    {
        sf::RenderTexture stencilTarget(sf::Vector2u(100, 100), sf::ContextSettings{0, 8});
        stencilTarget.clear(sf::Color::Black, 0);

        // Stamp the left half into the stencil buffer without touching the colors
        sf::RectangleShape mask({50, 100});
        stencilTarget.draw(mask,
                           sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Replace, 1, 0xFF, true});

        // Draw everywhere, visible only where the stencil was stamped
        sf::RectangleShape fill({100, 100});
        fill.setFillColor(sf::Color::Cyan);
        stencilTarget.draw(fill,
                           sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Keep, 1, 0xFF, false});

        const sf::Image image = render(stencilTarget);
        CHECK(image.getPixel({25, 50}) == sf::Color::Cyan);
        CHECK(image.getPixel({75, 50}) == sf::Color::Black);
    }

    SECTION("Fragment shader tint")
    {
        sf::Shader shader;
        REQUIRE(shader.loadFromMemory(hlsl ? tintFragmentHlsl : tintFragmentGlsl, sf::Shader::Type::Fragment));

        target.clear(sf::Color::Black);
        const auto quad = makeQuad({0, 0}, {100, 100}, sf::Color::White);
        target.draw(quad.data(), quad.size(), sf::PrimitiveType::Triangles, sf::RenderStates(&shader));
        const sf::Image image = render(target);
        CHECK(image.getPixel({50, 50}) == sf::Color::Green);
    }

    SECTION("Vertex buffer")
    {
        const auto quad = makeQuad({10, 10}, {80, 80}, sf::Color::Red);

        sf::VertexBuffer buffer(sf::PrimitiveType::Triangles);
        REQUIRE(buffer.create(quad.size()));
        REQUIRE(buffer.update(quad.data()));

        SECTION("Plain draw")
        {
            target.clear(sf::Color::Blue);
            target.draw(buffer);
            const sf::Image image = render(target);
            CHECK(image.getPixel({50, 50}) == sf::Color::Red);
            CHECK(image.getPixel({5, 5}) == sf::Color::Blue);
        }

        SECTION("Draw with a user shader")
        {
            sf::Shader shader;
            REQUIRE(shader.loadFromMemory(hlsl ? matricesVertexHlsl : matricesVertexGlsl,
                                          hlsl ? flatFragmentHlsl : flatFragmentGlsl));

            target.clear(sf::Color::Blue);
            target.draw(buffer, sf::RenderStates(&shader));
            const sf::Image image = render(target);
            CHECK(image.getPixel({50, 50}) == sf::Color::Green);
        }

        SECTION("Partial update with an offset")
        {
            // Replace the second triangle (lower-left) with a green copy of the first (upper-right)
            auto greenQuad = quad;
            for (auto& vertex : greenQuad)
                vertex.color = sf::Color::Green;
            REQUIRE(buffer.update(greenQuad.data(), 3, 3));

            target.clear(sf::Color::Blue);
            target.draw(buffer);
            const sf::Image image = render(target);
            CHECK(image.getPixel({20, 80}) == sf::Color::Blue);
            CHECK(image.getPixel({80, 20}) == sf::Color::Green);
        }
    }

    SECTION("Mipmap keeps the base level intact")
    {
        sf::Image   source(sf::Vector2u(64, 64), sf::Color::Magenta);
        sf::Texture texture(source);
        REQUIRE(texture.generateMipmap());

        sf::Sprite sprite(texture);
        target.clear(sf::Color::Black);
        target.draw(sprite);
        const sf::Image image = render(target);
        CHECK(image.getPixel({32, 32}) == sf::Color::Magenta);
    }

    SECTION("Render texture stays usable after generating a mipmap")
    {
        sf::RenderTexture inner(sf::Vector2u(64, 64));
        inner.clear(sf::Color::Yellow);
        inner.display();
        REQUIRE(inner.generateMipmap());

        // Rendering must reach the texture again after the mipmap generation
        inner.clear(sf::Color::Cyan);
        inner.display();
        CHECK(inner.getTexture().copyToImage().getPixel({32, 32}) == sf::Color::Cyan);
    }

    SECTION("Render texture drawn into another target")
    {
        sf::RenderTexture inner(sf::Vector2u(50, 50));
        inner.clear(sf::Color::Yellow);
        inner.display();

        target.clear(sf::Color::Black);
        target.draw(sf::Sprite(inner.getTexture()));
        const sf::Image image = render(target);
        CHECK(image.getPixel({25, 25}) == sf::Color::Yellow);
        CHECK(image.getPixel({75, 75}) == sf::Color::Black);
    }

    SECTION("Presentation modes")
    {
        for (const auto presentation : {sf::ContextSettings::Presentation::Auto,
                                        sf::ContextSettings::Presentation::Throughput,
                                        sf::ContextSettings::Presentation::LowLatency})
        {
            sf::ContextSettings settings;
            settings.presentation = presentation;

            sf::RenderWindow window(sf::VideoMode({120, 90}), "Parity", sf::Style::None, sf::State::Windowed, settings);

            // Exercise the paced path, then the uncapped path
            window.setVerticalSyncEnabled(true);
            for (int i = 0; i < 3; ++i)
            {
                window.clear(sf::Color::Green);
                window.display();
            }
            window.setVerticalSyncEnabled(false);
            window.clear(sf::Color::Green);
            window.display();

            window.clear(sf::Color::Green);
            sf::Texture texture(window.getSize());
            texture.update(window);
            CHECK(texture.copyToImage().getPixel({60, 45}) == sf::Color::Green);

#ifdef SFML_TEST_BACKEND_D3D11
            // The achieved presentation path is reported back, only an explicit
            // low-latency request selects the flip-model path
            if (presentation == sf::ContextSettings::Presentation::LowLatency)
                CHECK(window.getSettings().presentation == sf::ContextSettings::Presentation::LowLatency);
            else
                CHECK(window.getSettings().presentation == sf::ContextSettings::Presentation::Throughput);
#endif
        }
    }

    SECTION("Texture updated from a render window")
    {
        sf::RenderWindow window(sf::VideoMode({80, 60}), "Parity", sf::Style::None);
        window.clear(sf::Color::Red);

        const auto quad = makeQuad({0, 0}, {20, 20}, sf::Color::Green);
        window.draw(quad.data(), quad.size(), sf::PrimitiveType::Triangles);
        window.display();

        // Draw the same content again, update() reads the current back buffer
        window.clear(sf::Color::Red);
        window.draw(quad.data(), quad.size(), sf::PrimitiveType::Triangles);

        sf::Texture texture(sf::Vector2u(80, 60));
        texture.update(window);

        const sf::Image image = texture.copyToImage();
        CHECK(image.getPixel({10, 10}) == sf::Color::Green);
        CHECK(image.getPixel({50, 30}) == sf::Color::Red);
    }
}
