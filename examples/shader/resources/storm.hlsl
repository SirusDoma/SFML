cbuffer SFMLMatrices : register(b0)
{
    column_major float4x4 sfmlModelView;
    column_major float4x4 sfmlProjection;
    column_major float4x4 sfmlTextureMatrix;
};

float2 storm_position;
float storm_total_radius;
float storm_inner_radius;

struct VSInput
{
    float2 position  : POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

PSInput main(VSInput input)
{
    float4 vertex = mul(sfmlModelView, float4(input.position, 0.0f, 1.0f));
    float2 offset = vertex.xy - storm_position;
    float len = length(offset);
    if (len < storm_total_radius)
    {
        float push_distance = storm_inner_radius + len / storm_total_radius * (storm_total_radius - storm_inner_radius);
        vertex.xy = storm_position + normalize(offset) * push_distance;
    }

    PSInput output;
    output.position  = mul(sfmlProjection, vertex);
    output.color     = input.color;
    output.texCoords = mul(sfmlTextureMatrix, float4(input.texCoords, 0.0f, 1.0f)).xy;
    return output;
}
