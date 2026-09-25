#pragma once

#include "rndr/definitions.hpp"

#if RNDR_ANDROID

#include "opal/container/shared-ptr.h"
#include "opal/logging.h"

#include "rndr/generic-window.hpp"
#include "rndr/input-primitives.hpp"
#include "rndr/platform-application.hpp"
#include "rndr/platform/android-forward-def.hpp"

namespace Rndr
{

class AndroidWindow;

/**
 * The platform layer over android_native_app_glue. It owns nothing of the activity: the glue hands the
 * android_app to android_main, and android_main hands it here through ApplicationDesc.
 *
 * The OS owns the one window. The activity is given a native window when it comes to the foreground and
 * loses it when it goes to the background, while the application keeps running. The AndroidWindow outlives
 * both: its native handle becomes null and comes back as a different one, and each change is reported
 * through SystemMessageHandler::OnWindowNativeHandleChanged while the old handle is still valid, which is
 * the only time a surface built over it can be destroyed safely.
 */
class AndroidApplication : public PlatformApplication
{
public:
    AndroidApplication(struct SystemMessageHandler* message_handler, android_app* app);
    /** Finishes the activity if nothing did yet, and pumps until the glue says it is destroyed. */
    ~AndroidApplication() override;

    /**
     * Drains the looper: waits up to timeout_ms for the first event, then handles everything already
     * pending without waiting again.
     */
    void ProcessSystemEvents(u32 timeout_ms) override;

    /** Android draws no cursor for touch. Recorded and reported, nothing else. */
    void ShowCursor(bool show) override;
    [[nodiscard]] bool IsCursorVisible() const override;
    /** The platform cannot move the pointer. Does nothing. */
    void SetCursorPosition(const Vector2i& pos) override;
    /** Where the last touch or mouse event was, in screen space - which is window space, the window being the screen. */
    [[nodiscard]] Vector2i GetCursorPosition() const override;

    /** ClipboardManager through JNI, since the clipboard is a Java service with no NDK API. Text crosses as UTF-16. */
    ErrorCode SetClipboardText(const Opal::StringUtf8& text) override;
    [[nodiscard]] Opal::Expected<Opal::StringUtf8, ErrorCode> GetClipboardText() override;

    /**
     * One monitor, the size of the native window and scaled by the display density. The refresh rate is the one
     * AChoreographer last reported, 60 until it does and on a device below API 30.
     */
    [[nodiscard]] Opal::DynamicArray<MonitorInfo> GetMonitors() const override;
    [[nodiscard]] MonitorInfo GetPrimaryMonitor() const override;
    [[nodiscard]] MonitorInfo GetMonitorAtPosition(const Vector2i& pos) const override;
    [[nodiscard]] MonitorInfo GetMonitorForWindow(const GenericWindow& window) const override;

    /** The one live instance, or null. AndroidWindow uses it the way LinuxWindow uses LinuxApplication. */
    static AndroidApplication* Get();

    [[nodiscard]] android_app* GetAndroidApp() const { return m_app; }
    /** Density over the 160 dpi Android calls 1x. */
    [[nodiscard]] f32 GetDpiScale() const;

    /**
     * Pumps until the activity has a native window, so that window creation is synchronous the way it is
     * everywhere else.
     * @return ErrorCode::Success, or ErrorCode::PlatformError when the activity is destroyed first.
     */
    ErrorCode WaitForNativeWindow();

    /** Called by AndroidWindow as it is created and destroyed. There is at most one. */
    void SetWindow(AndroidWindow* window);
    [[nodiscard]] bool HasWindow() const { return m_window != nullptr; }

    /**
     * Runs a close the way the desktop close button does: through SystemMessageHandler::OnWindowClose,
     * which the application can veto. When it is not vetoed the activity is finished. Back and
     * GenericWindow::RequestClose both come through here.
     */
    void CloseWindow();

    /**
     * Copies the files of one directory of the APK's assets into a directory on disk, so that everything that
     * opens a path - File, assimp, KTX, stb - reads them as it does on the desktop. A file already there with the
     * same size is left alone, which makes a second launch cheap and means a changed asset of the same size is
     * not picked up until the app's data is cleared.
     *
     * Not recursive: the NDK lists the files of an asset directory but not its subdirectories, so each one is
     * extracted by its own call.
     * @param asset_directory Directory inside assets/, "" for its root.
     * @param destination Directory to write into, usually under android_app::activity->internalDataPath. Created
     *        when missing; its parent has to exist.
     * @return ErrorCode::Success, ErrorCode::FileNotFound when the asset directory holds no files, or
     *         ErrorCode::PlatformError when a file could not be read or written.
     */
    ErrorCode ExtractAssets(const char* asset_directory, const Opal::StringUtf8& destination);

    /**
     * Ask the activity to show itself this way up, through Activity.setRequestedOrientation - the NDK has no call
     * for it, so it goes through JNI. The turn itself arrives later, as a resize and a new native window size.
     * @return ErrorCode::PlatformError when the call could not be made or threw; the log says which.
     */
    ErrorCode RequestOrientation(ScreenOrientation orientation);

    /**
     * Read the window's safe insets again, and log them when they changed. Called by the event pump and when the
     * window is created; see GenericWindow::GetSafeInsets.
     */
    void RefreshSafeInsets();

    /**
     * Shows or hides the on-screen keyboard through dev.rndr.RndrActivity, whose invisible view takes the text an
     * on-screen keyboard commits. Under a plain NativeActivity it falls back to ANativeActivity_showSoftInput, whose
     * keyboard can only send key events.
     */
    ErrorCode SetTextInputActive(bool active) override;

    /**
     * Text an on-screen keyboard committed, handed over by RndrActivity on the UI thread. Queued, and delivered as
     * OnCharacter on this application's thread at the next ProcessSystemEvents, which the queueing wakes.
     */
    static void QueueCommittedText(const Opal::StringUtf32& text);

    /**
     * Hand the window's preferred refresh rate to its native window, through ANativeWindow_setFrameRate. A native
     * window forgets it when it is replaced, so this runs again for each one, and again on every resize and
     * configuration change: on a cold start into landscape the rate asked for at creation was gone by the first
     * frames. Nothing to do below API 30.
     * @return ErrorCode::Success, also while there is no native window; ErrorCode::PlatformError when it is refused.
     */
    ErrorCode ApplyPreferredRefreshRate();

private:
    static void OnAppCommand(android_app* app, i32 command);
    static i32 OnInputEvent(android_app* app, AInputEvent* event);

    void HandleCommand(i32 command);
    /** @return 1 when the event was consumed, 0 to let the system handle it. */
    i32 HandleInput(AInputEvent* event);
    i32 HandleKeyEvent(AInputEvent* event);
    i32 HandleMotionEvent(AInputEvent* event);
    /** The window's size changed or may have - rotation reports a config change before the resize. */
    void RefreshWindowSize();
    void RefreshDpiScale();
    void UpdateModifierKeys(i32 meta_state);
    /** Reports the pointer's new position as mouse motion, and remembers it as the cursor position. */
    void MovePointer(const Vector2i& position);

    /** Maps an AKEYCODE_* to the matching InputPrimitive, InputPrimitive::Invalid when there is none. */
    [[nodiscard]] static InputPrimitive TranslateKey(i32 key_code);

    android_app* m_app = nullptr;
    AndroidWindow* m_window = nullptr;
    Opal::SharedPtr<Opal::LogSink> m_logcat_sink;

    bool m_is_cursor_visible = true;
    f32 m_dpi_scale = 1.0f;
    /** Focus arrives as a command that can come before the window exists, so it is kept here. */
    bool m_has_focus = false;

    Vector2i m_cursor_pos;
    /** The pointer id of the one touch that is being reported as the left button, -1 when there is none. */
    i32 m_touch_pointer_id = -1;
    /** AMOTION_EVENT_BUTTON_* bits last reported for a mouse, diffed against each event's button state. */
    i32 m_mouse_button_state = 0;

    /** Whether the constructor attached this thread to the Java VM, which the destructor then undoes. */
    bool m_attached_to_java = false;
    /** android.view.KeyCharacterMap and its methods, for the character a key event types. Null when unavailable. */
    _jclass* m_key_character_map_class = nullptr;
    _jmethodID* m_key_character_map_load = nullptr;
    _jmethodID* m_key_character_map_get = nullptr;
    /** RndrActivity.setTextInputActive, or null when the activity is a plain NativeActivity. */
    _jmethodID* m_set_text_input_active = nullptr;

    /** The choreographer the refresh rate callback is registered with, or null below API 30. */
    AChoreographer* m_choreographer = nullptr;
    /** The display's refresh rate as the system last reported it, in hertz. */
    f32 m_refresh_rate = 60.0f;

    static void OnRefreshRateChanged(int64_t vsync_period_nanos, void* data);
    /** Committed text waiting for ProcessSystemEvents. Guarded by a mutex in the source, since the UI thread fills it. */
    Opal::DynamicArray<uchar32> m_pending_characters;

    void SetUpJava();
    void TearDownJava();
    /** The character a key event types, from KeyCharacterMap; 0 when it types none this layer reports. */
    uchar32 CharacterForKey(const AInputEvent* event) const;
    void DeliverPendingCharacters();
};

}  // namespace Rndr

#endif  // RNDR_ANDROID
