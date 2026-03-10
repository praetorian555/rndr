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
        // 12 pixel formats + 8 vertex formats = 20
        REQUIRE(k_count == 20);
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
}

TEST_CASE("Canvas Context Init can be called again after null handle throws", "[canvas]")
{
    // First call throws because of null handle.
    REQUIRE_THROWS_AS(Rndr::Canvas::Context::Init(nullptr), Opal::InvalidArgumentException);

    // Second call should also throw InvalidArgumentException (not the "called twice" message),
    // proving that the first failed Init did not set the s_context_exists flag.
    REQUIRE_THROWS_AS(Rndr::Canvas::Context::Init(nullptr), Opal::InvalidArgumentException);
}
