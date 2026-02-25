#include <catch2/catch2.hpp>

#include "opal/allocator.h"
#include "opal/exceptions.h"
#include "opal/logging.h"

#include "rndr/canvas/context.hpp"
#include "rndr/exception.hpp"

TEST_CASE("Canvas Format enum", "[canvas]")
{
    SECTION("EnumCount has expected value")
    {
        constexpr auto count = static_cast<Rndr::u8>(Rndr::Canvas::Format::EnumCount);
        // 12 pixel formats + 8 vertex formats = 20
        REQUIRE(count == 20);
    }
}

TEST_CASE("Canvas Context Init with null window handle throws", "[canvas]")
{
    Opal::MallocAllocator allocator;
    Opal::PushDefaultAllocator(&allocator);
    Opal::Logger logger;
    Opal::SetLogger(&logger);

    REQUIRE_THROWS_AS(Rndr::Canvas::Context::Init(nullptr), Opal::InvalidArgumentException);
}

TEST_CASE("Canvas Context Init can be called again after null handle throws", "[canvas]")
{
    Opal::MallocAllocator allocator;
    Opal::PushDefaultAllocator(&allocator);
    Opal::Logger logger;
    Opal::SetLogger(&logger);

    // First call throws because of null handle.
    REQUIRE_THROWS_AS(Rndr::Canvas::Context::Init(nullptr), Opal::InvalidArgumentException);

    // Second call should also throw InvalidArgumentException (not the "called twice" message),
    // proving that the first failed Init did not set the s_context_exists flag.
    REQUIRE_THROWS_AS(Rndr::Canvas::Context::Init(nullptr), Opal::InvalidArgumentException);
}
