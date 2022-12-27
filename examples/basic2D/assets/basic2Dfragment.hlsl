struct InData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoords : TEXCOORD0;
    float SDFThresholdBottom : PSIZE0;
    float SDFThresholdTop : PSIZE1;
};

cbuffer Constants
{
    float2 ScreenSize;
};

Texture2D Image;
SamplerState Sampler;

float3 ApplySRGBCurve_Fast(float3 x)
{
    return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(x - 0.00228) - 0.13448 * x + 0.005719;
}

float3 RemoveSRGBCurve_Fast(float3 x)
{
    return x < 0.04045 ? x / 12.92 : -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864;
}

float4 Main(InData In) : SV_TARGET0
{
    float Sample = Image.Sample(Sampler, In.TexCoords);
    float4 Color = In.Color;
    Color.a *= smoothstep(In.SDFThresholdBottom, In.SDFThresholdTop, Sample);
    Color.rgb = RemoveSRGBCurve_Fast(Color.rgb);
    return Color;
}
