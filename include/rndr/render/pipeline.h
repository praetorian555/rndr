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

    real Gamma = RNDR_GAMMA;

    rndr::Image* ColorImage = nullptr;
    rndr::Image* DepthImage = nullptr;
};

}  // namespace rndr