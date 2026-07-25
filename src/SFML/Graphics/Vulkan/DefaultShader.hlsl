// Built-in shaders replicating the fixed-function behavior of the OpenGL backend:
// transformed position, vertex color modulated with the sampled texture, texture
// coordinates run through a matrix to support pixel coordinates and padded sizes.
// Untextured draws bind a 1x1 white texture so a single pixel shader covers both cases.
//
// The source mirrors the Direct3D 11 built-in shaders; the resource bindings differ,
// and the matrices arrive as push constants rather than through a buffer, which spares
// the per-draw upload and descriptor rebinding an ever-changing transform would cost.
// The model-view and projection matrices are combined by the backend so the payload
// stays within the 128 bytes every Vulkan implementation guarantees. Compiled to
// SPIR-V the t/s registers land on descriptor set 0 at bindings 16 and 32.

struct SFMLMatrices
{
    column_major float4x4 sfmlModelViewProjection;
    column_major float4x4 sfmlTextureMatrix;
};

[[vk::push_constant]] SFMLMatrices sfmlMatrices;

struct VSInput
{
    float2 position  : POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

// The point size has to be written explicitly on Vulkan for point draws;
// builtins consume no location, the interface stays (COLOR0, TEXCOORD0)
struct VSOutput
{
    float4 position  : SV_POSITION;
    [[vk::builtin("PointSize")]] float pointSize : PSIZE;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position  = mul(sfmlMatrices.sfmlModelViewProjection, float4(input.position, 0.0f, 1.0f));
    output.pointSize = 1.0f;
    output.color     = input.color;
    output.texCoords = mul(sfmlMatrices.sfmlTextureMatrix, float4(input.texCoords, 0.0f, 1.0f)).xy;
    return output;
}

Texture2D sfmlTexture : register(t0);
SamplerState sfmlSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color * sfmlTexture.Sample(sfmlSampler, input.texCoords);
}
