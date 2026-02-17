#include <catch2/catch2.hpp>

#include "opal/exceptions.h"

#include "rndr/input-system.hpp"

static const auto& g_fake_window = *reinterpret_cast<const Rndr::GenericWindow*>(1);
static const auto& g_fake_window2 = *reinterpret_cast<const Rndr::GenericWindow*>(2);

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

// NOTE: Simultaneous combos (timeout == 0) currently use the same sequential matching as
// sequential combos, but without a timeout. True order-independent detection is not yet
// implemented. These tests document the current behavior.

TEST_CASE("Simultaneous combo: all keys pressed triggers callback", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B};
    context.AddAction("SpecialMove")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.0f);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(callback_count == 1);
}

TEST_CASE("Simultaneous combo: missing one key does not trigger", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B};
    context.AddAction("SpecialMove")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.0f);

    SECTION("Only first key")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 0);
    }

    SECTION("Only second key")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 0);
    }
}

TEST_CASE("Simultaneous combo: no timeout means no expiry", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B};
    context.AddAction("SpecialMove")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.0f);

    // Press first key.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.ProcessSystemEvents(0.0f);

    // Wait a very long time — no timeout should reset the combo.
    input_system.ProcessSystemEvents(100.0f);

    // Press second key — should still complete.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(callback_count == 1);
}

TEST_CASE("Simultaneous combo: wrong key resets combo", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B};
    context.AddAction("SpecialMove")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.0f);

    // Press A, then wrong key C.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.ProcessSystemEvents(0.0f);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::C, false);
    input_system.ProcessSystemEvents(0.0f);

    // Now press B — combo was reset so this doesn't complete it.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(callback_count == 0);
}

TEST_CASE("Simultaneous combo: can be retriggered", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B};
    context.AddAction("SpecialMove")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            callback_count++;
        })
        .BindCombo(keys, 0.0f);

    // First trigger.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.0f);
    REQUIRE(callback_count == 1);

    // Second trigger.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::A, false);
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::B, false);
    input_system.ProcessSystemEvents(0.0f);
    REQUIRE(callback_count == 2);
}

TEST_CASE("Context stack: default context exists on construction", "[input]")
{
    Rndr::InputSystem input_system;

    // Should not crash, default context is available.
    Rndr::InputContext& ctx = input_system.GetCurrentContext();
    REQUIRE(ctx.GetName() == Opal::StringUtf8("Default"));
}

TEST_CASE("Context stack: PushContext makes new context active", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext gameplay_context(Opal::StringUtf8("Gameplay"));

    input_system.PushContext(gameplay_context);

    REQUIRE(input_system.GetCurrentContext().GetName() == Opal::StringUtf8("Gameplay"));
}

TEST_CASE("Context stack: PopContext returns to previous context", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext gameplay_context(Opal::StringUtf8("Gameplay"));

    input_system.PushContext(gameplay_context);
    REQUIRE(input_system.GetCurrentContext().GetName() == Opal::StringUtf8("Gameplay"));

    bool result = input_system.PopContext();
    REQUIRE(result);
    REQUIRE(input_system.GetCurrentContext().GetName() == Opal::StringUtf8("Default"));
}

TEST_CASE("Context stack: PopContext on default context returns false", "[input]")
{
    Rndr::InputSystem input_system;

    bool result = input_system.PopContext();
    REQUIRE_FALSE(result);

    // Default context should still be there.
    REQUIRE(input_system.GetCurrentContext().GetName() == Opal::StringUtf8("Default"));
}

TEST_CASE("Context stack: top context handles events first", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& default_ctx = input_system.GetCurrentContext();

    bool default_fired = false;
    bool top_fired = false;

    default_ctx.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            default_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    Rndr::InputContext top_context(Opal::StringUtf8("Top"));
    top_context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            top_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    input_system.PushContext(top_context);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(top_fired);
    REQUIRE_FALSE(default_fired);
}

TEST_CASE("Context stack: consumed event does not propagate", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& default_ctx = input_system.GetCurrentContext();

    bool default_fired = false;
    bool top_fired = false;

    default_ctx.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            default_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    Rndr::InputContext top_context(Opal::StringUtf8("Top"));
    top_context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            top_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    input_system.PushContext(top_context);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);

    // Top consumed it, default should not fire.
    REQUIRE(top_fired);
    REQUIRE_FALSE(default_fired);
}

TEST_CASE("Context stack: unconsumed event propagates to lower context", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& default_ctx = input_system.GetCurrentContext();

    bool default_fired = false;

    default_ctx.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            default_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    // Top context has no binding for Space.
    Rndr::InputContext top_context(Opal::StringUtf8("Top"));
    top_context.AddAction("Shoot")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/) {})
        .Bind(Rndr::Key::F, Rndr::Trigger::Pressed);

    input_system.PushContext(top_context);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);

    // Top didn't handle Space, so default should get it.
    REQUIRE(default_fired);
}

TEST_CASE("Context stack: disabled context is skipped", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& default_ctx = input_system.GetCurrentContext();

    bool default_fired = false;
    bool top_fired = false;

    default_ctx.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            default_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    Rndr::InputContext top_context(Opal::StringUtf8("Top"));
    top_context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            top_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    top_context.SetEnabled(false);
    input_system.PushContext(top_context);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);

    // Top is disabled, default should handle it.
    REQUIRE_FALSE(top_fired);
    REQUIRE(default_fired);
}

TEST_CASE("Context stack: re-enabling context makes it process events", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& default_ctx = input_system.GetCurrentContext();

    bool default_fired = false;
    bool top_fired = false;

    default_ctx.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            default_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    Rndr::InputContext top_context(Opal::StringUtf8("Top"));
    top_context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            top_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    top_context.SetEnabled(false);
    input_system.PushContext(top_context);

    // While disabled, default handles events.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);
    REQUIRE_FALSE(top_fired);
    REQUIRE(default_fired);

    // Re-enable.
    default_fired = false;
    top_context.SetEnabled(true);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);
    REQUIRE(top_fired);
    REQUIRE_FALSE(default_fired);
}

TEST_CASE("Context stack: multiple contexts stacked correctly", "[input]")
{
    Rndr::InputSystem input_system;

    bool default_fired = false;
    bool a_fired = false;
    bool b_fired = false;

    input_system.GetCurrentContext().AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            default_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    Rndr::InputContext context_a(Opal::StringUtf8("A"));
    context_a.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            a_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    Rndr::InputContext context_b(Opal::StringUtf8("B"));
    context_b.AddAction("Jump")
        .OnButton([&](Rndr::Trigger /*trigger*/, bool /*is_repeat*/)
        {
            b_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    input_system.PushContext(context_a);
    input_system.PushContext(context_b);

    SECTION("Top context B handles event")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(b_fired);
        REQUIRE_FALSE(a_fired);
        REQUIRE_FALSE(default_fired);
    }

    SECTION("After popping B, A handles event")
    {
        input_system.PopContext();

        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(b_fired);
        REQUIRE(a_fired);
        REQUIRE_FALSE(default_fired);
    }

    SECTION("After popping both, default handles event")
    {
        input_system.PopContext();
        input_system.PopContext();

        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(b_fired);
        REQUIRE_FALSE(a_fired);
        REQUIRE(default_fired);
    }
}

TEST_CASE("Context stack: GetContextByName returns matching context", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext gameplay_context(Opal::StringUtf8("Gameplay"));
    Rndr::InputContext menu_context(Opal::StringUtf8("Menu"));

    input_system.PushContext(gameplay_context);
    input_system.PushContext(menu_context);

    SECTION("Returns pushed context by name")
    {
        Rndr::InputContext& found = input_system.GetContextByName(Opal::StringUtf8("Gameplay"));
        REQUIRE(found.GetName() == Opal::StringUtf8("Gameplay"));
    }

    SECTION("Returns default context by name")
    {
        Rndr::InputContext& found = input_system.GetContextByName(Opal::StringUtf8("Default"));
        REQUIRE(found.GetName() == Opal::StringUtf8("Default"));
    }

    SECTION("Throws for non-existent name")
    {
        REQUIRE_THROWS_AS(input_system.GetContextByName(Opal::StringUtf8("NonExistent")), Opal::Exception);
    }
}

// Action Management //////////////////////////////////////////////////////////////////////////////

TEST_CASE("Action management: AddAction with unique name succeeds", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    context.AddAction("Jump")
        .OnButton([](Rndr::Trigger, bool) {})
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    REQUIRE(context.ContainsAction("Jump"));
}

TEST_CASE("Action management: AddAction with duplicate name throws", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    context.AddAction("Jump")
        .OnButton([](Rndr::Trigger, bool) {})
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    REQUIRE_THROWS_AS(
        context.AddAction("Jump")
            .OnButton([](Rndr::Trigger, bool) {})
            .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed),
        Opal::Exception);
}

TEST_CASE("Action management: RemoveAction removes the action", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    context.AddAction("Jump")
        .OnButton([](Rndr::Trigger, bool) {})
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    REQUIRE(context.ContainsAction("Jump"));

    bool removed = context.RemoveAction("Jump");
    REQUIRE(removed);
    REQUIRE_FALSE(context.ContainsAction("Jump"));
}

TEST_CASE("Action management: RemoveAction returns false for non-existent", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool removed = context.RemoveAction("NonExistent");
    REQUIRE_FALSE(removed);
}

TEST_CASE("Action management: ContainsAction", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    REQUIRE_FALSE(context.ContainsAction("Jump"));

    context.AddAction("Jump")
        .OnButton([](Rndr::Trigger, bool) {})
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    REQUIRE(context.ContainsAction("Jump"));

    context.RemoveAction("Jump");
    REQUIRE_FALSE(context.ContainsAction("Jump"));
}

TEST_CASE("Action management: GetAction returns pointer or nullptr", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    REQUIRE(context.GetAction("Jump") == nullptr);

    context.AddAction("Jump")
        .OnButton([](Rndr::Trigger, bool) {})
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    Rndr::InputAction* action = context.GetAction("Jump");
    REQUIRE(action != nullptr);
    REQUIRE(action->GetName() == Opal::StringUtf8("Jump"));

    context.RemoveAction("Jump");
    REQUIRE(context.GetAction("Jump") == nullptr);
}

// Window Filter //////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Window filter: action with filter only fires for matching window", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;

    context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger, bool)
        {
            callback_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed)
        .ForWindow(&g_fake_window);

    SECTION("Matching window fires callback")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_fired);
    }

    SECTION("Different window does not fire callback")
    {
        input_system.OnButtonDown(g_fake_window2, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(callback_fired);
    }
}

TEST_CASE("Window filter: action with no filter fires for any window", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger, bool)
        {
            callback_count++;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);
    REQUIRE(callback_count == 1);

    input_system.OnButtonDown(g_fake_window2, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);
    REQUIRE(callback_count == 2);
}

TEST_CASE("Window filter: multiple actions with different window filters", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool window1_fired = false;
    bool window2_fired = false;

    context.AddAction("JumpW1")
        .OnButton([&](Rndr::Trigger, bool)
        {
            window1_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed)
        .ForWindow(&g_fake_window);

    context.AddAction("JumpW2")
        .OnButton([&](Rndr::Trigger, bool)
        {
            window2_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed)
        .ForWindow(&g_fake_window2);

    SECTION("Event from window1 only fires window1 action")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(window1_fired);
        REQUIRE_FALSE(window2_fired);
    }

    SECTION("Event from window2 only fires window2 action")
    {
        input_system.OnButtonDown(g_fake_window2, Rndr::InputPrimitive::Space, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(window1_fired);
        REQUIRE(window2_fired);
    }
}

// Multiple Callback Types Per Action /////////////////////////////////////////////////////////////

TEST_CASE("Multiple callbacks: OnButton and OnMouseButton on same action", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool button_fired = false;
    bool mouse_button_fired = false;

    context.AddAction("Interact")
        .OnButton([&](Rndr::Trigger, bool)
        {
            button_fired = true;
        })
        .OnMouseButton([&](Rndr::MouseButton, Rndr::Trigger, const Rndr::Vector2i&)
        {
            mouse_button_fired = true;
        })
        .Bind(Rndr::Key::E, Rndr::Trigger::Pressed)
        .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Pressed);

    SECTION("Key press fires button callback only")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(button_fired);
        REQUIRE_FALSE(mouse_button_fired);
    }

    SECTION("Mouse click fires mouse button callback only")
    {
        input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_LeftButton, Rndr::Vector2i{0, 0});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(button_fired);
        REQUIRE(mouse_button_fired);
    }
}

TEST_CASE("Multiple callbacks: OnButton with key and hold bindings", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("Interact")
        .OnButton([&](Rndr::Trigger, bool)
        {
            callback_count++;
        })
        .Bind(Rndr::Key::E, Rndr::Trigger::Pressed)
        .BindHold(Rndr::Key::F, 0.5f);

    SECTION("Key press fires immediately")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(callback_count == 1);
    }

    SECTION("Hold fires after duration")
    {
        input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::F, false);
        input_system.ProcessSystemEvents(0.3f);
        REQUIRE(callback_count == 0);

        input_system.ProcessSystemEvents(0.3f);
        REQUIRE(callback_count == 1);
    }
}

TEST_CASE("Multiple callbacks: OnMousePosition and OnMouseWheel on same action", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool position_fired = false;
    bool wheel_fired = false;

    context.AddAction("Camera")
        .OnMousePosition([&](Rndr::MouseAxis, Rndr::f32)
        {
            position_fired = true;
        })
        .OnMouseWheel([&](Rndr::f32, Rndr::f32)
        {
            wheel_fired = true;
        })
        .Bind(Rndr::MouseAxis::X)
        .Bind(Rndr::MouseAxis::WheelY);

    SECTION("Mouse move fires position callback only")
    {
        input_system.OnMouseMove(g_fake_window, 5.0f, 0.0f);
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE(position_fired);
        REQUIRE_FALSE(wheel_fired);
    }

    SECTION("Mouse wheel fires wheel callback only")
    {
        input_system.OnMouseWheel(g_fake_window, 120.0f, Rndr::Vector2i{0, 0});
        input_system.ProcessSystemEvents(0.0f);

        REQUIRE_FALSE(position_fired);
        REQUIRE(wheel_fired);
    }
}

// Edge Cases /////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Edge case: ProcessSystemEvents with no actions", "[input]")
{
    Rndr::InputSystem input_system;

    // Queue an event but no actions are registered.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);

    // Should not crash.
    REQUIRE(true);
}

TEST_CASE("Edge case: ProcessSystemEvents with no events", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    context.AddAction("Jump")
        .OnButton([](Rndr::Trigger, bool) {})
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    // No events queued.
    input_system.ProcessSystemEvents(0.0f);
    input_system.ProcessSystemEvents(1.0f);

    // Should not crash.
    REQUIRE(true);
}

TEST_CASE("Edge case: events handled by default context", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool callback_fired = false;

    context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger, bool)
        {
            callback_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    // No extra context pushed, default handles it.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(callback_fired);
}

TEST_CASE("Edge case: removing action does not affect other actions", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool jump_fired = false;
    bool shoot_fired = false;

    context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger, bool)
        {
            jump_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    context.AddAction("Shoot")
        .OnButton([&](Rndr::Trigger, bool)
        {
            shoot_fired = true;
        })
        .Bind(Rndr::Key::F, Rndr::Trigger::Pressed);

    context.RemoveAction("Jump");

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::F, false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE_FALSE(jump_fired);
    REQUIRE(shoot_fired);
}

TEST_CASE("Edge case: large delta_seconds does not cause issues", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    int callback_count = 0;

    context.AddAction("Hold")
        .OnButton([&](Rndr::Trigger, bool)
        {
            callback_count++;
        })
        .BindHold(Rndr::Key::E, 0.5f);

    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::E, false);
    input_system.ProcessSystemEvents(999999.0f);

    // Should fire exactly once, not crash.
    REQUIRE(callback_count == 1);
}

TEST_CASE("Edge case: multiple event types in same frame", "[input]")
{
    Rndr::InputSystem input_system;
    Rndr::InputContext& context = input_system.GetCurrentContext();

    bool key_fired = false;
    bool mouse_button_fired = false;
    bool mouse_move_fired = false;
    bool wheel_fired = false;
    bool text_fired = false;

    context.AddAction("Jump")
        .OnButton([&](Rndr::Trigger, bool)
        {
            key_fired = true;
        })
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);

    context.AddAction("Shoot")
        .OnMouseButton([&](Rndr::MouseButton, Rndr::Trigger, const Rndr::Vector2i&)
        {
            mouse_button_fired = true;
        })
        .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Pressed);

    context.AddAction("Look")
        .OnMousePosition([&](Rndr::MouseAxis, Rndr::f32)
        {
            mouse_move_fired = true;
        })
        .Bind(Rndr::MouseAxis::X);

    context.AddAction("Zoom")
        .OnMouseWheel([&](Rndr::f32, Rndr::f32)
        {
            wheel_fired = true;
        })
        .Bind(Rndr::MouseAxis::WheelY);

    context.AddAction("Chat")
        .OnText([&](Rndr::uchar32)
        {
            text_fired = true;
        })
        .BindText();

    // Queue all event types in the same frame.
    input_system.OnButtonDown(g_fake_window, Rndr::InputPrimitive::Space, false);
    input_system.OnMouseButtonDown(g_fake_window, Rndr::InputPrimitive::Mouse_LeftButton, Rndr::Vector2i{0, 0});
    input_system.OnMouseMove(g_fake_window, 5.0f, 0.0f);
    input_system.OnMouseWheel(g_fake_window, 120.0f, Rndr::Vector2i{0, 0});
    input_system.OnCharacter(g_fake_window, U'A', false);
    input_system.ProcessSystemEvents(0.0f);

    REQUIRE(key_fired);
    REQUIRE(mouse_button_fired);
    REQUIRE(mouse_move_fired);
    REQUIRE(wheel_fired);
    REQUIRE(text_fired);
}
