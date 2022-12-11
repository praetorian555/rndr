struct InData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoords : TEXCOORD0;
    float AtlasIndex : PSIZE;
};

float4 Main(InData In) : SV_TARGET0
{
    return In.Color;
}
