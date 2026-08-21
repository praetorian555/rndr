#pragma once

#include "rndr/input-primitives.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

struct SystemMessageHandler;

/**
 * Polls a single XInput gamepad slot and reports the changes to a SystemMessageHandler.
 *
 * XInput has no event queue, so state is sampled once per Tick and diffed against the previous
 * sample. Raw values are normalized but no dead zone is applied here: dead zones belong to the
 * individual input bindings.
 */
class WindowsGamepad
{
public:
    WindowsGamepad() = default;

    /**
     * @param gamepad_index XInput slot to poll, in [0, k_max_gamepads).
     * @param message_handler Receives the button, axis and connection events. Not owned.
     */
    void Initialize(u8 gamepad_index, SystemMessageHandler* message_handler);

    /**
     * Samples the slot and reports whatever changed since the last sample.
     * @param delta_seconds Time since the previous Tick, used to throttle reconnect polling.
     */
    void Tick(f32 delta_seconds);

    [[nodiscard]] bool IsConnected() const { return m_is_connected; }

private:
    /** Reports releases for everything still held, so a yanked pad does not leave input stuck on. */
    void ReportDisconnect();

    struct State
    {
        bool buttons[k_gamepad_button_count] = {};
        i16 left_stick_x = 0;
        i16 left_stick_y = 0;
        i16 right_stick_x = 0;
        i16 right_stick_y = 0;
        u8 left_trigger = 0;
        u8 right_trigger = 0;
    };

    SystemMessageHandler* m_message_handler = nullptr;
    u8 m_gamepad_index = 0;
    bool m_is_connected = false;

    // Sampling an empty slot is expensive, so disconnected slots are only retried periodically.
    f32 m_seconds_since_connect_poll = 0.0f;

    u32 m_last_packet_number = 0;
    State m_previous_state;
};

}  // namespace Rndr
