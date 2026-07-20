Texture2D tex : register(t0);
SamplerState texSampler : register(s0);

float pixel_threshold;

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float factor = 1.0f / (pixel_threshold + 0.001f);
    float2 pos = floor(input.texCoords * factor + 0.5f) / factor;
    return tex.Sample(texSampler, pos) * input.color;
}
