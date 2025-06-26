#include "rndr/platform/windows-application.hpp"

#include <Windowsx.h>

#include "glad/glad_wgl.h"

#include "opal/container/in-place-array.h"

#include "rndr/application.hpp"
#include "rndr/log.h"
#include "rndr/platform/windows-window.hpp"
#include "rndr/system-message-handler.hpp"

Rndr::WindowsApplication* g_windows_app = nullptr;

Rndr::WindowsApplication::WindowsApplication(SystemMessageHandler* message_handler, Opal::AllocatorBase* allocator)
    : PlatformApplication(message_handler, allocator)
{
    g_windows_app = this;
    // Helps to get physical pixel size of monitor and not the scaled version.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
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
        case WM_ACTIVATE:
        {
            if (LOWORD(param_w) != WA_INACTIVE)
            {
                m_focused_window = &window_checked;
            }
            else
            {
                if (m_focused_window.GetPtr() == &window_checked)
                {
                    m_focused_window = nullptr;
                }
            }
            return 0;
        }
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            const i32 virtual_key = TranslateKey(static_cast<i32>(param_w), static_cast<i32>(param_l));
            InputPrimitive primitive = InputPrimitive::A;
            if (!GetInputPrimitive(primitive, virtual_key))
            {
                RNDR_LOG_ERROR("Virtual key 0x%X is not supported!", virtual_key);
                return 1;
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
                RNDR_LOG_ERROR("Virtual key 0x%X is not supported!", virtual_key);
                return 1;
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
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_XBUTTONUP:
        {
            POINT cursor_pos_window;
            cursor_pos_window.x = GET_X_LPARAM(param_l);
            cursor_pos_window.y = GET_Y_LPARAM(param_l);
            ClientToScreen(window_handle, &cursor_pos_window);
            const Vector2i cursor_pos(cursor_pos_window.x, cursor_pos_window.y);

            InputPrimitive primitive = InputPrimitive::A;
            bool mouse_up = false;
            bool double_click = false;
            switch (msg_code)
            {
                case WM_LBUTTONDOWN:
                {
                    primitive = InputPrimitive::Mouse_LeftButton;
                    break;
                }
                case WM_LBUTTONDBLCLK:
                {
                    primitive = InputPrimitive::Mouse_LeftButton;
                    double_click = true;
                    break;
                }
                case WM_LBUTTONUP:
                {
                    primitive = InputPrimitive::Mouse_LeftButton;
                    mouse_up = true;
                    break;
                }
                case WM_RBUTTONDOWN:
                {
                    primitive = InputPrimitive::Mouse_RightButton;
                    break;
                }
                case WM_RBUTTONDBLCLK:
                {
                    primitive = InputPrimitive::Mouse_RightButton;
                    double_click = true;
                    break;
                }
                case WM_RBUTTONUP:
                {
                    primitive = InputPrimitive::Mouse_RightButton;
                    mouse_up = true;
                    break;
                }
                case WM_MBUTTONDOWN:
                {
                    primitive = InputPrimitive::Mouse_MiddleButton;
                    break;
                }
                case WM_MBUTTONDBLCLK:
                {
                    primitive = InputPrimitive::Mouse_MiddleButton;
                    double_click = true;
                    break;
                }
                case WM_MBUTTONUP:
                {
                    primitive = InputPrimitive::Mouse_MiddleButton;
                    mouse_up = true;
                    break;
                }
                case WM_XBUTTONDOWN:
                {
                    primitive = (HIWORD(param_w) & XBUTTON1) != 0 ? InputPrimitive::Mouse_XButton1 : InputPrimitive::Mouse_XButton2;
                    break;
                }
                case WM_XBUTTONDBLCLK:
                {
                    primitive = (HIWORD(param_w) & XBUTTON1) != 0 ? InputPrimitive::Mouse_XButton1 : InputPrimitive::Mouse_XButton2;
                    double_click = true;
                    break;
                }
                case WM_XBUTTONUP:
                {
                    primitive = (HIWORD(param_w) & XBUTTON1) != 0 ? InputPrimitive::Mouse_XButton1 : InputPrimitive::Mouse_XButton2;
                    mouse_up = true;
                    break;
                }
                default:
                {
                    RNDR_ASSERT(false, "This should never be reached!");
                }
            }
            if (mouse_up)
            {
                m_message_handler->OnMouseButtonUp(window_checked, primitive, cursor_pos);
            }
            else if (double_click)
            {
                m_message_handler->OnMouseDoubleClick(window_checked, primitive, cursor_pos);
            }
            else
            {
                m_message_handler->OnMouseButtonDown(window_checked, primitive, cursor_pos);
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            POINT cursor_pos_window;
            cursor_pos_window.x = GET_X_LPARAM(param_l);
            cursor_pos_window.y = GET_Y_LPARAM(param_l);
            ClientToScreen(window_handle, &cursor_pos_window);
            const Vector2i cursor_pos(cursor_pos_window.x, cursor_pos_window.y);

            const i16 wheel_delta = GET_WHEEL_DELTA_WPARAM(param_w);
            constexpr f32 k_rotation_constant = 1 / 120.0f;
            m_message_handler->OnMouseWheel(window_checked, static_cast<f32>(wheel_delta) * k_rotation_constant, cursor_pos);
            return 0;
        }
        case WM_INPUT:
        {
            UINT struct_size = sizeof(RAWINPUT);
            Opal::InPlaceArray<uint8_t, sizeof(RAWINPUT)> data_buffer;

            // NOLINTNEXTLINE
            GetRawInputData(reinterpret_cast<HRAWINPUT>(param_l), RID_INPUT, data_buffer.GetData(), &struct_size, sizeof(RAWINPUTHEADER));

            RAWINPUT* raw_data = reinterpret_cast<RAWINPUT*>(data_buffer.GetData());

            if (raw_data->header.dwType == RIM_TYPEMOUSE)
            {
                [[maybe_unused]] const bool is_absolute_input = (raw_data->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == MOUSE_MOVE_ABSOLUTE;
                RNDR_ASSERT(!is_absolute_input, "This is coming from a tablet or a virtual desktop which is not supported!");
                m_message_handler->OnMouseMove(window_checked, static_cast<float>(raw_data->data.mouse.lLastX),
                                               static_cast<float>(raw_data->data.mouse.lLastY));
            }
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

    if (m_cursor_pos_mode == CursorPositionMode::ResetToCenter && m_focused_window.IsValid())
    {
        RECT window_rect;
        GetWindowRect( reinterpret_cast<HWND>(m_focused_window->GetNativeHandle()), &window_rect);
        const int width = window_rect.right - window_rect.left;
        const int height = window_rect.bottom - window_rect.top;
        const int mid_x = window_rect.left + (width / 2);
        const int mid_y = window_rect.top + (height / 2);
        ::SetCursorPos(mid_x, mid_y);
    }
}
void Rndr::WindowsApplication::EnableHighPrecisionCursorMode(bool enable, const GenericWindow& window)
{
    HWND window_handle = nullptr;
    if (enable)
    {
        window_handle = reinterpret_cast<HWND>(window.GetNativeHandle());
    }

    constexpr uint16_t k_hid_usage_page_generic = 0x01;
    constexpr uint16_t k_hid_usage_generic_mouse = 0x02;
    Opal::InPlaceArray<RAWINPUTDEVICE, 1> raw_devices;
    raw_devices[0].usUsagePage = k_hid_usage_page_generic;
    raw_devices[0].usUsage = k_hid_usage_generic_mouse;
    raw_devices[0].dwFlags = RIDEV_INPUTSINK;
    raw_devices[0].hwndTarget = window_handle;
    RegisterRawInputDevices(raw_devices.GetData(), 1, sizeof(raw_devices[0]));
}

void Rndr::WindowsApplication::ShowCursor(bool show)
{
    ::ShowCursor(show ? TRUE : FALSE);
}

bool Rndr::WindowsApplication::IsCursorVisible() const
{
    CURSORINFO cursor_info;
    GetCursorInfo(&cursor_info);
    return (cursor_info.flags & CURSOR_SHOWING) != 0u;
}

void Rndr::WindowsApplication::SetCursorPosition(const Vector2i& pos)
{
    SetCursorPos(pos.x, pos.y);
}

Rndr::Vector2i Rndr::WindowsApplication::GetCursorPosition() const
{
    POINT cursor_pos;
    GetCursorPos(&cursor_pos);
    return {cursor_pos.x, cursor_pos.y};
}

void Rndr::WindowsApplication::SetCursorPositionMode(CursorPositionMode mode)
{
    m_cursor_pos_mode = mode;
}

Rndr::CursorPositionMode Rndr::WindowsApplication::GetCursorPositionMode() const
{
    return m_cursor_pos_mode;
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
        return false;
    }
    if (virtual_key == 0x0A || virtual_key == 0x0B || virtual_key == 0x0E || virtual_key == 0x0F)
    {
        return false;
    }
    if (virtual_key >= 0x15 && virtual_key <= 0x1A)
    {
        return false;
    }
    if (virtual_key >= 0x1C && virtual_key <= 0x1F)
    {
        return false;
    }
    if (virtual_key >= 0x29 && virtual_key <= 0x2C)
    {
        return false;
    }
    if (virtual_key == 0x29)
    {
        return false;
    }
    if (virtual_key >= 0x3A && virtual_key <= 0x40)
    {
        return false;
    }
    if (virtual_key >= 0x5D && virtual_key <= 0x5F)
    {
        return false;
    }
    if (virtual_key >= 0x88 && virtual_key <= 0x8F)
    {
        return false;
    }
    if (virtual_key >= 0x92 && virtual_key <= 0x9F)
    {
        return false;
    }
    if (virtual_key >= 0xA6 && virtual_key <= 0xB9)
    {
        return false;
    }
    if (virtual_key >= 0xC1 && virtual_key <= 0xDA)
    {
        return false;
    }
    if (virtual_key == 0xDF || virtual_key == 0xE0 || virtual_key == 0xE1)
    {
        return false;
    }
    out_primitive = static_cast<InputPrimitive>(virtual_key);
    return true;
}
