Texture2D tex : register(t0);
SamplerState texSampler : register(s0);

float edge_threshold;

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    const float offset = 1.0f / 512.0f;
    float2 offx = float2(offset, 0.0f);
    float2 offy = float2(0.0f, offset);

    float4 hEdge = tex.Sample(texSampler, input.texCoords - offy)        * -2.0f +
                   tex.Sample(texSampler, input.texCoords + offy)        *  2.0f +
                   tex.Sample(texSampler, input.texCoords - offx - offy) * -1.0f +
                   tex.Sample(texSampler, input.texCoords - offx + offy) *  1.0f +
                   tex.Sample(texSampler, input.texCoords + offx - offy) * -1.0f +
                   tex.Sample(texSampler, input.texCoords + offx + offy) *  1.0f;

    float4 vEdge = tex.Sample(texSampler, input.texCoords - offx)        *  2.0f +
                   tex.Sample(texSampler, input.texCoords + offx)        * -2.0f +
                   tex.Sample(texSampler, input.texCoords - offx - offy) *  1.0f +
                   tex.Sample(texSampler, input.texCoords - offx + offy) * -1.0f +
                   tex.Sample(texSampler, input.texCoords + offx - offy) *  1.0f +
                   tex.Sample(texSampler, input.texCoords + offx + offy) * -1.0f;

    float3 result = sqrt(hEdge.rgb * hEdge.rgb + vEdge.rgb * vEdge.rgb);
    float edge = length(result);
    float4 pixel = input.color * tex.Sample(texSampler, input.texCoords);
    if (edge > (edge_threshold * 8.0f))
        pixel.rgb = float3(0.0f, 0.0f, 0.0f);
    else
        pixel.a = edge_threshold;
    return pixel;
}
