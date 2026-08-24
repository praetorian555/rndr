#include <catch2/catch2.hpp>

#include "canvas-test-common.hpp"

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

TEST_CASE("Canvas Context CreateContext with null window handle reports InvalidArgument", "[canvas]")
{
    REQUIRE(Rndr::Canvas::Context::CreateContext(nullptr).GetError() == Rndr::ErrorCode::InvalidArgument);
}

TEST_CASE("Canvas Context CreateContext with valid window handle", "[canvas]")
{
    auto app = Rndr::Application::Create().GetValue();
    REQUIRE(app != nullptr);

    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    auto window = app->CreateGenericWindow(window_desc).GetValue();

    auto context = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window.Clone()));
    REQUIRE(context.IsValid());
    REQUIRE(context.GetWidth() > 0);
    REQUIRE(context.GetHeight() > 0);
    REQUIRE(context.IsVsyncEnabled());
    REQUIRE(context.GetColorFormat() == Rndr::Canvas::Format::RGBA8);
    REQUIRE(context.GetDepthStencilFormat() == Rndr::Canvas::Format::D24S8);
}

TEST_CASE("Canvas Context CreateContext can be called again after a null handle failure", "[canvas]")
{
    // First call fails because of null handle.
    REQUIRE(Rndr::Canvas::Context::CreateContext(nullptr).GetError() == Rndr::ErrorCode::InvalidArgument);

    // Second call should also report InvalidArgument, proving that the first failed call did not
    // register a primary context (otherwise this would create a shared context and succeed).
    REQUIRE(Rndr::Canvas::Context::CreateContext(nullptr).GetError() == Rndr::ErrorCode::InvalidArgument);
}

TEST_CASE("Canvas Context with custom desc", "[canvas]")
{
    auto app = Rndr::Application::Create().GetValue();
    REQUIRE(app != nullptr);

    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    auto window = app->CreateGenericWindow(window_desc).GetValue();

    SECTION("Vsync disabled")
    {
        Rndr::Canvas::ContextDesc desc;
        desc.vsync_enabled = false;
        auto context = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window.Clone(), desc));
        REQUIRE(context.IsValid());
        REQUIRE_FALSE(context.IsVsyncEnabled());
    }

    SECTION("Custom color format")
    {
        Rndr::Canvas::ContextDesc desc;
        desc.color_format = Rndr::Canvas::Format::RGB8;
        auto context = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window.Clone(), desc));
        REQUIRE(context.IsValid());
        REQUIRE(context.GetColorFormat() == Rndr::Canvas::Format::RGB8);
    }

    SECTION("Custom depth/stencil format")
    {
        Rndr::Canvas::ContextDesc desc;
        desc.depth_stencil_format = Rndr::Canvas::Format::D32F;
        auto context = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window.Clone(), desc));
        REQUIRE(context.IsValid());
        REQUIRE(context.GetDepthStencilFormat() == Rndr::Canvas::Format::D32F);
    }
}

TEST_CASE("Canvas Context CreateContext with no window and no primary reports InvalidArgument", "[canvas]")
{
    // With no primary yet, CreateContext() attempts to create the primary, which requires a window.
    REQUIRE(Rndr::Canvas::Context::CreateContext().GetError() == Rndr::ErrorCode::InvalidArgument);
}

TEST_CASE("Canvas Context CreateContext creates shared contexts after the primary", "[canvas]")
{
    auto app = Rndr::Application::Create().GetValue();
    REQUIRE(app != nullptr);

    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    auto window = app->CreateGenericWindow(window_desc).GetValue();

    auto context = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window.Clone()));
    REQUIRE(context.IsValid());

    SECTION("Shared context is valid and independent of primary lifetime")
    {
        auto shared = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext());
        REQUIRE(shared.IsValid());

        // Binding the shared context and rebinding the primary must both succeed.
        REQUIRE(shared.MakeCurrent());
        REQUIRE(context.MakeCurrent());
        REQUIRE(Rndr::Canvas::Context::ReleaseCurrent());

        // Rebind the primary so the rest of the test/cleanup has a current context.
        REQUIRE(context.MakeCurrent());
    }

    SECTION("Destroying a shared context leaves the primary intact")
    {
        {
            auto shared = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext());
            REQUIRE(shared.IsValid());
        }
        // The primary must still be valid and still registered as primary: another CreateContext()
        // call must succeed in producing a shared context rather than failing.
        REQUIRE(context.IsValid());
        REQUIRE(context.MakeCurrent());
        auto another_shared = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext());
        REQUIRE(another_shared.IsValid());
    }

    SECTION("Multiple shared contexts can coexist")
    {
        auto shared_a = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext());
        auto shared_b = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext());
        REQUIRE(shared_a.IsValid());
        REQUIRE(shared_b.IsValid());
    }
}

TEST_CASE("Canvas Context CreateContext supports multiple windows", "[canvas]")
{
    auto app = Rndr::Application::Create().GetValue();
    REQUIRE(app != nullptr);

    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    auto window_a = app->CreateGenericWindow(window_desc).GetValue();
    auto window_b = app->CreateGenericWindow(window_desc).GetValue();

    auto primary = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window_a.Clone()));
    REQUIRE(primary.IsValid());

    SECTION("Secondary per-window context owns its own surface")
    {
        auto secondary = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window_b.Clone()));
        REQUIRE(secondary.IsValid());
        // A per-window context owns a presentation surface, so it reports its window dimensions.
        REQUIRE(secondary.GetWidth() > 0);
        REQUIRE(secondary.GetHeight() > 0);

        // Both windows can be bound and presented.
        REQUIRE(secondary.MakeCurrent());
        secondary.Present();
        REQUIRE(primary.MakeCurrent());
        primary.Present();
    }

    SECTION("Per-window context honors its own desc")
    {
        Rndr::Canvas::ContextDesc desc;
        desc.depth_stencil_format = Rndr::Canvas::Format::D32F;
        auto secondary = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window_b.Clone(), desc));
        REQUIRE(secondary.IsValid());
        REQUIRE(secondary.GetDepthStencilFormat() == Rndr::Canvas::Format::D32F);
    }

    SECTION("Destroying a secondary window context leaves the primary intact")
    {
        {
            auto secondary = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window_b.Clone()));
            REQUIRE(secondary.IsValid());
        }
        REQUIRE(primary.IsValid());
        REQUIRE(primary.MakeCurrent());
        // Primary is still registered: another per-window context can be created.
        auto secondary_again = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window_b.Clone()));
        REQUIRE(secondary_again.IsValid());
    }
}

TEST_CASE("Canvas Context presentation features", "[canvas]")
{
    auto app = Rndr::Application::Create().GetValue();
    REQUIRE(app != nullptr);

    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    auto window = app->CreateGenericWindow(window_desc).GetValue();

    auto context = CanvasTest::Unwrap(Rndr::Canvas::Context::CreateContext(window.Clone()));
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
