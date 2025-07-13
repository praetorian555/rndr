#pragma once

#include "opal/container/dynamic-array.h"

#include "rndr/generic-window.hpp"
#include "rndr/math.hpp"
#include "rndr/types.hpp"

namespace Opal
{
struct AllocatorBase;
}
namespace Rndr
{

struct ModifierKeysState
{
    bool is_left_shift_down = false;
    bool is_right_shift_down = false;
    bool is_left_control_down = false;
    bool is_right_control_down = false;
    bool is_left_alt_down = false;
    bool is_right_alt_down = false;
    bool is_left_command_down = false;
    bool is_right_command_down = false;
    bool is_caps_locked = false;
};

/**
 * Represents how the application should modify cursor's position.
 */
enum class CursorPositionMode : u8
{
    /**
     * The cursor is moved by the user and will stay there until moved again. Default behaviour.
     */
    Normal,
    /**
     * The cursor is moved by the user, but it's reset to the center of the screen every frame (this reset will not trigger mouse position
     * update). Useful for FPS games.
     */
    ResetToCenter
};

class PlatformApplication
{
public:
    PlatformApplication(struct SystemMessageHandler* message_handler, Opal::AllocatorBase* allocator)
        : m_message_handler(message_handler), m_allocator(allocator), m_generic_windows(allocator)
    {
    }
    virtual ~PlatformApplication() = default;

    GenericWindow* CreateGenericWindow(const GenericWindowDesc& desc);
    void DestroyGenericWindow(GenericWindow* window);

    /**
     * Process any messages received from the OS in the previous frame, like input events.
     */
    virtual void ProcessSystemEvents() = 0;

    /**
     * Control if the OS provides more frequent and fine-grained curser movement updates.
     * @param enable If the mode should be enabled or not.
     * @param window To what window to apply this change.
     * @note On Windows this will trigger the generation of WM_INPUT system events.
     */
    virtual void EnableHighPrecisionCursorMode(bool enable, const GenericWindow& window) = 0;

    /**
     * Control cursor visibility.
     * @param show Should the cursor be shown or not.
     */
    virtual void ShowCursor(bool show) = 0;

    /**
     * Check if the cursor is visible.
     * @return Returns true if the cursor is visible, false otherwise.
     */
    [[nodiscard]] virtual bool IsCursorVisible() const = 0;

    /**
     * Set cursor position in screen space.
     * @param pos New cursor position.
     */
    virtual void SetCursorPosition(const Vector2i& pos) = 0;

    /**
     * Get the current cursor position.
     */
    [[nodiscard]] virtual Vector2i GetCursorPosition() const = 0;

    /**
     * Controls cursor position mode.
     * @param mode New cursor position mode.
     */
    virtual void SetCursorPositionMode(CursorPositionMode mode) = 0;

    /**
     * Get cursor position mode.
     */
    [[nodiscard]] virtual CursorPositionMode GetCursorPositionMode() const = 0;

    class GenericWindow* GetGenericWindowByNativeHandle(NativeWindowHandle handle);
    [[nodiscard]] const ModifierKeysState& GetModifierKeysState() const { return m_modifier_keys; }

protected:
    struct SystemMessageHandler* m_message_handler;
    Opal::AllocatorBase* m_allocator;
    Opal::DynamicArray<GenericWindow*> m_generic_windows;
    Opal::Ref<GenericWindow> m_focused_window;
    ModifierKeysState m_modifier_keys;
};

}  // namespace Rndr
