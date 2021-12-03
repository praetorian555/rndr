#pragma once

#include "rndr/render/shader.h"

namespace rndr
{

enum class DepthTest {
    None, GreaterThan, LesserThen
};

struct Pipeline
{
    rndr::VertexShader* VertexShader = nullptr;
    rndr::PixelShader* PixelShader = nullptr;

    rndr::DepthTest DepthTest = rndr::DepthTest::None;

    bool bApplyGammaCorrection = false;
    real Gamma = 2.4;
};

}  // namespace rndr