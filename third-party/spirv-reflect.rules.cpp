#include "builder/rules.hpp"

// SPIRV-Reflect at the tag of the Vulkan SDK rndr builds against: one C source, Khronos code rather
// than rndr's, so it builds with warnings off.
BUILDER_MODULE(SpirvReflect, m, t)
{
    m.public_system_includes = {"."};
    m.sources.Add("spirv_reflect.c");
    m.headers.Add("spirv_reflect.h");
    m.warnings = Warnings::Off;
}
