#pragma once

#include <string>

#include "rndr/core/definitions.h"

namespace rndr
{
/**
 * A dynamic string allocated on the heap using ASCII characters.
 */
using String = std::basic_string<char, std::char_traits<char>>;
}  // namespace rndr
