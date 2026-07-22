
////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics.hpp>

#import <Metal/Metal.h>
#include <array>
#include <filesystem>
#include <iostream>

#include <cmath>
#include <cstddef>
#include <cstdlib>

namespace
{
std::filesystem::path resourcesDir()
{
    // Shared with the OpenGL variant of this example
    return "../opengl/resources";
}

using Matrix = std::array<float, 16>; // column-major, like the matrices of the built-in pipeline

////////////////////////////////////////////////////////////
// Column-major matrix helpers for the cube transform
////////////////////////////////////////////////////////////
Matrix multiply(const Matrix& left, const Matrix& right)
{
    Matrix result{};
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 4; ++row)
            for (std::size_t i = 0; i < 4; ++i)
                result[column * 4 + row] += left[i * 4 + row] * right[column * 4 + i];
    return result;
}

Matrix translation(float x, float y, float z)
{
    // clang-format off
    return {1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            x,   y,   z,   1.f};
    // clang-format on
}

Matrix rotation(float degrees, int axis)
{
    const float radians = degrees * 3.141592654f / 180.f;
    const float c       = std::cos(radians);
    const float s       = std::sin(radians);

    // clang-format off
    if (axis == 0)
        return {1.f, 0.f, 0.f, 0.f,
                0.f, c,   s,   0.f,
                0.f, -s,  c,   0.f,
                0.f, 0.f, 0.f, 1.f};
    if (axis == 1)
        return {c,   0.f, -s,  0.f,
                0.f, 1.f, 0.f, 0.f,
                s,   0.f, c,   0.f,
                0.f, 0.f, 0.f, 1.f};
    return {c,   s,   0.f, 0.f,
            -s,  c,   0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f};
    // clang-format on
}

// The same frustum the OpenGL variant sets up, except that Metal clip space uses z in [0 .. 1]
Matrix perspective(float ratio, float zNear, float zFar)
{
    // clang-format off
    return {zNear / ratio, 0.f,   0.f,                           0.f,
            0.f,           zNear, 0.f,                           0.f,
            0.f,           0.f,   zFar / (zNear - zFar),         -1.f,
            0.f,           0.f,   zNear * zFar / (zNear - zFar), 0.f};
    // clang-format on
}

////////////////////////////////////////////////////////////
// Raw Metal pipeline drawing a colored, depth-tested cube
// between SFML draws, the Metal counterpart of the raw
// OpenGL cube in the opengl example
////////////////////////////////////////////////////////////
class RawCube
{
public:
    ~RawCube()
    {
        [m_depthState release];
        [m_vertexBuffer release];
        [m_pipeline release];
    }

    bool create(id<MTLDevice> device)
    {
        static constexpr const char* shaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct Vertex
{
    packed_float3 position;
    packed_float4 color;
};

struct RasterizerData
{
    float4 position [[position]];
    float4 color;
};

vertex RasterizerData cubeVertex(uint                 vertexId [[vertex_id]],
                                 const device Vertex* vertices [[buffer(0)]],
                                 constant float4x4&   mvp      [[buffer(1)]])
{
    RasterizerData output;
    output.position = mvp * float4(float3(vertices[vertexId].position), 1.0f);
    output.color    = float4(vertices[vertexId].color);
    return output;
}

fragment float4 cubeFragment(RasterizerData input [[stage_in]])
{
    return input.color;
}
)";

        @autoreleasepool
        {
            // Compile the shaders and build a pipeline targeting the formats
            // of the window surface, its depth-stencil texture is
            // MTLPixelFormatDepth32Float_Stencil8 so both the depth and the
            // stencil attachment use it
            NSError*       error   = nil;
            id<MTLLibrary> library = [device newLibraryWithSource:@(shaderSource) options:nil error:&error];

            if (!library)
            {
                std::cerr << "Failed to compile the cube shaders: "
                          << (error ? [[error localizedDescription] UTF8String] : "unknown error") << std::endl;
                return false;
            }

            id<MTLFunction> vertexFunction   = [library newFunctionWithName:@"cubeVertex"];
            id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"cubeFragment"];

            MTLRenderPipelineDescriptor* descriptor    = [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
            descriptor.vertexFunction                  = vertexFunction;
            descriptor.fragmentFunction                = fragmentFunction;
            descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            descriptor.depthAttachmentPixelFormat      = MTLPixelFormatDepth32Float_Stencil8;
            descriptor.stencilAttachmentPixelFormat    = MTLPixelFormatDepth32Float_Stencil8;

            m_pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];

            [fragmentFunction release];
            [vertexFunction release];
            [library release];

            if (!m_pipeline)
            {
                std::cerr << "Failed to create the cube pipeline: "
                          << (error ? [[error localizedDescription] UTF8String] : "unknown error") << std::endl;
                return false;
            }

            // Define a 3D cube (6 faces made of 2 triangles composed by 3 vertices)
            // clang-format off
            constexpr std::array<float, 252> cube =
            {
                // positions    // colors
                -20, -20, -20,  1, 0, 0, 1,
                -20,  20, -20,  1, 0, 0, 1,
                -20, -20,  20,  1, 0, 0, 1,
                -20, -20,  20,  1, 0, 0, 1,
                -20,  20, -20,  1, 0, 0, 1,
                -20,  20,  20,  1, 0, 0, 1,

                 20, -20, -20,  0, 1, 0, 1,
                 20,  20, -20,  0, 1, 0, 1,
                 20, -20,  20,  0, 1, 0, 1,
                 20, -20,  20,  0, 1, 0, 1,
                 20,  20, -20,  0, 1, 0, 1,
                 20,  20,  20,  0, 1, 0, 1,

                -20, -20, -20,  0, 0, 1, 1,
                 20, -20, -20,  0, 0, 1, 1,
                -20, -20,  20,  0, 0, 1, 1,
                -20, -20,  20,  0, 0, 1, 1,
                 20, -20, -20,  0, 0, 1, 1,
                 20, -20,  20,  0, 0, 1, 1,

                -20,  20, -20,  1, 1, 0, 1,
                 20,  20, -20,  1, 1, 0, 1,
                -20,  20,  20,  1, 1, 0, 1,
                -20,  20,  20,  1, 1, 0, 1,
                 20,  20, -20,  1, 1, 0, 1,
                 20,  20,  20,  1, 1, 0, 1,

                -20, -20, -20,  0, 1, 1, 1,
                 20, -20, -20,  0, 1, 1, 1,
                -20,  20, -20,  0, 1, 1, 1,
                -20,  20, -20,  0, 1, 1, 1,
                 20, -20, -20,  0, 1, 1, 1,
                 20,  20, -20,  0, 1, 1, 1,

                -20, -20,  20,  1, 0, 1, 1,
                 20, -20,  20,  1, 0, 1, 1,
                -20,  20,  20,  1, 0, 1, 1,
                -20,  20,  20,  1, 0, 1, 1,
                 20, -20,  20,  1, 0, 1, 1,
                 20,  20,  20,  1, 0, 1, 1
            };
            // clang-format on

            m_vertexBuffer = [device newBufferWithBytes:cube.data()
                                                 length:sizeof(cube)
                                                options:MTLResourceStorageModeShared];

            // Enable Z-buffer read and write
            MTLDepthStencilDescriptor* depthDescriptor = [[[MTLDepthStencilDescriptor alloc] init] autorelease];
            depthDescriptor.depthCompareFunction       = MTLCompareFunctionLess;
            depthDescriptor.depthWriteEnabled          = YES;

            m_depthState = [device newDepthStencilStateWithDescriptor:depthDescriptor];

            return m_vertexBuffer && m_depthState;
        }
    }

    void draw(id<MTLCommandBuffer> commandBuffer, id<MTLTexture> color, id<MTLTexture> depthStencil, const Matrix& mvp) const
    {
        if (!commandBuffer || !color || !depthStencil)
            return;

        // Build a pass that draws over the SFML rendering encoded so far and
        // clears the depth buffer, the depth-stencil contents are not needed
        // afterwards so nothing is stored
        MTLRenderPassDescriptor* pass        = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture     = color;
        pass.colorAttachments[0].loadAction  = MTLLoadActionLoad;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.depthAttachment.texture         = depthStencil;
        pass.depthAttachment.loadAction      = MTLLoadActionClear;
        pass.depthAttachment.clearDepth      = 1.0;
        pass.depthAttachment.storeAction     = MTLStoreActionDontCare;
        pass.stencilAttachment.texture       = depthStencil;
        pass.stencilAttachment.loadAction    = MTLLoadActionDontCare;
        pass.stencilAttachment.storeAction   = MTLStoreActionDontCare;

        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
        [encoder setRenderPipelineState:m_pipeline];
        [encoder setDepthStencilState:m_depthState];
        [encoder setVertexBuffer:m_vertexBuffer offset:0 atIndex:0];
        [encoder setVertexBytes:mvp.data() length:sizeof(Matrix) atIndex:1];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:36];
        [encoder endEncoding];
    }

private:
    id<MTLRenderPipelineState> m_pipeline     = nil;
    id<MTLBuffer>              m_vertexBuffer = nil;
    id<MTLDepthStencilState>   m_depthState   = nil;
};

} // namespace


////////////////////////////////////////////////////////////
/// Entry point of application
///
/// \return Application exit code
///
////////////////////////////////////////////////////////////
int main()
{
    // Select the Metal renderer before any graphics resource is created
    sf::setRenderer(sf::Renderer::Metal);
    if (sf::getRenderer() != sf::Renderer::Metal)
    {
        std::cerr << "Metal is not available on this system" << std::endl;
        return EXIT_FAILURE;
    }

    // Request a 24-bits depth buffer and an 8-bits stencil buffer when creating the window
    sf::ContextSettings contextSettings;
    contextSettings.depthBits   = 24;
    contextSettings.stencilBits = 8;

    // Create the main window
    sf::RenderWindow window(sf::VideoMode({800, 600}),
                            "SFML graphics with Metal",
                            sf::Style::Default,
                            sf::State::Windowed,
                            contextSettings);
    window.setVerticalSyncEnabled(true);
    window.setMinimumSize(sf::Vector2u(400, 300));
    window.setMaximumSize(sf::Vector2u(1200, 900));

    // Create a sprite for the background
    const sf::Texture backgroundTexture(resourcesDir() / "background.jpg");
    const sf::Sprite  background(backgroundTexture);

    // Create some text to draw on top of our Metal object
    const sf::Font font(resourcesDir() / "tuffy.ttf");

    sf::Text text(font, "SFML / Metal demo");
    text.setFillColor(sf::Color(255, 255, 255, 170));
    text.setPosition({300.f, 500.f});

    // Create the raw Metal cube pipeline
    RawCube    rawCube;
    const bool rawCubeReady = sf::Metal::getDevice() && rawCube.create(sf::Metal::getDevice());

    // Create a clock for measuring the time elapsed
    const sf::Clock clock;

    // Start game loop
    while (window.isOpen())
    {
        // Process events
        while (const std::optional event = window.pollEvent())
        {
            // Window closed or escape key pressed: exit
            if (event->is<sf::Event::Closed>() ||
                (event->is<sf::Event::KeyPressed>() &&
                 event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape))
                window.close();

            // Adjust the background view when the window is resized
            if (event->is<sf::Event::Resized>())
            {
                const sf::Vector2u textureSize = backgroundTexture.getSize();

                sf::View view;
                view.setSize(sf::Vector2f(textureSize));
                view.setCenter(sf::Vector2f(textureSize) / 2.f);
                window.setView(view);
            }
        }

        @autoreleasepool
        {
            // Draw the background
            window.draw(background);

            if (rawCubeReady)
            {
                // We get the position of the mouse cursor, so that we can move the box accordingly
                const sf::Vector2i pos = sf::Mouse::getPosition(window);

                const float x = static_cast<float>(pos.x) * 200.f / static_cast<float>(window.getSize().x) - 100.f;
                const float y = -static_cast<float>(pos.y) * 200.f / static_cast<float>(window.getSize().y) + 100.f;

                // Apply some transformations
                const float seconds = clock.getElapsedTime().asSeconds();
                const float ratio   = static_cast<float>(window.getSize().x) / static_cast<float>(window.getSize().y);

                Matrix modelViewProjection = perspective(ratio, 1.f, 500.f);
                modelViewProjection        = multiply(modelViewProjection, translation(x, y, -100.f));
                modelViewProjection        = multiply(modelViewProjection, rotation(seconds * 50.f, 0));
                modelViewProjection        = multiply(modelViewProjection, rotation(seconds * 30.f, 1));
                modelViewProjection        = multiply(modelViewProjection, rotation(seconds * 90.f, 2));

                // Make sure the SFML draws above are encoded ahead of our own pass, then
                // draw the cube with raw Metal calls and hand the pipeline back to SFML.
                // The command buffer and textures are per-frame objects: fetch them fresh
                // every frame after the flush, never cache them across frames
                sf::Metal::flush();
                rawCube.draw(sf::Metal::getCommandBuffer(),
                             sf::Metal::getRenderTargetTexture(),
                             sf::Metal::getDepthStencilTexture(),
                             modelViewProjection);
                sf::Metal::resetStates(window);
            }

            // Draw some text on top of our Metal object
            window.draw(text);

            // Finally, display the rendered frame on screen
            window.display();
        }
    }

    return EXIT_SUCCESS;
}
