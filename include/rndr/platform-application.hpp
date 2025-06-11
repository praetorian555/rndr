#pragma once

#include "opal/container/dynamic-array.h"

#include "rndr/generic-window.hpp"
#include "rndr/types.h"

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

    virtual void ProcessSystemEvents() = 0;
    virtual void EnableHighPrecisionCursorMode(bool enable, const GenericWindow& window) = 0;

    class GenericWindow* GetGenericWindowByNativeHandle(NativeWindowHandle handle);

    [[nodiscard]] const ModifierKeysState& GetModifierKeysState() const { return m_modifier_keys; }

protected:
    struct SystemMessageHandler* m_message_handler;
    Opal::AllocatorBase* m_allocator;
    Opal::DynamicArray<GenericWindow*> m_generic_windows;
    ModifierKeysState m_modifier_keys;
};

}  // namespace Rndr
