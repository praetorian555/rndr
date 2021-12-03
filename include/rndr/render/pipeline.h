#pragma once

#include "rndr/render/shader.h"

namespace rndr
{

enum class DepthTest
{
    None,
    GreaterThan,
    LesserThen
};

enum class WindingOrder : int
{
    CW = -1,
    CCW = 1
};

struct Pipeline
{
    rndr::WindingOrder WindingOrder = rndr::WindingOrder::CCW;

    rndr::VertexShader* VertexShader = nullptr;
    rndr::PixelShader* PixelShader = nullptr;

    rndr::DepthTest DepthTest = rndr::DepthTest::None;

    bool bApplyGammaCorrection = false;
    real Gamma = 2.4;
};

}  // namespace rndr