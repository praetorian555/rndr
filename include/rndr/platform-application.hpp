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
    virtual void ProcessDeferredMessages(f32 delta_seconds) = 0;

    class GenericWindow* GetGenericWindowByNativeHandle(NativeWindowHandle handle);

protected:
    struct SystemMessageHandler* m_message_handler;
    Opal::AllocatorBase* m_allocator;
    Opal::DynamicArray<GenericWindow*> m_generic_windows;
};

}  // namespace Rndr
