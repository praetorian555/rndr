struct InVertex
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BINORMAL;
    float2 TexCoords : TEXCOORD;

    // Instance data
    float4 RowX : ROWX;
    float4 RowY : ROWY;
    float4 RowZ : ROWZ;
    float4 RowW : ROWW;
    float4 InvRowX : ROWX;
    float4 InvRowY : ROWY;
    float4 InvRowZ : ROWZ;
    float4 InvRowW : ROWW;
};

struct OutVertex
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoords : TEXCOORD;
};

cbuffer Constants
{
    row_major matrix FromWorldToNDC;
};

OutVertex MaterialVertexShader(InVertex In)
{
    matrix FromModelToWorld = float4x4(In.RowX, In.RowY, In.RowZ, In.RowW);
    matrix NormalTransform = float4x4(In.InvRowX, In.InvRowY, In.InvRowZ, In.InvRowW);

    OutVertex Out;
    Out.Position = mul(float4(In.Position, 1.0f), FromModelToWorld);
    Out.Position = mul(Out.Position, FromWorldToNDC);
    Out.Normal = mul(In.Normal, (float3x3) NormalTransform);
    Out.Normal = normalize(Out.Normal);
    Out.TexCoords = In.TexCoords;

    return Out;
}
