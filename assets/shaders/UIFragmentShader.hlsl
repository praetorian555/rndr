struct InData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoords : TEXCOORD;
    float  AtlasIndex : BLENDINDICES;
};

Texture2DArray Images;
SamplerState Sampler;

float4 Main(InData In) : SV_TARGET0
{
    return In.Color * Images.Sample(Sampler, float3(In.TexCoords, In.AtlasIndex));
}