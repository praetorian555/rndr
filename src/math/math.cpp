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

#include "rndr/math/math.h"

namespace rndr
{

template <>
bool Point2<int>::HasNaNs() const
{
    return false;
}

template <>
bool Point3<int>::HasNaNs() const
{
    return false;
}

template <>
real Mod(real a, real b)
{
    return std::fmod(a, b);
}

real Radians(real deg)
{
    return (Pi / 180) * deg;
}

real Degrees(real rad)
{
    return (180 / Pi) * rad;
}

real Log2(real x)
{
    const real invLog2 = 1.442695040888963387004650940071;
    return std::log(x) * invLog2;
}

int32_t Log2Int(uint32_t v)
{
#if defined(_MSC_VER)
    unsigned long firstOneIndex = 0;
    _BitScanReverse(&firstOneIndex, v);
    return firstOneIndex;
#else
    return 31 - __builtin_clz(v);
#endif
}

int32_t RoundUpPow2(int32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}

int32_t CountTrailingZeros(uint32_t v)
{
#if defined(_MSC_VER)
    unsigned long firstOneIndex = 0;
    _BitScanReverse(&firstOneIndex, v);
    return 32 - firstOneIndex;
#else
    return __builtin_ctz(v);
#endif
}

}  // namespace rndr