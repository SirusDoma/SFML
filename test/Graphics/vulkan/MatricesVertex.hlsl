// Parity vertex shader consuming the SFML matrices, compiled to SPIR-V at
// build time the same way applications compile their own shaders for the
// Vulkan renderer without the runtime shader compiler
cbuffer SFMLMatrices : register(b0)
{
    column_major float4x4 sfmlModelView;
    column_major float4x4 sfmlProjection;
    column_major float4x4 sfmlTextureMatrix;
};

struct VSInput  { float2 position : POSITION; float4 color : COLOR0; float2 texCoords : TEXCOORD0; };
struct PSInput  { float4 position : SV_POSITION; float4 color : COLOR0; };

PSInput main(VSInput input)
{
    PSInput output;
    output.position = mul(sfmlProjection, mul(sfmlModelView, float4(input.position, 0.0f, 1.0f)));
    output.color    = input.color;
    return output;
}
