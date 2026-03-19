#include <catch2/catch2.hpp>

#include "opal/allocator.h"
#include "opal/exceptions.h"
#include "opal/logging.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/exception.hpp"
#include "rndr/generic-window.hpp"

CATCH_TRANSLATE_EXCEPTION(const Opal::Exception& e)
{
    return {*e.What()};
}

TEST_CASE("Canvas Format enum", "[canvas]")
{
    SECTION("EnumCount has expected value")
    {
        constexpr auto k_count = static_cast<Rndr::u8>(Rndr::Canvas::Format::EnumCount);
        // 14 pixel formats + 8 vertex formats = 22
        REQUIRE(k_count == 22);
    }
}

TEST_CASE("Canvas Context Init with null window handle throws", "[canvas]")
{
    REQUIRE_THROWS_AS(Rndr::Canvas::Context::Init(nullptr), Opal::InvalidArgumentException);
}

TEST_CASE("Canvas Context Init with valid window handle", "[canvas]")
{
    auto app = Rndr::Application::Create();
    REQUIRE(app != nullptr);

    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    auto window = app->CreateGenericWindow(window_desc);

    auto context = Rndr::Canvas::Context::Init(window.Clone());
    REQUIRE(context.IsValid());
    REQUIRE(context.GetWidth() > 0);
    REQUIRE(context.GetHeight() > 0);
    REQUIRE(context.IsVsyncEnabled());
    REQUIRE(context.GetColorFormat() == Rndr::Canvas::Format::RGBA8);
    REQUIRE(context.GetDepthStencilFormat() == Rndr::Canvas::Format::D24S8);
}

TEST_CASE("Canvas Context Init can be called again after null handle throws", "[canvas]")
{
    // First call throws because of null handle.
    REQUIRE_THROWS_AS(Rndr::Canvas::Context::Init(nullptr), Opal::InvalidArgumentException);

    // Second call should also throw InvalidArgumentException (not the "called twice" message),
    // proving that the first failed Init did not set the s_context_exists flag.
    REQUIRE_THROWS_AS(Rndr::Canvas::Context::Init(nullptr), Opal::InvalidArgumentException);
}

TEST_CASE("Canvas Context with custom desc", "[canvas]")
{
    auto app = Rndr::Application::Create();
    REQUIRE(app != nullptr);

    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    auto window = app->CreateGenericWindow(window_desc);

    SECTION("Vsync disabled")
    {
        Rndr::Canvas::ContextDesc desc;
        desc.vsync_enabled = false;
        auto context = Rndr::Canvas::Context::Init(window.Clone(), desc);
        REQUIRE(context.IsValid());
        REQUIRE_FALSE(context.IsVsyncEnabled());
    }

    SECTION("Custom color format")
    {
        Rndr::Canvas::ContextDesc desc;
        desc.color_format = Rndr::Canvas::Format::RGB8;
        auto context = Rndr::Canvas::Context::Init(window.Clone(), desc);
        REQUIRE(context.IsValid());
        REQUIRE(context.GetColorFormat() == Rndr::Canvas::Format::RGB8);
    }

    SECTION("Custom depth/stencil format")
    {
        Rndr::Canvas::ContextDesc desc;
        desc.depth_stencil_format = Rndr::Canvas::Format::D32F;
        auto context = Rndr::Canvas::Context::Init(window.Clone(), desc);
        REQUIRE(context.IsValid());
        REQUIRE(context.GetDepthStencilFormat() == Rndr::Canvas::Format::D32F);
    }
}

TEST_CASE("Canvas Context presentation features", "[canvas]")
{
    auto app = Rndr::Application::Create();
    REQUIRE(app != nullptr);

    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    auto window = app->CreateGenericWindow(window_desc);

    auto context = Rndr::Canvas::Context::Init(window.Clone());
    REQUIRE(context.IsValid());

    SECTION("Resize updates dimensions")
    {
        context.Resize(1920, 1080);
        REQUIRE(context.GetWidth() == 1920);
        REQUIRE(context.GetHeight() == 1080);
    }

    SECTION("SetVsync updates state")
    {
        context.SetVsync(false);
        REQUIRE_FALSE(context.IsVsyncEnabled());
        context.SetVsync(true);
        REQUIRE(context.IsVsyncEnabled());
    }

    SECTION("Present does not crash")
    {
        context.Present();
    }

    SECTION("Move constructor transfers ownership")
    {
        const Rndr::i32 w = context.GetWidth();
        const Rndr::i32 h = context.GetHeight();

        Rndr::Canvas::Context moved(std::move(context));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetWidth() == w);
        REQUIRE(moved.GetHeight() == h);
        REQUIRE_FALSE(context.IsValid());
    }

    SECTION("Move assignment transfers ownership")
    {
        const Rndr::i32 w = context.GetWidth();
        const Rndr::i32 h = context.GetHeight();

        Rndr::Canvas::Context moved = std::move(context);
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetWidth() == w);
        REQUIRE(moved.GetHeight() == h);
        REQUIRE_FALSE(context.IsValid());
    }

    SECTION("Destroy invalidates context")
    {
        REQUIRE(context.IsValid());
        context.Destroy();
        REQUIRE_FALSE(context.IsValid());
    }
}
