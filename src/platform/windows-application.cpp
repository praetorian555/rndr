#include "rndr/platform/windows-application.hpp"

#include "glad/glad_wgl.h"

#include "opal/container/in-place-array.h"

#include "rndr/application.hpp"
#include "rndr/log.h"
#include "rndr/platform/windows-window.hpp"
#include "rndr/system-message-handler.hpp"

Rndr::WindowsApplication* g_windows_app = nullptr;

Rndr::WindowsApplication::WindowsApplication(SystemMessageHandler* message_handler, Opal::AllocatorBase* allocator)
    : PlatformApplication(message_handler, allocator), m_deferred_messages(m_allocator)
{
    g_windows_app = this;
    // TODO: Iterate over connected devices using raw input API
}

LRESULT RndrPrivate::WindowProc(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l)
{
    return g_windows_app->ProcessMessage(window_handle, msg_code, param_w, param_l);
}

Rndr::i32 Rndr::WindowsApplication::ProcessMessage(HWND window_handle, UINT msg_code, WPARAM param_w, LPARAM param_l)
{
    GenericWindow* window = GetGenericWindowByNativeHandle(reinterpret_cast<NativeWindowHandle>(window_handle));
    if (window == nullptr)
    {
        return static_cast<i32>(DefWindowProc(window_handle, msg_code, param_w, param_l));
    }
    GenericWindow& window_checked = *window;
    switch (msg_code)
    {
        case WM_CLOSE:
        {
            m_message_handler->OnWindowClose(window_checked);
            return 0;
        }
        case WM_SIZE:
        {
            const i32 width = LOWORD(param_l);
            const i32 height = HIWORD(param_l);
            m_message_handler->OnWindowSizeChanged(window_checked, width, height);
            return 0;
        }
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            const i32 virtual_key = TranslateKey(static_cast<i32>(param_w), static_cast<i32>(param_l));
            InputPrimitive primitive = InputPrimitive::A;
            if (!GetInputPrimitive(primitive, virtual_key))
            {
                RNDR_LOG_ERROR("Virtual key is not supported!");
                return 0;
            }
            const bool is_repeated = (param_l & 0x40000000) != 0;
            m_message_handler->OnButtonDown(window_checked, primitive, is_repeated);
            return 0;
        }
        case WM_SYSKEYUP:
        case WM_KEYUP:
        {
            const i32 virtual_key = TranslateKey(static_cast<i32>(param_w), static_cast<i32>(param_l));
            InputPrimitive primitive = InputPrimitive::A;
            if (!GetInputPrimitive(primitive, virtual_key))
            {
                RNDR_LOG_ERROR("Virtual key is not supported!");
                return 0;
            }
            const bool is_repeated = (param_l & 0x40000000) == 0;
            m_message_handler->OnButtonUp(window_checked, primitive, is_repeated);
            return 0;
        }
        case WM_CHAR:
        {
            const char16 character = static_cast<char16>(param_w);
            Opal::EncodingUtf16LE<char16> encoding = Opal::EncodingUtf16LE<char16>();
            uchar32 utf32_char = 0;
            Opal::ArrayView<const char16> input_view{&character, &character + 1};
            encoding.DecodeOne(input_view, utf32_char);
            const bool is_repeated = (param_l & 0x40000000) == 0;
            m_message_handler->OnCharacter(window_checked, utf32_char, is_repeated);
            return 0;
        }
    }
    return static_cast<i32>(DefWindowProc(window_handle, msg_code, param_w, param_l));
}

void Rndr::WindowsApplication::ProcessSystemEvents()
{
    for (auto* window : m_generic_windows)
    {
        MSG msg;
        while (PeekMessage(&msg, reinterpret_cast<HWND>(window->GetNativeHandle()), 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}
Rndr::i32 Rndr::WindowsApplication::TranslateKey(i32 win_key, i32 desc)
{
    switch (win_key)
    {
        case VK_MENU:
            return ((desc & 0x01000000) != 0) ? VK_LMENU : VK_RMENU;
        case VK_CONTROL:
            return ((desc & 0x01000000) != 0) ? VK_LCONTROL : VK_RCONTROL;
        case VK_SHIFT:
            return MapVirtualKey((desc & 0x00FF0000) >> 16, MAPVK_VSC_TO_VK_EX);
        default:
            return win_key;
    }
}

bool Rndr::WindowsApplication::GetInputPrimitive(InputPrimitive& out_primitive, i32 virtual_key)
{
    if (virtual_key < VK_BACK || virtual_key > VK_OEM_102)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key == 0x0A || virtual_key == 0x0B || virtual_key == 0x0E || virtual_key == 0x0F)
    {
        RNDR_ASSERT(false, "Unsupported virtual key");
        return false;
    }
    if (virtual_key >= 0x15 && virtual_key <= 0x1A)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key >= 0x1C && virtual_key <= 0x1F)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key >= 0x29 && virtual_key <= 0x2C)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key == 0x29)
    {
        RNDR_ASSERT(false, "Unsupported virtual key");
        return false;
    }
    if (virtual_key >= 0x3A && virtual_key <= 0x40)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key >= 0x5D && virtual_key <= 0x5F)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key >= 0x88 && virtual_key <= 0x8F)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key >= 0x92 && virtual_key <= 0x9F)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key >= 0xA6 && virtual_key <= 0xB9)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key >= 0xC1 && virtual_key <= 0xDA)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    if (virtual_key == 0xDF || virtual_key == 0xE0 || virtual_key == 0xE1)
    {
        RNDR_ASSERT(false, "Unsupported virtual key range");
        return false;
    }
    out_primitive = static_cast<InputPrimitive>(virtual_key);
    return true;
}
