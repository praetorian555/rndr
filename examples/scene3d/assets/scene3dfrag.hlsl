struct InFragment
{
    float4 Positon : SV_POSITION;
    float4 Color : COLOR;
};

float4 Main(InFragment In) : SV_TARGET0
{
    return In.Color;
}