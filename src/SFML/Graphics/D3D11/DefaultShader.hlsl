// Built-in shaders replicating the fixed-function behavior of the OpenGL backend:
// transformed position, vertex color modulated with the sampled texture, texture
// coordinates run through a matrix to support pixel coordinates and padded sizes.
// Untextured draws bind a 1x1 white texture so a single pixel shader covers both cases.

cbuffer SFMLMatrices : register(b0)
{
    column_major float4x4 sfmlModelView;
    column_major float4x4 sfmlProjection;
    column_major float4x4 sfmlTextureMatrix;
};

struct VSInput
{
    float2 position  : POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position  = mul(sfmlProjection, mul(sfmlModelView, float4(input.position, 0.0f, 1.0f)));
    output.color     = input.color;
    output.texCoords = mul(sfmlTextureMatrix, float4(input.texCoords, 0.0f, 1.0f)).xy;
    return output;
}

Texture2D sfmlTexture : register(t0);
SamplerState sfmlSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color * sfmlTexture.Sample(sfmlSampler, input.texCoords);
}
