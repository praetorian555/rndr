#include <catch2/catch2.hpp>

#include "rndr/application.hpp"

TEST_CASE("Init", "[init]")
{
    SECTION("Default create and destroy")
    {
        auto app = Rndr::Application::Create();
        REQUIRE(app != nullptr);
    }
}
