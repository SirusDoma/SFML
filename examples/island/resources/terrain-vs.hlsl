cbuffer SFMLMatrices : register(b0)
{
    column_major float4x4 sfmlModelView;
    column_major float4x4 sfmlProjection;
    column_major float4x4 sfmlTextureMatrix;
};

struct VSInput
{
    float2 position  : POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR0;
    float3 normal   : TEXCOORD0;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.position = mul(sfmlProjection, mul(sfmlModelView, float4(input.position, 0.0f, 1.0f)));
    output.color    = input.color;
    output.normal   = float3(input.texCoords, 1.0f);
    return output;
}
