float blink_alpha;

struct PSInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float2 texCoords : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float4 pixel = input.color;
    pixel.a = blink_alpha;
    return pixel;
}
