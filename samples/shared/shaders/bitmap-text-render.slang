struct VertexInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

uniform float4x4 mvp;

[shader("vertex")]
VertexOutput VertexMain(VertexInput in)
{
    VertexOutput out;
    out.position = mul(mvp, float4(in.position, 0, 1));
    out.color = in.color;
    out.uv = in.uv;
    return out;
}

uniform Sampler2D glyph_atlas;

[shader("fragment")]
float4 FragmentMain(VertexOutput in)
{
    float alpha = glyph_atlas.Sample(in.uv).r;
    float4 color = float4(in.color.rgb, alpha);
    return color;
}