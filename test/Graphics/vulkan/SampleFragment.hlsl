// Parity fragment shader sampling the draw's texture, compiled to SPIR-V at
// build time the same way applications compile their own shaders for the
// Vulkan renderer without the runtime shader compiler
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
