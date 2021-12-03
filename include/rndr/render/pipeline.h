#pragma once

#include "rndr/render/shader.h"

namespace rndr
{

extern VertexShader g_DefaultVertexShader;
extern PixelShader g_DefaultPixelShader;

enum class DepthTest {
    None, GreaterThan, LesserThen
};

struct Pipeline
{
    rndr::VertexShader* VertexShader = &g_DefaultVertexShader;
    rndr::PixelShader* PixelShader = &g_DefaultPixelShader;

    rndr::DepthTest DepthTest = rndr::DepthTest::None;

    bool bApplyGammaCorrection = false;
    real Gamma = 2.4;
};

}  // namespace rndr