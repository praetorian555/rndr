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

TEST_CASE("Input binding", "[input]")
{
    REQUIRE(rndr::Init());
    SECTION("Create binding")
    {
        rndr::InputBinding binding;
        binding.primitive = rndr::InputPrimitive::Keyboard_A;
        binding.trigger = rndr::InputTrigger::ButtonDown;
        binding.modifier = 1.0f;
        REQUIRE(binding.primitive == rndr::InputPrimitive::Keyboard_A);
        REQUIRE(binding.trigger == rndr::InputTrigger::ButtonDown);
        REQUIRE(binding.modifier == 1.0f);
    }
    SECTION("Compare bindings")
    {
        rndr::InputBinding binding1;
        binding1.primitive = rndr::InputPrimitive::Keyboard_A;
        binding1.trigger = rndr::InputTrigger::ButtonDown;
        binding1.modifier = 1.0f;
        rndr::InputBinding binding2;
        binding2.primitive = rndr::InputPrimitive::Mouse_RightButton;
        binding2.trigger = rndr::InputTrigger::ButtonDown;
        binding2.modifier = 0.5f;
        rndr::InputBinding binding3;
        binding3.primitive = rndr::InputPrimitive::Keyboard_A;
        binding3.trigger = rndr::InputTrigger::ButtonUp;
        binding3.modifier = 1.0f;
        rndr::InputBinding binding4;
        binding4.primitive = rndr::InputPrimitive::Keyboard_A;
        binding4.trigger = rndr::InputTrigger::ButtonDown;
        binding4.modifier = 2.0f;
        REQUIRE(binding1 != binding3);
        REQUIRE(binding1 != binding2);
        REQUIRE(binding1 == binding4);
    }
    REQUIRE(rndr::Destroy());
}

TEST_CASE("Input context", "[input]")
{
    REQUIRE(rndr::Init());
    SECTION("Create context")
    {
        const rndr::InputContext context("TestContext");
        REQUIRE(context.GetName() == "TestContext");
    }
    SECTION("Add action")
    {
        rndr::InputContext context("TestContext");
        REQUIRE(context.GetName() == "TestContext");
        const rndr::InputAction action("TestAction");
        rndr::InputBinding bindings[]{{.primitive = rndr::InputPrimitive::Keyboard_A,
                                       .trigger = rndr::InputTrigger::ButtonDown,
                                       .modifier = 1.0f}};
        const rndr::InputCallback callback =
            [](rndr::InputPrimitive primitive, rndr::InputTrigger trigger, rndr::real value)
        {
            (void)primitive;
            (void)trigger;
            (void)value;
        };
        REQUIRE(context.AddAction(action, callback, bindings));
        REQUIRE(context.ContainsAction(action));
        REQUIRE(context.GetActionCallback(action) != nullptr);
        REQUIRE(context.GetActionBindings(action)[0] == bindings[0]);
        SECTION("Add additional binding")
        {
            rndr::InputBinding binding{.primitive = rndr::InputPrimitive::Keyboard_D,
                                       .trigger = rndr::InputTrigger::ButtonDown,
                                       .modifier = 1.0f};
            REQUIRE(context.AddBindingToAction(action, binding));
            rndr::Span<rndr::InputBinding> reg_bindings = context.GetActionBindings(action);
            REQUIRE(reg_bindings.size() == 2);
            REQUIRE(reg_bindings[0] != bindings[1]);
            REQUIRE(reg_bindings[1] == binding);
            SECTION("Remove binding")
            {
                REQUIRE(context.RemoveBindingFromAction(action, bindings[0]));
                reg_bindings = context.GetActionBindings(action);
                REQUIRE(reg_bindings.size() == 1);
                REQUIRE(reg_bindings[0] == binding);
            }
        }
    }
    REQUIRE(rndr::Destroy());
}

TEST_CASE("Input system", "[input]")
{
    REQUIRE(rndr::Init());

    SECTION("Default context")
    {
        const rndr::InputContext& context = rndr::InputSystem::GetCurrentContext();
        REQUIRE(context.GetName() == "Default");
    }
    SECTION("Push context")
    {
        const rndr::InputContext context("TestContext");
        REQUIRE(rndr::InputSystem::PushContext(context));
        REQUIRE(rndr::InputSystem::GetCurrentContext().GetName() == "TestContext");
        SECTION("Pop context")
        {
            rndr::InputSystem::PopContext();
            REQUIRE(rndr::InputSystem::GetCurrentContext().GetName() == "Default");
        }
    }
    SECTION("Pop default context")
    {
        REQUIRE(!rndr::InputSystem::PopContext());
        REQUIRE(rndr::InputSystem::GetCurrentContext().GetName() == "Default");
    }
    SECTION("Submit events")
    {
        REQUIRE(rndr::InputSystem::SubmitButtonEvent(nullptr,
                                                     rndr::InputPrimitive::Keyboard_A,
                                                     rndr::InputTrigger::ButtonDown));
        REQUIRE(rndr::InputSystem::SubmitMousePositionEvent(nullptr, {100, 100}, {1024, 768}));
        REQUIRE(rndr::InputSystem::SubmitMouseWheelEvent(nullptr, 1));
        REQUIRE(
            rndr::InputSystem::SubmitRelativeMousePositionEvent(nullptr, {100, 100}, {1024, 768}));
    }
    SECTION("Submit invalid events")
    {
        REQUIRE(!rndr::InputSystem::SubmitMousePositionEvent(nullptr, {100, 100}, {0, 0}));
        REQUIRE(!rndr::InputSystem::SubmitRelativeMousePositionEvent(nullptr, {100, 100}, {0, 0}));
    }

    REQUIRE(rndr::Destroy());
}
