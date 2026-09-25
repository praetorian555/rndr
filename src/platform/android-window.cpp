#include "rndr/platform/android-window.hpp"

#if RNDR_ANDROID

#include <android/native_window.h>

#include "android_native_app_glue.h"

#include "opal/allocator.h"
#include "opal/container/scope-ptr.h"

#include "rndr/log.hpp"
#include "rndr/platform/android-application.hpp"

Opal::Expected<Opal::ScopePtr<Rndr::GenericWindow>, Rndr::ErrorCode> Rndr::AndroidWindow::Create(const GenericWindowDesc& desc)
{
    using ResultType = Opal::Expected<Opal::ScopePtr<GenericWindow>, ErrorCode>;

    AndroidApplication* app = AndroidApplication::Get();
    if (app == nullptr)
    {
        RNDR_LOG_ERROR("An Android window needs an AndroidApplication, and there is none");
        return ResultType(ErrorCode::PlatformError);
    }
    if (app->HasWindow())
    {
        RNDR_LOG_ERROR("An Android activity has one window, and it already exists");
        return ResultType(ErrorCode::InvalidArgument);
    }
    // Asked for before the window is waited on, so that the first one the activity hands over is already turned.
    if (desc.orientation != ScreenOrientation::Any)
    {
        const ErrorCode orientation_status = app->RequestOrientation(desc.orientation);
        if (orientation_status != ErrorCode::Success)
        {
            return ResultType(orientation_status);
        }
    }
    const ErrorCode err = app->WaitForNativeWindow();
    if (err != ErrorCode::Success)
    {
        return ResultType(err);
    }

    Opal::ScopePtr<GenericWindow> window =
        Opal::MakeScoped<GenericWindow, AndroidWindow>(Opal::GetDefaultAllocator(), desc, app, app->GetAndroidApp()->window);
    if (!window.IsValid())
    {
        return ResultType(ErrorCode::OutOfMemory);
    }
    app->SetWindow(static_cast<AndroidWindow*>(window.Get()));
    app->RefreshSafeInsets();
    return ResultType(std::move(window));
}

Rndr::AndroidWindow::AndroidWindow(const GenericWindowDesc& desc, AndroidApplication* app, ANativeWindow* native_window)
    : GenericWindow(desc), m_app(app), m_native_window(native_window)
{
    m_width = ANativeWindow_getWidth(native_window);
    m_height = ANativeWindow_getHeight(native_window);
    m_dpi_scale = app->GetDpiScale();
}

Rndr::AndroidWindow::~AndroidWindow()
{
    m_app->SetWindow(nullptr);
}

Rndr::ErrorCode Rndr::AndroidWindow::RequestClose()
{
    if (m_is_closed)
    {
        return ErrorCode::WindowAlreadyClosed;
    }
    m_app->CloseWindow();
    return ErrorCode::Success;
}

Rndr::ErrorCode Rndr::AndroidWindow::Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height)
{
    RNDR_UNUSED(pos_x);
    RNDR_UNUSED(pos_y);
    RNDR_UNUSED(width);
    RNDR_UNUSED(height);
    return ErrorCode::FeatureNotSupported;
}

Rndr::ErrorCode Rndr::AndroidWindow::MoveTo(i32 pos_x, i32 pos_y)
{
    RNDR_UNUSED(pos_x);
    RNDR_UNUSED(pos_y);
    return ErrorCode::FeatureNotSupported;
}

void Rndr::AndroidWindow::BringToFront() {}

void Rndr::AndroidWindow::Destroy() {}

void Rndr::AndroidWindow::Minimize() {}

void Rndr::AndroidWindow::Maximize() {}

void Rndr::AndroidWindow::Restore() {}

void Rndr::AndroidWindow::Enable(bool enable)
{
    m_is_enabled = enable;
}

void Rndr::AndroidWindow::Show() {}

void Rndr::AndroidWindow::Hide() {}

void Rndr::AndroidWindow::Focus() {}

void Rndr::AndroidWindow::SetMode(GenericWindowMode mode)
{
    RNDR_UNUSED(mode);
}

Rndr::ErrorCode Rndr::AndroidWindow::SetOrientation(ScreenOrientation orientation)
{
    const ErrorCode status = m_app->RequestOrientation(orientation);
    if (status != ErrorCode::Success)
    {
        return status;
    }
    return GenericWindow::SetOrientation(orientation);
}

Rndr::ErrorCode Rndr::AndroidWindow::SetOpacity(f32 opacity)
{
    RNDR_UNUSED(opacity);
    return ErrorCode::FeatureNotSupported;
}

Rndr::ErrorCode Rndr::AndroidWindow::SetTitle(const Opal::StringUtf8& title)
{
    RNDR_UNUSED(title);
    return ErrorCode::FeatureNotSupported;
}

void Rndr::AndroidWindow::SetResizable(bool resizable)
{
    RNDR_UNUSED(resizable);
}

void Rndr::AndroidWindow::SetTitleBarVisible(bool visible)
{
    RNDR_UNUSED(visible);
}

void Rndr::AndroidWindow::SetBorderVisible(bool visible)
{
    RNDR_UNUSED(visible);
}

void Rndr::AndroidWindow::SetMinimizeSupported(bool supported)
{
    RNDR_UNUSED(supported);
}

void Rndr::AndroidWindow::SetMaximizeSupported(bool supported)
{
    RNDR_UNUSED(supported);
}

void Rndr::AndroidWindow::SetCloseSupported(bool supported)
{
    RNDR_UNUSED(supported);
}

void Rndr::AndroidWindow::SetVisibleInTaskbar(bool visible)
{
    RNDR_UNUSED(visible);
}

void Rndr::AndroidWindow::SetAlwaysOnTop(bool always_on_top)
{
    RNDR_UNUSED(always_on_top);
}

bool Rndr::AndroidWindow::IsMaximized() const
{
    return false;
}

bool Rndr::AndroidWindow::IsMinimized() const
{
    return m_native_window == nullptr;
}

bool Rndr::AndroidWindow::IsVisible() const
{
    return m_native_window != nullptr;
}

bool Rndr::AndroidWindow::IsFocused() const
{
    return m_is_focused;
}

bool Rndr::AndroidWindow::IsEnabled() const
{
    return m_is_enabled;
}

bool Rndr::AndroidWindow::IsBorderlessFullscreen() const
{
    return false;
}

bool Rndr::AndroidWindow::IsResizable() const
{
    return false;
}

bool Rndr::AndroidWindow::IsWindowed() const
{
    return true;
}

bool Rndr::AndroidWindow::IsMouseHovering() const
{
    return false;
}

void Rndr::AndroidWindow::EnableHighPrecisionCursorMode(bool enable)
{
    RNDR_UNUSED(enable);
}

bool Rndr::AndroidWindow::IsHighPrecisionCursorModeEnabled() const
{
    return false;
}

Rndr::Vector2i Rndr::AndroidWindow::GetPosition() const
{
    return {0, 0};
}

Rndr::Vector2i Rndr::AndroidWindow::GetSize() const
{
    return {m_width, m_height};
}

Rndr::Vector2i Rndr::AndroidWindow::GetCursorClientPosition() const
{
    return m_app->GetCursorPosition();
}

Rndr::GenericWindowMode Rndr::AndroidWindow::GetMode() const
{
    return GenericWindowMode::Windowed;
}

Rndr::NativeWindowHandle Rndr::AndroidWindow::GetNativeHandle() const
{
    return reinterpret_cast<NativeWindowHandle>(m_native_window);
}

Rndr::NativeDisplayHandle Rndr::AndroidWindow::GetNativeDisplayHandle() const
{
    return reinterpret_cast<NativeDisplayHandle>(m_app->GetAndroidApp());
}

#endif  // RNDR_ANDROID
