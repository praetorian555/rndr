#pragma once

#include "rndr/definitions.hpp"

#if RNDR_LINUX

#include <xcb/xcb.h>

#include "opal/container/expected.h"
#include "opal/container/scope-ptr.h"

#include "rndr/error-codes.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

class LinuxWindow : public GenericWindow
{
public:
    /**
     * Creates the OS window described by desc.
     * @return ErrorCode::InvalidArgument for a desc that makes no sense, ErrorCode::PlatformError when the OS
     * refuses to create the window.
     */
    [[nodiscard]] static Opal::Expected<Opal::ScopePtr<GenericWindow>, ErrorCode> Create(const GenericWindowDesc& desc);

    ~LinuxWindow();

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
    [[nodiscard]] NativeDisplayHandle GetNativeDisplayHandle() const override;

private:
    LinuxWindow(const GenericWindowDesc& desc);

    template <typename T, typename... Args>
    friend T* Opal::New(Opal::AllocatorBase* /*allocator*/, Args&&... /*args*/);

    /** The event pump updates the cached geometry and reads the enabled/close flags. */
    friend class LinuxApplication;

    /** The XCB half of Create: creates the OS window and applies the desc. */
    ErrorCode Initialize(const GenericWindowDesc& desc);

    /** Blocks until the window manager has applied the given size, or a short timeout expires. */
    void WaitForSize(i32 width, i32 height) const;

    /** Writes the _MOTIF_WM_HINTS decorations and functions derived from m_desc. */
    void ApplyDecorations();
    /** Writes WM_NORMAL_HINTS: position hints, plus a min==max size lock while not resizable. */
    void ApplySizeConstraints();
    /**
     * Asks the window manager to add or remove up to two _NET_WM_STATE atoms
     * (pass XCB_ATOM_NONE as second_atom for a single one).
     */
    void SendNetWmState(xcb_atom_t state_atom, xcb_atom_t second_atom, bool enable);
    /** Reads the current _NET_WM_STATE property and checks it for the given atom. */
    [[nodiscard]] bool HasNetWmState(xcb_atom_t state_atom) const;

    class LinuxApplication* m_app = nullptr;
    xcb_window_t m_window = XCB_NONE;
    bool m_high_precision_cursor = false;
    bool m_is_enabled = true;
    /** Distinguishes RequestClose from a user close so close vetoing only blocks the latter. */
    bool m_close_requested = false;
    GenericWindowMode m_mode = GenericWindowMode::Windowed;
    i32 m_pos_x = 0;
    i32 m_pos_y = 0;
    i32 m_width = 0;
    i32 m_height = 0;
};

}  // namespace Rndr

#endif  // RNDR_LINUX
