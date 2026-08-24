#pragma once

// #include "glad/glad_wgl.h"

#include "opal/container/expected.h"
#include "opal/container/scope-ptr.h"

#include "rndr/error-codes.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/platform/windows-header.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

class WindowsWindow : public GenericWindow
{
public:
    /**
     * Creates the OS window described by desc.
     * @return ErrorCode::InvalidArgument for a desc that makes no sense, ErrorCode::PlatformError when the OS
     * refuses to create the window.
     */
    [[nodiscard]] static Opal::Expected<Opal::ScopePtr<GenericWindow>, ErrorCode> Create(const GenericWindowDesc& desc);

    ~WindowsWindow();

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

    [[nodiscard]] bool IsMaximized() const override;
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

    [[nodiscard]] Vector2i GetPosition() const override;
    [[nodiscard]] Vector2i GetSize() const override;
    [[nodiscard]] GenericWindowMode GetMode() const override;
    [[nodiscard]] NativeWindowHandle GetNativeHandle() const override;

private:
    WindowsWindow(const GenericWindowDesc& desc);

    template <typename T, typename... Args>
    friend T* Opal::New(Opal::AllocatorBase* /*allocator*/, Args&&... /*args*/);

    /** The Win32 half of Create: registers the class and creates the OS window. */
    ErrorCode Initialize(const GenericWindowDesc& desc);

    /** Persistent decoration style of a windowed window. Does not include the initial state bits. */
    static i32 GetWindowedStyle(const GenericWindowDesc& desc);
    static i32 GetFullscreenStyle(const GenericWindowDesc& desc);
    /** Style bits that only describe the state the window starts in, used solely at creation time. */
    static i32 GetInitialStateStyle(const GenericWindowDesc& desc);
    static i32 GetExtendedStyle(const GenericWindowDesc& desc);

    /** Recomputes both styles from m_desc and m_mode and applies them to the live window. */
    void ApplyStyle();
    /** Enables or grays out the close entry of the system menu, following m_desc.supports_close. */
    void ApplyCloseSupport();

    NativeWindowHandle m_native_window_handle;
    bool m_high_precision_cursor = false;
    GenericWindowMode m_mode = GenericWindowMode::Windowed;
    WINDOWPLACEMENT m_pre_fullscreen_placement;
    i32 m_pos_x = 0;
    i32 m_pos_y = 0;
    i32 m_width = 0;
    i32 m_height = 0;
};

}  // namespace Rndr
