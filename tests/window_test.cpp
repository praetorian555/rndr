#include <catch2/catch_test_macros.hpp>

#include "rndr/core/window.h"

TEST_CASE("Creating a window", "[window]")
{
    REQUIRE(rndr::Create());
    SECTION("Default window")
    {
        rndr::Window* window = rndr::Window::Create();
        REQUIRE(rndr::Window::IsValid(*window));
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
        SECTION("Make maximized")
        {
            window->SetMaximized(true);
            window->ProcessEvents();
            REQUIRE(!window->IsWindowMinimized());
            REQUIRE(window->IsWindowMaximized());
            REQUIRE(window->IsVisible());
        }
        SECTION("Make hidden")
        {
            window->SetVisible(false);
            window->ProcessEvents();
            REQUIRE(!window->IsWindowMinimized());
            REQUIRE(!window->IsWindowMaximized());
            REQUIRE(!window->IsVisible());
        }
        rndr::Window::Destroy(*window);
    }
    SECTION("Hidden window")
    {
        rndr::Window* window = rndr::Window::Create(rndr::WindowDesc{.start_visible = false});
        REQUIRE(rndr::Window::IsValid(*window));
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
        REQUIRE(rndr::Window::IsValid(*window));
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
        REQUIRE(rndr::Window::IsValid(*window));
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
    REQUIRE(rndr::Destroy());
}

TEST_CASE("Resizing the window", "[window]")
{
    REQUIRE(rndr::Create());
    rndr::Window* window = rndr::Window::Create();
    REQUIRE(rndr::Window::IsValid(*window));
    window->ProcessEvents();
    REQUIRE(!window->IsClosed());
    REQUIRE(window->GetCursorMode() == rndr::CursorMode::Normal);
    REQUIRE(window->GetWidth() == 1024);
    REQUIRE(window->GetHeight() == 768);
    REQUIRE(!window->IsWindowMaximized());
    REQUIRE(!window->IsWindowMinimized());
    REQUIRE(window->IsVisible());
    SECTION("Resize to 800x600")
    {
        window->Resize(800, 600);
        window->ProcessEvents();
        REQUIRE(window->GetWidth() == 800);
        REQUIRE(window->GetHeight() == 600);
        REQUIRE(window->GetSize() == math::Vector2(800, 600));
    }
    SECTION("Resize to 1280x720")
    {
        window->Resize(1280, 720);
        window->ProcessEvents();
        REQUIRE(window->GetWidth() == 1280);
        REQUIRE(window->GetHeight() == 720);
        REQUIRE(window->GetSize() == math::Vector2(1280, 720));
    }
    SECTION("Resize to invalid")
    {
        window->Resize(0, 0);
        window->ProcessEvents();
        REQUIRE(window->GetWidth() == 1024);
        REQUIRE(window->GetHeight() == 768);
        REQUIRE(window->GetSize() == math::Vector2(1024, 768));
    }
    rndr::Window::Destroy(*window);
    REQUIRE(rndr::Destroy());
}

TEST_CASE("Closing the window", "[window]")
{
    REQUIRE(rndr::Create());
    rndr::Window* window = rndr::Window::Create();
    REQUIRE(rndr::Window::IsValid(*window));
    window->ProcessEvents();
    REQUIRE(!window->IsClosed());
    REQUIRE(window->GetCursorMode() == rndr::CursorMode::Normal);
    REQUIRE(window->GetWidth() == 1024);
    REQUIRE(window->GetHeight() == 768);
    REQUIRE(!window->IsWindowMaximized());
    REQUIRE(!window->IsWindowMinimized());
    REQUIRE(window->IsVisible());
    window->Close();
    window->ProcessEvents();
    REQUIRE(window->IsClosed());
    rndr::Window::Destroy(*window);
    REQUIRE(rndr::Destroy());
}

TEST_CASE("Changing cursor mode", "[window]")
{
    REQUIRE(rndr::Create());
    rndr::Window* window = rndr::Window::Create();
    REQUIRE(rndr::Window::IsValid(*window));
    window->ProcessEvents();
    REQUIRE(!window->IsClosed());
    REQUIRE(window->GetCursorMode() == rndr::CursorMode::Normal);
    REQUIRE(window->GetWidth() == 1024);
    REQUIRE(window->GetHeight() == 768);
    REQUIRE(!window->IsWindowMaximized());
    REQUIRE(!window->IsWindowMinimized());
    REQUIRE(window->IsVisible());
    SECTION("Set to hidden")
    {
        window->SetCursorMode(rndr::CursorMode::Hidden);
        window->ProcessEvents();
        REQUIRE(window->GetCursorMode() == rndr::CursorMode::Hidden);
    }
    SECTION("Set to infinite")
    {
        window->SetCursorMode(rndr::CursorMode::Infinite);
        window->ProcessEvents();
        REQUIRE(window->GetCursorMode() == rndr::CursorMode::Infinite);
    }
    SECTION("Set to normal")
    {
        window->SetCursorMode(rndr::CursorMode::Normal);
        window->ProcessEvents();
        REQUIRE(window->GetCursorMode() == rndr::CursorMode::Normal);
    }
    rndr::Window::Destroy(*window);
    REQUIRE(rndr::Destroy());
}
