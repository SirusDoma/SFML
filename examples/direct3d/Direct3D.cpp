
////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics.hpp>

#include <array>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <filesystem>
#include <iostream>
#include <wrl/client.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

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
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int i = 0; i < 4; ++i)
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

// The same frustum the OpenGL variant sets up, except that Direct3D clip space uses z in [0 .. 1]
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
// Raw Direct3D pipeline drawing a textured, depth-tested cube
// between SFML draws, the Direct3D counterpart of the raw
// OpenGL cube in the opengl example
////////////////////////////////////////////////////////////
class RawCube
{
public:
    bool create(ID3D11Device* device)
    {
        static constexpr const char* shaderSource = R"(
cbuffer Transform : register(b0)
{
    column_major float4x4 mvp;
};

struct VSInput { float3 position : POSITION; float2 texCoords : TEXCOORD0; };
struct PSInput { float4 position : SV_POSITION; float2 texCoords : TEXCOORD0; };

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position  = mul(mvp, float4(input.position, 1.0f));
    output.texCoords = input.texCoords;
    return output;
}

Texture2D tex : register(t0);
SamplerState texSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    return tex.Sample(texSampler, input.texCoords);
}
)";

        Microsoft::WRL::ComPtr<ID3DBlob> vertexBytecode;
        Microsoft::WRL::ComPtr<ID3DBlob> pixelBytecode;
        if (FAILED(
                D3DCompile(shaderSource, std::strlen(shaderSource), nullptr, nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vertexBytecode, nullptr)) ||
            FAILED(
                D3DCompile(shaderSource, std::strlen(shaderSource), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &pixelBytecode, nullptr)))
            return false;

        if (FAILED(device->CreateVertexShader(vertexBytecode->GetBufferPointer(),
                                              vertexBytecode->GetBufferSize(),
                                              nullptr,
                                              &m_vertexShader)) ||
            FAILED(device->CreatePixelShader(pixelBytecode->GetBufferPointer(), pixelBytecode->GetBufferSize(), nullptr, &m_pixelShader)))
            return false;

        constexpr D3D11_INPUT_ELEMENT_DESC layout[] =
            {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
             {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}};

        if (FAILED(
                device->CreateInputLayout(layout, 2, vertexBytecode->GetBufferPointer(), vertexBytecode->GetBufferSize(), &m_inputLayout)))
            return false;

        // Define a 3D cube (6 faces made of 2 triangles composed by 3 vertices)
        // clang-format off
        constexpr std::array<float, 180> cube =
        {
            // positions    // texture coordinates
            -20, -20, -20,  0, 0,
            -20,  20, -20,  1, 0,
            -20, -20,  20,  0, 1,
            -20, -20,  20,  0, 1,
            -20,  20, -20,  1, 0,
            -20,  20,  20,  1, 1,

             20, -20, -20,  0, 0,
             20,  20, -20,  1, 0,
             20, -20,  20,  0, 1,
             20, -20,  20,  0, 1,
             20,  20, -20,  1, 0,
             20,  20,  20,  1, 1,

            -20, -20, -20,  0, 0,
             20, -20, -20,  1, 0,
            -20, -20,  20,  0, 1,
            -20, -20,  20,  0, 1,
             20, -20, -20,  1, 0,
             20, -20,  20,  1, 1,

            -20,  20, -20,  0, 0,
             20,  20, -20,  1, 0,
            -20,  20,  20,  0, 1,
            -20,  20,  20,  0, 1,
             20,  20, -20,  1, 0,
             20,  20,  20,  1, 1,

            -20, -20, -20,  0, 0,
             20, -20, -20,  1, 0,
            -20,  20, -20,  0, 1,
            -20,  20, -20,  0, 1,
             20, -20, -20,  1, 0,
             20,  20, -20,  1, 1,

            -20, -20,  20,  0, 0,
             20, -20,  20,  1, 0,
            -20,  20,  20,  0, 1,
            -20,  20,  20,  0, 1,
             20, -20,  20,  1, 0,
             20,  20,  20,  1, 1
        };
        // clang-format on

        D3D11_BUFFER_DESC vertexBufferDesc{};
        vertexBufferDesc.ByteWidth = sizeof(cube);
        vertexBufferDesc.Usage     = D3D11_USAGE_IMMUTABLE;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexBufferData{};
        vertexBufferData.pSysMem = cube.data();

        if (FAILED(device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &m_vertexBuffer)))
            return false;

        D3D11_BUFFER_DESC constantBufferDesc{};
        constantBufferDesc.ByteWidth      = sizeof(Matrix);
        constantBufferDesc.Usage          = D3D11_USAGE_DYNAMIC;
        constantBufferDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (FAILED(device->CreateBuffer(&constantBufferDesc, nullptr, &m_constantBuffer)))
            return false;

        // Enable Z-buffer read and write
        D3D11_DEPTH_STENCIL_DESC depthDesc{};
        depthDesc.DepthEnable    = TRUE;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthDesc.DepthFunc      = D3D11_COMPARISON_LESS;

        if (FAILED(device->CreateDepthStencilState(&depthDesc, &m_depthState)))
            return false;

        // One sampler for mipmapped sampling, one that stays on the base level
        D3D11_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD   = D3D11_FLOAT32_MAX;

        if (FAILED(device->CreateSamplerState(&samplerDesc, &m_mipmapSampler)))
            return false;

        samplerDesc.MaxLOD = 0.f;

        return SUCCEEDED(device->CreateSamplerState(&samplerDesc, &m_baseLevelSampler));
    }

    void draw(ID3D11DeviceContext* context, const Matrix& mvp, ID3D11ShaderResourceView* texture, bool mipmapped) const
    {
        // Upload the transform
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            return;
        std::memcpy(mapped.pData, mvp.data(), sizeof(Matrix));
        context->Unmap(m_constantBuffer.Get(), 0);

        // Clear the depth buffer
        if (auto* depthStencilView = sf::D3D11::getDepthStencilView())
            context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.f, 0);

        // Set every state this rendering relies on, the blend state is deliberately
        // inherited from SFML so the cube blends like the OpenGL variant does
        const UINT stride = sizeof(float) * 5;
        const UINT offset = 0;

        ID3D11Buffer*       vertexBuffer   = m_vertexBuffer.Get();
        ID3D11Buffer*       constantBuffer = m_constantBuffer.Get();
        ID3D11SamplerState* sampler        = mipmapped ? m_mipmapSampler.Get() : m_baseLevelSampler.Get();

        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        context->IASetInputLayout(m_inputLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer);
        context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        context->PSSetShaderResources(0, 1, &texture);
        context->PSSetSamplers(0, 1, &sampler);
        context->OMSetDepthStencilState(m_depthState.Get(), 0);
        context->Draw(36, 0);
    }

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_mipmapSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_baseLevelSampler;
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
    // Select the Direct3D 11 backend before any graphics resource is created
    if (!sf::setGraphicsBackend(sf::GraphicsBackend::Direct3D11))
        std::cerr << "Direct3D 11 is not available, running on OpenGL without the raw Direct3D cube" << std::endl;

    bool exit = false;
    bool sRgb = false;

    while (!exit)
    {
        // Request a 24-bits depth buffer when creating the window
        sf::ContextSettings contextSettings;
        contextSettings.depthBits   = 24;
        contextSettings.sRgbCapable = sRgb;

        // Create the main window
        sf::RenderWindow window(sf::VideoMode({800, 600}),
                                "SFML graphics with Direct3D 11",
                                sf::Style::Default,
                                sf::State::Windowed,
                                contextSettings);
        window.setVerticalSyncEnabled(true);
        window.setMinimumSize(sf::Vector2u(400, 300));
        window.setMaximumSize(sf::Vector2u(1200, 900));

        // Create a sprite for the background
        const sf::Texture backgroundTexture(resourcesDir() / "background.jpg", sRgb);
        const sf::Sprite  background(backgroundTexture);

        // Create some text to draw on top of our Direct3D object
        const sf::Font font(resourcesDir() / "tuffy.ttf");

        sf::Text text(font, "SFML / Direct3D 11 demo");
        sf::Text sRgbInstructions(font, "Press space to toggle sRGB conversion");
        sf::Text mipmapInstructions(font, "Press return to toggle mipmapping");
        text.setFillColor(sf::Color(255, 255, 255, 170));
        sRgbInstructions.setFillColor(sf::Color(255, 255, 255, 170));
        mipmapInstructions.setFillColor(sf::Color(255, 255, 255, 170));
        text.setPosition({280.f, 450.f});
        sRgbInstructions.setPosition({175.f, 500.f});
        mipmapInstructions.setPosition({200.f, 550.f});

        // Load a texture to apply to our 3D cube
        sf::Texture texture(resourcesDir() / "logo.png");

        // Attempt to generate a mipmap for our cube texture
        // We don't check the return value here since
        // mipmapping is purely optional in this example
        (void)texture.generateMipmap();

        // Create the raw Direct3D cube pipeline
        RawCube    rawCube;
        const bool rawCubeReady = sf::D3D11::getDevice() && rawCube.create(sf::D3D11::getDevice());

        // Create a clock for measuring the time elapsed
        const sf::Clock clock;

        // Flag to track whether mipmapping is currently enabled
        bool mipmapEnabled = true;

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
                {
                    exit = true;
                    window.close();
                }

                // Return key: toggle mipmapping
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>();
                    keyPressed && keyPressed->code == sf::Keyboard::Key::Enter)
                {
                    if (mipmapEnabled)
                    {
                        // We simply reload the texture to disable mipmapping
                        texture = sf::Texture(resourcesDir() / "logo.png");

                        mipmapEnabled = false;
                    }
                    else if (texture.generateMipmap())
                    {
                        mipmapEnabled = true;
                    }
                }

                // Space key: toggle sRGB conversion
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>();
                    keyPressed && keyPressed->code == sf::Keyboard::Key::Space)
                {
                    sRgb = !sRgb;
                    window.close();
                }

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

                // Draw the cube with raw Direct3D calls, then hand the pipeline back to SFML
                rawCube.draw(sf::D3D11::getContext(),
                             modelViewProjection,
                             sf::D3D11::getShaderResourceView(texture),
                             mipmapEnabled);
                sf::D3D11::resetStates(window);
            }

            // Draw some text on top of our Direct3D object
            window.draw(text);
            window.draw(sRgbInstructions);
            window.draw(mipmapInstructions);

            // Finally, display the rendered frame on screen
            window.display();
        }
    }

    return EXIT_SUCCESS;
}
