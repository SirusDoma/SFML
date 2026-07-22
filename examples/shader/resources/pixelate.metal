#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float pixel_threshold;
};

struct FragmentInput
{
    float4 position [[position]];
    float4 color;
    float2 texCoords;
};

fragment float4 pixelateFS(FragmentInput input [[stage_in]],
                           texture2d<float> texture [[texture(0)]],
                           sampler texSampler [[sampler(0)]],
                           constant Uniforms& uniforms [[buffer(0)]])
{
    float factor = 1.0 / (uniforms.pixel_threshold + 0.001);
    float2 pos = floor(input.texCoords * factor + 0.5) / factor;
    return texture.sample(texSampler, pos) * input.color;
}
