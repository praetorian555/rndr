struct InstanceData
{
    float2 BottomLeft : POSITION0;
    float2 TopRight : POSITION1;
    float2 TexCoordsBottomLeft : TEXCOORD0;
    float2 TexCoordsTopRight : TEXCOORD1;
    float4 Color : COLOR;
    float AtlasIndex : BLENDINDICES;
    uint VertexIndex : SV_VertexID;
};

struct OutData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoords : TEXCOORD;
    float AtlasIndex : BLENDINDICES;
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
    
    static float2 DefaultTexCoords[] =
    {
        {   0,   0 },
        { 1.0,   0 },
        {   0, 1.0 },
        { 1.0, 1.0 }
    };
    
    float2 HalfSize = (In.TopRight - In.BottomLeft) / 2;
    float2 Center = In.BottomLeft + HalfSize;
    float2 TexCoordsHalfSize = (In.TexCoordsTopRight - In.TexCoordsBottomLeft) / 2;
    float2 TexCoordsCenter = In.TexCoordsBottomLeft + TexCoordsHalfSize;
    
    float2 ScreenPosition = DefaultPositions[In.VertexIndex] * HalfSize + Center;
    float2 TexCoords = DefaultTexCoords[In.VertexIndex] * TexCoordsHalfSize + TexCoordsCenter;
    OutData Out;
    Out.Position = float4(
        2 * ScreenPosition.x / ScreenSize.x - 1,
        2 * ScreenPosition.y / ScreenSize.y - 1,
        0,
        1
    );
    Out.Color = In.Color;
    Out.TexCoords = TexCoords;
    Out.AtlasIndex = In.AtlasIndex;
    
    return Out;
}