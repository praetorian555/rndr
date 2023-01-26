struct InFragment
{
    float4 Positon : SV_POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL0;
    float3 ViewDirection : NORMAL1;
};

cbuffer Constants
{
    row_major matrix FromWorldToNDC;
    float3 CameraPositionWorld;
    float Shininess;
};

float4 Main(InFragment In) : SV_TARGET0
{
    In.Normal = normalize(In.Normal);
    In.ViewDirection = normalize(In.ViewDirection);
    float3 LightDirection = float3(0, 0, 0) - float3(-1, 1, -1);
    LightDirection = normalize(LightDirection);
    float Diffuse = max(dot(In.Normal, -LightDirection), 0);
    float3 DiffuseColor = Diffuse * In.Color.xyz;
    
    float Ambient = 0.02f;
    float3 AmbientColor = Ambient * float3(1, 1, 1);
    
    float3 ReflectionDir = reflect(LightDirection, In.Normal);
    float ShineFactor = dot(ReflectionDir, In.ViewDirection);
    float Specular = pow(max(0.0, ShineFactor), Shininess);
    float3 SpecularColor = 0.2f * Specular * float3(1, 1, 1);
    
    float4 FinalColor = float4(AmbientColor + DiffuseColor + SpecularColor, 1); 
    return saturate(FinalColor);
}