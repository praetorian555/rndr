#pragma once

#include "opal/container/dynamic-array.h"
#include "opal/container/expected.h"
#include "opal/container/scope-ptr.h"

#include "rndr/error-codes.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/location.hpp"
#include "rndr/math.hpp"
#include "rndr/monitor-info.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

struct ModifierKeysState
{
    bool is_left_shift_down = false;
    bool is_right_shift_down = false;
    bool is_left_control_down = false;
    bool is_right_control_down = false;
    bool is_left_alt_down = false;
    bool is_right_alt_down = false;
    bool is_left_command_down = false;
    bool is_right_command_down = false;
    bool is_caps_locked = false;
};

class PlatformApplication
{
public:
    PlatformApplication(struct SystemMessageHandler* message_handler) : m_message_handler(message_handler) {}
    virtual ~PlatformApplication();

    Opal::Expected<Opal::Ref<GenericWindow>, ErrorCode> CreateGenericWindow(const GenericWindowDesc& desc);
    void DestroyGenericWindow(Opal::Ref<GenericWindow> window);

    /**
     * Process any messages received from the OS, like input events.
     * @param timeout_ms How long to block waiting for an event before returning. Use 0 to return immediately
     *                  if no events are pending, or InfiniteTimeout to block until at least one event arrives.
     */
    virtual void ProcessSystemEvents(u32 timeout_ms) = 0;

    /**
     * Control cursor visibility.
     * @param show Should the cursor be shown or not.
     */
    virtual void ShowCursor(bool show) = 0;

    /**
     * Check if the cursor is visible.
     * @return Returns true if the cursor is visible, false otherwise.
     */
    [[nodiscard]] virtual bool IsCursorVisible() const = 0;

    /**
     * Set cursor position in screen space.
     * @param pos New cursor position.
     */
    virtual void SetCursorPosition(const Vector2i& pos) = 0;

    /**
     * Get the current cursor position.
     */
    [[nodiscard]] virtual Vector2i GetCursorPosition() const = 0;

    /**
     * Check if a gamepad is connected on the given slot.
     * @param gamepad_index Slot in [0, k_max_gamepads).
     */
    [[nodiscard]] virtual bool IsGamepadConnected(u8 gamepad_index) const
    {
        (void)gamepad_index;
        return false;
    }

    /**
     * Put text on the system clipboard, replacing whatever was there.
     * @param text UTF-8 text.
     * @return ErrorCode::Success, ErrorCode::InvalidArgument if the text is not valid UTF-8, or
     *         ErrorCode::PlatformError if the window system would not take it.
     */
    virtual ErrorCode SetClipboardText(const Opal::StringUtf8& text) = 0;

    /**
     * Get the text on the system clipboard. An empty clipboard, or one holding something that is not
     * text, gives an empty string. On Linux, when another application owns the clipboard, this waits
     * for it to send the text over, for up to a second. On Android 10 and later only the app with the
     * focus is given the clipboard, so one in the background reads it as empty.
     * @return The text, ErrorCode::CorruptData if what the clipboard holds is not valid text in its
     *         claimed encoding, or ErrorCode::PlatformError if the window system could not deliver it.
     */
    [[nodiscard]] virtual Opal::Expected<Opal::StringUtf8, ErrorCode> GetClipboardText() = 0;

    /**
     * Ask for text from the user, or stop asking. Characters arrive through SystemMessageHandler::OnCharacter
     * either way. A desktop keyboard always types, so there this only records the request; on Android it shows
     * or hides the on-screen keyboard.
     * @return ErrorCode::Success, or ErrorCode::PlatformError when the keyboard could not be asked for.
     */
    virtual ErrorCode SetTextInputActive(bool active)
    {
        m_is_text_input_active = active;
        return ErrorCode::Success;
    }
    [[nodiscard]] bool IsTextInputActive() const { return m_is_text_input_active; }

    /**
     * Hold the display on, or let it dim and lock for inactivity again. Meant for a screen the user watches without
     * touching, such as a timer or a video. On Windows it asks the system to keep the display on; on Android it sets
     * the activity window's keep-screen-on flag. A platform without either records the request and reports it.
     * @param keep_on True to keep the display on, false to let it sleep again.
     * @return ErrorCode::Success, ErrorCode::FeatureNotSupported where the platform has no way to do it, or
     *         ErrorCode::PlatformError when the system refused.
     */
    virtual ErrorCode SetKeepScreenOn(bool keep_on)
    {
        m_is_keep_screen_on = keep_on;
        return ErrorCode::FeatureNotSupported;
    }
    /** Whether the last SetKeepScreenOn asked for the display to stay on. */
    [[nodiscard]] bool IsKeepScreenOn() const { return m_is_keep_screen_on; }

    /**
     * Draw the icons in the system's status and navigation bars dark, for a light window background, or light, for a dark
     * one. Only Android draws bars over the window.
     * @param dark True for dark icons, false for light ones.
     * @return ErrorCode::Success, ErrorCode::FeatureNotSupported where the system draws no bars over the window or the
     *         activity cannot change them, or ErrorCode::PlatformError when the call failed.
     */
    virtual ErrorCode SetSystemBarsDarkContent(bool dark)
    {
        (void)dark;
        return ErrorCode::FeatureNotSupported;
    }

    /**
     * Buzz the device's vibrator for a moment, at its default strength. Only Android has one; there the manifest must ask
     * for android.permission.VIBRATE.
     * @param milliseconds How long to vibrate.
     * @return ErrorCode::Success, ErrorCode::FeatureNotSupported where there is no vibrator, no permission for it or no
     *         way to reach it, or ErrorCode::PlatformError when the call failed.
     */
    virtual ErrorCode Vibrate(u32 milliseconds)
    {
        (void)milliseconds;
        return ErrorCode::FeatureNotSupported;
    }

    /**
     * Say the text aloud with the system's text-to-speech voice, cutting off whatever was still being said. It returns at once and the voice follows, a moment later the first time while the engine starts.
     * Windows speaks through SAPI. Android speaks through TextToSpeech, and turns other audio, such as music, down
     * while the voice speaks rather than stopping it; it needs dev.rndr.RndrActivity, and from Android 11 a manifest
     * that lists the engines it looks for: <queries><intent><action android:name="android.intent.action.TTS_SERVICE" /></intent></queries>.
     * @param text What to say, in UTF-8.
     * @param language The language to say it in, as a BCP 47 tag such as "en" or "en-GB"; empty for the system's. A
     *                 language the voice lacks falls back to the system's. Windows speaks with its default voice.
     * @return ErrorCode::Success when the voice was asked to speak, ErrorCode::InvalidArgument when the text is not
     *         UTF-8, ErrorCode::FeatureNotSupported where there is no voice or no way to reach one, or
     *         ErrorCode::PlatformError when the call failed.
     */
    virtual ErrorCode Speak(const Opal::StringUtf8& text, const Opal::StringUtf8& language)
    {
        (void)text;
        (void)language;
        return ErrorCode::FeatureNotSupported;
    }

    /**
     * Cut off whatever Speak is still saying.
     * @return ErrorCode::Success, ErrorCode::FeatureNotSupported where there is no voice, or ErrorCode::PlatformError
     *         when the call failed.
     */
    virtual ErrorCode StopSpeaking() { return ErrorCode::FeatureNotSupported; }

    /** Whether the precise location may be read. Only Android has a GPS to read; elsewhere it is always Denied. */
    [[nodiscard]] virtual LocationPermission GetLocationPermission() const { return LocationPermission::Denied; }

    /**
     * Ask the user for the precise location. The system shows its own prompt, or answers at once when the user already
     * decided; either way the answer arrives later as SystemMessageHandler::OnLocationPermissionChanged. On Android 13
     * and later this also asks to post notifications, which the tracking notification needs to be seen.
     * @return ErrorCode::Success when the question was asked, ErrorCode::FeatureNotSupported where there is no location
     *         to ask for, or ErrorCode::PlatformError when the call failed.
     */
    virtual ErrorCode RequestLocationPermission() { return ErrorCode::FeatureNotSupported; }

    /**
     * Start reporting GPS fixes, as SystemMessageHandler::OnLocationFix, until StopLocationUpdates. Starting again
     * replaces the running updates.
     * @param desc How often, and whether to keep going in the background.
     * @return ErrorCode::Success; ErrorCode::FeatureNotSupported where there is no GPS, or no way to reach it;
     *         ErrorCode::InvalidArgument without the permission; ErrorCode::PlatformError when the location is switched
     *         off in the system settings or the call failed.
     */
    virtual ErrorCode StartLocationUpdates(const LocationUpdatesDesc& desc)
    {
        (void)desc;
        return ErrorCode::FeatureNotSupported;
    }

    /** Stop the updates StartLocationUpdates started, and their notification. Does nothing when none run. */
    virtual ErrorCode StopLocationUpdates() { return ErrorCode::FeatureNotSupported; }

    [[nodiscard]] virtual Opal::DynamicArray<MonitorInfo> GetMonitors() const = 0;
    [[nodiscard]] virtual MonitorInfo GetPrimaryMonitor() const = 0;
    [[nodiscard]] virtual MonitorInfo GetMonitorAtPosition(const Vector2i& pos) const = 0;
    [[nodiscard]] virtual MonitorInfo GetMonitorForWindow(const GenericWindow& window) const = 0;

    Opal::Ref<class GenericWindow> GetGenericWindowByNativeHandle(NativeWindowHandle handle);
    [[nodiscard]] const ModifierKeysState& GetModifierKeysState() const { return m_modifier_keys; }

protected:
    struct SystemMessageHandler* m_message_handler;
    Opal::DynamicArray<Opal::ScopePtr<GenericWindow>> m_generic_windows;
    Opal::Ref<GenericWindow> m_focused_window;
    ModifierKeysState m_modifier_keys;
    bool m_is_text_input_active = false;
    bool m_is_keep_screen_on = false;
};

}  // namespace Rndr
