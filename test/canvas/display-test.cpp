#include <catch2/catch2.hpp>

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/display.hpp"
#include "rndr/generic-window.hpp"

TEST_CASE("Canvas Display", "[canvas]")
{
    auto app = Rndr::Application::Create();
    REQUIRE(app != nullptr);

    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    auto window = app->CreateGenericWindow(window_desc);

    auto context = Rndr::Canvas::Context::Init(window.Clone());
    REQUIRE(context.IsValid());

    SECTION("Create display with valid window")
    {
        const Rndr::Canvas::Display display(context, window.Clone());
        REQUIRE(display.IsValid());
        REQUIRE(display.GetWidth() > 0);
        REQUIRE(display.GetHeight() > 0);
    }

    SECTION("Default display is invalid")
    {
        const Rndr::Canvas::Display display;
        REQUIRE_FALSE(display.IsValid());
    }

    SECTION("GetDesc returns configured values")
    {
        const bool vsync = false;
        const Rndr::Canvas::Display display(context, window.Clone(), vsync);
        REQUIRE_FALSE(display.IsVsyncEnabled());
    }

    SECTION("Resize updates dimensions")
    {
        Rndr::Canvas::Display display(context, window.Clone());
        display.Resize(1920, 1080);
        REQUIRE(display.GetWidth() == 1920);
        REQUIRE(display.GetHeight() == 1080);
    }

    SECTION("SetVsync updates desc")
    {
        Rndr::Canvas::Display display(context, window.Clone());
        display.SetVsync(false);
        REQUIRE_FALSE(display.IsVsyncEnabled());
        display.SetVsync(true);
        REQUIRE(display.IsVsyncEnabled());
    }

    SECTION("Move constructor transfers ownership")
    {
        Rndr::Canvas::Display display(context, window.Clone());
        const Rndr::i32 w = display.GetWidth();
        const Rndr::i32 h = display.GetHeight();

        const Rndr::Canvas::Display moved(std::move(display));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetWidth() == w);
        REQUIRE(moved.GetHeight() == h);
        REQUIRE_FALSE(display.IsValid());
    }

    SECTION("Move assignment transfers ownership")
    {
        Rndr::Canvas::Display display(context, window.Clone());
        const Rndr::i32 w = display.GetWidth();
        const Rndr::i32 h = display.GetHeight();

        Rndr::Canvas::Display moved;
        moved = std::move(display);
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetWidth() == w);
        REQUIRE(moved.GetHeight() == h);
        REQUIRE_FALSE(display.IsValid());
    }

    SECTION("Destroy invalidates display")
    {
        Rndr::Canvas::Display display(context, window.Clone());
        REQUIRE(display.IsValid());
        display.Destroy();
        REQUIRE_FALSE(display.IsValid());
    }

    SECTION("Present does not crash")
    {
        Rndr::Canvas::Display display(context, window.Clone());
        display.Present();
    }
}
