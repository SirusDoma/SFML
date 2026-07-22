#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float lightFactor;
};

struct FragmentInput
{
    float4 position [[position]];
    float4 color;
    float3 normal;
};

fragment float4 terrainPS(FragmentInput input [[stage_in]],
                          constant Uniforms& uniforms [[buffer(0)]])
{
    float3 lightPosition = float3(-1.0, 1.0, 1.0);
    float3 eyePosition   = float3(0.0, 0.0, 1.0);
    float3 halfVector    = normalize(lightPosition + eyePosition);
    float  intensity     = uniforms.lightFactor +
                           (1.0 - uniforms.lightFactor) * dot(normalize(input.normal), normalize(halfVector));
    return input.color * float4(intensity, intensity, intensity, 1.0);
}
