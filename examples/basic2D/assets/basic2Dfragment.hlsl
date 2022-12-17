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
    float4 Sample = Images.Sample(Sampler, float3(In.TexCoords, In.AtlasIndex));
    return In.Color * Sample;
}
