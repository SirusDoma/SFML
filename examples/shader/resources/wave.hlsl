cbuffer SFMLMatrices : register(b0)
{
    column_major float4x4 sfmlModelView;
    column_major float4x4 sfmlProjection;
    column_major float4x4 sfmlTextureMatrix;
};

float wave_phase;
float2 wave_amplitude;

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
    float4 vertex = float4(input.position, 0.0f, 1.0f);
    vertex.x += cos(input.position.y * 0.02f + wave_phase * 3.8f) * wave_amplitude.x
              + sin(input.position.y * 0.02f + wave_phase * 6.3f) * wave_amplitude.x * 0.3f;
    vertex.y += sin(input.position.x * 0.02f + wave_phase * 2.4f) * wave_amplitude.y
              + cos(input.position.x * 0.02f + wave_phase * 5.2f) * wave_amplitude.y * 0.3f;

    PSInput output;
    output.position  = mul(sfmlProjection, mul(sfmlModelView, vertex));
    output.color     = input.color;
    output.texCoords = mul(sfmlTextureMatrix, float4(input.texCoords, 0.0f, 1.0f)).xy;
    return output;
}
