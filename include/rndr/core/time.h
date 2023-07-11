#pragma once

#include "rndr/core/base.h"

namespace Rndr
{

using Timestamp = int64_t;

/**
 * @brief Get the current timestamp in microseconds.
 * @return The current timestamp in microseconds.
 */
Timestamp GetTimestamp();

/**
 * @brief Get the duration between two timestamps in seconds.
 * @param start The start timestamp.
 * @param end The end timestamp.
 * @return The duration between the two timestamps in seconds.
 */
double GetDuration(Timestamp start, Timestamp end);

/**
 * @brief Get the current time in seconds.
 * @return The current time in seconds.
 */
double GetSystemTime();

}  // namespace Rndr
