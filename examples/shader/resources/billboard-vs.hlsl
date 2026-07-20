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

struct GSInput
{
    float4 position : SV_POSITION;
};

GSInput main(VSInput input)
{
    // Transform the vertex position
    GSInput output;
    output.position = mul(sfmlProjection, mul(sfmlModelView, float4(input.position, 0.0f, 1.0f)));
    return output;
}
