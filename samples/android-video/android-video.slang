// One video frame drawn as a quad, letterboxed into the window and turned the way the swap chain is pre-rotated. The
// frame is sampled through the YCbCr conversion its immutable sampler carries, so it reads as RGB here.

[[vk::binding(0, 0)]] Sampler2D frame;

struct Placement
{
    /** The pre-rotation as a 2x2 matrix, row by row: x' = rotation.x * x + rotation.y * y, y' = rotation.z * x + rotation.w * y. */
    float4 rotation;
    /** How much of the window the quad covers on each axis, in clip space. */
    float2 scale;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// Two triangles over the quad. SV_VulkanVertexID is gl_VertexIndex as it is; SV_VertexID would subtract the base
// vertex, which needs the draw parameters feature.
[shader("vertex")]
VertexOutput main_vertex(uint vertex_id : SV_VulkanVertexID, uniform Placement placement)
{
    const float2 k_corners[6] = {float2(0, 0), float2(1, 0), float2(0, 1), float2(0, 1), float2(1, 0), float2(1, 1)};
    const float2 uv = k_corners[vertex_id];
    const float2 position = (uv * 2.0 - 1.0) * placement.scale;
    VertexOutput output;
    output.position = float4(placement.rotation.x * position.x + placement.rotation.y * position.y,
                             placement.rotation.z * position.x + placement.rotation.w * position.y, 0.0, 1.0);
    output.uv = uv;
    return output;
}

[shader("fragment")]
float4 main_fragment(VertexOutput input) : SV_Target
{
    return float4(frame.Sample(input.uv).rgb, 1.0);
}
