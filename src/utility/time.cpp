#include "rndr/utility/time.h"

#include <chrono>

rndr::Timestamp rndr::GetTimestamp()
{
    using namespace std::chrono;
    const auto timestamp = high_resolution_clock::now();
    const auto timestamp_us = duration_cast<microseconds>(timestamp.time_since_epoch());
    const int64_t result_int = timestamp_us.count();
    return result_int;
}

real GetDuration(rndr::Timestamp start, rndr::Timestamp end)
{
    return static_cast<real>(end - start) / MATH_REALC(1'000'000.0);
}

real rndr::GetSystemTime()
{
    return GetDuration(0, GetTimestamp());
}
