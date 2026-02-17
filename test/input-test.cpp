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
}

#endif  // RNDR_OLD_INPUT_SYSTEM
