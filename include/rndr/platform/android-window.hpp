#pragma once

#include "rndr/definitions.hpp"

#if RNDR_ANDROID

#include "opal/container/expected.h"
#include "opal/container/scope-ptr.h"

#include "rndr/error-codes.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/platform/android-forward-def.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

/**
 * The activity's one full-screen window, mapped onto GenericWindow. What the platform has no notion of -
 * moving, resizing, titles, decorations, minimizing and the rest - returns ErrorCode::FeatureNotSupported
 * where the method returns a code and does nothing where it does not.
 *
 * The native handle is the ANativeWindow while the activity is in the foreground and null while it is in the
 * background; see AndroidApplication for how the change is reported.
 */
class AndroidWindow : public GenericWindow
{
public:
    /**
     * Waits for the activity's native window and wraps it.
     * @return ErrorCode::InvalidArgument when the window already exists, since there is only one;
     *         ErrorCode::PlatformError when there is no AndroidApplication or the activity was destroyed
     *         before it had a window.
     */
    [[nodiscard]] static Opal::Expected<Opal::ScopePtr<GenericWindow>, ErrorCode> Create(const GenericWindowDesc& desc);
    ~AndroidWindow() override;

    /** Runs the close through AndroidApplication::CloseWindow. */
    ErrorCode RequestClose() override;
    ErrorCode Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height) override;
    ErrorCode MoveTo(i32 pos_x, i32 pos_y) override;
    void BringToFront() override;
    void Destroy() override;
    void Minimize() override;
    void Maximize() override;
    void Restore() override;
    void Enable(bool enable) override;
    void Show() override;
    void Hide() override;
    void Focus() override;
    void SetMode(GenericWindowMode mode) override;
    ErrorCode SetOpacity(f32 opacity) override;
    ErrorCode SetTitle(const Opal::StringUtf8& title) override;
    void SetResizable(bool resizable) override;
    void SetTitleBarVisible(bool visible) override;
    void SetBorderVisible(bool visible) override;
    void SetMinimizeSupported(bool supported) override;
    void SetMaximizeSupported(bool supported) override;
    void SetCloseSupported(bool supported) override;
    void SetVisibleInTaskbar(bool visible) override;
    void SetAlwaysOnTop(bool always_on_top) override;
    /** Through AndroidApplication::RequestOrientation. */
    ErrorCode SetOrientation(ScreenOrientation orientation) override;

    [[nodiscard]] bool IsMaximized() const override;
    /** True while the activity is in the background and has no native window. */
    [[nodiscard]] bool IsMinimized() const override;
    [[nodiscard]] bool IsVisible() const override;
    [[nodiscard]] bool IsFocused() const override;
    [[nodiscard]] bool IsEnabled() const override;
    [[nodiscard]] bool IsBorderlessFullscreen() const override;
    [[nodiscard]] bool IsResizable() const override;
    [[nodiscard]] bool IsWindowed() const override;
    [[nodiscard]] bool IsMouseHovering() const override;

    void EnableHighPrecisionCursorMode(bool enable) override;
    [[nodiscard]] bool IsHighPrecisionCursorModeEnabled() const override;

    /** Always (0, 0): the window is the screen. */
    [[nodiscard]] Vector2i GetPosition() const override;
    /** The native window's size, or the last one it had while the activity is in the background. */
    [[nodiscard]] Vector2i GetSize() const override;
    /** Where the last touch or mouse event was. */
    [[nodiscard]] Vector2i GetCursorClientPosition() const override;
    [[nodiscard]] GenericWindowMode GetMode() const override;
    /** The ANativeWindow, or null while the activity is in the background. */
    [[nodiscard]] NativeWindowHandle GetNativeHandle() const override;
    /** The android_app, for a caller that needs the asset manager or internalDataPath. */
    [[nodiscard]] NativeDisplayHandle GetNativeDisplayHandle() const override;

private:
    AndroidWindow(const GenericWindowDesc& desc, class AndroidApplication* app, ANativeWindow* native_window);

    template <typename T, typename... Args>
    friend T* Opal::New(Opal::AllocatorBase* /*allocator*/, Args&&... /*args*/);
    /** The event pump swaps the native window and updates the cached size, focus and cursor position. */
    friend class AndroidApplication;

    class AndroidApplication* m_app = nullptr;
    ANativeWindow* m_native_window = nullptr;
    bool m_is_focused = false;
    bool m_is_enabled = true;
    i32 m_width = 0;
    i32 m_height = 0;
};

}  // namespace Rndr

#endif  // RNDR_ANDROID
