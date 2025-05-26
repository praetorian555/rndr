#pragma once

#include "opal/container/string.h"

#include "rndr/error-codes.h"
#include "rndr/types.h"

namespace Opal
{
struct AllocatorBase;
}

namespace Rndr
{

enum class GenericWindowMode : u8
{
    Windowed,
    BorderlessFullscreen,

    Count
};

struct GenericWindowDesc
{
    int width = 1024;
    int height = 768;
    int start_x = 0;
    int start_y = 0;
    const char* name = "Default Window";
    bool resizable = true;
    bool supports_maximize = true;
    bool supports_minimize = true;
    bool supports_transparency = true;
    bool start_minimized = false;
    bool start_maximized = false;
    bool start_visible = true;
};

class GenericWindow
{
public:
    virtual ~GenericWindow() = default;

    /**
     * Requests closing of the window. Should trigger Application::on_window_close as if the user pressed x in the UI.
     */
    virtual ErrorCode RequestClose() = 0;

    /**
     * Marks a window as closed without triggering the Application::on_window_close event.
     */
    virtual ErrorCode ForceClose() = 0;

    virtual ErrorCode Reshape(i32 pos_x, i32 pos_y, i32 width, i32 height) = 0;
    virtual ErrorCode MoveTo(i32 pos_x, i32 pos_y) = 0;
    virtual ErrorCode BringToFront() = 0;
    virtual ErrorCode Destroy() = 0;
    virtual ErrorCode Minimize() = 0;
    virtual ErrorCode Maximize() = 0;
    virtual ErrorCode Restore() = 0;
    virtual ErrorCode Enable(bool enable) = 0;
    virtual ErrorCode Show() = 0;
    virtual ErrorCode Hide() = 0;
    virtual ErrorCode Focus() = 0;
    virtual ErrorCode SetMode(GenericWindowMode mode) = 0;
    virtual ErrorCode SetOpacity(f32 opacity) = 0;
    virtual ErrorCode SetTitle(const Opal::StringUtf8& title) = 0;

    [[nodiscard]] virtual bool IsClosed() const = 0;
    [[nodiscard]] virtual bool IsMaximized() const = 0;
    [[nodiscard]] virtual bool IsMinimized() const = 0;
    [[nodiscard]] virtual bool IsVisible() const = 0;
    [[nodiscard]] virtual bool IsFocused() const = 0;
    [[nodiscard]] virtual bool IsEnabled() const = 0;
    [[nodiscard]] virtual bool IsBorderless() const = 0;
    [[nodiscard]] virtual bool IsResizable() const = 0;
    [[nodiscard]] virtual bool IsWindowed() const = 0;
    [[nodiscard]] virtual bool IsMouseHovering() const = 0;

    virtual ErrorCode GetPositionAndSize(i32& pos_x, i32& pos_y, i32& width, i32& height) const = 0;
    [[nodiscard]] virtual GenericWindowMode GetMode() const = 0;
    [[nodiscard]] virtual NativeWindowHandle GetNativeHandle() const = 0;

protected:
    GenericWindow(const GenericWindowDesc& desc, Opal::AllocatorBase* allocator) : m_desc(desc), m_allocator(allocator) {}

    GenericWindowDesc m_desc;
    Opal::AllocatorBase* m_allocator;
};

} // namespace Rndr