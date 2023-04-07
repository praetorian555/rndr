#include <catch2/catch_test_macros.hpp>

#include "rndr/core/window.h"

TEST_CASE("Creating a window", "[window]")
{
    REQUIRE(rndr::Init());
    SECTION("Default window")
    {
        rndr::Window window;
        window.ProcessEvents();
        REQUIRE(!window.IsClosed());
        REQUIRE(window.GetCursorMode() == rndr::CursorMode::Normal);
        REQUIRE(window.GetWidth() == 1024);
        REQUIRE(window.GetHeight() == 768);
        REQUIRE(!window.IsWindowMaximized());
        REQUIRE(!window.IsWindowMinimized());
        REQUIRE(window.IsVisible());
        SECTION("Make minimized")
        {
            window.SetMinimized(true);
            window.ProcessEvents();
            REQUIRE(window.IsWindowMinimized());
            REQUIRE(!window.IsWindowMaximized());
            REQUIRE(!window.IsVisible());
        }
        SECTION("Make maximized")
        {
            window.SetMaximized(true);
            window.ProcessEvents();
            REQUIRE(!window.IsWindowMinimized());
            REQUIRE(window.IsWindowMaximized());
            REQUIRE(window.IsVisible());
        }
        SECTION("Make hidden")
        {
            window.SetVisible(false);
            window.ProcessEvents();
            REQUIRE(!window.IsWindowMinimized());
            REQUIRE(!window.IsWindowMaximized());
            REQUIRE(!window.IsVisible());
        }
    }
    SECTION("Hidden window")
    {
        rndr::Window window{{.start_visible = false}};
        window.ProcessEvents();
        REQUIRE(!window.IsClosed());
        REQUIRE(window.GetCursorMode() == rndr::CursorMode::Normal);
        REQUIRE(window.GetWidth() == 1024);
        REQUIRE(window.GetHeight() == 768);
        REQUIRE(!window.IsWindowMaximized());
        REQUIRE(!window.IsWindowMinimized());
        REQUIRE(!window.IsVisible());
    }
    SECTION("Minimized window")
    {
        rndr::Window window{{.start_minimized = true}};
        window.ProcessEvents();
        REQUIRE(!window.IsClosed());
        REQUIRE(window.GetCursorMode() == rndr::CursorMode::Normal);
        REQUIRE(window.GetWidth() == 1024);
        REQUIRE(window.GetHeight() == 768);
        REQUIRE(!window.IsWindowMaximized());
        REQUIRE(window.IsWindowMinimized());
        REQUIRE(!window.IsVisible());
    }
    SECTION("Maximized window")
    {
        rndr::Window window{{.start_maximized = true}};
        window.ProcessEvents();
        REQUIRE(!window.IsClosed());
        REQUIRE(window.GetCursorMode() == rndr::CursorMode::Normal);
        REQUIRE(window.GetWidth() == 1024);
        REQUIRE(window.GetHeight() == 768);
        REQUIRE(window.IsWindowMaximized());
        REQUIRE(!window.IsWindowMinimized());
        REQUIRE(window.IsVisible());
    }
    REQUIRE(rndr::Destroy());
}

TEST_CASE("Moving a window", "[window]")
{
    REQUIRE(rndr::Init());
    SECTION("Moving default window")
    {
        rndr::Window first_window;
        REQUIRE(!first_window.IsClosed());
        rndr::NativeWindowHandle handle = first_window.GetNativeWindowHandle();
        REQUIRE(handle != rndr::k_invalid_window_handle);
        rndr::Window second_window(std::move(first_window));
        REQUIRE(first_window.IsClosed());
        REQUIRE(first_window.GetNativeWindowHandle() == rndr::k_invalid_window_handle);
        REQUIRE(!second_window.IsClosed());
        REQUIRE(second_window.GetNativeWindowHandle() == handle);
    }
    SECTION("Replacing existing window")
    {
        rndr::Window first_window;
        REQUIRE(!first_window.IsClosed());
        rndr::NativeWindowHandle handle = first_window.GetNativeWindowHandle();
        REQUIRE(handle != rndr::k_invalid_window_handle);
        rndr::Window second_window;
        REQUIRE(!second_window.IsClosed());
        REQUIRE(second_window.GetNativeWindowHandle() != handle);
        second_window = std::move(first_window);
        REQUIRE(first_window.IsClosed());
        REQUIRE(first_window.GetNativeWindowHandle() == rndr::k_invalid_window_handle);
        REQUIRE(!second_window.IsClosed());
        REQUIRE(second_window.GetNativeWindowHandle() == handle);
    }
    REQUIRE(rndr::Destroy());
}

TEST_CASE("Resizing the window", "[window]")
{
    REQUIRE(rndr::Init());
    SECTION("Create default window")
    {
        rndr::Window window;
        window.ProcessEvents();
        REQUIRE(!window.IsClosed());
        REQUIRE(window.GetCursorMode() == rndr::CursorMode::Normal);
        REQUIRE(window.GetWidth() == 1024);
        REQUIRE(window.GetHeight() == 768);
        REQUIRE(!window.IsWindowMaximized());
        REQUIRE(!window.IsWindowMinimized());
        REQUIRE(window.IsVisible());
        SECTION("Resize to 800x600")
        {
            window.Resize(800, 600);
            window.ProcessEvents();
            REQUIRE(window.GetWidth() == 800);
            REQUIRE(window.GetHeight() == 600);
            REQUIRE(window.GetSize() == math::Vector2(800, 600));
        }
        SECTION("Resize to 1280x720")
        {
            window.Resize(1280, 720);
            window.ProcessEvents();
            REQUIRE(window.GetWidth() == 1280);
            REQUIRE(window.GetHeight() == 720);
            REQUIRE(window.GetSize() == math::Vector2(1280, 720));
        }
        SECTION("Resize to invalid")
        {
            window.Resize(0, 0);
            window.ProcessEvents();
            REQUIRE(window.GetWidth() == 1024);
            REQUIRE(window.GetHeight() == 768);
            REQUIRE(window.GetSize() == math::Vector2(1024, 768));
        }
    }
    REQUIRE(rndr::Destroy());
}

TEST_CASE("Closing the window", "[window]")
{
    REQUIRE(rndr::Init());
    rndr::Window window;
    window.ProcessEvents();
    REQUIRE(!window.IsClosed());
    REQUIRE(window.GetCursorMode() == rndr::CursorMode::Normal);
    REQUIRE(window.GetWidth() == 1024);
    REQUIRE(window.GetHeight() == 768);
    REQUIRE(!window.IsWindowMaximized());
    REQUIRE(!window.IsWindowMinimized());
    REQUIRE(window.IsVisible());
    window.Close();
    window.ProcessEvents();
    REQUIRE(window.IsClosed());
    REQUIRE(rndr::Destroy());
}

TEST_CASE("Changing cursor mode", "[window]")
{
    REQUIRE(rndr::Init());
    SECTION("Create default window")
    {
        rndr::Window window;
        window.ProcessEvents();
        REQUIRE(!window.IsClosed());
        SECTION("Set to hidden")
        {
            window.SetCursorMode(rndr::CursorMode::Hidden);
            window.ProcessEvents();
            REQUIRE(window.GetCursorMode() == rndr::CursorMode::Hidden);
        }
        SECTION("Set to infinite")
        {
            window.SetCursorMode(rndr::CursorMode::Infinite);
            window.ProcessEvents();
            REQUIRE(window.GetCursorMode() == rndr::CursorMode::Infinite);
        }
        SECTION("Set to normal")
        {
            window.SetCursorMode(rndr::CursorMode::Normal);
            window.ProcessEvents();
            REQUIRE(window.GetCursorMode() == rndr::CursorMode::Normal);
        }
    }
    REQUIRE(rndr::Destroy());
}
