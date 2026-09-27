#pragma once

#include "opal/container/optional.h"

#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"

using VmaAllocation = struct VmaAllocation_T*;

namespace Rndr::Forge
{

/**
 * The memory type and heap an allocation came from, read off the allocator and the physical device. Empty for a
 * null allocation, which is what an empty object and a wrapped native image hold. Buffer and Texture both answer
 * GetMemoryInfo with it.
 */
[[nodiscard]] Opal::Optional<MemoryInfo> DescribeAllocation(const Device& device, VmaAllocation allocation);

}  // namespace Rndr::Forge
