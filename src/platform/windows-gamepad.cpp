#include "rndr/platform/windows-gamepad.hpp"

#include "rndr/platform/windows-header.hpp"

#include <Xinput.h>

#include "rndr/log.hpp"
#include "rndr/system-message-handler.hpp"

namespace
{

struct ButtonMapping
{
    Rndr::GamepadButton button;
    WORD xinput_flag;
};

// Only the digital buttons. The triggers are analog and are mapped further down.
constexpr ButtonMapping k_button_mappings[] = {
    {Rndr::GamepadButton::A, XINPUT_GAMEPAD_A},
    {Rndr::GamepadButton::B, XINPUT_GAMEPAD_B},
    {Rndr::GamepadButton::X, XINPUT_GAMEPAD_X},
    {Rndr::GamepadButton::Y, XINPUT_GAMEPAD_Y},
    {Rndr::GamepadButton::LeftBumper, XINPUT_GAMEPAD_LEFT_SHOULDER},
    {Rndr::GamepadButton::RightBumper, XINPUT_GAMEPAD_RIGHT_SHOULDER},
    {Rndr::GamepadButton::Back, XINPUT_GAMEPAD_BACK},
    {Rndr::GamepadButton::Start, XINPUT_GAMEPAD_START},
    {Rndr::GamepadButton::LeftThumb, XINPUT_GAMEPAD_LEFT_THUMB},
    {Rndr::GamepadButton::RightThumb, XINPUT_GAMEPAD_RIGHT_THUMB},
    {Rndr::GamepadButton::DPadUp, XINPUT_GAMEPAD_DPAD_UP},
    {Rndr::GamepadButton::DPadDown, XINPUT_GAMEPAD_DPAD_DOWN},
    {Rndr::GamepadButton::DPadLeft, XINPUT_GAMEPAD_DPAD_LEFT},
    {Rndr::GamepadButton::DPadRight, XINPUT_GAMEPAD_DPAD_RIGHT},
};

/** How long a disconnected slot waits before it is sampled again. */
constexpr Rndr::f32 k_disconnected_poll_seconds = 1.0f;

/** Sticks report [-32768, 32767], which is asymmetric, so each half is scaled by its own extent. */
Rndr::f32 NormalizeStick(Rndr::i16 value)
{
    return value >= 0 ? static_cast<Rndr::f32>(value) / 32767.0f : static_cast<Rndr::f32>(value) / 32768.0f;
}

Rndr::f32 NormalizeTrigger(Rndr::u8 value)
{
    return static_cast<Rndr::f32>(value) / 255.0f;
}

}  // namespace

void Rndr::WindowsGamepad::Initialize(u8 gamepad_index, SystemMessageHandler* message_handler)
{
    m_gamepad_index = gamepad_index;
    m_message_handler = message_handler;
}

void Rndr::WindowsGamepad::Tick(f32 delta_seconds)
{
    if (m_message_handler == nullptr)
    {
        return;
    }

    if (!m_is_connected)
    {
        // XInputGetState on an empty slot is orders of magnitude slower than on a live one, so back
        // off instead of paying that cost every frame for every unused slot.
        m_seconds_since_connect_poll += delta_seconds;
        if (m_seconds_since_connect_poll < k_disconnected_poll_seconds)
        {
            return;
        }
        m_seconds_since_connect_poll = 0.0f;
    }

    XINPUT_STATE xinput_state = {};
    const DWORD result = XInputGetState(m_gamepad_index, &xinput_state);
    if (result != ERROR_SUCCESS)
    {
        if (m_is_connected)
        {
            m_is_connected = false;
            RNDR_LOG_INFO("Gamepad disconnected on slot %d", static_cast<i32>(m_gamepad_index));
            ReportDisconnect();
            m_message_handler->OnGamepadConnectionChanged(m_gamepad_index, false);
        }
        return;
    }

    // A fresh connection has to diff against the zeroed state even if the packet number happens to
    // match whatever the previous pad on this slot last reported.
    bool force_diff = false;
    if (!m_is_connected)
    {
        m_is_connected = true;
        force_diff = true;
        RNDR_LOG_INFO("Gamepad connected on slot %d", static_cast<i32>(m_gamepad_index));
        m_message_handler->OnGamepadConnectionChanged(m_gamepad_index, true);
    }

    if (!force_diff && xinput_state.dwPacketNumber == m_last_packet_number)
    {
        return;
    }
    m_last_packet_number = xinput_state.dwPacketNumber;

    const XINPUT_GAMEPAD& pad = xinput_state.Gamepad;

    State current_state;
    for (const ButtonMapping& mapping : k_button_mappings)
    {
        current_state.buttons[static_cast<u8>(mapping.button)] = (pad.wButtons & mapping.xinput_flag) != 0;
    }
    // The triggers are analog, but they are bindable as buttons too, so they get a digital reading
    // as well. They still report their axis value below.
    current_state.buttons[static_cast<u8>(GamepadButton::LeftTrigger)] = pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    current_state.buttons[static_cast<u8>(GamepadButton::RightTrigger)] = pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    current_state.left_stick_x = pad.sThumbLX;
    current_state.left_stick_y = pad.sThumbLY;
    current_state.right_stick_x = pad.sThumbRX;
    current_state.right_stick_y = pad.sThumbRY;
    current_state.left_trigger = pad.bLeftTrigger;
    current_state.right_trigger = pad.bRightTrigger;

    for (u8 button_index = 0; button_index < k_gamepad_button_count; ++button_index)
    {
        if (current_state.buttons[button_index] == m_previous_state.buttons[button_index])
        {
            continue;
        }
        const auto button = static_cast<GamepadButton>(button_index);
        if (current_state.buttons[button_index])
        {
            m_message_handler->OnGamepadButtonDown(m_gamepad_index, button);
        }
        else
        {
            m_message_handler->OnGamepadButtonUp(m_gamepad_index, button);
        }
    }

    // Axes report raw normalized values. Applying XInput's own dead zones here would filter twice,
    // once against the platform constant and again against the binding's dead zone.
    if (current_state.left_stick_x != m_previous_state.left_stick_x)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::LeftStickX, NormalizeStick(current_state.left_stick_x));
    }
    if (current_state.left_stick_y != m_previous_state.left_stick_y)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::LeftStickY, NormalizeStick(current_state.left_stick_y));
    }
    if (current_state.right_stick_x != m_previous_state.right_stick_x)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::RightStickX, NormalizeStick(current_state.right_stick_x));
    }
    if (current_state.right_stick_y != m_previous_state.right_stick_y)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::RightStickY, NormalizeStick(current_state.right_stick_y));
    }
    if (current_state.left_trigger != m_previous_state.left_trigger)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::LeftTrigger, NormalizeTrigger(current_state.left_trigger));
    }
    if (current_state.right_trigger != m_previous_state.right_trigger)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::RightTrigger, NormalizeTrigger(current_state.right_trigger));
    }

    m_previous_state = current_state;
}

void Rndr::WindowsGamepad::ReportDisconnect()
{
    for (u8 button_index = 0; button_index < k_gamepad_button_count; ++button_index)
    {
        if (m_previous_state.buttons[button_index])
        {
            m_message_handler->OnGamepadButtonUp(m_gamepad_index, static_cast<GamepadButton>(button_index));
        }
    }

    // Centering the sticks matters as much as releasing the buttons: a pad unplugged mid-deflection
    // would otherwise leave the last non-zero axis value latched in whatever it was driving.
    if (m_previous_state.left_stick_x != 0)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::LeftStickX, 0.0f);
    }
    if (m_previous_state.left_stick_y != 0)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::LeftStickY, 0.0f);
    }
    if (m_previous_state.right_stick_x != 0)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::RightStickX, 0.0f);
    }
    if (m_previous_state.right_stick_y != 0)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::RightStickY, 0.0f);
    }
    if (m_previous_state.left_trigger != 0)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::LeftTrigger, 0.0f);
    }
    if (m_previous_state.right_trigger != 0)
    {
        m_message_handler->OnGamepadAxis(m_gamepad_index, GamepadAxis::RightTrigger, 0.0f);
    }

    m_previous_state = State{};
}
