#include <catch2/catch2.hpp>

#include "rndr/core/input.h"
#include "rndr/core/window.h"

TEST_CASE("Input action", "[input]")
{
    REQUIRE(Rndr::Init());
    SECTION("Create action")
    {
        Rndr::InputAction action("TestAction");
        REQUIRE(action.GetName() == "TestAction");
        REQUIRE(action.IsValid());
    }
    SECTION("Create invalid action")
    {
        Rndr::InputAction action;
        REQUIRE(action.GetName() == "");
        REQUIRE(!action.IsValid());
    }
    SECTION("Compare actions")
    {
        Rndr::InputAction action1("TestAction1");
        Rndr::InputAction action2("TestAction2");
        Rndr::InputAction action3("TestAction1");
        REQUIRE(action1 == action3);
        REQUIRE(action1 != action2);
    }
    SECTION("Compare invalid actions")
    {
        Rndr::InputAction action1;
        Rndr::InputAction action2;
        REQUIRE(action1 == action2);
    }
    SECTION("Compare valid and invalid actions")
    {
        Rndr::InputAction action1("TestAction");
        Rndr::InputAction action2;
        REQUIRE(action1 != action2);
    }
    REQUIRE(Rndr::Destroy());
}

TEST_CASE("Input binding", "[input]")
{
    REQUIRE(Rndr::Init());
    SECTION("Create binding")
    {
        Rndr::InputBinding binding;
        binding.primitive = Rndr::InputPrimitive::Keyboard_A;
        binding.trigger = Rndr::InputTrigger::ButtonPressed;
        binding.modifier = 1.0f;
        REQUIRE(binding.primitive == Rndr::InputPrimitive::Keyboard_A);
        REQUIRE(binding.trigger == Rndr::InputTrigger::ButtonPressed);
        REQUIRE(binding.modifier == 1.0f);
    }
    SECTION("Compare bindings")
    {
        Rndr::InputBinding binding1;
        binding1.primitive = Rndr::InputPrimitive::Keyboard_A;
        binding1.trigger = Rndr::InputTrigger::ButtonPressed;
        binding1.modifier = 1.0f;
        Rndr::InputBinding binding2;
        binding2.primitive = Rndr::InputPrimitive::Mouse_RightButton;
        binding2.trigger = Rndr::InputTrigger::ButtonPressed;
        binding2.modifier = 0.5f;
        Rndr::InputBinding binding3;
        binding3.primitive = Rndr::InputPrimitive::Keyboard_A;
        binding3.trigger = Rndr::InputTrigger::ButtonReleased;
        binding3.modifier = 1.0f;
        Rndr::InputBinding binding4;
        binding4.primitive = Rndr::InputPrimitive::Keyboard_A;
        binding4.trigger = Rndr::InputTrigger::ButtonPressed;
        binding4.modifier = 1.0f;
        REQUIRE(binding1 != binding3);
        REQUIRE(binding1 != binding2);
        REQUIRE(binding1 == binding4);
    }
    REQUIRE(Rndr::Destroy());
}

TEST_CASE("Input context", "[input]")
{
    REQUIRE(Rndr::Init());
    SECTION("Create context")
    {
        const Rndr::InputContext context("TestContext");
        REQUIRE(context.GetName() == "TestContext");
    }
    SECTION("Add action")
    {
        Rndr::InputContext context("TestContext");
        REQUIRE(context.GetName() == "TestContext");
        const Rndr::InputAction action("TestAction");
        Rndr::InputBinding bindings[] = {{.primitive = Rndr::InputPrimitive::Keyboard_A,
                                          .trigger = Rndr::InputTrigger::ButtonPressed,
                                          .modifier = 1.0f}};
        const Rndr::InputCallback callback =
            [](Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, float value)
        {
            (void)primitive;
            (void)trigger;
            (void)value;
        };
        REQUIRE(context.AddAction(action, {callback, nullptr, bindings}));
        REQUIRE(context.ContainsAction(action));
        REQUIRE(context.GetActionCallback(action) != nullptr);
        REQUIRE(context.GetActionBindings(action)[0] == bindings[0]);
        SECTION("Add additional binding")
        {
            Rndr::InputBinding binding{.primitive = Rndr::InputPrimitive::Keyboard_D,
                                       .trigger = Rndr::InputTrigger::ButtonPressed,
                                       .modifier = 1.0f};
            REQUIRE(context.AddBindingToAction(action, binding));
            Rndr::Span<Rndr::InputBinding> reg_bindings = context.GetActionBindings(action);
            REQUIRE(reg_bindings.GetSize() == 2);
            REQUIRE(reg_bindings[0] == bindings[0]);
            REQUIRE(reg_bindings[1] == binding);
            SECTION("Remove binding")
            {
                REQUIRE(context.RemoveBindingFromAction(action, bindings[0]));
                reg_bindings = context.GetActionBindings(action);
                REQUIRE(reg_bindings.GetSize() == 1);
                REQUIRE(reg_bindings[0] == binding);
            }
        }
        SECTION("Add invalid binding")
        {
            Rndr::InputBinding binding{.primitive = Rndr::InputPrimitive::Keyboard_D,
                                       .trigger = Rndr::InputTrigger::AxisChangedRelative,
                                       .modifier = 1.0f};
            REQUIRE(!context.AddBindingToAction(action, binding));
            Rndr::Span<Rndr::InputBinding> reg_bindings = context.GetActionBindings(action);
            REQUIRE(reg_bindings.GetSize() == 1);
            REQUIRE(reg_bindings[0] == bindings[0]);
        }
    }
    REQUIRE(Rndr::Destroy());
}

TEST_CASE("Input system", "[input]")
{
    REQUIRE(Rndr::Init({.enable_input_system = true}));

    SECTION("Default context")
    {
        const Rndr::InputContext& context = Rndr::InputSystem::GetCurrentContext();
        REQUIRE(context.GetName() == "Default");
    }
    SECTION("Push context")
    {
        const Rndr::InputContext context("TestContext");
        REQUIRE(Rndr::InputSystem::PushContext(context));
        REQUIRE(Rndr::InputSystem::GetCurrentContext().GetName() == "TestContext");
        SECTION("Pop context")
        {
            Rndr::InputSystem::PopContext();
            REQUIRE(Rndr::InputSystem::GetCurrentContext().GetName() == "Default");
        }
    }
    SECTION("Pop default context")
    {
        REQUIRE(!Rndr::InputSystem::PopContext());
        REQUIRE(Rndr::InputSystem::GetCurrentContext().GetName() == "Default");
    }
    SECTION("Submit events")
    {
        REQUIRE(Rndr::InputSystem::SubmitButtonEvent(nullptr,
                                                     Rndr::InputPrimitive::Keyboard_A,
                                                     Rndr::InputTrigger::ButtonPressed));
        REQUIRE(Rndr::InputSystem::SubmitMousePositionEvent(nullptr, {100, 100}, {1024, 768}));
        REQUIRE(Rndr::InputSystem::SubmitMouseWheelEvent(nullptr, 1));
        REQUIRE(
            Rndr::InputSystem::SubmitRelativeMousePositionEvent(nullptr, {100, 100}, {1024, 768}));
    }
    SECTION("Submit invalid events")
    {
        REQUIRE(!Rndr::InputSystem::SubmitMousePositionEvent(nullptr, {100, 100}, {0, 0}));
        REQUIRE(!Rndr::InputSystem::SubmitRelativeMousePositionEvent(nullptr, {100, 100}, {0, 0}));
    }
    SECTION("Process button events with same bindings but different actions")
    {
        Rndr::InputContext context("TestContext");
        const Rndr::InputAction action1("TestAction1");
        const Rndr::InputAction action2("TestAction2");
        Rndr::InputBinding bindings[] = {{.primitive = Rndr::InputPrimitive::Keyboard_A,
                                          .trigger = Rndr::InputTrigger::ButtonPressed,
                                          .modifier = 2.0f}};
        float value1 = 0;
        float value2 = 0;
        const Rndr::InputCallback callback1 =
            [&value1](Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, float val)
        {
            (void)primitive;
            (void)trigger;
            (void)value1;
            value1 += val;
        };
        const Rndr::InputCallback callback2 =
            [&value2](Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, float val)
        {
            (void)primitive;
            (void)trigger;
            (void)value2;
            value2 -= val;
        };
        REQUIRE(context.AddAction(action1, {callback1, nullptr, bindings}));
        REQUIRE(context.AddAction(action2, {callback2, nullptr, bindings}));
        REQUIRE(Rndr::InputSystem::PushContext(context));
        REQUIRE(Rndr::InputSystem::SubmitButtonEvent(nullptr,
                                                     Rndr::InputPrimitive::Keyboard_A,
                                                     Rndr::InputTrigger::ButtonPressed));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 2.0f);
        REQUIRE(value2 == -2.0f);
        REQUIRE(Rndr::InputSystem::SubmitButtonEvent(nullptr,
                                                     Rndr::InputPrimitive::Keyboard_A,
                                                     Rndr::InputTrigger::ButtonReleased));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 2.0f);
        REQUIRE(value2 == -2.0f);
        REQUIRE(Rndr::InputSystem::SubmitButtonEvent(nullptr,
                                                     Rndr::InputPrimitive::Keyboard_D,
                                                     Rndr::InputTrigger::ButtonPressed));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 2.0f);
        REQUIRE(value2 == -2.0f);
        REQUIRE(Rndr::InputSystem::SubmitButtonEvent(nullptr,
                                                     Rndr::InputPrimitive::Keyboard_A,
                                                     Rndr::InputTrigger::ButtonPressed));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 4.0f);
        REQUIRE(value2 == -4.0f);
    }
    SECTION("Process mouse position events")
    {
        Rndr::InputContext context("TestContext");
        const Rndr::InputAction action("TestAction");
        Rndr::InputBinding bindings[] = {{.primitive = Rndr::InputPrimitive::Mouse_AxisX,
                                          .trigger = Rndr::InputTrigger::AxisChangedAbsolute,
                                          .modifier = 2.0f},
                                         {.primitive = Rndr::InputPrimitive::Mouse_AxisY,
                                          .trigger = Rndr::InputTrigger::AxisChangedAbsolute,
                                          .modifier = 2.0f}};
        float value1 = 0;
        float value2 = 0;
        const Rndr::InputCallback callback = [&value1, &value2](Rndr::InputPrimitive primitive,
                                                                Rndr::InputTrigger trigger,
                                                                float val)
        {
            (void)trigger;
            if (primitive == Rndr::InputPrimitive::Mouse_AxisX)
            {
                value1 += val;
            }
            else if (primitive == Rndr::InputPrimitive::Mouse_AxisY)
            {
                value2 += val;
            }
        };
        REQUIRE(context.AddAction(action, {callback, nullptr, bindings}));
        REQUIRE(Rndr::InputSystem::PushContext(context));
        REQUIRE(Rndr::InputSystem::SubmitMousePositionEvent(nullptr, {100, 200}, {1024, 768}));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 200.0f);
        REQUIRE(value2 == 400.0f);
        REQUIRE(Rndr::InputSystem::SubmitMousePositionEvent(nullptr, {100, 200}, {1024, 768}));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 400.0f);
        REQUIRE(value2 == 800.0f);
        REQUIRE(!Rndr::InputSystem::SubmitMousePositionEvent(nullptr, {100, 200}, {0, 0}));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 400.0f);
        REQUIRE(value2 == 800.0f);
    }
    SECTION("Process relative mouse event")
    {
        Rndr::InputContext context("TestContext");
        const Rndr::InputAction action("TestAction");
        Rndr::InputBinding bindings[] = {{.primitive = Rndr::InputPrimitive::Mouse_AxisX,
                                          .trigger = Rndr::InputTrigger::AxisChangedRelative,
                                          .modifier = 2.0f},
                                         {.primitive = Rndr::InputPrimitive::Mouse_AxisY,
                                          .trigger = Rndr::InputTrigger::AxisChangedRelative,
                                          .modifier = 2.0f}};
        float value1 = 0;
        float value2 = 0;
        const Rndr::InputCallback callback = [&value1, &value2](Rndr::InputPrimitive primitive,
                                                                Rndr::InputTrigger trigger,
                                                                float val)
        {
            (void)trigger;
            if (primitive == Rndr::InputPrimitive::Mouse_AxisX)
            {
                value1 += val;
            }
            else if (primitive == Rndr::InputPrimitive::Mouse_AxisY)
            {
                value2 += val;
            }
        };
        REQUIRE(context.AddAction(action, {callback, nullptr, bindings}));
        REQUIRE(Rndr::InputSystem::PushContext(context));
        REQUIRE(
            Rndr::InputSystem::SubmitRelativeMousePositionEvent(nullptr, {100, 200}, {1024, 768}));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 200.0f);
        REQUIRE(value2 == 400.0f);
        REQUIRE(
            Rndr::InputSystem::SubmitRelativeMousePositionEvent(nullptr, {100, 200}, {1024, 768}));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 400.0f);
        REQUIRE(value2 == 800.0f);
        REQUIRE(!Rndr::InputSystem::SubmitRelativeMousePositionEvent(nullptr, {100, 200}, {0, 0}));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 400.0f);
        REQUIRE(value2 == 800.0f);
    }
    SECTION("Process mouse wheel event")
    {
        Rndr::InputContext context("TestContext");
        const Rndr::InputAction action("TestAction");
        Rndr::InputBinding bindings[] = {{.primitive = Rndr::InputPrimitive::Mouse_AxisWheel,
                                          .trigger = Rndr::InputTrigger::AxisChangedRelative,
                                          .modifier = 2.0f}};
        float value1 = 0;
        const Rndr::InputCallback callback =
            [&value1](Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, float val)
        {
            (void)trigger;
            if (primitive == Rndr::InputPrimitive::Mouse_AxisWheel)
            {
                value1 += val;
            }
        };
        REQUIRE(context.AddAction(action, {callback, nullptr, bindings}));
        REQUIRE(Rndr::InputSystem::PushContext(context));
        REQUIRE(Rndr::InputSystem::SubmitMouseWheelEvent(nullptr, 100));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 200.0f);
        REQUIRE(Rndr::InputSystem::SubmitMouseWheelEvent(nullptr, 100));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 400.0f);
        REQUIRE(Rndr::InputSystem::SubmitMouseWheelEvent(nullptr, -100));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 200.0f);
    }
    SECTION("Process button event but filter with native window handle")
    {
        Rndr::InputContext context("TestContext");
        const Rndr::InputAction action("TestAction");
        Rndr::InputBinding bindings[] = {{.primitive = Rndr::InputPrimitive::Mouse_LeftButton,
                                          .trigger = Rndr::InputTrigger::ButtonPressed}};
        float value1 = 0;
        const Rndr::InputCallback callback =
            [&value1](Rndr::InputPrimitive primitive, Rndr::InputTrigger trigger, float val)
        {
            (void)trigger;
            if (primitive == Rndr::InputPrimitive::Mouse_LeftButton)
            {
                value1 += val;
            }
        };
        Rndr::NativeWindowHandle handle = reinterpret_cast<Rndr::NativeWindowHandle>(0x1234);
        Rndr::NativeWindowHandle bad_handle = reinterpret_cast<Rndr::NativeWindowHandle>(0x5678);
        REQUIRE(context.AddAction(action, {callback, handle, bindings}));
        REQUIRE(Rndr::InputSystem::PushContext(context));
        REQUIRE(Rndr::InputSystem::SubmitButtonEvent(handle,
                                                     Rndr::InputPrimitive::Mouse_LeftButton,
                                                     Rndr::InputTrigger::ButtonPressed));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 1.0f);
        REQUIRE(Rndr::InputSystem::SubmitButtonEvent(handle,
                                                     Rndr::InputPrimitive::Mouse_LeftButton,
                                                     Rndr::InputTrigger::ButtonPressed));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 2.0f);
        REQUIRE(Rndr::InputSystem::SubmitButtonEvent(bad_handle,
                                                     Rndr::InputPrimitive::Mouse_LeftButton,
                                                     Rndr::InputTrigger::ButtonPressed));
        REQUIRE(Rndr::InputSystem::ProcessEvents(1));
        REQUIRE(value1 == 2.0f);
    }
    SECTION("Check is primitive a button")
    {
        REQUIRE(Rndr::InputSystem::IsButton(Rndr::InputPrimitive::Keyboard_A));
        REQUIRE(Rndr::InputSystem::IsButton(Rndr::InputPrimitive::Keyboard_B));
        REQUIRE(Rndr::InputSystem::IsButton(Rndr::InputPrimitive::Keyboard_Esc));
        REQUIRE(Rndr::InputSystem::IsButton(Rndr::InputPrimitive::Mouse_LeftButton));
        REQUIRE(Rndr::InputSystem::IsButton(Rndr::InputPrimitive::Mouse_MiddleButton));
        REQUIRE(Rndr::InputSystem::IsButton(Rndr::InputPrimitive::Mouse_RightButton));
        REQUIRE(!Rndr::InputSystem::IsButton(Rndr::InputPrimitive::Mouse_AxisX));
        REQUIRE(Rndr::InputSystem::IsKeyboardButton(Rndr::InputPrimitive::Keyboard_A));
        REQUIRE(Rndr::InputSystem::IsKeyboardButton(Rndr::InputPrimitive::Keyboard_B));
        REQUIRE(Rndr::InputSystem::IsKeyboardButton(Rndr::InputPrimitive::Keyboard_Esc));
        REQUIRE(!Rndr::InputSystem::IsKeyboardButton(Rndr::InputPrimitive::Mouse_LeftButton));
        REQUIRE(!Rndr::InputSystem::IsKeyboardButton(Rndr::InputPrimitive::Mouse_MiddleButton));
        REQUIRE(!Rndr::InputSystem::IsKeyboardButton(Rndr::InputPrimitive::Mouse_RightButton));
        REQUIRE(!Rndr::InputSystem::IsKeyboardButton(Rndr::InputPrimitive::Mouse_AxisX));
        REQUIRE(Rndr::InputSystem::IsMouseButton(Rndr::InputPrimitive::Mouse_LeftButton));
        REQUIRE(Rndr::InputSystem::IsMouseButton(Rndr::InputPrimitive::Mouse_MiddleButton));
        REQUIRE(Rndr::InputSystem::IsMouseButton(Rndr::InputPrimitive::Mouse_RightButton));
        REQUIRE(!Rndr::InputSystem::IsMouseButton(Rndr::InputPrimitive::Keyboard_A));
        REQUIRE(!Rndr::InputSystem::IsMouseButton(Rndr::InputPrimitive::Keyboard_B));
        REQUIRE(!Rndr::InputSystem::IsMouseButton(Rndr::InputPrimitive::Keyboard_Esc));
    }
    SECTION("Is primitive axis")
    {
        REQUIRE(Rndr::InputSystem::IsAxis(Rndr::InputPrimitive::Mouse_AxisX));
        REQUIRE(Rndr::InputSystem::IsAxis(Rndr::InputPrimitive::Mouse_AxisY));
        REQUIRE(Rndr::InputSystem::IsAxis(Rndr::InputPrimitive::Mouse_AxisWheel));
        REQUIRE(!Rndr::InputSystem::IsAxis(Rndr::InputPrimitive::Keyboard_A));
        REQUIRE(!Rndr::InputSystem::IsAxis(Rndr::InputPrimitive::Keyboard_B));
        REQUIRE(!Rndr::InputSystem::IsAxis(Rndr::InputPrimitive::Keyboard_Esc));
        REQUIRE(!Rndr::InputSystem::IsAxis(Rndr::InputPrimitive::Mouse_LeftButton));
        REQUIRE(!Rndr::InputSystem::IsAxis(Rndr::InputPrimitive::Mouse_MiddleButton));
        REQUIRE(!Rndr::InputSystem::IsAxis(Rndr::InputPrimitive::Mouse_RightButton));
    }
    SECTION("Is primitive mouse wheel axis")
    {
        REQUIRE(Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive::Mouse_AxisWheel));
        REQUIRE(!Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive::Mouse_AxisX));
        REQUIRE(!Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive::Mouse_AxisY));
        REQUIRE(!Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive::Keyboard_A));
        REQUIRE(!Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive::Keyboard_B));
        REQUIRE(!Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive::Keyboard_Esc));
        REQUIRE(!Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive::Mouse_LeftButton));
        REQUIRE(!Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive::Mouse_MiddleButton));
        REQUIRE(!Rndr::InputSystem::IsMouseWheelAxis(Rndr::InputPrimitive::Mouse_RightButton));
    }
    SECTION("Is binding valid")
    {
        Rndr::InputBinding binding = {.primitive = Rndr::InputPrimitive::Mouse_LeftButton,
                                      .trigger = Rndr::InputTrigger::ButtonPressed};
        REQUIRE(Rndr::InputSystem::IsBindingValid(binding));
        binding.primitive = Rndr::InputPrimitive::Mouse_AxisX;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));
        binding.primitive = Rndr::InputPrimitive::Mouse_AxisY;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));
        binding.primitive = Rndr::InputPrimitive::Mouse_AxisWheel;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));
        binding.primitive = Rndr::InputPrimitive::Keyboard_A;
        REQUIRE(Rndr::InputSystem::IsBindingValid(binding));
        binding.primitive = Rndr::InputPrimitive::Keyboard_B;
        REQUIRE(Rndr::InputSystem::IsBindingValid(binding));
        binding.primitive = Rndr::InputPrimitive::Keyboard_Esc;
        REQUIRE(Rndr::InputSystem::IsBindingValid(binding));

        binding.primitive = Rndr::InputPrimitive::Mouse_AxisWheel;
        binding.trigger = Rndr::InputTrigger::AxisChangedRelative;
        REQUIRE(Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::AxisChangedAbsolute;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::ButtonPressed;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));

        binding.primitive = Rndr::InputPrimitive::Mouse_AxisX;
        binding.trigger = Rndr::InputTrigger::AxisChangedRelative;
        REQUIRE(Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::AxisChangedAbsolute;
        REQUIRE(Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::ButtonPressed;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::ButtonReleased;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::ButtonDoubleClick;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));

        binding.primitive = Rndr::InputPrimitive::Mouse_AxisY;
        binding.trigger = Rndr::InputTrigger::AxisChangedRelative;
        REQUIRE(Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::AxisChangedAbsolute;
        REQUIRE(Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::ButtonPressed;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::ButtonReleased;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));
        binding.trigger = Rndr::InputTrigger::ButtonDoubleClick;
        REQUIRE(!Rndr::InputSystem::IsBindingValid(binding));
    }

    REQUIRE(Rndr::Destroy());
}
