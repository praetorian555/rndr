cbuffer PerApplication : register( b0 )
{
    matrix ProjectionMatrix;
}

cbuffer PerFrame : register( b1 )
{
    matrix ViewMatrix;
}

cbuffer PerObject : register( b2 )
{
    matrix WorldMatrix;
}

struct AppData
{
    float3 Position : POSITION;
    float3 Color: COLOR;
};

struct VertexShaderOutput
{
    float4 Color : COLOR;
    float4 Position : SV_POSITION;
};

VertexShaderOutput SimpleVertexShader(AppData IN)
{
    VertexShaderOutput OUT;
    matrix MVP = mul( ProjectionMatrix, mul( ViewMatrix, WorldMatrix ) );
    OUT.Position = mul( MVP, float4( IN.Position, 1.0f ) );
    OUT.Color = float4( IN.Color, 1.0f );
    return OUT;
}
