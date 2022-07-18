struct InData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoords : TEXCOORD0;
    float2 ScreenCenter : TEXCOORD1;
    float2 ScreenHalfSize : TEXCOORD2;
    float2 ScreenPosition : TEXCOORD3;
    float AtlasIndex : BLENDINDICES0;
    float CornerRadius : BLENDINDICES1;
    float EdgeSoftness : BLENDINDICES2;
    float BorderThickness : BLENDINDICES3;
};

Texture2DArray Images;
SamplerState Sampler;

float RoundedRectSDF(float2 SamplePos, float2 RectCenter, float2 RectHalfSize, float Radius)
{
    float2 Dist2 = (abs(RectCenter - SamplePos) - RectHalfSize + float2(Radius, Radius));
    return min(max(Dist2.x, Dist2.y), 0.0) + length(max(Dist2, 0.0)) - Radius;
}

float4 Main(InData In) : SV_TARGET0
{
    // Roundness factor
    float4 Sample = Images.Sample(Sampler, float3(In.TexCoords, In.AtlasIndex));
    float Softness = In.EdgeSoftness;
    float2 SoftnessPadding = float2(max(0, Softness * 2 - 1), max(0, Softness * 2 - 1));
    float Dist = RoundedRectSDF(In.ScreenPosition, In.ScreenCenter, In.ScreenHalfSize - SoftnessPadding, In.CornerRadius);
    float SdfFactor = 1.f - smoothstep(0, 2 * Softness, Dist);
    
    // Border factor
    float BorderFactor = 1.f;
    if (In.BorderThickness != 0)
    {
        float2 InteriorHalfSize = In.ScreenHalfSize - float2(In.BorderThickness, In.BorderThickness);

        float InteriorRadiusReduceFactor = min(InteriorHalfSize.x / In.ScreenHalfSize.x, InteriorHalfSize.y / In.ScreenHalfSize.y);
        float InteriorCornerRadius = (In.CornerRadius * InteriorRadiusReduceFactor * InteriorRadiusReduceFactor);

        float InsideDist = RoundedRectSDF(In.ScreenPosition, In.ScreenCenter, InteriorHalfSize - SoftnessPadding, InteriorCornerRadius);

        float InsideFactor = smoothstep(0, 2 * Softness, InsideDist);
        BorderFactor = InsideFactor;
    }
    
    return In.Color * Sample * SdfFactor * BorderFactor;
}