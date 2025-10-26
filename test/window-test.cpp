#include <catch2/catch2.hpp>

#include "rndr/application.hpp"
#include "rndr/generic-window.hpp"

// TODO: Add tests for cursor position mode in app

TEST_CASE("Creating a window", "[window]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    SECTION("Default window")
    {
        Rndr::GenericWindow* window = app->CreateGenericWindow();
        REQUIRE(window != nullptr);
        app->ProcessSystemEvents(1.0f);
        REQUIRE(!window->IsClosed());
        auto size = window->GetSize().GetValue();
        REQUIRE(size.x == 1024);
        REQUIRE(size.y == 768);
        REQUIRE(!window->IsMaximized());
        REQUIRE(!window->IsMinimized());
        REQUIRE(window->IsVisible());
        SECTION("Make minimized")
        {
            window->Minimize();
            app->ProcessSystemEvents(1.0f);
            REQUIRE(window->IsMinimized());
            REQUIRE(!window->IsMaximized());
            REQUIRE(!window->IsVisible());
        }
        SECTION("Make maximized")
        {
            window->Maximize();
            app->ProcessSystemEvents(1.0f);
            REQUIRE(!window->IsMinimized());
            REQUIRE(window->IsMaximized());
            REQUIRE(window->IsVisible());
        }
        SECTION("Make hidden")
        {
            window->Hide();
            app->ProcessSystemEvents(1.0f);
            REQUIRE(!window->IsMinimized());
            REQUIRE(!window->IsMaximized());
            REQUIRE(!window->IsVisible());
        }
        app->DestroyGenericWindow(window);
    }
    SECTION("Hidden window")
    {
        Rndr::GenericWindow* window = app->CreateGenericWindow({.start_visible = false});
        REQUIRE(window != nullptr);
        app->ProcessSystemEvents(1.0f);
        REQUIRE(!window->IsClosed());
        auto size = window->GetSize().GetValue();
        REQUIRE(size.x == 1024);
        REQUIRE(size.y == 768);
        REQUIRE(!window->IsMaximized());
        REQUIRE(!window->IsMinimized());
        REQUIRE(!window->IsVisible());
        app->DestroyGenericWindow(window);
    }
    SECTION("Minimized window")
    {
        Rndr::GenericWindow* window = app->CreateGenericWindow({.start_minimized = true});
        REQUIRE(window != nullptr);
        app->ProcessSystemEvents(1.0f);
        REQUIRE(!window->IsClosed());
        auto size = window->GetSize().GetValue();
        REQUIRE(size.x == 0);
        REQUIRE(size.y == 0);
        REQUIRE(!window->IsMaximized());
        REQUIRE(window->IsMinimized());
        REQUIRE(!window->IsVisible());
        app->DestroyGenericWindow(window);
    }
    SECTION("Maximized window")
    {
        Rndr::GenericWindow* window = app->CreateGenericWindow({.start_maximized = true});
        REQUIRE(window != nullptr);
        app->ProcessSystemEvents(1.0f);
        REQUIRE(!window->IsClosed());
        REQUIRE(window->IsMaximized());
        REQUIRE(!window->IsMinimized());
        REQUIRE(window->IsVisible());
        app->DestroyGenericWindow(window);
    }
    Rndr::Application::Destroy();
}

TEST_CASE("Resizing the window", "[window]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    SECTION("Create default window")
    {
        Rndr::GenericWindow* window = app->CreateGenericWindow();
        REQUIRE(window != nullptr);
        app->ProcessSystemEvents(1.0f);
        int32_t start_x, start_y, start_width, start_height;
        window->GetPositionAndSize(start_x, start_y, start_width, start_height);
        REQUIRE(start_x == 0);
        REQUIRE(start_y == 0);
        REQUIRE(start_width == 1024);
        REQUIRE(start_height == 768);
        SECTION("Resize to smaller")
        {
            window->Reshape(100, 100, 800, 600);
            app->ProcessSystemEvents(1.0f);
            int32_t x, y, w, h;
            window->GetPositionAndSize(x, y, w, h);
            REQUIRE(x == 100);
            REQUIRE(y == 100);
            REQUIRE(w == 800);
            REQUIRE(h == 600);
            REQUIRE(window->GetSize().GetValue() == Rndr::Vector2i(800, 600));
        }
        SECTION("Resize to larger")
        {
            window->Reshape(100, 100, 1440, 900);
            app->ProcessSystemEvents(1.0f);
            int32_t x, y, w, h;
            window->GetPositionAndSize(x, y, w, h);
            REQUIRE(x == 100);
            REQUIRE(y == 100);
            REQUIRE(w == 1440);
            REQUIRE(h == 900);
            REQUIRE(window->GetSize().GetValue() == Rndr::Vector2i(1440, 900));
        }
        SECTION("Invalid x position")
        {
            Rndr::ErrorCode err = window->Reshape(-1, 100, 100, 100);
            REQUIRE(err == Rndr::ErrorCode::OutOfBounds);
            app->ProcessSystemEvents(1.0f);
            int32_t x, y, w, h;
            window->GetPositionAndSize(x, y, w, h);
            REQUIRE(x == 0);
            REQUIRE(y == 0);
            REQUIRE(w == 1024);
            REQUIRE(h == 768);
            REQUIRE(window->GetSize().GetValue() == Rndr::Vector2i(1024, 768));
        }
        SECTION("Invalid y position")
        {
            Rndr::ErrorCode err = window->Reshape(100, -1, 100, 100);
            REQUIRE(err == Rndr::ErrorCode::OutOfBounds);
            app->ProcessSystemEvents(1.0f);
            int32_t x, y, w, h;
            window->GetPositionAndSize(x, y, w, h);
            REQUIRE(x == 0);
            REQUIRE(y == 0);
            REQUIRE(w == 1024);
            REQUIRE(h == 768);
            REQUIRE(window->GetSize().GetValue() == Rndr::Vector2i(1024, 768));
        }
        SECTION("Invalid width")
        {
            Rndr::ErrorCode err = window->Reshape(100, 100, 0, 100);
            REQUIRE(err == Rndr::ErrorCode::InvalidArgument);
            app->ProcessSystemEvents(1.0f);
            int32_t x, y, w, h;
            window->GetPositionAndSize(x, y, w, h);
            REQUIRE(x == 0);
            REQUIRE(y == 0);
            REQUIRE(w == 1024);
            REQUIRE(h == 768);
            REQUIRE(window->GetSize().GetValue() == Rndr::Vector2i(1024, 768));
        }
        SECTION("Invalid height")
        {
            Rndr::ErrorCode err = window->Reshape(100, 100, 100, 0);
            REQUIRE(err == Rndr::ErrorCode::InvalidArgument);
            app->ProcessSystemEvents(1.0f);
            int32_t x, y, w, h;
            window->GetPositionAndSize(x, y, w, h);
            REQUIRE(x == 0);
            REQUIRE(y == 0);
            REQUIRE(w == 1024);
            REQUIRE(h == 768);
            REQUIRE(window->GetSize().GetValue() == Rndr::Vector2i(1024, 768));
        }
        SECTION("Resize while window is already closed")
        {
            window->RequestClose();
            app->ProcessSystemEvents(1.0f);
            REQUIRE(window->IsClosed());
            Rndr::ErrorCode err = window->Reshape(100, 100, 100, 100);
            REQUIRE(err == Rndr::ErrorCode::WindowAlreadyClosed);
            int32_t x, y, w, h;
            window->GetPositionAndSize(x, y, w, h);
            REQUIRE(x == 0);
            REQUIRE(y == 0);
            REQUIRE(w == 1024);
            REQUIRE(h == 768);
            REQUIRE(window->GetSize().GetValue() == Rndr::Vector2i(1024, 768));
        }
    }
    Rndr::Application::Destroy();
}

TEST_CASE("Closing the window", "[window]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::GenericWindow* window = app->CreateGenericWindow();
    REQUIRE(window != nullptr);
    app->ProcessSystemEvents(1.0f);
    REQUIRE(!window->IsClosed());
    window->RequestClose();
    app->ProcessSystemEvents(1.0f);
    REQUIRE(window->IsClosed());

    Rndr::Application::Destroy();
}

// TEST_CASE("Changing cursor mode", "[window]")
// {
//     Rndr::Application* app = Rndr::Application::Create();
//     REQUIRE(app != nullptr);
//     SECTION("Create default window")
//     {
//         Rndr::GenericWindow* window = app->CreateGenericWindow();
//         REQUIRE(window != nullptr);
//         app->ProcessSystemEvents(1.0f);
//         REQUIRE(!window->IsClosed());
//         SECTION("Set to hidden")
//         {
//             window.SetCursorMode(Rndr::CursorMode::Hidden);
//             window.ProcessEvents();
//             REQUIRE(window.GetCursorMode() == Rndr::CursorMode::Hidden);
//         }
//         SECTION("Set to infinite")
//         {
//             window.SetCursorMode(Rndr::CursorMode::Infinite);
//             window.ProcessEvents();
//             REQUIRE(window.GetCursorMode() == Rndr::CursorMode::Infinite);
//         }
//         SECTION("Set to normal")
//         {
//             window.SetCursorMode(Rndr::CursorMode::Normal);
//             window.ProcessEvents();
//             REQUIRE(window.GetCursorMode() == Rndr::CursorMode::Normal);
//         }
//     }
//     Rndr::Application::Destroy();
// }
