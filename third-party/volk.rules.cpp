#include "builder/rules.hpp"

// volk's header; its implementation is compiled in rndr's src/volk-implementation.cpp.
BUILDER_EXTERNAL(Volk, e, t)
{
    e.system_includes = {"."};
}
