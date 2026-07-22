////////////////////////////////////////////////////////////
// The built-in shaders replicating the fixed-function pipeline
// of the OpenGL backend. Vertex data is bound at buffer index 0
// through the vertex descriptor, the matrices at buffer index 1;
// user shaders must avoid these two indices.
////////////////////////////////////////////////////////////

#include <metal_stdlib>
using namespace metal;

struct SFMLConstants
{
    float4x4 modelView;
    float4x4 projection;
    float4x4 textureMatrix;
};

struct SFMLVertexInput
{
    float2 position  [[attribute(0)]];
    float4 color     [[attribute(1)]];
    float2 texCoords [[attribute(2)]];
};

struct SFMLFragmentInput
{
    float4 position [[position]];
    float4 color;
    float2 texCoords;
    float  pointSize [[point_size]];
};

vertex SFMLFragmentInput sfmlDefaultVS(SFMLVertexInput input [[stage_in]],
                                       constant SFMLConstants& constants [[buffer(1)]])
{
    SFMLFragmentInput output;
    output.position  = constants.projection * (constants.modelView * float4(input.position, 0.0, 1.0));
    output.color     = input.color;
    output.texCoords = (constants.textureMatrix * float4(input.texCoords, 0.0, 1.0)).xy;
    output.pointSize = 1.0;
    return output;
}

fragment float4 sfmlDefaultFS(SFMLFragmentInput input [[stage_in]],
                              texture2d<float> sfmlTexture [[texture(0)]],
                              sampler sfmlSampler [[sampler(0)]])
{
    return input.color * sfmlTexture.sample(sfmlSampler, input.texCoords);
}
