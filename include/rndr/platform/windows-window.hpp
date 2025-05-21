#pragma once

// #include "glad/glad_wgl.h"

#include "rndr/platform/windows-header.h"
#include "rndr/generic-window.hpp"
#include "rndr/types.h"

namespace Rndr
{

class WindowsWindow : public GenericWindow
{
public:
    WindowsWindow(const GenericWindowDesc& desc, Opal::AllocatorBase* allocator);

    ErrorCode RequestClose() override;
    ErrorCode ForceClose() override;

    ErrorCode Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height) override;
    ErrorCode MoveTo(i32 pos_x, i32 pos_y) override;
    ErrorCode BringToFront() override;
    ErrorCode Destroy() override;
    ErrorCode Minimize() override;
    ErrorCode Maximize() override;
    ErrorCode Restore() override;
    ErrorCode Enable(bool enable) override;
    ErrorCode Show() override;
    ErrorCode Hide() override;
    ErrorCode Focus() override;

    ErrorCode SetMode(GenericWindowMode mode) override;
    ErrorCode SetOpacity(f32 opacity) override;
    ErrorCode SetTitle(const Opal::StringUtf8& title) override;

    [[nodiscard]] bool IsClosed() const override;
    [[nodiscard]] bool IsMaximized() const override;
    [[nodiscard]] bool IsMinimized() const override;
    [[nodiscard]] bool IsVisible() const override;
    [[nodiscard]] bool IsFocused() const override;
    [[nodiscard]] bool IsEnabled() const override;
    [[nodiscard]] bool IsBorderless() const override;
    [[nodiscard]] bool IsResizable() const override;
    [[nodiscard]] bool IsWindowed() const override;
    [[nodiscard]] bool IsMouseHovering() const override;

    ErrorCode GetPositionAndSize(i32& pos_x, i32& pos_y, i32& width, i32& height) const override;
    [[nodiscard]] GenericWindowMode GetMode() const override;
    [[nodiscard]] NativeWindowHandle GetNativeHandle() const override;

private:
    static i32 GetWindowedStyle(const GenericWindowDesc& desc);
    static i32 GetFullscreenStyle(const GenericWindowDesc& desc);

    NativeWindowHandle m_native_window_handle;
    GenericWindowMode m_mode;
    WINDOWPLACEMENT m_pre_fullscreen_placement;
    bool m_is_closed = false;
    i32 m_pos_x = 0;
    i32 m_pos_y = 0;
    i32 m_width = 0;
    i32 m_height = 0;
};

}  // namespace Rndr
