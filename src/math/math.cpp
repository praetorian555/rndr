#include "rndr/core/math.h"

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