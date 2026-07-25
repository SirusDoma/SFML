// Parity fragment shader writing a flat color, compiled to SPIR-V at build
// time the same way applications compile their own shaders for the Vulkan
// renderer without the runtime shader compiler
struct PSInput { float4 position : SV_POSITION; float4 color : COLOR0; };

float4 main(PSInput input) : SV_TARGET
{
    return float4(0.0f, 1.0f, 0.0f, 1.0f);
}
