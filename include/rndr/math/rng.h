// MIT License
//
// Copyright (c) 2021 Marko Kostić
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstdint>

#include "rndr/rndr.h"

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

private:
    uint64_t state, inc;
};

}  // namespace pbr