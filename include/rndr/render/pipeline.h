#pragma once

#include "rndr/render/shader.h"

namespace rndr
{

class Image;

struct Pipeline
{
    rndr::WindingOrder WindingOrder = rndr::WindingOrder::CCW;

    std::shared_ptr<rndr::VertexShader> VertexShader;
    std::shared_ptr<rndr::PixelShader> PixelShader;

    rndr::DepthTest DepthTest = rndr::DepthTest::None;

    bool bApplyGammaCorrection = false;
    real Gamma = 2.4;

    rndr::Image* ColorImage = nullptr;
    rndr::Image* DepthImage = nullptr;
};

}  // namespace rndr