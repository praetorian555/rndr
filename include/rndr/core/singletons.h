#pragma once

#include "rndr/core/base.h"

namespace rndr
{

/**
 * This RAII construct is used to initialize all singletons in constructor and to shut them down in
 * destructor.
 */
struct Singletons
{
    Singletons();
    ~Singletons();
};

}  // namespace rndr