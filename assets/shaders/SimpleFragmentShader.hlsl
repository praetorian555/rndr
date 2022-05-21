struct FragmentShaderInput
{
    float4 Color : COLOR;
};

float4 SimpleFragmentShader( FragmentShaderInput IN ) : SV_TARGET
{
    return IN.Color;
}
