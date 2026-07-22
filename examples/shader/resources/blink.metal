#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float blink_alpha;
};

struct FragmentInput
{
    float4 position [[position]];
    float4 color;
    float2 texCoords;
};

fragment float4 blinkFS(FragmentInput input [[stage_in]],
                        constant Uniforms& uniforms [[buffer(0)]])
{
    float4 pixel = input.color;
    pixel.a = uniforms.blink_alpha;
    return pixel;
}
