#include <catch2/catch2.hpp>

#include "rndr/application.hpp"

TEST_CASE("Init", "[init]")
{
    SECTION("Default create and destroy")
    {
        auto app = Rndr::Application::Create();
        REQUIRE(app.HasValue());
        REQUIRE(app.GetValue() != nullptr);
    }
    SECTION("Second live application reports InvalidArgument")
    {
        auto app = Rndr::Application::Create();
        REQUIRE(app.HasValue());
        auto second = Rndr::Application::Create();
        REQUIRE_FALSE(second.HasValue());
        REQUIRE(second.GetError() == Rndr::ErrorCode::InvalidArgument);
    }
}
