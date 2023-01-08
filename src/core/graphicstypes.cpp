#include "rndr/render/graphicstypes.h"

#include "rndr/utility/stackarray.h"

// Should be kept in line with rndr::PixelFormat
// clang-format off

static constexpr size_t kPixelSizesCount = static_cast<size_t>(rndr::PixelFormat::Count);
static const rndr::StackArray<int, kPixelSizesCount> GPixelSizes
{
    // clang-tidy off
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
    2,
    // clang-tidy on
};
// clang-format on

int rndr::GetPixelSize(PixelFormat Format)
{
    const int Index = static_cast<int>(Format);
    assert(Index >= 0 && Index < GPixelSizes.size());
    return GPixelSizes[Index];
}
