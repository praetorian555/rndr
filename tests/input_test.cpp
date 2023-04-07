#include <catch2/catch_test_macros.hpp>

#include "rndr/core/input.h"

TEST_CASE("Input action", "[input]")
{
    REQUIRE(rndr::Init());
    SECTION("Create action")
    {
        rndr::InputAction action("TestAction");
        REQUIRE(action.GetName() == "TestAction");
        REQUIRE(action.IsValid());
    }
    SECTION("Create invalid action")
    {
        rndr::InputAction action;
        REQUIRE(action.GetName() == "");
        REQUIRE(!action.IsValid());
    }
    SECTION("Compare actions")
    {
        rndr::InputAction action1("TestAction1");
        rndr::InputAction action2("TestAction2");
        rndr::InputAction action3("TestAction1");
        REQUIRE(action1 == action3);
        REQUIRE(action1 != action2);
    }
    SECTION("Compare invalid actions")
    {
        rndr::InputAction action1;
        rndr::InputAction action2;
        REQUIRE(action1 == action2);
    }
    SECTION("Compare valid and invalid actions")
    {
        rndr::InputAction action1("TestAction");
        rndr::InputAction action2;
        REQUIRE(action1 != action2);
    }
    REQUIRE(rndr::Destroy());
}