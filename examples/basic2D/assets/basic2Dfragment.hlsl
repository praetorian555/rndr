struct InData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoords : TEXCOORD0;
    float AtlasIndex : PSIZE;
};

Texture2DArray Images;
SamplerState Sampler;

float4 Main(InData In) : SV_TARGET0
{
    float Sample = Images.Sample(Sampler, float3(In.TexCoords, In.AtlasIndex));
    float4 Color = In.Color;
    Color.a *= smoothstep(0.6, 0.7, Sample);
    return Color;
}
