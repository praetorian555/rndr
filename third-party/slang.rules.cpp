#include "builder/rules.hpp"

// The prebuilt Slang release, one archive per host. The library rndr compiles shaders with at run
// time, with every runtime library it loads copied next to the programs, and slangc, the compiler
// rndr's tests compare it with.
BUILDER_EXTERNAL(Slang, e, t)
{
    e.system_includes = {"include"};
    if (t.platform.Is(Platform::Windows))
    {
        e.libs = {"lib/slang.lib"};
        e.runtime_deps = {"bin/slang.dll",         "bin/slang-compiler.dll", "bin/slang-glsl-module.dll",
                          "bin/slang-glslang.dll", "bin/slang-rt.dll",       "bin/slang-llvm.dll"};
    }
    else
    {
        e.libs = {"lib/libslang.so"};
        e.runtime_deps = {"lib/libslang.so", "lib/libslang-glsl-module-2026.10.2.so", "lib/libslang-glslang-2026.10.2.so",
                          "lib/libslang-rt.so", "lib/libslang-llvm.so"};
    }
}

BUILDER_EXTERNAL(Slangc, e, t)
{
    e.executable = t.platform.Is(Platform::Windows) ? "bin/slangc.exe" : "bin/slangc";
}
