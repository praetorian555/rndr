struct InData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoords : TEXCOORD0;
    float ThresholdBottom : PSIZE0;
    float ThresholdTop : PSIZE1;
};

cbuffer Constants
{
    float2 ScreenSize;
};

Texture2D Image;
SamplerState Sampler;

float sdf_smoothstep(float threshold_bottom, float threshold_top, float distance)
{
    if (threshold_bottom == threshold_top)
    {
        float width = 0.7 * length(float2(ddx(distance), ddy(distance)));
        return smoothstep(threshold_bottom - width, threshold_bottom + width, distance);
    }
    else
    {
        return smoothstep(threshold_bottom, threshold_top, distance);
    }
}

float4 Main(InData In) : SV_TARGET0
{
    float Sample = Image.Sample(Sampler, In.TexCoords);
    float4 Color = In.Color;
    Color.a *= sdf_smoothstep(In.ThresholdBottom, In.ThresholdTop, Sample);
    return Color;
}
