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
    float2 storm_position;
    float storm_total_radius;
    float storm_inner_radius;
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
    float  pointSize [[point_size]];
};

vertex FragmentInput stormVS(VertexInput input [[stage_in]],
                             constant SFMLMatrices& matrices [[buffer(1)]],
                             constant Uniforms& uniforms [[buffer(2)]])
{
    float4 position = matrices.modelView * float4(input.position, 0.0, 1.0);
    float2 offset = position.xy - uniforms.storm_position;
    float len = length(offset);
    if (len < uniforms.storm_total_radius)
    {
        float push_distance = uniforms.storm_inner_radius +
                              len / uniforms.storm_total_radius *
                                  (uniforms.storm_total_radius - uniforms.storm_inner_radius);
        position.xy = uniforms.storm_position + normalize(offset) * push_distance;
    }

    FragmentInput output;
    output.position  = matrices.projection * position;
    output.color     = input.color;
    output.texCoords = (matrices.textureMatrix * float4(input.texCoords, 0.0, 1.0)).xy;
    output.pointSize = 1.0;
    return output;
}
