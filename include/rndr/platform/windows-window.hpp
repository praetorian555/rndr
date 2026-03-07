#pragma once

// #include "glad/glad_wgl.h"

#include "rndr/generic-window.hpp"
#include "rndr/platform/windows-header.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

class WindowsWindow : public GenericWindow
{
public:
    WindowsWindow(const GenericWindowDesc& desc);
    ~WindowsWindow();

    void RequestClose() override;

    void Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height) override;
    void MoveTo(i32 pos_x, i32 pos_y) override;
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
    void SetOpacity(f32 opacity) override;
    void SetTitle(const Opal::StringUtf8& title) override;

    [[nodiscard]] bool IsMaximized() const override;
    [[nodiscard]] bool IsMinimized() const override;
    [[nodiscard]] bool IsVisible() const override;
    [[nodiscard]] bool IsFocused() const override;
    [[nodiscard]] bool IsEnabled() const override;
    [[nodiscard]] bool IsBorderless() const override;
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
    static i32 GetWindowedStyle(const GenericWindowDesc& desc);
    static i32 GetFullscreenStyle(const GenericWindowDesc& desc);

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
