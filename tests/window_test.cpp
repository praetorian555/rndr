#include <catch2/catch_test_macros.hpp>

#include "rndr/core/window.h"

TEST_CASE("Creating a window", "[window]")
{
    REQUIRE(rndr::Create());
    SECTION("Default window")
    {
        rndr::Window* window = rndr::Window::Create();
        REQUIRE(window != nullptr);
        window->ProcessEvents();
        REQUIRE(!window->IsClosed());
        REQUIRE(window->GetCursorMode() == rndr::CursorMode::Normal);
        REQUIRE(window->GetWidth() == 1024);
        REQUIRE(window->GetHeight() == 768);
        REQUIRE(!window->IsWindowMaximized());
        REQUIRE(!window->IsWindowMinimized());
        REQUIRE(window->IsVisible());
        SECTION("Make minimized")
        {
            window->SetMinimized(true);
            window->ProcessEvents();
            REQUIRE(window->IsWindowMinimized());
            REQUIRE(!window->IsWindowMaximized());
            REQUIRE(!window->IsVisible());
        }
        rndr::Window::Destroy(*window);
    }
    SECTION("Hidden window")
    {
        rndr::Window* window = rndr::Window::Create(rndr::WindowDesc{.start_visible = false});
        REQUIRE(window != nullptr);
        window->ProcessEvents();
        REQUIRE(!window->IsClosed());
        REQUIRE(window->GetCursorMode() == rndr::CursorMode::Normal);
        REQUIRE(window->GetWidth() == 1024);
        REQUIRE(window->GetHeight() == 768);
        REQUIRE(!window->IsWindowMaximized());
        REQUIRE(!window->IsWindowMinimized());
        REQUIRE(!window->IsVisible());
        rndr::Window::Destroy(*window);
    }
    SECTION("Minimized window")
    {
        rndr::Window* window = rndr::Window::Create(rndr::WindowDesc{.start_minimized = true});
        REQUIRE(window != nullptr);
        window->ProcessEvents();
        REQUIRE(!window->IsClosed());
        REQUIRE(window->GetCursorMode() == rndr::CursorMode::Normal);
        REQUIRE(window->GetWidth() == 1024);
        REQUIRE(window->GetHeight() == 768);
        REQUIRE(!window->IsWindowMaximized());
        REQUIRE(window->IsWindowMinimized());
        REQUIRE(!window->IsVisible());
        rndr::Window::Destroy(*window);
    }
    SECTION("Maximized window")
    {
        rndr::Window* window = rndr::Window::Create(rndr::WindowDesc{.start_maximized = true});
        REQUIRE(window != nullptr);
        window->ProcessEvents();
        REQUIRE(!window->IsClosed());
        REQUIRE(window->GetCursorMode() == rndr::CursorMode::Normal);
        REQUIRE(window->GetWidth() == 1024);
        REQUIRE(window->GetHeight() == 768);
        REQUIRE(window->IsWindowMaximized());
        REQUIRE(!window->IsWindowMinimized());
        REQUIRE(window->IsVisible());
        rndr::Window::Destroy(*window);
    }
    SECTION("User controlled window")
    {
        rndr::Window* window = rndr::Window::Create();
        REQUIRE(window != nullptr);
        while (!window->IsClosed())
        {
            window->ProcessEvents();
        }
        rndr::Window::Destroy(*window);
    }
    REQUIRE(rndr::Destroy());
}