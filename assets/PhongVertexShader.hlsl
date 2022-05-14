struct VSInput
{
    float3 Position : POSITION;
    float2 TexCoords : TEXCOORD;
    float3 Normal : NORMAL;
    float4 RowX : ROWX;
    float4 RowY : ROWY;
    float4 RowZ : ROWZ;
    float4 RowW : ROWW;
};

struct VSOutput
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

VSOutput PhongVertexShader(VSInput Input)
{
    VSOutput Out;
    matrix FromModelToWorld = float4x4(Input.RowX, Input.RowY, Input.RowZ, Input.RowW);
    Out.PositionWorld = mul(float4(Input.Position, 1.0f), FromModelToWorld);
    Out.Position = mul(Out.PositionWorld, FromWorldToNDC);
    Out.TexCoords = Input.TexCoords;
    Out.Normal = mul(Input.Normal, FromModelToWorld);

    return Out;
}
