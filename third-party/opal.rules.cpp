#include "builder/rules.hpp"

// opal 0.8.0, the release rndr pins, which predates opal's own rules file. The library only; opal's
// tests are opal's build. Warnings follow opal's CMake: strict, as errors.
BUILDER_MODULE(Opal, m, t)
{
    m.cxx_standard = 20;
    m.public_includes = {"include", "third-party/rapidhash/include"};
    m.sources.AddDir("src");
    m.headers.AddDir("include", Recurse::Yes);
    m.warnings = Warnings::Strict;
    m.warnings_as_errors = true;
    if (t.platform.Is(Platform::Windows))
    {
        m.libs = {"synchronization.lib", "ws2_32.lib"};
    }
    if (t.toolchain.Is(Compiler::Msvc))
    {
        m.private_defines = {"UNICODE", "_UNICODE"};
    }
}
