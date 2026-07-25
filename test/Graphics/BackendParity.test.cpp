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

// The Vulkan parity shaders are precompiled to SPIR-V at build time when the
// shader compiler was found, exercising the bytecode path applications use
#ifdef SFML_TEST_VULKAN_SPIRV_SHADERS
#include <VulkanFlatFragmentShader.hpp>
#include <VulkanMatricesVertexShader.hpp>
#include <VulkanSampleFragmentShader.hpp>
#include <VulkanTintFragmentShader.hpp>
#endif

namespace
{
////////////////////////////////////////////////////////////
// The same rendering checks run on every backend: the default
// one, Direct3D 11 when this file is compiled into the
// test-sfml-graphics-d3d11 target, Metal when compiled into
// the test-sfml-graphics-metal target, and Vulkan when
// compiled into the test-sfml-graphics-vulkan target.
////////////////////////////////////////////////////////////
bool selectBackend()
{
#if defined(SFML_TEST_BACKEND_D3D11)
    sf::setRenderer(sf::Renderer::Direct3D11);
    return sf::getRenderer() == sf::Renderer::Direct3D11;
#elif defined(SFML_TEST_BACKEND_METAL)
    sf::setRenderer(sf::Renderer::Metal);
    return sf::getRenderer() == sf::Renderer::Metal;
#elif defined(SFML_TEST_BACKEND_VULKAN)
    sf::setRenderer(sf::Renderer::Vulkan);
    return sf::getRenderer() == sf::Renderer::Vulkan;
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

constexpr std::string_view tintFragmentMsl = R"(
#include <metal_stdlib>
using namespace metal;

struct FragmentInput
{
    float4 position [[position]];
    float4 color;
    float2 texCoords;
};

fragment float4 tint(FragmentInput input [[stage_in]])
{
    return input.color * float4(0.0, 1.0, 0.0, 1.0);
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

constexpr std::string_view matricesVertexMsl = R"(
#include <metal_stdlib>
using namespace metal;

struct SFMLConstants
{
    float4x4 modelView;
    float4x4 projection;
    float4x4 textureMatrix;
};

struct VertexInput
{
    float2 position  [[attribute(0)]];
    float4 color     [[attribute(1)]];
    float2 texCoords [[attribute(2)]];
};

struct VertexOutput
{
    float4 position [[position]];
    float4 color;
};

vertex VertexOutput transformVertex(VertexInput input [[stage_in]],
                                    constant SFMLConstants& sfmlConstants [[buffer(1)]])
{
    VertexOutput output;
    output.position = sfmlConstants.projection * (sfmlConstants.modelView * float4(input.position, 0.0, 1.0));
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

constexpr std::string_view flatFragmentMsl = R"(
#include <metal_stdlib>
using namespace metal;

struct FragmentInput
{
    float4 position [[position]];
    float4 color;
};

fragment float4 flat(FragmentInput input [[stage_in]])
{
    return float4(0.0, 1.0, 0.0, 1.0);
}
)";

constexpr std::string_view sampleFragmentGlsl = R"(
uniform sampler2D texture;

void main()
{
    gl_FragColor = texture2D(texture, gl_TexCoord[0].xy);
}
)";

constexpr std::string_view sampleFragmentHlsl = R"(
Texture2D    tex        : register(t0);
SamplerState texSampler : register(s0);

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    return tex.Sample(texSampler, input.texCoords);
}
)";

constexpr std::string_view sampleFragmentMsl = R"(
#include <metal_stdlib>
using namespace metal;

struct FragmentInput
{
    float4 position [[position]];
    float4 color;
    float2 texCoords;
};

fragment float4 sampleTexture(FragmentInput input [[stage_in]],
                              texture2d<float> tex [[texture(0)]],
                              sampler texSampler [[sampler(0)]])
{
    return tex.sample(texSampler, input.texCoords);
}
)";

// View precompiled SPIR-V words as the bytes sf::Shader consumes
template <std::size_t N>
std::string_view spirvView([[maybe_unused]] const std::uint32_t (&words)[N])
{
#ifdef SFML_TEST_VULKAN_SPIRV_SHADERS
    return {reinterpret_cast<const char*>(words), N * sizeof(std::uint32_t)};
#else
    return {};
#endif
}

// Pick the shader source matching the shading language of the selected backend.
// SPIR-V bytecode is only available when the shader compiler was found at build
// time; tests that need it skip otherwise.
std::string_view selectShader(std::string_view glsl, std::string_view hlsl, std::string_view msl, std::string_view spirv = {})
{
    switch (sf::getShadingLanguage())
    {
        case sf::ShadingLanguage::Hlsl:
            return hlsl;
        case sf::ShadingLanguage::Msl:
            return msl;
        case sf::ShadingLanguage::SpirV:
            return spirv;
        default:
            return glsl;
    }
}

// The parity SPIR-V blobs, empty when the build had no shader compiler
#ifdef SFML_TEST_VULKAN_SPIRV_SHADERS
const std::string_view tintFragmentSpirv   = spirvView(vulkanTintFragmentShaderSpirv);
const std::string_view matricesVertexSpirv = spirvView(vulkanMatricesVertexShaderSpirv);
const std::string_view flatFragmentSpirv   = spirvView(vulkanFlatFragmentShaderSpirv);
const std::string_view sampleFragmentSpirv = spirvView(vulkanSampleFragmentShaderSpirv);
#else
constexpr std::string_view tintFragmentSpirv;
constexpr std::string_view matricesVertexSpirv;
constexpr std::string_view flatFragmentSpirv;
constexpr std::string_view sampleFragmentSpirv;
#endif

// Name the draw's texture is bound to, "texture" being a reserved HLSL word
const char* currentTextureName()
{
    const sf::ShadingLanguage language = sf::getShadingLanguage();
    return ((language == sf::ShadingLanguage::Hlsl) || (language == sf::ShadingLanguage::SpirV)) ? "tex" : "texture";
}

// Tell whether the shader sections cannot run for lack of sources
bool shaderSourcesUnavailable()
{
    return (sf::getShadingLanguage() == sf::ShadingLanguage::SpirV) && tintFragmentSpirv.empty();
}

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
        SKIP("The requested backend is not available");

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

    SECTION("Consecutive draws with an incomplete trailing primitive")
    {
        target.clear(sf::Color::Black);

        // Each draw leaves a dangling third vertex that completes no line. A
        // backend that collects draws must not pair it with the first vertex
        // of the next one into a line that was never asked for.
        // Coordinates sit on pixel centres so every backend's line rasterizer
        // covers the same row, whatever its fill convention
        const std::array left  = {sf::Vertex{{10.5f, 20.5f}, sf::Color::Red},
                                  sf::Vertex{{40.5f, 20.5f}, sf::Color::Red},
                                  sf::Vertex{{10.5f, 50.5f}, sf::Color::Red}};
        const std::array right = {sf::Vertex{{60.5f, 80.5f}, sf::Color::Red},
                                  sf::Vertex{{90.5f, 80.5f}, sf::Color::Red},
                                  sf::Vertex{{60.5f, 95.5f}, sf::Color::Red}};

        target.draw(left.data(), left.size(), sf::PrimitiveType::Lines);
        target.draw(right.data(), right.size(), sf::PrimitiveType::Lines);

        const sf::Image image = render(target);

        // The two complete lines are drawn
        CHECK(image.getPixel({25, 20}) == sf::Color::Red);
        CHECK(image.getPixel({75, 80}) == sf::Color::Red);

        // The phantom line would run from the dangling (10.5, 50.5) to
        // (60.5, 80.5), passing through its midpoint
        CHECK(image.getPixel({35, 65}) == sf::Color::Black);
    }

    SECTION("Render texture resized between draws")
    {
        sf::RenderTexture inner(sf::Vector2u(64, 64));
        inner.clear(sf::Color::Yellow);
        inner.display();

        // A draw is in flight on the main target when the other one is
        // re-created, which re-creates its attachment image mid-frame
        target.clear(sf::Color::Black);
        const auto quad = makeQuad({0, 0}, {50, 100}, sf::Color::Blue);
        target.draw(quad.data(), quad.size(), sf::PrimitiveType::Triangles);

        REQUIRE(inner.resize(sf::Vector2u(32, 32)));

        inner.clear(sf::Color::Green);
        inner.display();
        CHECK(inner.getTexture().copyToImage().getPixel({16, 16}) == sf::Color::Green);

        const sf::Image image = render(target);
        CHECK(image.getPixel({25, 50}) == sf::Color::Blue);
        CHECK(image.getPixel({75, 50}) == sf::Color::Black);
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

    SECTION("Texture loaded from an image sub-rectangle")
    {
        sf::Image source(sf::Vector2u(4, 4), sf::Color::White);
        for (unsigned int x = 2; x < 4; ++x)
            for (unsigned int y = 2; y < 4; ++y)
                source.setPixel({x, y}, sf::Color::Cyan);

        const sf::Texture texture(source, false, sf::IntRect({2, 2}, {2, 2}));

        sf::Sprite sprite(texture);
        sprite.setScale({50.f, 50.f});

        target.clear(sf::Color::Black);
        target.draw(sprite);
        const sf::Image image = render(target);
        CHECK(image.getPixel({50, 50}) == sf::Color::Cyan);
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

    SECTION("Scissored clear after a draw under the same view")
    {
        target.clear(sf::Color::Black);

        sf::View view = target.getDefaultView();
        view.setScissor(sf::FloatRect({0, 0}, {0.5f, 1}));
        target.setView(view);

        // Nothing changes state between the draw and the clear, so a backend
        // that collects draws has to flush them itself: the clear comes after
        // the quad and has to land on top of it
        const auto quad = makeQuad({0, 0}, {100, 100}, sf::Color::Red);
        target.draw(quad.data(), quad.size(), sf::PrimitiveType::Triangles);
        target.clear(sf::Color::Green);

        target.setView(target.getDefaultView());

        const sf::Image image = render(target);
        CHECK(image.getPixel({25, 50}) == sf::Color::Green);
        CHECK(image.getPixel({75, 50}) == sf::Color::Black);
    }

    SECTION("Texture changes between draws with the same shader")
    {
        if (shaderSourcesUnavailable())
            SKIP("No shader compiler was found at build time");

        sf::Shader shader;
        REQUIRE(shader.loadFromMemory(selectShader(sampleFragmentGlsl, sampleFragmentHlsl, sampleFragmentMsl, sampleFragmentSpirv),
                                      sf::Shader::Type::Fragment));
        shader.setUniform(currentTextureName(), sf::Shader::CurrentTexture);

        const sf::Texture redTexture(sf::Image(sf::Vector2u(4, 4), sf::Color::Red));
        const sf::Texture blueTexture(sf::Image(sf::Vector2u(4, 4), sf::Color::Blue));

        target.clear(sf::Color::Black);

        // Only the texture changes between the two draws: the shader, the
        // blending and the geometry stay the same, so a backend that keys its
        // resource binding on the shader alone keeps sampling the first one
        sf::Sprite sprite(redTexture);
        sprite.setScale({10.f, 10.f});
        target.draw(sprite, sf::RenderStates(&shader));

        sprite.setTexture(blueTexture, true);
        sprite.setPosition({50, 0});
        target.draw(sprite, sf::RenderStates(&shader));

        const sf::Image image = render(target);
        CHECK(image.getPixel({20, 20}) == sf::Color::Red);
        CHECK(image.getPixel({70, 20}) == sf::Color::Blue);
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

    SECTION("Scissored stencil clear")
    {
        sf::RenderTexture stencilTarget(sf::Vector2u(100, 100), sf::ContextSettings{0, 8});
        stencilTarget.clear(sf::Color::Black, 0);

        // Stamp the whole stencil buffer to 1 without touching the colors
        sf::RectangleShape stamp({100, 100});
        stencilTarget.draw(stamp,
                           sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Replace, 1, 0xFF, true});

        // Clear only the left half of the stencil buffer back to 0
        sf::View view = stencilTarget.getDefaultView();
        view.setScissor(sf::FloatRect({0, 0}, {0.5f, 1}));
        stencilTarget.setView(view);
        stencilTarget.clearStencil(0);
        stencilTarget.setView(stencilTarget.getDefaultView());

        // Draw everywhere, visible only where the stencil kept its 1
        sf::RectangleShape fill({100, 100});
        fill.setFillColor(sf::Color::Green);
        stencilTarget.draw(fill,
                           sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Keep, 1, 0xFF, false});

        const sf::Image image = render(stencilTarget);
        CHECK(image.getPixel({25, 50}) == sf::Color::Black);
        CHECK(image.getPixel({75, 50}) == sf::Color::Green);
    }

    SECTION("Scissored combined clear")
    {
        sf::RenderTexture stencilTarget(sf::Vector2u(100, 100), sf::ContextSettings{0, 8});
        stencilTarget.clear(sf::Color::Black, 0);

        // Clear only the left half to red with a stencil of 1
        sf::View view = stencilTarget.getDefaultView();
        view.setScissor(sf::FloatRect({0, 0}, {0.5f, 1}));
        stencilTarget.setView(view);
        stencilTarget.clear(sf::Color::Red, 1);
        stencilTarget.setView(stencilTarget.getDefaultView());

        // Draw everywhere, visible only where the combined clear stamped its 1
        sf::RectangleShape fill({100, 100});
        fill.setFillColor(sf::Color::Green);
        stencilTarget.draw(fill,
                           sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Keep, 1, 0xFF, false});

        const sf::Image image = render(stencilTarget);
        CHECK(image.getPixel({25, 50}) == sf::Color::Green);
        CHECK(image.getPixel({75, 50}) == sf::Color::Black);
    }

    SECTION("Fragment shader tint")
    {
        if (shaderSourcesUnavailable())
            SKIP("No shader compiler was found at build time");

        sf::Shader shader;
        REQUIRE(shader.loadFromMemory(selectShader(tintFragmentGlsl, tintFragmentHlsl, tintFragmentMsl, tintFragmentSpirv),
                                      sf::Shader::Type::Fragment));

        target.clear(sf::Color::Black);
        const auto quad = makeQuad({0, 0}, {100, 100}, sf::Color::White);
        target.draw(quad.data(), quad.size(), sf::PrimitiveType::Triangles, sf::RenderStates(&shader));
        const sf::Image image = render(target);
        CHECK(image.getPixel({50, 50}) == sf::Color::Green);
    }

    SECTION("Blend mode changes between draws with the same shader")
    {
        if (shaderSourcesUnavailable())
            SKIP("No shader compiler was found at build time");

        sf::Shader shader;
        REQUIRE(shader.loadFromMemory(selectShader(flatFragmentGlsl, flatFragmentHlsl, flatFragmentMsl, flatFragmentSpirv),
                                      sf::Shader::Type::Fragment));

        target.clear(sf::Color::Red);
        const auto quad = makeQuad({0, 0}, {100, 100}, sf::Color::White);

        // The shader outputs green: adding it over red gives yellow, replacing gives green
        sf::RenderStates states(&shader);
        states.blendMode = sf::BlendAdd;
        target.draw(quad.data(), quad.size(), sf::PrimitiveType::Triangles, states);
        CHECK(render(target).getPixel({50, 50}) == sf::Color::Yellow);

        states.blendMode = sf::BlendNone;
        target.draw(quad.data(), quad.size(), sf::PrimitiveType::Triangles, states);
        CHECK(render(target).getPixel({50, 50}) == sf::Color::Green);
    }

    SECTION("Draw with a shader that failed to load after a working one")
    {
        if (shaderSourcesUnavailable())
            SKIP("No shader compiler was found at build time");

        sf::Shader working;
        REQUIRE(working.loadFromMemory(selectShader(tintFragmentGlsl, tintFragmentHlsl, tintFragmentMsl, tintFragmentSpirv),
                                       sf::Shader::Type::Fragment));

        sf::Shader broken;
        REQUIRE_FALSE(broken.loadFromMemory("this is not a shader", sf::Shader::Type::Fragment));

        target.clear(sf::Color::Black);

        // The tint shader turns the left half green
        const auto left = makeQuad({0, 0}, {50, 100}, sf::Color::White);
        target.draw(left.data(), left.size(), sf::PrimitiveType::Triangles, sf::RenderStates(&working));

        // The right half asked for a shader that does not exist, so it draws
        // with the built-in pipeline rather than inheriting the previous one
        const auto right = makeQuad({50, 0}, {50, 100}, sf::Color::White);
        target.draw(right.data(), right.size(), sf::PrimitiveType::Triangles, sf::RenderStates(&broken));

        const sf::Image image = render(target);
        CHECK(image.getPixel({25, 50}) == sf::Color::Green);
        CHECK(image.getPixel({75, 50}) == sf::Color::White);
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
            if (shaderSourcesUnavailable())
                SKIP("No shader compiler was found at build time");

            sf::Shader shader;
            REQUIRE(
                shader.loadFromMemory(selectShader(matricesVertexGlsl, matricesVertexHlsl, matricesVertexMsl, matricesVertexSpirv),
                                      selectShader(flatFragmentGlsl, flatFragmentHlsl, flatFragmentMsl, flatFragmentSpirv)));

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

            // On Metal the explicit intents trade window readback for the direct-to-display
            // fast path, only the balanced default keeps the drawables readable
#ifdef SFML_TEST_BACKEND_METAL
            const bool readable = presentation == sf::ContextSettings::Presentation::Auto;
#else
            const bool readable = true;
#endif
            if (readable)
            {
                window.clear(sf::Color::Green);
                sf::Texture texture(window.getSize());
                texture.update(window);
                CHECK(texture.copyToImage().getPixel({60, 45}) == sf::Color::Green);
            }

#if defined(SFML_TEST_BACKEND_D3D11) || defined(SFML_TEST_BACKEND_METAL) || defined(SFML_TEST_BACKEND_VULKAN)
            // The achieved presentation path is reported back, only an explicit
            // low-latency request selects the low-latency path
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
