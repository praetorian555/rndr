struct InVertex
{
    float4 Position : POSITION;
    float4 Color : COLOR;

    // Instance data
    float4 RowX : ROWX;
    float4 RowY : ROWY;
    float4 RowZ : ROWZ;
    float4 RowW : ROWW;
};

struct OutVertex
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

cbuffer Constants
{
    row_major matrix FromWorldToNDC;
};

OutVertex Main(InVertex In)
{
    matrix FromModelToWorld = float4x4(In.RowX, In.RowY, In.RowZ, In.RowW);

    OutVertex Out;
    Out.Position = mul(float4(In.Position.xyz, 1.0f), FromModelToWorld);
    Out.Position = mul(Out.Position, FromWorldToNDC);
    Out.Color = In.Color;

    return Out;
}