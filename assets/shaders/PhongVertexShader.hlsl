struct Input
{
    float3 Position : POSITION;
    float2 TexCoords : TEXCOORD;
    float3 Normal : NORMAL;
    // Instance matrix - model to world
    float4 RowX : ROWX;
    float4 RowY : ROWY;
    float4 RowZ : ROWZ;
    float4 RowW : ROWW;
};

struct Output
{
    float4 Position : SV_POSITION;
    float4 PositionWorld : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoords : TEXCOORD;
};

cbuffer Constants
{
    row_major matrix ProjectionMatrix;
};

Output PhongVertexShader(Input In)
{
    Output Out;
    // Extract per instance model to world matrix
    matrix ModelToWorld = float4x4(In.RowX, In.RowY, In.RowZ, In.RowW);
    Out.PositionWorld = mul(float4(In.Position, 1.0f), ModelToWorld);
    Out.Position = mul(Out.PositionWorld, ProjectionMatrix);
    Out.TexCoords = In.TexCoords;
    Out.Normal = mul(In.Normal, ModelToWorld);

    return Out;
}
