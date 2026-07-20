Texture2D tex : register(t0);
SamplerState texSampler : register(s0);

float blur_radius;

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float2 offx = float2(blur_radius, 0.0f);
    float2 offy = float2(0.0f, blur_radius);

    float4 pixel = tex.Sample(texSampler, input.texCoords)               * 4.0f +
                   tex.Sample(texSampler, input.texCoords - offx)        * 2.0f +
                   tex.Sample(texSampler, input.texCoords + offx)        * 2.0f +
                   tex.Sample(texSampler, input.texCoords - offy)        * 2.0f +
                   tex.Sample(texSampler, input.texCoords + offy)        * 2.0f +
                   tex.Sample(texSampler, input.texCoords - offx - offy) * 1.0f +
                   tex.Sample(texSampler, input.texCoords - offx + offy) * 1.0f +
                   tex.Sample(texSampler, input.texCoords + offx - offy) * 1.0f +
                   tex.Sample(texSampler, input.texCoords + offx + offy) * 1.0f;

    return input.color * (pixel / 16.0f);
}
