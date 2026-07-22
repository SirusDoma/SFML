#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float edge_threshold;
};

struct FragmentInput
{
    float4 position [[position]];
    float4 color;
    float2 texCoords;
};

fragment float4 edgeFS(FragmentInput input [[stage_in]],
                       texture2d<float> texture [[texture(0)]],
                       sampler texSampler [[sampler(0)]],
                       constant Uniforms& uniforms [[buffer(0)]])
{
    const float offset = 1.0 / 512.0;
    float2 offx = float2(offset, 0.0);
    float2 offy = float2(0.0, offset);

    float4 hEdge = texture.sample(texSampler, input.texCoords - offy)        * -2.0 +
                   texture.sample(texSampler, input.texCoords + offy)        *  2.0 +
                   texture.sample(texSampler, input.texCoords - offx - offy) * -1.0 +
                   texture.sample(texSampler, input.texCoords - offx + offy) *  1.0 +
                   texture.sample(texSampler, input.texCoords + offx - offy) * -1.0 +
                   texture.sample(texSampler, input.texCoords + offx + offy) *  1.0;

    float4 vEdge = texture.sample(texSampler, input.texCoords - offx)        *  2.0 +
                   texture.sample(texSampler, input.texCoords + offx)        * -2.0 +
                   texture.sample(texSampler, input.texCoords - offx - offy) *  1.0 +
                   texture.sample(texSampler, input.texCoords - offx + offy) * -1.0 +
                   texture.sample(texSampler, input.texCoords + offx - offy) *  1.0 +
                   texture.sample(texSampler, input.texCoords + offx + offy) * -1.0;

    float3 result = sqrt(hEdge.rgb * hEdge.rgb + vEdge.rgb * vEdge.rgb);
    float edge = length(result);
    float4 pixel = input.color * texture.sample(texSampler, input.texCoords);
    if (edge > (uniforms.edge_threshold * 8.0))
        pixel.rgb = float3(0.0, 0.0, 0.0);
    else
        pixel.a = uniforms.edge_threshold;
    return pixel;
}
