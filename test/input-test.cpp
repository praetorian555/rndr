#if RNDR_OLD_INPUT_SYSTEM

// Old input system tests are disabled since the old tests were already commented out.
// This file is reserved for new input system tests.

#else

#include <catch2/catch2.hpp>

#include "rndr/input-system.hpp"

static const auto& g_fake_window = *reinterpret_cast<const Rndr::GenericWindow*>(1);

TEST_CASE("Input system keyboard binding", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;
    Rndr::Trigger received_trigger = Rndr::Trigger::Released;

    context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger trigger, bool /*is_repeat*/)
        {
            callback_fired = true;
            received_trigger = trigger;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    SECTION("Pressing bound key triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
        REQUIRE(received_trigger == Rndr::Trigger::Pressed);
    }

    SECTION("Pressing unbound key does not trigger callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Return, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }

    SECTION("Release trigger does not fire on press event")
    {
        context.RemoveAction("Jump");

        context.AddAction("JumpRelease")
            .OnButton([&](Rndr::Trigger trigger, bool /*is_repeat*/)
            {
                callback_fired = true;
                received_trigger = trigger;
            })
            .Bind(Rndr::Key::Space, Rndr::Trigger::Released);

        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }

    SECTION("Release event triggers release binding")
    {
        context.RemoveAction("Jump");

        context.AddAction("JumpRelease")
            .OnButton([&](Rndr::Trigger trigger, bool /*is_repeat*/)
            {
                callback_fired = true;
                received_trigger = trigger;
            })
            .Bind(Rndr::Key::Space, Rndr::Trigger::Released);

        input_system.OnButtonUp(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
        REQUIRE(received_trigger == Rndr::Trigger::Released);
    }
}

TEST_CASE("Multiple keys bound to same action", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("MoveForward")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .Bind(Rndr::Key::W, Rndr::Trigger::Pressed)
        .Bind(Rndr::Key::UpArrow, Rndr::Trigger::Pressed);

    SECTION("First key triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::W, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
    }

    SECTION("Second key also triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::UpArrow, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
    }

    SECTION("Both keys in same frame each trigger callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::W, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::UpArrow, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 2);
    }
}

TEST_CASE("Repeated key events pass is_repeat flag", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool received_is_repeat = false;

    context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool is_repeat)
        {
            received_is_repeat = is_repeat;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    SECTION("Non-repeated key has is_repeat false")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(received_is_repeat);
    }

    SECTION("Repeated key has is_repeat true")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, true);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(received_is_repeat);
    }
}

TEST_CASE("Same key with different triggers on separate actions", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool press_fired = false;
    bool release_fired = false;

    context.AddAction("JumpPress")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            press_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    context.AddAction("JumpRelease")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            release_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Released);

    SECTION("Press event only fires press action")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(press_fired);
        REQUIRE_FALSE(release_fired);
    }

    SECTION("Release event only fires release action")
    {
        input_system.OnButtonUp(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(press_fired);
        REQUIRE(release_fired);
    }
}

TEST_CASE("Modifier key: Ctrl+Key binding", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;

    context.AddAction("SelectAll")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_fired = true;
        })
        .Bind(Rndr::Key::A, Rndr::Trigger::Pressed, Rndr::Modifiers::Ctrl);

    SECTION("Ctrl+A triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftCtrl, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }

    SECTION("A without Ctrl does not trigger callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }

    SECTION("RightCtrl+A also triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::RightCtrl, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }

    SECTION("Generic Control primitive also sets Ctrl modifier")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Control, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }
}

TEST_CASE("Modifier key: Shift+Key binding", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;

    context.AddAction("ShiftAction")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_fired = true;
        })
        .Bind(Rndr::Key::A, Rndr::Trigger::Pressed, Rndr::Modifiers::Shift);

    SECTION("Shift+A triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftShift, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }

    SECTION("A without Shift does not trigger callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }

    SECTION("RightShift+A also triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::RightShift, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }
}

TEST_CASE("Modifier key: Alt+Key binding", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;

    context.AddAction("AltAction")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_fired = true;
        })
        .Bind(Rndr::Key::A, Rndr::Trigger::Pressed, Rndr::Modifiers::Alt);

    SECTION("Alt+A triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftAlt, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }

    SECTION("A without Alt does not trigger callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }

    SECTION("RightAlt+A also triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::RightAlt, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }
}

TEST_CASE("Combined modifiers: Ctrl+Shift+Key", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;

    context.AddAction("CtrlShiftA")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_fired = true;
        })
        .Bind(Rndr::Key::A, Rndr::Trigger::Pressed, Rndr::Modifiers::Ctrl | Rndr::Modifiers::Shift);

    SECTION("Ctrl+Shift+A triggers callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftCtrl, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftShift, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }

    SECTION("Only Ctrl+A does not trigger Ctrl+Shift+A binding")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftCtrl, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }

    SECTION("Only Shift+A does not trigger Ctrl+Shift+A binding")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftShift, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }

    SECTION("A alone does not trigger Ctrl+Shift+A binding")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }
}

TEST_CASE("No-modifier binding fires regardless of held modifiers", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;

    context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    SECTION("Space fires without modifiers")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }

    SECTION("Space fires even with Ctrl held")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftCtrl, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }

    SECTION("Space fires even with Ctrl+Shift+Alt held")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftCtrl, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftShift, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftAlt, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }
}

TEST_CASE("Releasing modifier clears modifier state", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("CtrlA")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .Bind(Rndr::Key::A, Rndr::Trigger::Pressed, Rndr::Modifiers::Ctrl);

    SECTION("Ctrl down, Ctrl up, then A does not trigger")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftCtrl, false);
        input_system.ProcessSystemEvents(0.0f);

        input_system.OnButtonUp(g_fake_window, Rndr::InputPrimitive::LeftCtrl, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 0);
    }

    SECTION("Ctrl down, A fires, Ctrl up, A again does not fire")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::LeftCtrl, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);

        input_system.OnButtonUp(g_fake_window, Rndr::InputPrimitive::LeftCtrl, false);
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
    }
}

TEST_CASE("Input system mouse button binding", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;
    Rndr::MouseButton received_button = Rndr::MouseButton::Left;
    Rndr::Trigger received_trigger = Rndr::Trigger::Released;
    Rndr::Vector2i received_position{};

    context.AddAction("Shoot")
        .OnMouseButton([&](Rndr::MouseButton button, Rndr::Trigger trigger, const Rndr::Vector2i& pos)
        {
            callback_fired = true;
            received_button = button;
            received_trigger = trigger;
            received_position = pos;
        })
        .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Pressed);

    SECTION("Left mouse button press triggers callback")
    {
        input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_LeftButton, Rndr::Vector2i{100, 200});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
        REQUIRE(received_button == Rndr::MouseButton::Left);
        REQUIRE(received_trigger == Rndr::Trigger::Pressed);
        REQUIRE(received_position[0] == 100);
        REQUIRE(received_position[1] == 200);
    }

    SECTION("Right mouse button does not trigger left button binding")
    {
        input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_RightButton, Rndr::Vector2i{50, 60});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }

    SECTION("Mouse button release triggers release binding")
    {
        context.RemoveAction("Shoot");

        context.AddAction("ShootRelease")
            .OnMouseButton([&](Rndr::MouseButton button, Rndr::Trigger trigger, const Rndr::Vector2i& pos)
            {
                callback_fired = true;
                received_button = button;
                received_trigger = trigger;
                received_position = pos;
            })
            .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Released);

        input_system.OnMouseButtonUp(g_fake_window, Rndr::InputPrimitive::Mouse_LeftButton, Rndr::Vector2i{10, 20});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
        REQUIRE(received_trigger == Rndr::Trigger::Released);
    }
}

TEST_CASE("Middle, X1, X2 mouse button bindings", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    Rndr::MouseButton received_button{};
    int callback_count = 0;

    auto callback = [&](Rndr::MouseButton button, Rndr::Trigger /*trigger*/, const Rndr::Vector2i& /*pos*/)
    {
        received_button = button;
        callback_count++;
    };

    SECTION("Middle mouse button triggers callback")
    {
        context.AddAction("MiddleClick")
            .OnMouseButton(callback)
            .Bind(Rndr::MouseButton::Middle, Rndr::Trigger::Pressed);

        input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_MiddleButton, Rndr::Vector2i{0, 0});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
        REQUIRE(received_button == Rndr::MouseButton::Middle);
    }

    SECTION("X1 mouse button triggers callback")
    {
        context.AddAction("X1Click")
            .OnMouseButton(callback)
            .Bind(Rndr::MouseButton::X1, Rndr::Trigger::Pressed);

        input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_XButton1, Rndr::Vector2i{0, 0});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
        REQUIRE(received_button == Rndr::MouseButton::X1);
    }

    SECTION("X2 mouse button triggers callback")
    {
        context.AddAction("X2Click")
            .OnMouseButton(callback)
            .Bind(Rndr::MouseButton::X2, Rndr::Trigger::Pressed);

        input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_XButton2, Rndr::Vector2i{0, 0});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
        REQUIRE(received_button == Rndr::MouseButton::X2);
    }
}

TEST_CASE("Mouse button cursor position passed correctly", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    Rndr::Vector2i received_position{};

    context.AddAction("Click")
        .OnMouseButton([&](Rndr::MouseButton /*button*/, Rndr::Trigger /*trigger*/, const Rndr::Vector2i& pos)
        {
            received_position = pos;
        })
        .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Pressed);

    SECTION("Positive coordinates")
    {
        input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_LeftButton, Rndr::Vector2i{1920, 1080});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(received_position[0] == 1920);
        REQUIRE(received_position[1] == 1080);
    }

    SECTION("Zero coordinates")
    {
        input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_LeftButton, Rndr::Vector2i{0, 0});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(received_position[0] == 0);
        REQUIRE(received_position[1] == 0);
    }

    SECTION("Negative coordinates")
    {
        input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_LeftButton, Rndr::Vector2i{-10, -20});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(received_position[0] == -10);
        REQUIRE(received_position[1] == -20);
    }
}

TEST_CASE("Double click is treated as press event", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;
    Rndr::Trigger received_trigger = Rndr::Trigger::Released;

    context.AddAction("Click")
        .OnMouseButton([&](Rndr::MouseButton /*button*/, Rndr::Trigger trigger, const Rndr::Vector2i& /*pos*/)
        {
            callback_fired = true;
            received_trigger = trigger;
        })
        .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Pressed);

    input_system.OnMouseDoubleClick(g_fake_window, Rndr::InputPrimitive::Mouse_LeftButton, Rndr::Vector2i{50, 50});
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(callback_fired);
    REQUIRE(received_trigger == Rndr::Trigger::Pressed);
}

TEST_CASE("Input system mouse move binding", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    Rndr::f32 received_delta_x = 0.0f;
    Rndr::f32 received_delta_y = 0.0f;
    int callback_count = 0;

    context.AddAction("Look")
        .OnMousePosition([&](Rndr::MouseAxis axis, Rndr::f32 delta)
        {
            callback_count++;
            if (axis == Rndr::MouseAxis::X)
            {
                received_delta_x = delta;
            }
            else if (axis == Rndr::MouseAxis::Y)
            {
                received_delta_y = delta;
            }
        })
        .Bind(Rndr::MouseAxis::X)
        .Bind(Rndr::MouseAxis::Y);

    SECTION("Mouse move dispatches both axes")
    {
        input_system.OnMouseMove(g_fake_window, 5.0f, -3.0f);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 2);
        REQUIRE(received_delta_x == 5.0f);
        REQUIRE(received_delta_y == -3.0f);
    }

    SECTION("Zero delta axis is not dispatched")
    {
        input_system.OnMouseMove(g_fake_window, 2.0f, 0.0f);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
        REQUIRE(received_delta_x == 2.0f);
        REQUIRE(received_delta_y == 0.0f);
    }
}

TEST_CASE("Mouse move binding only X axis ignores Y", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;
    Rndr::MouseAxis received_axis{};

    context.AddAction("LookX")
        .OnMousePosition([&](Rndr::MouseAxis axis, Rndr::f32 /*delta*/)
        {
            callback_count++;
            received_axis = axis;
        })
        .Bind(Rndr::MouseAxis::X);

    SECTION("X movement fires callback")
    {
        input_system.OnMouseMove(g_fake_window, 5.0f, 0.0f);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
        REQUIRE(received_axis == Rndr::MouseAxis::X);
    }

    SECTION("Y-only movement does not fire callback")
    {
        input_system.OnMouseMove(g_fake_window, 0.0f, 5.0f);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 0);
    }

    SECTION("Both axes moving only fires X")
    {
        input_system.OnMouseMove(g_fake_window, 3.0f, 7.0f);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
        REQUIRE(received_axis == Rndr::MouseAxis::X);
    }
}

TEST_CASE("Mouse move binding only Y axis ignores X", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;
    Rndr::MouseAxis received_axis{};

    context.AddAction("LookY")
        .OnMousePosition([&](Rndr::MouseAxis axis, Rndr::f32 /*delta*/)
        {
            callback_count++;
            received_axis = axis;
        })
        .Bind(Rndr::MouseAxis::Y);

    SECTION("Y movement fires callback")
    {
        input_system.OnMouseMove(g_fake_window, 0.0f, 5.0f);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
        REQUIRE(received_axis == Rndr::MouseAxis::Y);
    }

    SECTION("X-only movement does not fire callback")
    {
        input_system.OnMouseMove(g_fake_window, 5.0f, 0.0f);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 0);
    }

    SECTION("Both axes moving only fires Y")
    {
        input_system.OnMouseMove(g_fake_window, 3.0f, 7.0f);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
        REQUIRE(received_axis == Rndr::MouseAxis::Y);
    }
}

TEST_CASE("Multiple mouse move events dispatched individually", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    Rndr::f32 total_delta_x = 0.0f;
    int callback_count = 0;

    context.AddAction("LookX")
        .OnMousePosition([&](Rndr::MouseAxis axis, Rndr::f32 delta)
        {
            if (axis == Rndr::MouseAxis::X)
            {
                total_delta_x += delta;
                callback_count++;
            }
        })
        .Bind(Rndr::MouseAxis::X);

    input_system.OnMouseMove(g_fake_window, 2.0f, 0.0f);
    input_system.OnMouseMove(g_fake_window, 3.0f, 0.0f);
    input_system.OnMouseMove(g_fake_window, -1.0f, 0.0f);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(callback_count == 3);
    REQUIRE(total_delta_x == 4.0f);
}

TEST_CASE("Input system mouse wheel binding", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;
    Rndr::f32 received_delta_y = 0.0f;

    context.AddAction("Zoom")
        .OnMouseWheel([&](Rndr::f32 /*delta_x*/, Rndr::f32 delta_y)
        {
            callback_fired = true;
            received_delta_y = delta_y;
        })
        .Bind(Rndr::MouseAxis::WheelY);

    SECTION("Mouse wheel event triggers callback")
    {
        input_system.OnMouseWheel(g_fake_window, 120.0f, Rndr::Vector2i{0, 0});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
        REQUIRE(received_delta_y == 120.0f);
    }

    SECTION("Negative wheel delta is passed through")
    {
        input_system.OnMouseWheel(g_fake_window, -120.0f, Rndr::Vector2i{0, 0});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
        REQUIRE(received_delta_y == -120.0f);
    }
}

TEST_CASE("Text input: character event triggers text callback", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    Rndr::uchar32 received_char = 0;
    bool callback_fired = false;

    context.AddAction("TextInput")
        .OnText([&](Rndr::uchar32 character)
        {
            callback_fired = true;
            received_char = character;
        })
        .BindText();

    SECTION("Single character fires callback")
    {
        input_system.OnCharacter(g_fake_window, U'A', false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
        REQUIRE(received_char == U'A');
    }

    SECTION("Unicode character fires callback")
    {
        input_system.OnCharacter(g_fake_window, U'\u00E9', false);  // é
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
        REQUIRE(received_char == U'\u00E9');
    }
}

TEST_CASE("Text input: multiple characters each fire callback", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    Opal::DynamicArray<Rndr::uchar32> received_chars;

    context.AddAction("TextInput")
        .OnText([&](Rndr::uchar32 character)
        {
            received_chars.PushBack(character);
        })
        .BindText();

    input_system.OnCharacter(g_fake_window, U'H', false);
    input_system.OnCharacter(g_fake_window, U'i', false);
    input_system.OnCharacter(g_fake_window, U'!', false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(received_chars.GetSize() == 3);
    REQUIRE(received_chars[0] == U'H');
    REQUIRE(received_chars[1] == U'i');
    REQUIRE(received_chars[2] == U'!');
}

TEST_CASE("Text input: text binding does not fire on key events", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool text_callback_fired = false;

    context.AddAction("TextInput")
        .OnText([&](Rndr::uchar32 /*character*/)
        {
            text_callback_fired = true;
        })
        .BindText();

    SECTION("Key down does not fire text callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(text_callback_fired);
    }

    SECTION("Key up does not fire text callback")
    {
        input_system.OnButtonUp(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(text_callback_fired);
    }
}

TEST_CASE("Text input: key binding does not fire on character events", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool button_callback_fired = false;

    context.AddAction("PressA")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            button_callback_fired = true;
        })
        .Bind(Rndr::Key::A, Rndr::Trigger::Pressed);

    input_system.OnCharacter(g_fake_window, U'A', false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE_FALSE(button_callback_fired);
}

TEST_CASE("Hold binding fires after duration elapses", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("Interact")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindHold(Rndr::Key::E, 0.5f);

    // Press the key.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.ProcessSystemEvents(0.0f);
    REQUIRE(callback_count == 0);

    SECTION("Does not fire before duration")
    {
        input_system.ProcessSystemEvents(0.3f);
        REQUIRE(callback_count == 0);
    }

    SECTION("Fires exactly when duration is reached")
    {
        input_system.ProcessSystemEvents(0.5f);
        REQUIRE(callback_count == 1);
    }

    SECTION("Fires after duration exceeded")
    {
        input_system.ProcessSystemEvents(1.0f);
        REQUIRE(callback_count == 1);
    }
}

TEST_CASE("Hold binding fires exactly once", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("Interact")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindHold(Rndr::Key::E, 0.5f);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.ProcessSystemEvents(0.6f);
    REQUIRE(callback_count == 1);

    // Keep holding, should not fire again.
    input_system.ProcessSystemEvents(0.5f);
    REQUIRE(callback_count == 1);

    input_system.ProcessSystemEvents(1.0f);
    REQUIRE(callback_count == 1);
}

TEST_CASE("Hold binding: releasing key before duration cancels hold", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("Interact")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindHold(Rndr::Key::E, 0.5f);

    // Press and hold partway.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.ProcessSystemEvents(0.3f);
    REQUIRE(callback_count == 0);

    // Release before threshold.
    input_system.OnButtonUp(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.ProcessSystemEvents(0.3f);
    REQUIRE(callback_count == 0);

    // More time passes, still no fire.
    input_system.ProcessSystemEvents(1.0f);
    REQUIRE(callback_count == 0);
}

TEST_CASE("Hold binding: release and re-press restarts timer", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("Interact")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindHold(Rndr::Key::E, 0.5f);

    // Press and hold partway.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.ProcessSystemEvents(0.4f);
    REQUIRE(callback_count == 0);

    // Release.
    input_system.OnButtonUp(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.ProcessSystemEvents(0.0f);

    // Re-press. Timer should restart.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.ProcessSystemEvents(0.3f);
    REQUIRE(callback_count == 0);

    // Not enough total time from re-press.
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 0);

    // Now enough time from re-press (0.3 + 0.1 + 0.2 = 0.6 > 0.5).
    input_system.ProcessSystemEvents(0.2f);
    REQUIRE(callback_count == 1);
}

TEST_CASE("Hold binding: timer accumulates across multiple frames", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("Interact")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindHold(Rndr::Key::E, 0.5f);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);

    // Accumulate in small increments.
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 0);
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 0);
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 0);
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 0);
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 1);
}

TEST_CASE("Hold binding: small duration fires on next frame", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("QuickHold")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindHold(Rndr::Key::E, 0.001f);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.ProcessSystemEvents(0.016f);  // ~1 frame at 60fps

    REQUIRE(callback_count == 1);
}

TEST_CASE("Hold binding: multiple holds on different keys", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int short_hold_count = 0;
    int long_hold_count = 0;

    context.AddAction("ShortHold")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            short_hold_count++;
        })
        .BindHold(Rndr::Key::E, 0.2f);

    context.AddAction("LongHold")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            long_hold_count++;
        })
        .BindHold(Rndr::Key::F, 1.0f);

    // Press both keys.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::F, false);
    input_system.ProcessSystemEvents(0.0f);

    SECTION("Short hold fires first, long hold not yet")
    {
        input_system.ProcessSystemEvents(0.3f);
        REQUIRE(short_hold_count == 1);
        REQUIRE(long_hold_count == 0);
    }

    SECTION("Both fire after enough time")
    {
        input_system.ProcessSystemEvents(1.0f);
        REQUIRE(short_hold_count == 1);
        REQUIRE(long_hold_count == 1);
    }
}

TEST_CASE("Hold binding: unbound key does not affect hold", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("Interact")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindHold(Rndr::Key::E, 0.5f);

    // Press a different key.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::W, false);
    input_system.ProcessSystemEvents(1.0f);

    REQUIRE(callback_count == 0);
}

TEST_CASE("Sequential combo: correct sequence triggers callback", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::UpArrow, Rndr::Key::UpArrow, Rndr::Key::DownArrow};
    context.AddAction("SecretCode")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.5f);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::UpArrow, false);
    input_system.ProcessSystemEvents(0.1f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::UpArrow, false);
    input_system.ProcessSystemEvents(0.1f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::DownArrow, false);
    input_system.ProcessSystemEvents(0.1f);

    REQUIRE(callback_count == 1);
}

TEST_CASE("Sequential combo: wrong key resets combo", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B, Rndr::Key::C};
    context.AddAction("Combo")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.5f);

    // A, then wrong key X.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.ProcessSystemEvents(0.1f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::X, false);
    input_system.ProcessSystemEvents(0.1f);

    // Combo should be reset, so B,C won't complete it.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.1f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::C, false);
    input_system.ProcessSystemEvents(0.1f);

    REQUIRE(callback_count == 0);
}

TEST_CASE("Sequential combo: can be triggered again after completion", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B};
    context.AddAction("Combo")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.5f);

    // First completion.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.ProcessSystemEvents(0.1f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 1);

    // Second completion.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.ProcessSystemEvents(0.1f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 2);
}

TEST_CASE("Sequential combo: times out between steps", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B};
    context.AddAction("Combo")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.3f);

    // Press A.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.ProcessSystemEvents(0.0f);

    // Wait longer than timeout.
    input_system.ProcessSystemEvents(0.4f);

    // Press B — combo should have been reset.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(callback_count == 0);
}

TEST_CASE("Sequential combo: 2-key combo works", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::W, Rndr::Key::S};
    context.AddAction("Combo2")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.5f);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::W, false);
    input_system.ProcessSystemEvents(0.1f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::S, false);
    input_system.ProcessSystemEvents(0.1f);

    REQUIRE(callback_count == 1);
}

TEST_CASE("Sequential combo: partial combo does not fire", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B, Rndr::Key::C};
    context.AddAction("Combo")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.5f);

    // Only press first 2 of 3.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.ProcessSystemEvents(0.1f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.1f);

    REQUIRE(callback_count == 0);
}

TEST_CASE("Sequential combo: timer resets between each step", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B, Rndr::Key::C};
    context.AddAction("Combo")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.3f);

    // Each step is within timeout, even though total time exceeds it.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.ProcessSystemEvents(0.2f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.2f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::C, false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(callback_count == 1);
}

TEST_CASE("Sequential combo: release events do not advance combo", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B};
    context.AddAction("Combo")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.5f);

    // Press A, release A, release B (as if B was pressed), press B.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.ProcessSystemEvents(0.1f);

    // Release events should not advance.
    input_system.OnButtonUp(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 0);

    // Now actually press B to complete.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.1f);
    REQUIRE(callback_count == 1);
}

#endif  // RNDR_OLD_INPUT_SYSTEM
