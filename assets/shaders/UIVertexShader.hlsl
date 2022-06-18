struct InstanceData
{
    float2 Position : POSITION0;
    float2 BottomLeft : POSITION1;
    float2 TopRight : POSITION2;
    float4 Color : COLOR;
};

struct OutData
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

cbuffer Constants
{
    float2 ScreenSize;
};

OutData Main(InstanceData In)
{
    float2 HalfSize = (In.TopRight - In.BottomLeft) / 2;
    float2 Center = In.BottomLeft + HalfSize;
    
    float2 ScreenPosition = In.Position * HalfSize + Center;
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