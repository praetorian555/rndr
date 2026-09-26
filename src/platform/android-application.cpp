#include "rndr/platform/android-application.hpp"

#if RNDR_ANDROID

#include <climits>
#include <cmath>
#include <cstring>
#include <mutex>

#include <sys/stat.h>

#include <android/api-level.h>
#include <jni.h>

#include <android/asset_manager.h>
#include <android/choreographer.h>
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

#include "android-jni.hpp"

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

/**
 * Guards what the UI thread hands AndroidApplication - the committed text queue, the stale insets flag and the
 * on-screen keyboard's insets - and the pointer to the application that owns them, which the UI thread reaches while
 * android_main may be tearing it down.
 */
std::mutex g_text_input_mutex;

/** RndrActivity.nativeCommitText. Runs on the UI thread. */
void JNICALL NativeCommitText(JNIEnv* env, jclass /*activity_class*/, jstring text)
{
    if (text == nullptr)
    {
        return;
    }
    const jsize length = env->GetStringLength(text);
    const jchar* characters = env->GetStringChars(text, nullptr);
    if (characters == nullptr)
    {
        return;
    }
    const Opal::StringWide wide_text(reinterpret_cast<const Rndr::char16*>(characters), static_cast<Opal::StringWide::size_type>(length));
    env->ReleaseStringChars(text, characters);
    Opal::StringUtf32 code_points;
    if (Opal::Transcode(wide_text, code_points) != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("The on-screen keyboard committed text that is not valid UTF-16");
        return;
    }
    Rndr::AndroidApplication::QueueCommittedText(code_points);
}

/** RndrActivity.nativeWindowInsetsChanged. Runs on the UI thread. */
void JNICALL NativeWindowInsetsChanged(JNIEnv* /*env*/, jclass /*activity_class*/, jboolean keyboard_visible, jint keyboard_height)
{
    Rndr::AndroidApplication::QueueWindowInsetsChange(keyboard_visible == JNI_TRUE, keyboard_height);
}

/**
 * The characters OnCharacter reports, as on the desktop: printable ones, and backspace, tab and carriage return for
 * the keys that type them there. Enter types a line feed on Android and a carriage return on Windows and X11.
 */
Rndr::uchar32 ToReportedCharacter(Rndr::uchar32 character)
{
    if (character == '\n')
    {
        return '\r';
    }
    if (character >= 0x20 && character != 0x7F)
    {
        return character;
    }
    if (character == '\b' || character == '\t' || character == '\r')
    {
        return character;
    }
    return 0;
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
    SetUpJava();

    // The choreographer belongs to this thread's looper and calls back through it, so the rate arrives inside
    // ProcessSystemEvents like any other event. The first registration is guaranteed a call with the current rate.
    if (__builtin_available(android 30, *))
    {
        m_choreographer = AChoreographer_getInstance();
        if (m_choreographer != nullptr)
        {
            AChoreographer_registerRefreshRateCallback(m_choreographer, &OnRefreshRateChanged, this);
        }
    }
}

Rndr::AndroidApplication::~AndroidApplication()
{
    if (m_choreographer != nullptr)
    {
        if (__builtin_available(android 30, *))
        {
            AChoreographer_unregisterRefreshRateCallback(m_choreographer, &OnRefreshRateChanged, this);
        }
        m_choreographer = nullptr;
    }
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

    TearDownJava();
    if (m_logcat_sink.IsValid())
    {
        Opal::GetLogger().RemoveSink(m_logcat_sink);
    }
    const std::lock_guard<std::mutex> lock(g_text_input_mutex);
    g_android_app = nullptr;
}

void Rndr::AndroidApplication::ProcessSystemEvents(u32 timeout_ms)
{
    int first_timeout = -1;
    if (timeout_ms != Application::k_infinite_timeout)
    {
        first_timeout = timeout_ms > static_cast<u32>(INT_MAX) ? INT_MAX : static_cast<int>(timeout_ms);
    }
    if (PollOnce(m_app, first_timeout))
    {
        while (PollOnce(m_app, 0))
        {
        }
    }
    DeliverPendingCharacters();
    bool are_insets_stale = false;
    {
        const std::lock_guard<std::mutex> lock(g_text_input_mutex);
        are_insets_stale = m_are_insets_stale;
        m_are_insets_stale = false;
    }
    if (are_insets_stale)
    {
        RefreshSafeInsets();
        RefreshOnScreenKeyboard();
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

Rndr::ErrorCode Rndr::AndroidApplication::RequestOrientation(ScreenOrientation orientation)
{
    // The values of android.content.pm.ActivityInfo's SCREEN_ORIENTATION_ constants.
    constexpr jint k_unspecified = -1;
    constexpr jint k_sensor_landscape = 6;
    constexpr jint k_sensor_portrait = 7;
    jint requested = k_unspecified;
    switch (orientation)
    {
        case ScreenOrientation::Landscape:
            requested = k_sensor_landscape;
            break;
        case ScreenOrientation::Portrait:
            requested = k_sensor_portrait;
            break;
        default:
            break;
    }

    const JniScope jni(m_app->activity);
    if (!jni.IsValid())
    {
        return ErrorCode::PlatformError;
    }
    jclass activity_class = jni->GetObjectClass(jni.GetActivity());
    jmethodID set_requested_orientation = jni->GetMethodID(activity_class, "setRequestedOrientation", "(I)V");
    if (jni.Threw("looking up Activity.setRequestedOrientation"))
    {
        return ErrorCode::PlatformError;
    }
    jni->CallVoidMethod(jni.GetActivity(), set_requested_orientation, requested);
    if (jni.Threw("setting the requested screen orientation"))
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

void Rndr::AndroidApplication::SetUpJava()
{
    // This thread calls into Java on every key press and for the insets, so it stays attached for as long as the
    // application lives, and each JniScope finds it attached.
    JavaVM* vm = m_app->activity->vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED)
    {
        m_attached_to_java = vm->AttachCurrentThread(&env, nullptr) == JNI_OK;
    }

    const JniScope jni(m_app->activity);
    if (!jni.IsValid())
    {
        return;
    }

    jclass key_character_map = jni->FindClass("android/view/KeyCharacterMap");
    if (!jni.Threw("finding KeyCharacterMap"))
    {
        m_key_character_map_load = jni->GetStaticMethodID(key_character_map, "load", "(I)Landroid/view/KeyCharacterMap;");
        m_key_character_map_get = jni->GetMethodID(key_character_map, "get", "(II)I");
        if (jni.Threw("looking up the KeyCharacterMap methods"))
        {
            m_key_character_map_load = nullptr;
            m_key_character_map_get = nullptr;
        }
        else
        {
            m_key_character_map_class = static_cast<jclass>(jni->NewGlobalRef(key_character_map));
        }
    }

    // RndrActivity declares these; a plain NativeActivity does not, and registering them there throws. That is an
    // app that chose key events only, and insets read on resizes only, not an error.
    jclass activity_class = jni->GetObjectClass(jni.GetActivity());
    const JNINativeMethod natives[] = {
        {"nativeCommitText", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&NativeCommitText)},
        {"nativeWindowInsetsChanged", "(ZI)V", reinterpret_cast<void*>(&NativeWindowInsetsChanged)},
    };
    if (jni->RegisterNatives(activity_class, natives, static_cast<jint>(sizeof(natives) / sizeof(natives[0]))) != JNI_OK)
    {
        jni->ExceptionClear();
        RNDR_LOG_INFO("The activity is not dev.rndr.RndrActivity, so the on-screen keyboard can only send key events");
        return;
    }
    m_set_text_input_active = jni->GetMethodID(activity_class, "setTextInputActive", "(Z)V");
    if (jni.Threw("looking up RndrActivity.setTextInputActive"))
    {
        m_set_text_input_active = nullptr;
    }
}

void Rndr::AndroidApplication::TearDownJava()
{
    {
        const JniScope jni(m_app->activity);
        if (jni.IsValid() && m_key_character_map_class != nullptr)
        {
            jni->DeleteGlobalRef(m_key_character_map_class);
        }
    }
    m_key_character_map_class = nullptr;
    if (m_attached_to_java)
    {
        m_app->activity->vm->DetachCurrentThread();
        m_attached_to_java = false;
    }
}

Rndr::uchar32 Rndr::AndroidApplication::CharacterForKey(const AInputEvent* event) const
{
    // The keys that type a control character on the desktop do so here as well, whatever the keyboard's map says.
    switch (AKeyEvent_getKeyCode(event))
    {
        case AKEYCODE_DEL:
            return '\b';
        case AKEYCODE_TAB:
            return '\t';
        case AKEYCODE_ENTER:
        case AKEYCODE_NUMPAD_ENTER:
            return '\r';
        default:
            break;
    }
    if (m_key_character_map_class == nullptr)
    {
        return 0;
    }
    const JniScope jni(m_app->activity);
    if (!jni.IsValid())
    {
        return 0;
    }
    jobject map = jni->CallStaticObjectMethod(m_key_character_map_class, m_key_character_map_load, AInputEvent_getDeviceId(event));
    if (jni.Threw("loading the key character map") || map == nullptr)
    {
        return 0;
    }
    const jint character = jni->CallIntMethod(map, m_key_character_map_get, AKeyEvent_getKeyCode(event), AKeyEvent_getMetaState(event));
    if (jni.Threw("reading the key's character"))
    {
        return 0;
    }
    // KeyCharacterMap.COMBINING_ACCENT marks a dead key, which types nothing by itself; there is no composition here.
    constexpr jint k_combining_accent = static_cast<jint>(0x80000000u);
    if ((character & k_combining_accent) != 0)
    {
        return 0;
    }
    return ToReportedCharacter(static_cast<uchar32>(character));
}

Rndr::ErrorCode Rndr::AndroidApplication::SetTextInputActive(bool active)
{
    if (m_set_text_input_active == nullptr)
    {
        if (active)
        {
            ANativeActivity_showSoftInput(m_app->activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_IMPLICIT);
        }
        else
        {
            ANativeActivity_hideSoftInput(m_app->activity, 0);
        }
        return PlatformApplication::SetTextInputActive(active);
    }
    const JniScope jni(m_app->activity);
    if (!jni.IsValid())
    {
        return ErrorCode::PlatformError;
    }
    jni->CallVoidMethod(jni.GetActivity(), m_set_text_input_active, static_cast<jboolean>(active ? JNI_TRUE : JNI_FALSE));
    if (jni.Threw(active ? "showing the on-screen keyboard" : "hiding the on-screen keyboard"))
    {
        return ErrorCode::PlatformError;
    }
    return PlatformApplication::SetTextInputActive(active);
}

void Rndr::AndroidApplication::OnRefreshRateChanged(int64_t vsync_period_nanos, void* data)
{
    if (vsync_period_nanos <= 0)
    {
        return;
    }
    auto* app = static_cast<AndroidApplication*>(data);
    const f32 rate = static_cast<f32>(1.0e9 / static_cast<f64>(vsync_period_nanos));
    if (std::abs(rate - app->m_refresh_rate) < 0.01f)
    {
        return;
    }
    app->m_refresh_rate = rate;
    RNDR_LOG_INFO("Display refresh rate: {:.2f} Hz", rate);
    app->m_message_handler->OnMonitorChange();
}

Rndr::ErrorCode Rndr::AndroidApplication::ApplyPreferredRefreshRate()
{
    if (m_window == nullptr || m_window->m_native_window == nullptr)
    {
        return ErrorCode::Success;
    }
    if (__builtin_available(android 30, *))
    {
        const int32_t status = ANativeWindow_setFrameRate(m_window->m_native_window, m_window->GetPreferredRefreshRate(),
                                                          ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT);
        if (status != 0)
        {
            RNDR_LOG_ERROR("ANativeWindow_setFrameRate({}) failed, error {}", m_window->GetPreferredRefreshRate(), status);
            return ErrorCode::PlatformError;
        }
    }
    return ErrorCode::Success;
}

void Rndr::AndroidApplication::QueueCommittedText(const Opal::StringUtf32& text)
{
    const std::lock_guard<std::mutex> lock(g_text_input_mutex);
    if (g_android_app == nullptr)
    {
        return;
    }
    for (const uchar32 character : text)
    {
        const uchar32 reported = ToReportedCharacter(character);
        if (reported != 0)
        {
            g_android_app->m_pending_characters.PushBack(reported);
        }
    }
    ALooper_wake(g_android_app->m_app->looper);
}

void Rndr::AndroidApplication::QueueWindowInsetsChange(bool keyboard_visible, i32 keyboard_height)
{
    const std::lock_guard<std::mutex> lock(g_text_input_mutex);
    if (g_android_app == nullptr)
    {
        return;
    }
    g_android_app->m_are_insets_stale = true;
    g_android_app->m_is_keyboard_visible = keyboard_visible;
    g_android_app->m_keyboard_height = keyboard_height;
    ALooper_wake(g_android_app->m_app->looper);
}

void Rndr::AndroidApplication::RefreshOnScreenKeyboard()
{
    if (m_window == nullptr)
    {
        return;
    }
    OnScreenKeyboard keyboard;
    i32 height = 0;
    {
        const std::lock_guard<std::mutex> lock(g_text_input_mutex);
        keyboard.is_visible = m_is_keyboard_visible;
        height = m_keyboard_height;
    }
    // The inset is from the bottom edge, so where the keyboard starts moves with the window's height, which is why a
    // resize comes through here as well. Android says nothing of its sides; the keyboard's window keeps clear of a
    // cutout or a navigation bar on either side, as the safe insets do, so they bound it there.
    const Vector2i window_size = m_window->GetSize();
    const SafeInsets& safe = m_window->m_safe_insets;
    height = height < 0 ? 0 : (height > window_size.y ? window_size.y : height);
    const i32 width = window_size.x - safe.left - safe.right;
    if (keyboard.is_visible && height > 0 && width > 0)
    {
        keyboard.position = Vector2i(safe.left, window_size.y - height);
        keyboard.size = Vector2i(width, height);
    }
    if (keyboard == m_window->m_on_screen_keyboard)
    {
        return;
    }
    m_window->m_on_screen_keyboard = keyboard;
    RNDR_LOG_INFO("On-screen keyboard: {}, position ({}, {}), size ({}, {})", keyboard.is_visible ? "up" : "down", keyboard.position.x,
                  keyboard.position.y, keyboard.size.x, keyboard.size.y);
}

void Rndr::AndroidApplication::DeliverPendingCharacters()
{
    Opal::DynamicArray<uchar32> characters;
    {
        const std::lock_guard<std::mutex> lock(g_text_input_mutex);
        if (m_pending_characters.IsEmpty())
        {
            return;
        }
        characters = std::move(m_pending_characters);
        m_pending_characters = Opal::DynamicArray<uchar32>();
    }
    // Text typed while no window exists has nowhere to go.
    if (m_window == nullptr)
    {
        return;
    }
    for (const uchar32 character : characters)
    {
        m_message_handler->OnCharacter(*m_window, character, false);
    }
}

namespace
{
/**
 * WindowManager.getCurrentWindowMetrics(), API 30. A WindowManager query rather than a View's, so it is answered on the
 * calling thread without the UI thread.
 * @return The metrics, a local reference in the scope's frame, or null when a call threw, which the scope has logged.
 */
jobject GetCurrentWindowMetrics(const Rndr::JniScope& jni)
{
    jmethodID get_window_manager = jni->GetMethodID(jni->GetObjectClass(jni.GetActivity()), "getWindowManager", "()Landroid/view/WindowManager;");
    if (jni.Threw("looking up Activity.getWindowManager"))
    {
        return nullptr;
    }
    jobject window_manager = jni->CallObjectMethod(jni.GetActivity(), get_window_manager);
    if (jni.Threw("getting the window manager") || window_manager == nullptr)
    {
        return nullptr;
    }
    jmethodID get_current_window_metrics =
        jni->GetMethodID(jni->GetObjectClass(window_manager), "getCurrentWindowMetrics", "()Landroid/view/WindowMetrics;");
    if (jni.Threw("looking up WindowManager.getCurrentWindowMetrics"))
    {
        return nullptr;
    }
    jobject metrics = jni->CallObjectMethod(window_manager, get_current_window_metrics);
    if (jni.Threw("getting the window metrics"))
    {
        return nullptr;
    }
    return metrics;
}

/** The window's size in pixels, from WindowManager.getCurrentWindowMetrics().getBounds(). */
Opal::Expected<Rndr::Vector2i, Rndr::ErrorCode> ReadWindowSize(const Rndr::JniScope& jni)
{
    using Result = Opal::Expected<Rndr::Vector2i, Rndr::ErrorCode>;

    jobject metrics = GetCurrentWindowMetrics(jni);
    if (metrics == nullptr)
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    jmethodID get_bounds = jni->GetMethodID(jni->GetObjectClass(metrics), "getBounds", "()Landroid/graphics/Rect;");
    if (jni.Threw("looking up WindowMetrics.getBounds"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    jobject bounds = jni->CallObjectMethod(metrics, get_bounds);
    if (jni.Threw("getting the window bounds") || bounds == nullptr)
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    jclass rect_class = jni->GetObjectClass(bounds);
    jmethodID width = jni->GetMethodID(rect_class, "width", "()I");
    jmethodID height = jni->GetMethodID(rect_class, "height", "()I");
    if (jni.Threw("looking up Rect.width and Rect.height"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    const Rndr::Vector2i size(jni->CallIntMethod(bounds, width), jni->CallIntMethod(bounds, height));
    if (jni.Threw("measuring the window bounds"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    return Result(size);
}

/**
 * The window's system bar and display cutout insets, from
 * WindowManager.getCurrentWindowMetrics().getWindowInsets().getInsets(Type.systemBars() | Type.displayCutout()).
 */
Opal::Expected<Rndr::SafeInsets, Rndr::ErrorCode> ReadSafeInsets(const Rndr::JniScope& jni)
{
    using Result = Opal::Expected<Rndr::SafeInsets, Rndr::ErrorCode>;

    jobject metrics = GetCurrentWindowMetrics(jni);
    if (metrics == nullptr)
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    jmethodID get_window_insets = jni->GetMethodID(jni->GetObjectClass(metrics), "getWindowInsets", "()Landroid/view/WindowInsets;");
    if (jni.Threw("looking up WindowMetrics.getWindowInsets"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    jobject window_insets = jni->CallObjectMethod(metrics, get_window_insets);
    if (jni.Threw("getting the window insets") || window_insets == nullptr)
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }

    jclass type_class = jni->FindClass("android/view/WindowInsets$Type");
    if (jni.Threw("finding WindowInsets.Type"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    jmethodID system_bars = jni->GetStaticMethodID(type_class, "systemBars", "()I");
    if (jni.Threw("looking up WindowInsets.Type.systemBars"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    jmethodID display_cutout = jni->GetStaticMethodID(type_class, "displayCutout", "()I");
    if (jni.Threw("looking up WindowInsets.Type.displayCutout"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    const jint types = jni->CallStaticIntMethod(type_class, system_bars) | jni->CallStaticIntMethod(type_class, display_cutout);
    if (jni.Threw("asking for the inset types"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }

    jmethodID get_insets = jni->GetMethodID(jni->GetObjectClass(window_insets), "getInsets", "(I)Landroid/graphics/Insets;");
    if (jni.Threw("looking up WindowInsets.getInsets"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    jobject insets = jni->CallObjectMethod(window_insets, get_insets, types);
    if (jni.Threw("getting the insets") || insets == nullptr)
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    jclass insets_class = jni->GetObjectClass(insets);
    jfieldID left = jni->GetFieldID(insets_class, "left", "I");
    jfieldID top = jni->GetFieldID(insets_class, "top", "I");
    jfieldID right = jni->GetFieldID(insets_class, "right", "I");
    jfieldID bottom = jni->GetFieldID(insets_class, "bottom", "I");
    if (jni.Threw("looking up the fields of Insets"))
    {
        return Result(Rndr::ErrorCode::PlatformError);
    }
    return Result(Rndr::SafeInsets{.left = jni->GetIntField(insets, left),
                                   .top = jni->GetIntField(insets, top),
                                   .right = jni->GetIntField(insets, right),
                                   .bottom = jni->GetIntField(insets, bottom)});
}
}  // namespace

void Rndr::AndroidApplication::RefreshSafeInsets()
{
    if (m_window == nullptr)
    {
        return;
    }
    // WindowMetrics is API 30. Forge needs Vulkan 1.3, which no device older than that ships, so an older one is
    // not worth a second path through the deprecated View insets; it reports none.
    if (android_get_device_api_level() < 30)
    {
        return;
    }
    const JniScope jni(m_app->activity);
    if (!jni.IsValid())
    {
        return;
    }
    const Opal::Expected<SafeInsets, ErrorCode> insets = ReadSafeInsets(jni);
    if (!insets.HasValue() || insets.GetValue() == m_window->m_safe_insets)
    {
        return;
    }
    m_window->m_safe_insets = insets.GetValue();
    RNDR_LOG_INFO("Safe insets: left {}, top {}, right {}, bottom {}", m_window->m_safe_insets.left, m_window->m_safe_insets.top,
                  m_window->m_safe_insets.right, m_window->m_safe_insets.bottom);
}

Rndr::Vector2i Rndr::AndroidApplication::QueryWindowSize(ANativeWindow* native_window) const
{
    if (android_get_device_api_level() >= 30)
    {
        const JniScope jni(m_app->activity);
        if (jni.IsValid())
        {
            const Opal::Expected<Vector2i, ErrorCode> size = ReadWindowSize(jni);
            if (size.HasValue() && size.GetValue().x > 0 && size.GetValue().y > 0)
            {
                return size.GetValue();
            }
        }
    }
    return {ANativeWindow_getWidth(native_window), ANativeWindow_getHeight(native_window)};
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
            RefreshSafeInsets();
            m_window->m_native_window = m_app->window;
            // The rate was asked of the native window this one replaces.
            (void)ApplyPreferredRefreshRate();
            const Vector2i new_size = QueryWindowSize(m_app->window);
            m_window->m_width = new_size.x;
            m_window->m_height = new_size.y;
            RefreshOnScreenKeyboard();
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
            RefreshSafeInsets();
            RefreshWindowSize();
            // Asked again: a rate asked for before a turn of the screen was seen dropped. See ApplyPreferredRefreshRate.
            (void)ApplyPreferredRefreshRate();
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
    const Vector2i size = QueryWindowSize(m_window->m_native_window);
    const i32 width = size.x;
    const i32 height = size.y;
    if (width == m_window->m_width && height == m_window->m_height)
    {
        return;
    }
    m_window->m_width = width;
    m_window->m_height = height;
    RefreshOnScreenKeyboard();
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
        const bool is_repeated = AKeyEvent_getRepeatCount(event) > 0;
        m_message_handler->OnButtonDown(*m_window, primitive, is_repeated);
        const uchar32 character = CharacterForKey(event);
        if (character != 0)
        {
            m_message_handler->OnCharacter(*m_window, character, is_repeated);
        }
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

namespace
{
/** The ClipboardManager, from Context.getSystemService(Context.CLIPBOARD_SERVICE), or null with the reason logged. */
jobject GetClipboardManager(const Rndr::JniScope& jni)
{
    jclass activity_class = jni->GetObjectClass(jni.GetActivity());
    jmethodID get_system_service = jni->GetMethodID(activity_class, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (jni.Threw("looking up Context.getSystemService"))
    {
        return nullptr;
    }
    jstring service_name = jni->NewStringUTF("clipboard");
    if (jni.Threw("naming the clipboard service"))
    {
        return nullptr;
    }
    jobject manager = jni->CallObjectMethod(jni.GetActivity(), get_system_service, service_name);
    if (jni.Threw("getting the clipboard service"))
    {
        return nullptr;
    }
    if (manager == nullptr)
    {
        RNDR_LOG_ERROR("The system has no clipboard service");
    }
    return manager;
}
}  // namespace

Rndr::ErrorCode Rndr::AndroidApplication::SetClipboardText(const Opal::StringUtf8& text)
{
    // Java strings are UTF-16. NewStringUTF takes only modified UTF-8, which spells characters outside the basic
    // plane differently from UTF-8, and CheckJNI aborts a debuggable app that hands it the real thing.
    Opal::StringWide wide_text;
    if (Opal::Transcode(text, wide_text) != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Clipboard text is not valid UTF-8");
        return ErrorCode::InvalidArgument;
    }

    const JniScope jni(m_app->activity);
    if (!jni.IsValid())
    {
        return ErrorCode::PlatformError;
    }
    jobject manager = GetClipboardManager(jni);
    if (manager == nullptr)
    {
        return ErrorCode::PlatformError;
    }
    jstring java_text = jni->NewString(reinterpret_cast<const jchar*>(wide_text.GetData()), static_cast<jsize>(wide_text.GetSize()));
    if (jni.Threw("making the clipboard text"))
    {
        return ErrorCode::PlatformError;
    }
    jstring label = jni->NewStringUTF("rndr");
    if (jni.Threw("making the clip's label"))
    {
        return ErrorCode::PlatformError;
    }

    jclass clip_data_class = jni->FindClass("android/content/ClipData");
    if (jni.Threw("finding ClipData"))
    {
        return ErrorCode::PlatformError;
    }
    jmethodID new_plain_text = jni->GetStaticMethodID(clip_data_class, "newPlainText",
                                                      "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    if (jni.Threw("looking up ClipData.newPlainText"))
    {
        return ErrorCode::PlatformError;
    }
    jobject clip = jni->CallStaticObjectMethod(clip_data_class, new_plain_text, label, java_text);
    if (jni.Threw("making the clip"))
    {
        return ErrorCode::PlatformError;
    }

    jmethodID set_primary_clip = jni->GetMethodID(jni->GetObjectClass(manager), "setPrimaryClip", "(Landroid/content/ClipData;)V");
    if (jni.Threw("looking up ClipboardManager.setPrimaryClip"))
    {
        return ErrorCode::PlatformError;
    }
    jni->CallVoidMethod(manager, set_primary_clip, clip);
    if (jni.Threw("putting the clip on the clipboard"))
    {
        return ErrorCode::PlatformError;
    }
    return ErrorCode::Success;
}

Opal::Expected<Opal::StringUtf8, Rndr::ErrorCode> Rndr::AndroidApplication::GetClipboardText()
{
    using Result = Opal::Expected<Opal::StringUtf8, ErrorCode>;

    const JniScope jni(m_app->activity);
    if (!jni.IsValid())
    {
        return Result(ErrorCode::PlatformError);
    }
    jobject manager = GetClipboardManager(jni);
    if (manager == nullptr)
    {
        return Result(ErrorCode::PlatformError);
    }

    // Android 10 and later hand the clip only to the app with the focus; any other gets null, which reads as empty.
    jmethodID get_primary_clip = jni->GetMethodID(jni->GetObjectClass(manager), "getPrimaryClip", "()Landroid/content/ClipData;");
    if (jni.Threw("looking up ClipboardManager.getPrimaryClip"))
    {
        return Result(ErrorCode::PlatformError);
    }
    jobject clip = jni->CallObjectMethod(manager, get_primary_clip);
    if (jni.Threw("reading the clip off the clipboard"))
    {
        return Result(ErrorCode::PlatformError);
    }
    if (clip == nullptr)
    {
        return Result(Opal::StringUtf8());
    }

    jclass clip_class = jni->GetObjectClass(clip);
    jmethodID get_item_count = jni->GetMethodID(clip_class, "getItemCount", "()I");
    if (jni.Threw("looking up ClipData.getItemCount"))
    {
        return Result(ErrorCode::PlatformError);
    }
    jmethodID get_item_at = jni->GetMethodID(clip_class, "getItemAt", "(I)Landroid/content/ClipData$Item;");
    if (jni.Threw("looking up ClipData.getItemAt"))
    {
        return Result(ErrorCode::PlatformError);
    }
    const jint item_count = jni->CallIntMethod(clip, get_item_count);
    if (jni.Threw("counting the clip's items"))
    {
        return Result(ErrorCode::PlatformError);
    }
    if (item_count == 0)
    {
        return Result(Opal::StringUtf8());
    }
    jobject item = jni->CallObjectMethod(clip, get_item_at, 0);
    if (jni.Threw("reading the clip's first item"))
    {
        return Result(ErrorCode::PlatformError);
    }

    // The text the clip would paste as: a plain text item's own text, a URI's contents or the URI itself.
    jmethodID coerce_to_text =
        jni->GetMethodID(jni->GetObjectClass(item), "coerceToText", "(Landroid/content/Context;)Ljava/lang/CharSequence;");
    if (jni.Threw("looking up ClipData.Item.coerceToText"))
    {
        return Result(ErrorCode::PlatformError);
    }
    jobject char_sequence = jni->CallObjectMethod(item, coerce_to_text, jni.GetActivity());
    if (jni.Threw("turning the clip into text"))
    {
        return Result(ErrorCode::PlatformError);
    }
    if (char_sequence == nullptr)
    {
        return Result(Opal::StringUtf8());
    }
    jmethodID to_string = jni->GetMethodID(jni->GetObjectClass(char_sequence), "toString", "()Ljava/lang/String;");
    if (jni.Threw("looking up CharSequence.toString"))
    {
        return Result(ErrorCode::PlatformError);
    }
    auto java_text = static_cast<jstring>(jni->CallObjectMethod(char_sequence, to_string));
    if (jni.Threw("reading the clip's text"))
    {
        return Result(ErrorCode::PlatformError);
    }
    if (java_text == nullptr)
    {
        return Result(Opal::StringUtf8());
    }

    const jsize length = jni->GetStringLength(java_text);
    const jchar* characters = jni->GetStringChars(java_text, nullptr);
    if (characters == nullptr)
    {
        (void)jni.Threw("copying the clip's text out");
        return Result(ErrorCode::OutOfMemory);
    }
    const Opal::StringWide wide_text(reinterpret_cast<const char16*>(characters), static_cast<Opal::StringWide::size_type>(length));
    jni->ReleaseStringChars(java_text, characters);

    // A lone surrogate is allowed in a Java string, and has no UTF-8 spelling.
    Opal::StringUtf8 text;
    if (Opal::Transcode(wide_text, text) != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("The clipboard holds text that is not valid UTF-16");
        return Result(ErrorCode::CorruptData);
    }
    return Result(std::move(text));
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
        monitor.size = QueryWindowSize(m_app->window);
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
    monitor.refresh_rate = static_cast<i32>(std::lround(m_refresh_rate));
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
