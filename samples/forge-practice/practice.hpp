#pragma once

/**
 * What every practice sample shares, and nothing more: stopping when a Forge call fails. Everything else is the
 * sample's to write. The steps, what each exercises and when it counts as done are in the comment at the top of
 * each file.
 *
 * RNDR_PRACTICE_DIR is this directory, for Slang files kept beside the sample rather than in a string.
 */

#include <utility>

#include "opal/assert.h"
#include "opal/container/expected.h"

#include "rndr/error-codes.hpp"

/**
 * Unwrap what a Forge call reported. Forge logs which call failed and why before it hands back a code, so a
 * sample that cannot get through its own setup only has to stop.
 */
template <typename T>
T Require(Opal::Expected<T, Rndr::ErrorCode>&& result)
{
    if (!result.HasValue())
    {
        Opal::HandleContractViolation("A Forge call failed. The log above says which and why.");
    }
    return std::move(result).GetValue();
}

/** The same for a call that reports a code and nothing else. */
inline void RequireOk(Rndr::ErrorCode status)
{
    if (status != Rndr::ErrorCode::Success)
    {
        Opal::HandleContractViolation("A Forge call failed. The log above says which and why.");
    }
}
