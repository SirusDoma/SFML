Texture2D tex : register(t0);
SamplerState texSampler : register(s0);

struct PSInput
{
    float4 position  : SV_POSITION;
    float2 texCoords : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    // Read and apply a color from the texture
    return tex.Sample(texSampler, input.texCoords);
}
