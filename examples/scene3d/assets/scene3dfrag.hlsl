struct InFragment
{
    float4 Positon : SV_POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
};

float4 Main(InFragment In) : SV_TARGET0
{
    In.Normal = normalize(In.Normal);
    float3 LightDirection = float3(0, 0, 0) - float3(-1, 1, -1);
    LightDirection = normalize(LightDirection);
    float Ambient = 0.04f;
    float Diffuse = max(dot(In.Normal, -LightDirection), 0);

    float4 FinalColor = float4(In.Color.xyz * (Ambient + Diffuse), 1); 
    return saturate(FinalColor);
}