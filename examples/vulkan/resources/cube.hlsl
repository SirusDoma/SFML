// Shaders of the raw Vulkan cube, compiled offline into the committed
// SPIR-V binaries next to this file with the DirectX shader compiler:
//
//   dxc -spirv -fspv-target-env=vulkan1.2 -T vs_6_0 -E VSMain cube.hlsl -Fo cube-vs.spv
//   dxc -spirv -fspv-target-env=vulkan1.2 -T ps_6_0 -E PSMain cube.hlsl -Fo cube-ps.spv

struct Transform
{
    column_major float4x4 mvp;
};

[[vk::push_constant]] Transform transform;

struct VSInput { float3 position : POSITION; float2 texCoords : TEXCOORD0; };
struct PSInput { float4 position : SV_POSITION; float2 texCoords : TEXCOORD0; };

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position  = mul(transform.mvp, float4(input.position, 1.0f));
    output.texCoords = input.texCoords;
    return output;
}

[[vk::binding(0, 0)]] Texture2D    tex;
[[vk::binding(1, 0)]] SamplerState texSampler;

float4 PSMain(PSInput input) : SV_TARGET
{
    return tex.Sample(texSampler, input.texCoords);
}
