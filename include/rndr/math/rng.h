#pragma once

#include "rndr/core/base.h"

namespace rndr
{

// Largest floating-point constant less then 1
#ifdef RNDR_REAL_AS_DOUBLE
static const real OneMinusEpsilon = 0x1.fffffffffffffp-1;
#else
static const real OneMinusEpsilon = 0x1.fffffep-1;
#endif

// Pseudo-random number generator based on the paper PCG: A Family of Simple Fast
// Space-Efficient Statistically Good Algorithms for Random Number Generation by O'Neill (2014).
class RNG
{
public:
    RNG();
    RNG(uint64_t startingIndex);

    void SetSequence(uint64_t startingIndex);

    // Uniform number in range [0, UINT32_MAX - 1]
    uint32_t UniformUInt32();

    // Uniform number in range [0, limit - 1]
    uint32_t UniformUInt32(uint32_t limit);

    // Uniform number in range [0, 1)
    real UniformReal();

    // Uniform number in range [Start, End)
    real UniformRealInRange(real Start, real End);

private:
    uint64_t state, inc;
};

}  // namespace rndr