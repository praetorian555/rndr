struct InFragment
{
    float4 Positon : SV_POSITION;
    float3 PositionWorld : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoords : TEXCOORD;
};

cbuffer Constants
{
    float3 ViewerPosition;
    float3 LightDirection;
    float3 LightColor;
    float AmbientStrength;
    float Shininess;
};

Texture2D DiffuseTexture;
Texture2D NormalTexture;
Texture2D SpecularTexture;
SamplerState Sampler;

float4 VectorToColor(float3 Vec)
{
    float3 Normalized = normalize(Vec);
    float3 Color = (Normalized + 1) / 2;
    return saturate(float4(Color, 1));
}

float4 MaterialFragmentShader(InFragment In) : SV_TARGET0
{
    float3 DiffuseColor = DiffuseTexture.Sample(Sampler, In.TexCoords);
    float DiffuseStrength = dot(In.Normal, -LightDirection);
    DiffuseStrength = max(DiffuseStrength, 0);
    float3 SpecularColor = SpecularTexture.Sample(Sampler, In.TexCoords);
    float3 ViewerDirection = normalize(ViewerPosition - In.PositionWorld);
    float3 HalfwayDirection = normalize(ViewerDirection - LightDirection);
    float SpecularStrength = pow(max(dot(In.Normal, HalfwayDirection), 0), Shininess);
    float3 OutColor = (AmbientStrength + DiffuseStrength) * DiffuseColor + SpecularStrength * SpecularColor;
    return saturate(float4(OutColor * LightColor, 1));
}
