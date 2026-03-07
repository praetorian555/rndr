#pragma once

#include "opal/container/string.h"

#include "rndr/math.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

struct MonitorInfo
{
    i32 index = 0;
    Opal::StringUtf8 name;
    Vector2i position;
    Vector2i size;
    Vector2i work_area_position;
    Vector2i work_area_size;
    f32 dpi_scale = 1.0f;
    i32 refresh_rate = 60;
    bool is_primary = false;
};

}  // namespace Rndr
