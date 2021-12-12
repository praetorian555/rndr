#include "rndr/core/rndr.h"

#include <cmath>

#include "Windows.h"

static bool g_bTableSetup = false;
static constexpr int g_TableSize = 100'001;
static real g_GammaCorrectTable[g_TableSize];
static real g_LinearTable[g_TableSize];

void rndr::SetupGammaTable(real Gamma)
{
    const real OneOverGamma = 1 / Gamma;
    for (int i = 0; i < g_TableSize; i++)
    {
        const real Value = i / (real)100'000;

        if (i <= 313)
        {
            g_GammaCorrectTable[i] = Value * 12.92;
            continue;
        }

#if !defined(RNDR_REAL_AS_DOUBLE)
        g_GammaCorrectTable[i] = 1.055 * std::powf(Value, OneOverGamma) - 0.055;
#else
        g_GammaCorrectTable[i] = 1.055 * std::pow(Value, OneOverGamma) - 0.055;
#endif
    }

    for (int i = 0; i < g_TableSize; i++)
    {
        const real Value = i / (real)100'000;

        if (i <= 4045)
        {
            g_LinearTable[i] = Value / 12.92;
            continue;
        }

        const real Tmp = (Value + 0.055) / 1.055;

#if !defined(RNDR_REAL_AS_DOUBLE)
        g_LinearTable[i] = std::powf(Tmp, Gamma);
#else
        g_LinearTable[i] = std::pow(Tmp, Gamma);
#endif
    }

    g_bTableSetup = true;
}

real rndr::ToGammaCorrectSpace(real Value, real Gamma)
{
    assert(g_bTableSetup);
    const int Index = std::round(Value * 100'000);

    if (Index <= 0)
    {
        return 0;
    }

    if (Index >= g_TableSize - 1)
    {
        return 1;
    }

    return g_GammaCorrectTable[Index];
}

real rndr::ToLinearSpace(real Value, real Gamma)
{
    assert(g_bTableSetup);
    const int Index = std::round(Value * 100'000);

    if (Index <= 0)
    {
        return 0;
    }

    if (Index >= g_TableSize - 1)
    {
        return 1;
    }

    return g_LinearTable[Index];
}

int rndr::GetPixelSize(PixelLayout Layout)
{
    switch (Layout)
    {
        case PixelLayout::A8R8G8B8:
        {
            return 4;
        }
        default:
        {
            assert(false);
        }
    }

    return 0;
}

void rndr_private::DebugBreak()
{
    ::DebugBreak();
}
