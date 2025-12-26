#pragma once

#include "opal/exceptions.h"

#include "rndr/types.hpp"

namespace Rndr
{

struct GraphicsAPIException : Opal::Exception
{
    explicit GraphicsAPIException(u32 error_code, const char* message) : Opal::Exception(Opal::StringEx("Graphics API error: ") + static_cast<u64>(error_code) + " - " + message)
    {
    }
};

}  // namespace Rndr