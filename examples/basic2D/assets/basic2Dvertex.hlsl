struct InstanceData
{
    float2 BottomLeft : POSITION0;
    float2 TopRight : POSITION1;
    float2 TexBottomLeft : TEXCOORD0;
    float2 TexTopRight : TEXCOORD1;
    float4 Color : COLOR;
    float AtlasIndex : PSIZE;
    uint VertexIndex : SV_VERTEXID;
};

struct OutData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoords : TEXCOORD0;
    float AtlasIndex : PSIZE;
};

cbuffer Constants
{
    float2 ScreenSize;
};

OutData Main(InstanceData In)
{
    static float2 DefaultPositions[] =
    {
        {-1.0, -1.0 },
        { 1.0, -1.0 },
        {-1.0,  1.0 },
        { 1.0,  1.0 }
    };

    float2 HalfSize = (In.TopRight - In.BottomLeft) / 2;
    float2 Center = In.BottomLeft + HalfSize;
    float2 ScreenPosition = DefaultPositions[In.VertexIndex] * HalfSize + Center;
    

    OutData Out;
    Out.Position = float4(
        2 * ScreenPosition.x / ScreenSize.x - 1,
        2 * ScreenPosition.y / ScreenSize.y - 1,
        0,
        1
    );
    Out.Color = In.Color;

    return Out;
}
