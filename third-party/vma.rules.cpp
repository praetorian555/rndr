#include "builder/rules.hpp"

// The Vulkan Memory Allocator's header; its implementation is compiled in rndr's src/vma-implementation.cpp.
BUILDER_EXTERNAL(Vma, e, t)
{
    e.system_includes = {"include"};
}
