#pragma once

#include "rndr/render/shader.h"

namespace rndr
{

class Image;

enum class BlendFactor
{
    Zero,
    One,
    SrcColor,
    DstColor,
    OneMinusSrcColor,
    OneMinusDstColor,
    SrcAlpha,
    DstAlpha,
    OneMinusSrcAlpha,
    OneMinusDstAlpha,
    ConstColor,
    OneMinusConstColor,
    ConstAlpha,
    OneMinusConstAlpha
};

enum class BlendOperator
{
    Add,
    Subtract, // Source - Destination
    ReverseSubtract, // Destination - Source
    Min,
    Max
};

struct Pipeline
{
    rndr::WindingOrder WindingOrder = rndr::WindingOrder::CCW;

    std::shared_ptr<rndr::VertexShader> VertexShader;
    std::shared_ptr<rndr::FragmentShader> FragmentShader;

    BlendFactor SrcColorBlendFactor;
    BlendFactor DstColorBlendFactor;
    BlendFactor SrcAlphaBlendFactor;
    BlendFactor DstAlphaBlendFactor;
    BlendOperator ColorBlendOperator;
    BlendOperator AlphaBlendOperator;
    Vector3r ConstBlendColor;
    real ConstBlendAlpha;

    rndr::DepthTest DepthTestOperator;
    real MinDepth;
    real MaxDepth;

    real Gamma = RNDR_GAMMA;

    rndr::Image* ColorImage = nullptr;
    rndr::Image* DepthImage = nullptr;

    // Helper functions.
    Vector4r Blend(const Vector4r& Src, const Vector4r Dst) const;
    bool DepthTest(real Src, real Dst) const;

private:
    Vector3r GetBlendColorFactor(BlendFactor FactorName, const Vector4r& Src, const Vector4r& Dst) const;
    real GetBlendAlphaFactor(BlendFactor FactorName, const Vector4r& Src, const Vector4r& Dst) const;
    Vector3r PerformBlendOperation(BlendOperator Op, const Vector3r& Src, const Vector3r& Dst) const;
    real PerformBlendOperation(BlendOperator Op, real SrcAlpha, real DstAlpha) const;
};

}  // namespace rndr