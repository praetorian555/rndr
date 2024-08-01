#pragma once

#include "opal/error-codes.h"

#include "rndr/core/types.h"

namespace Rndr
{

enum class ErrorCode : u8
{
    Success = static_cast<u8>(Opal::ErrorCode::Success),
    OutOfBounds = static_cast<u8>(Opal::ErrorCode::OutOfBounds),
    OutOfMemory = static_cast<u8>(Opal::ErrorCode::OutOfMemory),
    InvalidArgument = static_cast<u8>(Opal::ErrorCode::BadInput),
    GraphicsAPIError,
};

}  // namespace Rndr
