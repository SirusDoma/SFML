// The render target's resolution (used for scaling)
float2 resolution;

// The billboards' size
float2 size;

struct GSInput
{
    float4 position : SV_POSITION;
};

struct PSInput
{
    float4 position  : SV_POSITION;
    float2 texCoords : TEXCOORD0;
};

// The output will consist of triangle strips with four vertices each
[maxvertexcount(4)]
void main(point GSInput input[1], inout TriangleStream<PSInput> stream)
{
    // Calculate the half width/height of the billboards, scaled by resolution
    float2 half_size = (size / 2.0f) / resolution;

    // Retrieve the passed vertex position
    float2 pos = input[0].position.xy;

    PSInput vertex;

    // Bottom left vertex
    vertex.position  = float4(pos - half_size, 0.0f, 1.0f);
    vertex.texCoords = float2(1.0f, 1.0f);
    stream.Append(vertex);

    // Bottom right vertex
    vertex.position  = float4(pos.x + half_size.x, pos.y - half_size.y, 0.0f, 1.0f);
    vertex.texCoords = float2(0.0f, 1.0f);
    stream.Append(vertex);

    // Top left vertex
    vertex.position  = float4(pos.x - half_size.x, pos.y + half_size.y, 0.0f, 1.0f);
    vertex.texCoords = float2(1.0f, 0.0f);
    stream.Append(vertex);

    // Top right vertex
    vertex.position  = float4(pos + half_size, 0.0f, 1.0f);
    vertex.texCoords = float2(0.0f, 0.0f);
    stream.Append(vertex);

    stream.RestartStrip();
}
