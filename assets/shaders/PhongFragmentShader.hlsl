struct FSInput
{
    float4 Position : SV_POSITION;
    float4 PositionWorld : POSITION;
    float2 TexCoords : TEXCOORD;
    float3 Normal : NORMAL;
};

cbuffer Camera
{
    matrix FromWorldToNDC;
    float3 LightPosition;
    float3 ViewPosition;
};

Texture2D Texture;
SamplerState Sampler;

float4 PhongFragmentShader(FSInput Input) : SV_TARGET0
{
    float4 TextureColor;
    TextureColor = Texture.Sample(Sampler, Input.TexCoords);
    return TextureColor;
}
