#include <utility>

#include <catch2/catch2.hpp>

#include "rndr/application.hpp"
#include "rndr/generic-window.hpp"

#if RNDR_WINDOWS
#include "rndr/platform/windows-window.hpp"
#endif

using namespace Rndr;

namespace
{
// A window that never shows, so the suite does not steal focus from whoever is using the machine.
Opal::Ref<GenericWindow> CreateHiddenWindow(Application& app)
{
    return app.CreateGenericWindow({.width = 64,
                                    .height = 64,
                                    .name = "Window test",
                                    .resizable = false,
                                    .has_title_bar = false,
                                    .has_border = false,
                                    .show_in_taskbar = false,
                                    .start_visible = false})
        .GetValue();
}
}  // namespace

TEST_CASE("A window keeps the cursor shape it is given", "[window]")
{
    Opal::ScopePtr<Application> app = Application::Create({}).GetValue();
    Opal::Ref<GenericWindow> window = CreateHiddenWindow(*app);

    REQUIRE(window->GetCursorShape() == CursorShape::Arrow);

    window->SetCursorShape(CursorShape::IBeam);
    REQUIRE(window->GetCursorShape() == CursorShape::IBeam);

    window->SetCursorShape(CursorShape::ResizeHorizontal);
    REQUIRE(window->GetCursorShape() == CursorShape::ResizeHorizontal);

    // Count is not a shape: the window keeps the one it had.
    window->SetCursorShape(CursorShape::Count);
    REQUIRE(window->GetCursorShape() == CursorShape::ResizeHorizontal);

    app->DestroyGenericWindow(std::move(window));
}

#if RNDR_WINDOWS
TEST_CASE("Every cursor shape maps to a system cursor", "[window]")
{
    for (u8 shape = 0; shape <= static_cast<u8>(CursorShape::Count); ++shape)
    {
        INFO("shape " << static_cast<int>(shape));
        REQUIRE(GetSystemCursor(static_cast<CursorShape>(shape)) != nullptr);
    }
    // Count falls back to the arrow.
    REQUIRE(GetSystemCursor(CursorShape::Count) == GetSystemCursor(CursorShape::Arrow));
    REQUIRE(GetSystemCursor(CursorShape::IBeam) != GetSystemCursor(CursorShape::Arrow));
}
#endif
