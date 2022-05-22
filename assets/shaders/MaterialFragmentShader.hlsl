struct InFragment
{
    float4 Positon : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoords : TEXCOORD;
};

cbuffer Constants
{
    float4 LightDirection;
    float4 LightColor;
};

Texture2D DiffuseTexture;
Texture2D NormalTexture;
Texture2D SpecularTexture;
SamplerState Sampler;

float4 MaterialFragmentShader(InFragment In) : SV_TARGET0
{
    float4 OutColor;
    OutColor = DiffuseTexture.Sample(Sampler, In.TexCoords);
    return OutColor;
}
