#include "builder/rules.hpp"

// Dear ImGui 1.91.9b with the backends rndr uses, as rndr's CMake builds it: the core, the OpenGL 3
// renderer and, on Windows, the Win32 platform backend.
BUILDER_MODULE(Imgui, m, t)
{
    m.public_includes = {"."};
    for (const char* source : {"imgui.cpp", "imgui_draw.cpp", "imgui_demo.cpp", "imgui_tables.cpp", "imgui_widgets.cpp",
                               "backends/imgui_impl_opengl3.cpp"})
    {
        m.sources.Add(source);
    }
    m.headers.Add("imgui.h");
    m.headers.Add("imconfig.h");
    m.headers.Add("imgui_internal.h");
    m.headers.Add("backends/imgui_impl_opengl3.h");
    if (t.platform.Is(Platform::Windows))
    {
        m.sources.Add("backends/imgui_impl_win32.cpp");
        m.headers.Add("backends/imgui_impl_win32.h");
    }
}
