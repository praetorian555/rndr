#pragma once

#include "opal/container/dynamic-array.h"

#include "rndr/definitions.h"
#include "rndr/generic-window.hpp"
#include "rndr/input-primitives.h"
#include "rndr/platform-application.hpp"

#if RNDR_WINDOWS
#include "rndr/platform/windows-forward-def.h"
#endif

namespace Rndr
{

struct WindowsDeferredMessage
{
    class WindowsWindow* window;
    UINT code;
    WPARAM param_w;
    LPARAM param_l;
};

class WindowsApplication : public PlatformApplication
{
public:
    WindowsApplication(struct SystemMessageHandler* message_handler, Opal::AllocatorBase* allocator);

    i32 ProcessMessage(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l);

    void ProcessSystemEvents() override;

private:
    i32 TranslateKey(i32 win_key, i32 desc);
    bool GetInputPrimitive(InputPrimitive& out_primitive, i32 virtual_key);

    Opal::DynamicArray<WindowsDeferredMessage> m_deferred_messages;
};

} // Rndr

namespace RndrPrivate
{
#if RNDR_WINDOWS
LRESULT CALLBACK WindowProc(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l);
#endif
}  // namespace RndrPrivate
