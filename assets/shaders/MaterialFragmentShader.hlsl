struct InFragment
{
    float4 Positon : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoords : TEXCOORD;
};

cbuffer Constants
{
    float3 LightDirection;
    float3 LightColor;
    float AmbientStrength;
};

Texture2D DiffuseTexture;
Texture2D NormalTexture;
Texture2D SpecularTexture;
SamplerState Sampler;

float4 MaterialFragmentShader(InFragment In) : SV_TARGET0
{
    float4 OutColor;
    OutColor = DiffuseTexture.Sample(Sampler, In.TexCoords);
    float DiffuseStrength = dot(In.Normal, -LightDirection);
    DiffuseStrength = saturate(DiffuseStrength);
    float3 FinalLightColor = (AmbientStrength + DiffuseStrength) * LightColor;
    return OutColor * float4(FinalLightColor, 1);
}
