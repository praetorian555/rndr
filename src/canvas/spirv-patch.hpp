#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/array-view.h"

#include "rndr/types.hpp"

namespace Rndr::Impl
{

void PatchSpirv(Opal::DynamicArray<u32>& bytecode);

}