struct InstanceData
{
    float2 BottomLeft : POSITION0;
    float2 TopRight : POSITION1;
    float2 TexBottomLeft : TEXCOORD0;
    float2 TexTopRight : TEXCOORD1;
    float4 Color : COLOR;
    float SDFThresholdBottom : PSIZE0;
    float SDFThresholdTop : PSIZE1;
    uint VertexIndex : SV_VERTEXID;
};

struct OutData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 TexCoords : TEXCOORD0;
    float SDFThresholdBottom : PSIZE0;
    float SDFThresholdTop : PSIZE1;
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
    
    // Since the image data is stored from top to bottom and from left to right
    // we had to modify how UV coordinates map to our rectagle that goes from left to right
    // and from bottom to the top.
    static float2 DefaultTexCoords[] =
    {
        {   0, 1.0 },
        { 1.0, 1.0 },
        {   0,   0 },
        { 1.0,   0 }
    };
    
    float2 TexCoordsSize = (In.TexTopRight - In.TexBottomLeft);
    float2 TexCoords = In.TexBottomLeft + DefaultTexCoords[In.VertexIndex] * TexCoordsSize;
    
    OutData Out;
    Out.Position = float4(
        2 * ScreenPosition.x / ScreenSize.x - 1,
        2 * ScreenPosition.y / ScreenSize.y - 1,
        0,
        1
    );
    Out.Color = In.Color;
    Out.TexCoords = TexCoords;
    Out.SDFThresholdBottom = In.SDFThresholdBottom;
    Out.SDFThresholdTop = In.SDFThresholdTop;

    return Out;
}
