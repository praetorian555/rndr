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

#include "rndr/math/rng.h"

#include <algorithm>

namespace rndr
{

#define DEFAULT_STATE 0x853c49e6748fea9bULL
#define DEFAULT_STREAM 0xda3e39cb94b95bdbULL
#define MULT 0x5851f42d4c957f2dULL

RNG::RNG() : state(DEFAULT_STATE), inc(DEFAULT_STREAM) {}

RNG::RNG(uint64_t startingIndex)
{
    SetSequence(startingIndex);
}

void RNG::SetSequence(uint64_t startingIndex)
{
    state = 0u;
    inc = (startingIndex << 1u) | 1u;
    UniformUInt32();
    state += DEFAULT_STATE;
    UniformUInt32();
}

uint32_t RNG::UniformUInt32()
{
    uint64_t oldstate = state;
    state = oldstate * MULT + inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31));
}

uint32_t RNG::UniformUInt32(uint32_t limit)
{
    uint32_t threshold = (~limit + 1u) % limit;
    while (true)
    {
        uint32_t r = UniformUInt32();
        if (r >= threshold)
        {
            return r % limit;
        }
    }
}

real RNG::UniformReal()
{
    return std::min(OneMinusEpsilon, UniformUInt32() * 0x1p-32f);
}

}  // namespace pbr