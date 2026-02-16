#include <catch2/catch2.hpp>

#include "rndr/frames-per-second-counter.h"

TEST_CASE("Frames per second counter tests", "[fps]")
{
    Rndr::FramesPerSecondCounter counter(1.0f);

    SECTION("Initial frames per second should be 60")
    {
        REQUIRE(counter.GetFramesPerSecond() == 60.0f);
    }

    SECTION("Frames per second should be 1 after 1 second")
    {
        counter.Update(1.0f);
        REQUIRE(counter.GetFramesPerSecond() == 1.0f);
    }

    SECTION("Frames per second should be 0.5 after 2 seconds")
    {
        counter.Update(2.0f);
        REQUIRE(counter.GetFramesPerSecond() == 0.5f);
    }

    SECTION("Frames per second should be 2 after 0.5 seconds")
    {
        counter.Update(0.5f);
        counter.Update(0.5f);
        REQUIRE(counter.GetFramesPerSecond() == 2.0f);
    }
}