#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float blur_radius;
};

struct FragmentInput
{
    float4 position [[position]];
    float4 color;
    float2 texCoords;
};

fragment float4 blurFS(FragmentInput input [[stage_in]],
                       texture2d<float> texture [[texture(0)]],
                       sampler texSampler [[sampler(0)]],
                       constant Uniforms& uniforms [[buffer(0)]])
{
    float2 offx = float2(uniforms.blur_radius, 0.0);
    float2 offy = float2(0.0, uniforms.blur_radius);

    float4 pixel = texture.sample(texSampler, input.texCoords)               * 4.0 +
                   texture.sample(texSampler, input.texCoords - offx)        * 2.0 +
                   texture.sample(texSampler, input.texCoords + offx)        * 2.0 +
                   texture.sample(texSampler, input.texCoords - offy)        * 2.0 +
                   texture.sample(texSampler, input.texCoords + offy)        * 2.0 +
                   texture.sample(texSampler, input.texCoords - offx - offy) * 1.0 +
                   texture.sample(texSampler, input.texCoords - offx + offy) * 1.0 +
                   texture.sample(texSampler, input.texCoords + offx - offy) * 1.0 +
                   texture.sample(texSampler, input.texCoords + offx + offy) * 1.0;

    return input.color * (pixel / 16.0);
}
