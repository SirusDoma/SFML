#include <metal_stdlib>
using namespace metal;

struct SFMLMatrices
{
    float4x4 modelView;
    float4x4 projection;
    float4x4 textureMatrix;
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
    float3 normal;
};

vertex FragmentInput terrainVS(VertexInput input [[stage_in]],
                               constant SFMLMatrices& matrices [[buffer(1)]])
{
    FragmentInput output;
    output.position = matrices.projection * (matrices.modelView * float4(input.position, 0.0, 1.0));
    output.color    = input.color;
    output.normal   = float3(input.texCoords, 1.0);
    return output;
}
