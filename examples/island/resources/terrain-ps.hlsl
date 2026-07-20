float lightFactor;

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR0;
    float3 normal   : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float3 lightPosition = float3(-1.0f, 1.0f, 1.0f);
    float3 eyePosition   = float3(0.0f, 0.0f, 1.0f);
    float3 halfVector    = normalize(lightPosition + eyePosition);
    float  intensity     = lightFactor + (1.0f - lightFactor) * dot(normalize(input.normal), normalize(halfVector));
    return input.color * float4(intensity, intensity, intensity, 1.0f);
}
