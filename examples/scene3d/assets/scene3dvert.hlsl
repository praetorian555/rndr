struct InVertex
{
    float4 Position : POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;

    // Instance world transform
    float4 RowX : ROWX;
    float4 RowY : ROWY;
    float4 RowZ : ROWZ;
    float4 RowW : ROWW;

    // Normal transform
    float4 NormalRowX : ROWX;
    float4 NormalRowY : ROWY;
    float4 NormalRowZ : ROWZ;
    float4 NormalRowW : ROWW;
};

struct OutVertex
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float3 Normal : NORMAL;
};

cbuffer Constants
{
    row_major matrix FromWorldToNDC;
};

OutVertex Main(InVertex In)
{
    matrix FromModelToWorld = float4x4(In.RowX, In.RowY, In.RowZ, In.RowW);
    matrix NormalTransform = float4x4(In.NormalRowX, In.NormalRowY, In.NormalRowZ, In.NormalRowW);

    OutVertex Out;
    Out.Position = mul(float4(In.Position.xyz, 1.0f), FromModelToWorld);
    Out.Position = mul(Out.Position, FromWorldToNDC);
    Out.Color = In.Color;
    Out.Normal = mul(In.Normal, (float3x3) NormalTransform);
    Out.Normal = normalize(Out.Normal);

    return Out;
}