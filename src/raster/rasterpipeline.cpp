#include "rndr/raster/rasterpipeline.h"

#if defined RNDR_RASTER

rndr::InFragmentInfo& rndr::Triangle::GetFragmentInfo(int X, int Y)
{
    assert(rndr::Inside(math::Point2{X, Y}, Bounds));
    assert(Fragments);

    return Fragments[(X - Bounds.pMin.X) + (Y - Bounds.pMin.Y) * Bounds.Diagonal().X];
}

rndr::math::Vector4rndr::Pipeline::Blend(const Vector4r& Src, const math::Vector4Dst) const
{
    const math::Vector3 SrcColorFactor = GetBlendColorFactor(SrcColorBlendFactor, Src, Dst);
    const math::Vector3 DstColorFactor = GetBlendColorFactor(DstColorBlendFactor, Src, Dst);
    const math::Vector3 SrcColor = Clamp(Src.XYZ() * SrcColorFactor, 0, 1);
    const math::Vector3 DstColor = Clamp(Dst.XYZ() * DstColorFactor, 0, 1);
    const math::Vector3 BlendedColor = PerformBlendOperation(ColorBlendOperator, SrcColor, DstColor);

    const real SrcAlphaFactor = GetBlendAlphaFactor(SrcAlphaBlendFactor, Src, Dst);
    const real DstAlphaFactor = GetBlendAlphaFactor(DstAlphaBlendFactor, Src, Dst);
    const real SrcAlpha = Src.A * SrcAlphaFactor;
    const real DstAlpha = Dst.A * DstAlphaFactor;
    const real BlendedAlpha = PerformBlendOperation(AlphaBlendOperator, SrcAlpha, DstAlpha);

    return Vector4r(BlendedColor, BlendedAlpha);
}

rndr::math::Vector3 rndr::Pipeline::GetBlendColorFactor(BlendFactor FactorName, const Vector4r& Src, const Vector4r& Dst) const
{
    switch (FactorName)
    {
        case BlendFactor::Zero:
            return Vector3r(0, 0, 0);
        case BlendFactor::One:
            return Vector3r(1, 1, 1);
        case BlendFactor::SrcColor:
            return Src.XYZ();
        case BlendFactor::DstColor:
            return Dst.XYZ();
        case BlendFactor::OneMinusSrcColor:
            return Vector3r(1, 1, 1) - Src.XYZ();
        case BlendFactor::OneMinusDstColor:
            return Vector3r(1, 1, 1) - Dst.XYZ();
        case BlendFactor::SrcAlpha:
            return Vector3r(Src.A, Src.A, Src.A);
        case BlendFactor::DstAlpha:
            return Vector3r(Dst.A, Dst.A, Dst.A);
        case BlendFactor::OneMinusSrcAlpha:
            return Vector3r(1 - Src.A, 1 - Src.A, 1 - Src.A);
        case BlendFactor::OneMinusDstAlpha:
            return Vector3r(1 - Dst.A, 1 - Dst.A, 1 - Dst.A);
        case BlendFactor::ConstColor:
            return ConstBlendColor;
        case BlendFactor::OneMinusConstColor:
            return Vector3r(1, 1, 1) - ConstBlendColor;
        case BlendFactor::ConstAlpha:
            return Vector3r(ConstBlendAlpha, ConstBlendAlpha, ConstBlendAlpha);
        case BlendFactor::OneMinusConstAlpha:
            return Vector3r(1 - ConstBlendAlpha, 1 - ConstBlendAlpha, 1 - ConstBlendAlpha);
        default:
            assert(false);
    }

    return Vector3r();
}

real rndr::Pipeline::GetBlendAlphaFactor(BlendFactor FactorName, const Vector4r& Src, const Vector4r& Dst) const
{
    switch (FactorName)
    {
        case BlendFactor::Zero:
            return 0;
        case BlendFactor::One:
            return 1;
        case BlendFactor::SrcAlpha:
            return Src.A;
        case BlendFactor::DstAlpha:
            return Dst.A;
        case BlendFactor::OneMinusSrcAlpha:
            return 1 - Src.A;
        case BlendFactor::OneMinusDstAlpha:
            return 1 - Dst.A;
        case BlendFactor::ConstAlpha:
            return ConstBlendAlpha;
        case BlendFactor::OneMinusConstAlpha:
            return 1 - ConstBlendAlpha;
        case BlendFactor::SrcColor:
        case BlendFactor::DstColor:
        case BlendFactor::OneMinusSrcColor:
        case BlendFactor::OneMinusDstColor:
        case BlendFactor::ConstColor:
        case BlendFactor::OneMinusConstColor:
        default:
            assert(false);
    }

    return 1;
}

rndr::math::Vector3 rndr::Pipeline::PerformBlendOperation(BlendOperator Op, const Vector3r& Src, const Vector3r& Dst) const
{
    math::Vector3 Result;
    switch (Op)
    {
        case BlendOperator::Add:
            Result = Src + Dst;
            break;
        case BlendOperator::Subtract:
            Result = Src - Dst;
            break;
        case BlendOperator::ReverseSubtract:
            Result = Dst - Src;
            break;
        case BlendOperator::Min:
            Result = rndr::Min(Src, Dst);
            break;
        case BlendOperator::Max:
            Result = rndr::Max(Src, Dst);
            break;
        default:
            assert(false);
    }

    Result = Clamp(Result, 0, 1);
    return Result;
}

real rndr::Pipeline::PerformBlendOperation(BlendOperator Op, real Src, real Dst) const
{
    real Result;
    switch (Op)
    {
        case BlendOperator::Add:
            Result = Src + Dst;
            break;
        case BlendOperator::Subtract:
            Result = Src - Dst;
            break;
        case BlendOperator::ReverseSubtract:
            Result = Dst - Src;
            break;
        case BlendOperator::Min:
            Result = std::min(Src, Dst);
            break;
        case BlendOperator::Max:
            Result = std::max(Src, Dst);
            break;
        default:
            assert(false);
    }

    Result = Clamp(Result, 0, 1);
    return Result;
}

bool rndr::Pipeline::DepthTest(real Src, real Dst) const
{
    if (Src < 0 || Src > 1)
    {
        return false;
    }

    switch (DepthTestOperator)
    {
        case DepthTest::Never:
            return false;
        case DepthTest::Always:
            return true;
        case DepthTest::Less:
            return Src < Dst;
        case DepthTest::Greater:
            return Src > Dst;
        case DepthTest::Equal:
            // TODO(mkostic): We should probably setup a proper function for this
            return Src == Dst;
        case DepthTest::NotEqual:
            return Src != Dst;
        case DepthTest::LessEqual:
            return Src <= Dst;
        case DepthTest::GreaterEqual:
            return Src >= Dst;
        default:
            assert(false);
    }

    return false;
}

#endif  // RNDR_RASTER
