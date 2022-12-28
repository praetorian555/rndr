#include "rndr/render/graphicstypes.h"

// Should be kept in line with rndr::PixelFormat
// clang-format off
static int s_PixelSizes[] = {
    4,
    4,
    4,
    4,
    4,
    4,
    4,

    4,

    1,
    1,
    1,
    1,

    16,
    16,
    16,
    
    12,
    12,
    12,

    8,
    8,
    8,

    4,
    4,
    4,

    4,
    2
};
// clang-format on

static_assert((sizeof(s_PixelSizes) / sizeof(int)) ==
              static_cast<size_t>(rndr::PixelFormat::Count));

int rndr::GetPixelSize(PixelFormat Format)
{
    return s_PixelSizes[static_cast<int>(Format)];
}
