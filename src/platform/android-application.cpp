#include "rndr/platform/android-application.hpp"

#if RNDR_ANDROID

#include <climits>
#include <cstring>

#include <sys/stat.h>

#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include "android_native_app_glue.h"

#include "opal/file-system.h"

#include "rndr/application.hpp"
#include "rndr/log.hpp"
#include "rndr/monitor-info.hpp"
#include "rndr/platform/android-window.hpp"
#include "rndr/system-message-handler.hpp"

namespace
{

Rndr::AndroidApplication* g_android_app = nullptr;

/** An activity's stdout goes nowhere, so the log goes to logcat as well, tagged with its category. */
class LogcatSink final : public Opal::LogSink
{
public:
    void Write(Opal::LogLevel level, Opal::StringViewUtf8 category, Opal::StringViewUtf8 formatted_message) override
    {
        char tag[64] = {};
        const Rndr::u64 tag_size = category.GetSize() < sizeof(tag) - 1 ? category.GetSize() : sizeof(tag) - 1;
        memcpy(tag, category.GetData(), tag_size);
        __android_log_print(ToPriority(level), tag, "%.*s", static_cast<int>(formatted_message.GetSize()), formatted_message.GetData());
    }

    void Flush() override {}

private:
    static int ToPriority(Opal::LogLevel level)
    {
        switch (level)
        {
            case Opal::LogLevel::Fatal:
                return ANDROID_LOG_FATAL;
            case Opal::LogLevel::Error:
                return ANDROID_LOG_ERROR;
            case Opal::LogLevel::Warning:
                return ANDROID_LOG_WARN;
            case Opal::LogLevel::Info:
                return ANDROID_LOG_INFO;
            case Opal::LogLevel::Verbose:
                return ANDROID_LOG_VERBOSE;
            default:
                return ANDROID_LOG_SILENT;
        }
    }
};

/**
 * Handles one looper event, waiting up to timeout_ms for it; -1 waits for ever.
 * @return False when nothing arrived.
 */
bool PollOnce(android_app* app, int timeout_ms)
{
    android_poll_source* source = nullptr;
    const int ident = ALooper_pollOnce(timeout_ms, nullptr, nullptr, reinterpret_cast<void**>(&source));
    if (ident == ALOOPER_POLL_TIMEOUT || ident == ALOOPER_POLL_ERROR)
    {
        return false;
    }
    if (source != nullptr)
    {
        source->process(app, source);
    }
    return true;
}

Rndr::f32 ReadDpiScale(const android_app* app)
{
    const int32_t density = AConfiguration_getDensity(app->config);
    if (density == ACONFIGURATION_DENSITY_DEFAULT || density == ACONFIGURATION_DENSITY_ANY || density == ACONFIGURATION_DENSITY_NONE)
    {
        return 1.0f;
    }
    return static_cast<Rndr::f32>(density) / static_cast<Rndr::f32>(ACONFIGURATION_DENSITY_MEDIUM);
}

Rndr::Vector2i GetPointerPosition(const AInputEvent* event, size_t pointer_index)
{
    return {static_cast<Rndr::i32>(AMotionEvent_getX(event, pointer_index)), static_cast<Rndr::i32>(AMotionEvent_getY(event, pointer_index))};
}

/** @return The index of the pointer with this id in the event, or -1 when it is not in it. */
Rndr::i32 FindPointer(const AInputEvent* event, Rndr::i32 pointer_id)
{
    const size_t count = AMotionEvent_getPointerCount(event);
    for (size_t i = 0; i < count; ++i)
    {
        if (AMotionEvent_getPointerId(event, i) == pointer_id)
        {
            return static_cast<Rndr::i32>(i);
        }
    }
    return -1;
}

}  // namespace

Rndr::AndroidApplication* Rndr::AndroidApplication::Get()
{
    return g_android_app;
}

Rndr::AndroidApplication::AndroidApplication(SystemMessageHandler* message_handler, android_app* app)
    : PlatformApplication(message_handler), m_app(app)
{
    g_android_app = this;

    m_logcat_sink = Opal::MakeShared<Opal::LogSink, LogcatSink>(Opal::GetDefaultAllocator());
    if (m_logcat_sink.IsValid())
    {
        Opal::GetLogger().AddSink(m_logcat_sink);
    }

    m_app->userData = this;
    m_app->onAppCmd = &OnAppCommand;
    m_app->onInputEvent = &OnInputEvent;
    m_dpi_scale = ReadDpiScale(m_app);
}

Rndr::AndroidApplication::~AndroidApplication()
{
    // The window reports into this object while it is destroyed, so it goes first, the way ~LinuxApplication
    // drains its windows before the connection.
    while (m_generic_windows.GetSize() > 0)
    {
        auto it = m_generic_windows.begin();
        Opal::ScopePtr<GenericWindow> window = std::move(*it);
        m_generic_windows.Erase(it);
    }

    // android_main must not return before the glue asks it to. The activity's own thread hands every window and
    // input queue change to this one and waits for it to be taken, so returning early leaves it waiting for good.
    // Nothing is reported from here on: the handler is the Application, which is halfway through its destructor.
    m_app->onAppCmd = nullptr;
    m_app->onInputEvent = nullptr;
    m_app->userData = nullptr;
    if (m_app->destroyRequested == 0)
    {
        ANativeActivity_finish(m_app->activity);
    }
    while (m_app->destroyRequested == 0)
    {
        PollOnce(m_app, -1);
    }

    if (m_logcat_sink.IsValid())
    {
        Opal::GetLogger().RemoveSink(m_logcat_sink);
    }
    g_android_app = nullptr;
}

void Rndr::AndroidApplication::ProcessSystemEvents(u32 timeout_ms)
{
    int first_timeout = -1;
    if (timeout_ms != Application::k_infinite_timeout)
    {
        first_timeout = timeout_ms > static_cast<u32>(INT_MAX) ? INT_MAX : static_cast<int>(timeout_ms);
    }
    if (!PollOnce(m_app, first_timeout))
    {
        return;
    }
    while (PollOnce(m_app, 0))
    {
    }
}

Rndr::ErrorCode Rndr::AndroidApplication::WaitForNativeWindow()
{
    while (m_app->window == nullptr)
    {
        if (m_app->destroyRequested != 0)
        {
            RNDR_LOG_ERROR("The activity was destroyed before it was given a window");
            return ErrorCode::PlatformError;
        }
        PollOnce(m_app, -1);
    }
    return ErrorCode::Success;
}

void Rndr::AndroidApplication::SetWindow(AndroidWindow* window)
{
    m_window = window;
    if (m_window != nullptr)
    {
        m_window->m_is_focused = m_has_focus;
    }
}

void Rndr::AndroidApplication::CloseWindow()
{
    if (m_window == nullptr || m_window->IsClosed())
    {
        return;
    }
    m_message_handler->OnWindowClose(*m_window);
    if (m_window->IsClosed())
    {
        ANativeActivity_finish(m_app->activity);
    }
}

Rndr::ErrorCode Rndr::AndroidApplication::ExtractAssets(const char* asset_directory, const Opal::StringUtf8& destination)
{
    const Opal::ErrorCode create_status = Opal::CreateDirectory(destination);
    if (create_status != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Could not create {} to extract the assets into, error {}", destination.GetData(), static_cast<i32>(create_status));
        return ErrorCode::PlatformError;
    }

    AAssetManager* manager = m_app->activity->assetManager;
    AAssetDir* directory = AAssetManager_openDir(manager, asset_directory);
    if (directory == nullptr)
    {
        RNDR_LOG_ERROR("The APK has no asset directory '{}'", asset_directory);
        return ErrorCode::FileNotFound;
    }
    ErrorCode status = ErrorCode::Success;
    u32 file_count = 0;
    u32 extracted_count = 0;
    for (const char* name = AAssetDir_getNextFileName(directory); name != nullptr; name = AAssetDir_getNextFileName(directory))
    {
        ++file_count;
        Opal::StringUtf8 asset_path(asset_directory);
        if (!asset_path.IsEmpty())
        {
            asset_path += "/";
        }
        asset_path += name;
        Opal::StringUtf8 file_path = destination.Clone();
        file_path += "/";
        file_path += name;

        AAsset* asset = AAssetManager_open(manager, asset_path.GetData(), AASSET_MODE_STREAMING);
        if (asset == nullptr)
        {
            RNDR_LOG_ERROR("Could not open the asset {}", asset_path.GetData());
            status = ErrorCode::PlatformError;
            continue;
        }
        const i64 size = AAsset_getLength64(asset);
        struct stat existing = {};
        if (stat(file_path.GetData(), &existing) == 0 && existing.st_size == size)
        {
            AAsset_close(asset);
            continue;
        }
        Opal::DynamicArray<u8> content(size);
        i64 read_total = 0;
        while (read_total < size)
        {
            const int read = AAsset_read(asset, content.GetData() + read_total, static_cast<size_t>(size - read_total));
            if (read <= 0)
            {
                break;
            }
            read_total += read;
        }
        AAsset_close(asset);
        if (read_total != size)
        {
            RNDR_LOG_ERROR("Could not read the asset {}", asset_path.GetData());
            status = ErrorCode::PlatformError;
            continue;
        }
        if (Opal::WriteBytesToFile(file_path, {content.GetData(), content.GetSize()}) != Opal::ErrorCode::Success)
        {
            RNDR_LOG_ERROR("Could not write the asset {} to {}", asset_path.GetData(), file_path.GetData());
            status = ErrorCode::PlatformError;
            continue;
        }
        ++extracted_count;
    }
    AAssetDir_close(directory);

    if (file_count == 0)
    {
        RNDR_LOG_ERROR("The APK's asset directory '{}' holds no files", asset_directory);
        return ErrorCode::FileNotFound;
    }
    RNDR_LOG_INFO("Extracted {} of {} assets from '{}' into {}", extracted_count, file_count, asset_directory, destination.GetData());
    return status;
}

Rndr::f32 Rndr::AndroidApplication::GetDpiScale() const
{
    return m_dpi_scale;
}

void Rndr::AndroidApplication::OnAppCommand(android_app* app, i32 command)
{
    static_cast<AndroidApplication*>(app->userData)->HandleCommand(command);
}

Rndr::i32 Rndr::AndroidApplication::OnInputEvent(android_app* app, AInputEvent* event)
{
    return static_cast<AndroidApplication*>(app->userData)->HandleInput(event);
}

void Rndr::AndroidApplication::HandleCommand(i32 command)
{
    switch (command)
    {
        case APP_CMD_INIT_WINDOW:
        {
            // Before the window exists this is the window WaitForNativeWindow is waiting for, and the window picks
            // it up as it is created.
            if (m_window == nullptr)
            {
                break;
            }
            const Vector2i old_size = m_window->GetSize();
            m_window->m_native_window = m_app->window;
            m_window->m_width = ANativeWindow_getWidth(m_app->window);
            m_window->m_height = ANativeWindow_getHeight(m_app->window);
            m_message_handler->OnWindowNativeHandleChanged(*m_window);
            if (m_window->GetSize() != old_size)
            {
                m_message_handler->OnWindowSizeChanged(*m_window, m_window->m_width, m_window->m_height);
            }
            break;
        }
        case APP_CMD_TERM_WINDOW:
        {
            // The glue lets the old window go when this returns, so whatever was built over it has to be destroyed
            // inside the handler.
            if (m_window == nullptr)
            {
                break;
            }
            m_window->m_native_window = nullptr;
            m_message_handler->OnWindowNativeHandleChanged(*m_window);
            break;
        }
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
        {
            RefreshDpiScale();
            RefreshWindowSize();
            break;
        }
        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_LOST_FOCUS:
        {
            m_has_focus = command == APP_CMD_GAINED_FOCUS;
            if (m_window != nullptr)
            {
                m_window->m_is_focused = m_has_focus;
            }
            break;
        }
        case APP_CMD_DESTROY:
        {
            // Swiped away from recents, or finished by the system: the activity goes whatever the application says,
            // so a veto is logged and overruled.
            if (m_window == nullptr || m_window->IsClosed())
            {
                break;
            }
            m_message_handler->OnWindowClose(*m_window);
            if (!m_window->IsClosed())
            {
                RNDR_LOG_WARNING("The activity is being destroyed, closing the window despite the veto");
                m_window->MarkClosed();
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

void Rndr::AndroidApplication::RefreshWindowSize()
{
    if (m_window == nullptr || m_window->m_native_window == nullptr)
    {
        return;
    }
    const i32 width = ANativeWindow_getWidth(m_window->m_native_window);
    const i32 height = ANativeWindow_getHeight(m_window->m_native_window);
    if (width == m_window->m_width && height == m_window->m_height)
    {
        return;
    }
    m_window->m_width = width;
    m_window->m_height = height;
    m_message_handler->OnWindowSizeChanged(*m_window, width, height);
}

void Rndr::AndroidApplication::RefreshDpiScale()
{
    const f32 new_dpi_scale = ReadDpiScale(m_app);
    if (new_dpi_scale == m_dpi_scale)
    {
        return;
    }
    m_dpi_scale = new_dpi_scale;
    if (m_window != nullptr)
    {
        m_window->SetDpiScale(new_dpi_scale);
        m_window->on_dpi_change.Execute(new_dpi_scale);
        m_message_handler->OnWindowDpiChanged(*m_window, new_dpi_scale);
    }
}

Rndr::i32 Rndr::AndroidApplication::HandleInput(AInputEvent* event)
{
    if (m_window == nullptr)
    {
        return 0;
    }
    switch (AInputEvent_getType(event))
    {
        case AINPUT_EVENT_TYPE_KEY:
            return HandleKeyEvent(event);
        case AINPUT_EVENT_TYPE_MOTION:
            return HandleMotionEvent(event);
        default:
            return 0;
    }
}

Rndr::i32 Rndr::AndroidApplication::HandleKeyEvent(AInputEvent* event)
{
    const i32 key_code = AKeyEvent_getKeyCode(event);
    const i32 action = AKeyEvent_getAction(event);
    UpdateModifierKeys(AKeyEvent_getMetaState(event));

    if (key_code == AKEYCODE_BACK)
    {
        // Leaving is what back means, so it is the close button rather than a key. Taken on the release, as the
        // system takes it, and not when a gesture cancelled it.
        if (action == AKEY_EVENT_ACTION_UP && (AKeyEvent_getFlags(event) & AKEY_EVENT_FLAG_CANCELED) == 0)
        {
            CloseWindow();
        }
        return 1;
    }

    const InputPrimitive primitive = TranslateKey(key_code);
    if (primitive == InputPrimitive::Invalid)
    {
        // Volume, media and the rest stay the system's.
        return 0;
    }
    if (action == AKEY_EVENT_ACTION_DOWN)
    {
        m_message_handler->OnButtonDown(*m_window, primitive, AKeyEvent_getRepeatCount(event) > 0);
    }
    else if (action == AKEY_EVENT_ACTION_UP)
    {
        m_message_handler->OnButtonUp(*m_window, primitive, false);
    }
    return 1;
}

Rndr::i32 Rndr::AndroidApplication::HandleMotionEvent(AInputEvent* event)
{
    const i32 source = AInputEvent_getSource(event);
    const i32 action = AMotionEvent_getAction(event);
    const i32 masked_action = action & AMOTION_EVENT_ACTION_MASK;

    if ((source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE)
    {
        const Vector2i position = GetPointerPosition(event, 0);
        if (masked_action == AMOTION_EVENT_ACTION_SCROLL)
        {
            m_message_handler->OnMouseWheel(*m_window, AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, 0), position);
            return 1;
        }
        MovePointer(position);

        // Diffed rather than read off the action: which button an ACTION_BUTTON_PRESS is about needs API 33.
        struct ButtonMapping
        {
            i32 bit;
            InputPrimitive primitive;
        };
        constexpr ButtonMapping k_buttons[] = {
            {AMOTION_EVENT_BUTTON_PRIMARY, InputPrimitive::Mouse_LeftButton},
            {AMOTION_EVENT_BUTTON_SECONDARY, InputPrimitive::Mouse_RightButton},
            {AMOTION_EVENT_BUTTON_TERTIARY, InputPrimitive::Mouse_MiddleButton},
            {AMOTION_EVENT_BUTTON_BACK, InputPrimitive::Mouse_XButton1},
            {AMOTION_EVENT_BUTTON_FORWARD, InputPrimitive::Mouse_XButton2},
        };
        const i32 button_state = AMotionEvent_getButtonState(event);
        for (const ButtonMapping& button : k_buttons)
        {
            const bool was_down = (m_mouse_button_state & button.bit) != 0;
            const bool is_down = (button_state & button.bit) != 0;
            if (is_down && !was_down)
            {
                m_message_handler->OnMouseButtonDown(*m_window, button.primitive, position);
            }
            else if (!is_down && was_down)
            {
                m_message_handler->OnMouseButtonUp(*m_window, button.primitive, position);
            }
        }
        m_mouse_button_state = button_state;
        return 1;
    }

    if ((source & AINPUT_SOURCE_TOUCHSCREEN) != AINPUT_SOURCE_TOUCHSCREEN)
    {
        return 0;
    }
    // The first finger down is the left button until it lifts; any other finger is not reported.
    switch (masked_action)
    {
        case AMOTION_EVENT_ACTION_DOWN:
        {
            m_touch_pointer_id = AMotionEvent_getPointerId(event, 0);
            // A new touch starts where it lands, not with a jump from where the last one ended.
            m_cursor_pos = GetPointerPosition(event, 0);
            m_message_handler->OnMouseButtonDown(*m_window, InputPrimitive::Mouse_LeftButton, m_cursor_pos);
            break;
        }
        case AMOTION_EVENT_ACTION_MOVE:
        {
            const i32 index = FindPointer(event, m_touch_pointer_id);
            if (index >= 0)
            {
                MovePointer(GetPointerPosition(event, static_cast<size_t>(index)));
            }
            break;
        }
        case AMOTION_EVENT_ACTION_POINTER_UP:
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
        {
            if (m_touch_pointer_id < 0)
            {
                break;
            }
            if (masked_action == AMOTION_EVENT_ACTION_POINTER_UP)
            {
                const auto index = static_cast<size_t>((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                                       AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
                if (AMotionEvent_getPointerId(event, index) != m_touch_pointer_id)
                {
                    break;
                }
            }
            m_touch_pointer_id = -1;
            m_message_handler->OnMouseButtonUp(*m_window, InputPrimitive::Mouse_LeftButton, m_cursor_pos);
            break;
        }
        default:
        {
            break;
        }
    }
    return 1;
}

void Rndr::AndroidApplication::MovePointer(const Vector2i& position)
{
    const f32 delta_x = static_cast<f32>(position.x - m_cursor_pos.x);
    const f32 delta_y = static_cast<f32>(position.y - m_cursor_pos.y);
    m_cursor_pos = position;
    if ((delta_x == 0.0f && delta_y == 0.0f) || !m_window->m_is_enabled)
    {
        return;
    }
    m_message_handler->OnMouseMove(*m_window, delta_x, delta_y, position);
}

void Rndr::AndroidApplication::UpdateModifierKeys(i32 meta_state)
{
    m_modifier_keys.is_left_shift_down = (meta_state & AMETA_SHIFT_LEFT_ON) != 0;
    m_modifier_keys.is_right_shift_down = (meta_state & AMETA_SHIFT_RIGHT_ON) != 0;
    m_modifier_keys.is_left_control_down = (meta_state & AMETA_CTRL_LEFT_ON) != 0;
    m_modifier_keys.is_right_control_down = (meta_state & AMETA_CTRL_RIGHT_ON) != 0;
    m_modifier_keys.is_left_alt_down = (meta_state & AMETA_ALT_LEFT_ON) != 0;
    m_modifier_keys.is_right_alt_down = (meta_state & AMETA_ALT_RIGHT_ON) != 0;
    m_modifier_keys.is_left_command_down = (meta_state & AMETA_META_LEFT_ON) != 0;
    m_modifier_keys.is_right_command_down = (meta_state & AMETA_META_RIGHT_ON) != 0;
    m_modifier_keys.is_caps_locked = (meta_state & AMETA_CAPS_LOCK_ON) != 0;
}

void Rndr::AndroidApplication::ShowCursor(bool show)
{
    m_is_cursor_visible = show;
}

bool Rndr::AndroidApplication::IsCursorVisible() const
{
    return m_is_cursor_visible;
}

void Rndr::AndroidApplication::SetCursorPosition(const Vector2i& pos)
{
    RNDR_UNUSED(pos);
}

Rndr::Vector2i Rndr::AndroidApplication::GetCursorPosition() const
{
    return m_cursor_pos;
}

Rndr::ErrorCode Rndr::AndroidApplication::SetClipboardText(const Opal::StringUtf8& text)
{
    RNDR_UNUSED(text);
    return ErrorCode::FeatureNotSupported;
}

Opal::Expected<Opal::StringUtf8, Rndr::ErrorCode> Rndr::AndroidApplication::GetClipboardText()
{
    return Opal::Expected<Opal::StringUtf8, ErrorCode>(ErrorCode::FeatureNotSupported);
}

Opal::DynamicArray<Rndr::MonitorInfo> Rndr::AndroidApplication::GetMonitors() const
{
    Opal::DynamicArray<MonitorInfo> monitors;
    monitors.PushBack(GetPrimaryMonitor());
    return monitors;
}

Rndr::MonitorInfo Rndr::AndroidApplication::GetPrimaryMonitor() const
{
    MonitorInfo monitor;
    monitor.index = 0;
    monitor.name = "Display";
    if (m_app->window != nullptr)
    {
        monitor.size = Vector2i(ANativeWindow_getWidth(m_app->window), ANativeWindow_getHeight(m_app->window));
    }
    else if (m_window != nullptr)
    {
        monitor.size = m_window->GetSize();
    }
    else
    {
        monitor.size = Vector2i(0, 0);
    }
    monitor.position = Vector2i(0, 0);
    monitor.work_area_position = monitor.position;
    monitor.work_area_size = monitor.size;
    monitor.dpi_scale = m_dpi_scale;
    monitor.refresh_rate = 60;
    monitor.is_primary = true;
    return monitor;
}

Rndr::MonitorInfo Rndr::AndroidApplication::GetMonitorAtPosition(const Vector2i& pos) const
{
    RNDR_UNUSED(pos);
    return GetPrimaryMonitor();
}

Rndr::MonitorInfo Rndr::AndroidApplication::GetMonitorForWindow(const GenericWindow& window) const
{
    RNDR_UNUSED(window);
    return GetPrimaryMonitor();
}

Rndr::InputPrimitive Rndr::AndroidApplication::TranslateKey(i32 key_code)
{
    if (key_code >= AKEYCODE_A && key_code <= AKEYCODE_Z)
    {
        return static_cast<InputPrimitive>(static_cast<u16>(InputPrimitive::A) + (key_code - AKEYCODE_A));
    }
    if (key_code >= AKEYCODE_0 && key_code <= AKEYCODE_9)
    {
        return static_cast<InputPrimitive>(static_cast<u16>(InputPrimitive::Digit_0) + (key_code - AKEYCODE_0));
    }
    if (key_code >= AKEYCODE_F1 && key_code <= AKEYCODE_F12)
    {
        return static_cast<InputPrimitive>(static_cast<u16>(InputPrimitive::F1) + (key_code - AKEYCODE_F1));
    }
    if (key_code >= AKEYCODE_NUMPAD_0 && key_code <= AKEYCODE_NUMPAD_9)
    {
        return static_cast<InputPrimitive>(static_cast<u16>(InputPrimitive::Numpad_0) + (key_code - AKEYCODE_NUMPAD_0));
    }
    switch (key_code)
    {
        case AKEYCODE_DEL:
            return InputPrimitive::Backspace;
        case AKEYCODE_TAB:
            return InputPrimitive::Tab;
        case AKEYCODE_CLEAR:
            return InputPrimitive::Clear;
        case AKEYCODE_ENTER:
        case AKEYCODE_NUMPAD_ENTER:
            return InputPrimitive::Return;
        case AKEYCODE_SHIFT_LEFT:
            return InputPrimitive::LeftShift;
        case AKEYCODE_SHIFT_RIGHT:
            return InputPrimitive::RightShift;
        case AKEYCODE_CTRL_LEFT:
            return InputPrimitive::LeftCtrl;
        case AKEYCODE_CTRL_RIGHT:
            return InputPrimitive::RightCtrl;
        case AKEYCODE_ALT_LEFT:
            return InputPrimitive::LeftAlt;
        case AKEYCODE_ALT_RIGHT:
            return InputPrimitive::RightAlt;
        case AKEYCODE_META_LEFT:
            return InputPrimitive::LeftLogo;
        case AKEYCODE_META_RIGHT:
            return InputPrimitive::RightLogo;
        case AKEYCODE_BREAK:
            return InputPrimitive::Pause;
        case AKEYCODE_CAPS_LOCK:
            return InputPrimitive::CapsLock;
        case AKEYCODE_NUM_LOCK:
            return InputPrimitive::NumLock;
        case AKEYCODE_SCROLL_LOCK:
            return InputPrimitive::ScrollLock;
        case AKEYCODE_ESCAPE:
            return InputPrimitive::Escape;
        case AKEYCODE_SPACE:
            return InputPrimitive::Space;
        case AKEYCODE_PAGE_UP:
            return InputPrimitive::PageUp;
        case AKEYCODE_PAGE_DOWN:
            return InputPrimitive::PageDown;
        case AKEYCODE_MOVE_HOME:
            return InputPrimitive::Home;
        case AKEYCODE_MOVE_END:
            return InputPrimitive::End;
        case AKEYCODE_INSERT:
            return InputPrimitive::Insert;
        case AKEYCODE_FORWARD_DEL:
            return InputPrimitive::Delete;
        case AKEYCODE_DPAD_LEFT:
            return InputPrimitive::LeftArrow;
        case AKEYCODE_DPAD_UP:
            return InputPrimitive::UpArrow;
        case AKEYCODE_DPAD_RIGHT:
            return InputPrimitive::RightArrow;
        case AKEYCODE_DPAD_DOWN:
            return InputPrimitive::DownArrow;
        case AKEYCODE_NUMPAD_MULTIPLY:
        case AKEYCODE_STAR:
            return InputPrimitive::Multiply;
        case AKEYCODE_NUMPAD_ADD:
            return InputPrimitive::Add;
        case AKEYCODE_NUMPAD_COMMA:
            return InputPrimitive::Separator;
        case AKEYCODE_NUMPAD_SUBTRACT:
            return InputPrimitive::Subtract;
        case AKEYCODE_NUMPAD_DOT:
            return InputPrimitive::Decimal;
        case AKEYCODE_NUMPAD_DIVIDE:
            return InputPrimitive::Divide;
        case AKEYCODE_SEMICOLON:
            return InputPrimitive::Semicolon;
        case AKEYCODE_EQUALS:
        case AKEYCODE_PLUS:
            return InputPrimitive::Plus;
        case AKEYCODE_COMMA:
            return InputPrimitive::Comma;
        case AKEYCODE_MINUS:
            return InputPrimitive::Minus;
        case AKEYCODE_PERIOD:
            return InputPrimitive::Period;
        case AKEYCODE_SLASH:
            return InputPrimitive::Slash;
        case AKEYCODE_GRAVE:
            return InputPrimitive::Tilde;
        case AKEYCODE_LEFT_BRACKET:
            return InputPrimitive::OpenBracket;
        case AKEYCODE_RIGHT_BRACKET:
            return InputPrimitive::CloseBracket;
        case AKEYCODE_BACKSLASH:
            return InputPrimitive::Backslash;
        case AKEYCODE_APOSTROPHE:
            return InputPrimitive::Apostrophe;
        default:
            return InputPrimitive::Invalid;
    }
}

#endif  // RNDR_ANDROID
