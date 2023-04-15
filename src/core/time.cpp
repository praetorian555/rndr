#include "rndr/core/time.h"

#include <chrono>

Rndr::Timestamp Rndr::GetTimestamp()
{
    using namespace std::chrono;
    const auto timestamp = high_resolution_clock::now();
    const auto timestamp_us = duration_cast<microseconds>(timestamp.time_since_epoch());
    const int64_t result_int = timestamp_us.count();
    return result_int;
}

Rndr::real Rndr::GetDuration(Rndr::Timestamp start, Rndr::Timestamp end)
{
    return static_cast<real>(end - start) / MATH_REALC(1'000'000.0);
}

Rndr::real Rndr::GetSystemTime()
{
    return GetDuration(0, GetTimestamp());
}
