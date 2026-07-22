#include <metal_stdlib>
using namespace metal;

struct SFMLMatrices
{
    float4x4 modelView;
    float4x4 projection;
    float4x4 textureMatrix;
};

struct Uniforms
{
    float wave_phase;
    float2 wave_amplitude;
};

struct VertexInput
{
    float2 position  [[attribute(0)]];
    float4 color     [[attribute(1)]];
    float2 texCoords [[attribute(2)]];
};

struct FragmentInput
{
    float4 position [[position]];
    float4 color;
    float2 texCoords;
};

vertex FragmentInput waveVS(VertexInput input [[stage_in]],
                            constant SFMLMatrices& matrices [[buffer(1)]],
                            constant Uniforms& uniforms [[buffer(2)]])
{
    float4 position = float4(input.position, 0.0, 1.0);
    position.x += cos(input.position.y * 0.02 + uniforms.wave_phase * 3.8) * uniforms.wave_amplitude.x
                + sin(input.position.y * 0.02 + uniforms.wave_phase * 6.3) * uniforms.wave_amplitude.x * 0.3;
    position.y += sin(input.position.x * 0.02 + uniforms.wave_phase * 2.4) * uniforms.wave_amplitude.y
                + cos(input.position.x * 0.02 + uniforms.wave_phase * 5.2) * uniforms.wave_amplitude.y * 0.3;

    FragmentInput output;
    output.position  = matrices.projection * (matrices.modelView * position);
    output.color     = input.color;
    output.texCoords = (matrices.textureMatrix * float4(input.texCoords, 0.0, 1.0)).xy;
    return output;
}
