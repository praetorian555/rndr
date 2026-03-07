#include "rndr/platform/windows-application.hpp"

#include <Windowsx.h>

#include "glad/glad_wgl.h"

#include "opal/container/in-place-array.h"

#include <ShellScalingApi.h>

#include "rndr/application.hpp"
#include "rndr/log.hpp"
#include "rndr/monitor-info.hpp"
#include "rndr/platform/windows-window.hpp"
#include "rndr/system-message-handler.hpp"

Rndr::WindowsApplication* g_windows_app = nullptr;

Rndr::WindowsApplication::WindowsApplication(SystemMessageHandler* message_handler)
    : PlatformApplication(message_handler)
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
    Opal::Ref<GenericWindow> window = GetGenericWindowByNativeHandle(reinterpret_cast<NativeWindowHandle>(window_handle));
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
                RNDR_LOG_ERROR("Virtual key {:#X} is not supported!", virtual_key);
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
                RNDR_LOG_ERROR("Virtual key {:#X} is not supported!", virtual_key);
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
        case WM_DISPLAYCHANGE:
        {
            m_message_handler->OnMonitorChange();
            return 0;
        }
        case WM_DPICHANGED:
        {
            const f32 new_dpi_scale = static_cast<f32>(HIWORD(param_w)) / 96.0f;
            m_message_handler->OnWindowDpiChanged(window_checked, new_dpi_scale);
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
    for (const auto& window : m_generic_windows)
    {
        MSG msg;
        while (PeekMessage(&msg, reinterpret_cast<HWND>(window->GetNativeHandle()), 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (m_focused_window.IsValid() && m_focused_window->GetCursorPositionMode() == CursorPositionMode::ResetToCenter)
    {
        RECT window_rect;
        GetWindowRect(reinterpret_cast<HWND>(m_focused_window->GetNativeHandle()), &window_rect);
        const int width = window_rect.right - window_rect.left;
        const int height = window_rect.bottom - window_rect.top;
        const int mid_x = window_rect.left + (width / 2);
        const int mid_y = window_rect.top + (height / 2);
        ::SetCursorPos(mid_x, mid_y);
    }
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

namespace
{

/**
 * Uses the CCD API (QueryDisplayConfig + DisplayConfigGetDeviceInfo) to get the friendly monitor
 * name (e.g., "DELL U2720Q") for a given GDI device name (e.g., "\\\\.\\DISPLAY1").
 */
Opal::StringUtf8 GetMonitorFriendlyName(const wchar_t* gdi_device_name)
{
    UINT32 path_count = 0;
    UINT32 mode_count = 0;
    if (::GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) != ERROR_SUCCESS)
    {
        return Opal::StringUtf8();
    }

    Opal::DynamicArray<DISPLAYCONFIG_PATH_INFO> paths(path_count);
    Opal::DynamicArray<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
    if (::QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.GetData(), &mode_count, modes.GetData(), nullptr) != ERROR_SUCCESS)
    {
        return Opal::StringUtf8();
    }

    for (UINT32 i = 0; i < path_count; i++)
    {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name = {};
        source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source_name.header.size = sizeof(source_name);
        source_name.header.adapterId = paths[i].sourceInfo.adapterId;
        source_name.header.id = paths[i].sourceInfo.id;
        if (::DisplayConfigGetDeviceInfo(&source_name.header) != ERROR_SUCCESS)
        {
            continue;
        }

        if (wcscmp(source_name.viewGdiDeviceName, gdi_device_name) != 0)
        {
            continue;
        }

        DISPLAYCONFIG_TARGET_DEVICE_NAME target_name = {};
        target_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        target_name.header.size = sizeof(target_name);
        target_name.header.adapterId = paths[i].targetInfo.adapterId;
        target_name.header.id = paths[i].targetInfo.id;
        if (::DisplayConfigGetDeviceInfo(&target_name.header) != ERROR_SUCCESS)
        {
            continue;
        }

        if (target_name.flags.friendlyNameFromEdid)
        {
            Opal::StringWide wide_name(target_name.monitorFriendlyDeviceName);
            Opal::StringUtf8 utf8_name(wide_name.GetSize() * 4, '\0');
            if (Opal::Transcode(wide_name, utf8_name) == Opal::ErrorCode::Success)
            {
                return utf8_name;
            }
        }
        break;
    }

    return Opal::StringUtf8();
}

Rndr::MonitorInfo MonitorInfoFromHMONITOR(HMONITOR hmonitor, Rndr::i32 index)
{
    MONITORINFOEX mi = {};
    mi.cbSize = sizeof(MONITORINFOEX);
    ::GetMonitorInfo(hmonitor, &mi);

    Rndr::MonitorInfo info;
    info.index = index;
    info.position = {mi.rcMonitor.left, mi.rcMonitor.top};
    info.size = {mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top};
    info.work_area_position = {mi.rcWork.left, mi.rcWork.top};
    info.work_area_size = {mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top};
    info.is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

    UINT dpi_x = 96;
    UINT dpi_y = 96;
    ::GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
    info.dpi_scale = static_cast<Rndr::f32>(dpi_x) / 96.0f;

    DEVMODE dev_mode = {};
    dev_mode.dmSize = sizeof(DEVMODE);
    if (::EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &dev_mode))
    {
        info.refresh_rate = static_cast<Rndr::i32>(dev_mode.dmDisplayFrequency);
    }

    info.name = GetMonitorFriendlyName(mi.szDevice);
    if (info.name.IsEmpty())
    {
        // Fallback to GDI device name if friendly name is unavailable.
        Opal::StringWide wide_name(mi.szDevice);
        Opal::StringUtf8 utf8_name(wide_name.GetSize() * 4, '\0');
        if (Opal::Transcode(wide_name, utf8_name) == Opal::ErrorCode::Success)
        {
            info.name = std::move(utf8_name);
        }
    }

    return info;
}

struct EnumMonitorsData
{
    Opal::DynamicArray<Rndr::MonitorInfo>* monitors;
    Rndr::i32 current_index;
};

BOOL CALLBACK EnumMonitorsCallback(HMONITOR hmonitor, HDC, LPRECT, LPARAM user_data)
{
    auto* data = reinterpret_cast<EnumMonitorsData*>(user_data);
    data->monitors->PushBack(MonitorInfoFromHMONITOR(hmonitor, data->current_index));
    data->current_index++;
    return TRUE;
}

}  // namespace

Opal::DynamicArray<Rndr::MonitorInfo> Rndr::WindowsApplication::GetMonitors() const
{
    Opal::DynamicArray<MonitorInfo> monitors;
    EnumMonitorsData data = {&monitors, 0};
    ::EnumDisplayMonitors(nullptr, nullptr, EnumMonitorsCallback, reinterpret_cast<LPARAM>(&data));
    return monitors;
}

Rndr::MonitorInfo Rndr::WindowsApplication::GetPrimaryMonitor() const
{
    const POINT origin = {0, 0};
    HMONITOR hmonitor = ::MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    return MonitorInfoFromHMONITOR(hmonitor, 0);
}

Rndr::MonitorInfo Rndr::WindowsApplication::GetMonitorAtPosition(const Vector2i& pos) const
{
    const POINT point = {pos.x, pos.y};
    HMONITOR hmonitor = ::MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    return MonitorInfoFromHMONITOR(hmonitor, 0);
}

Rndr::MonitorInfo Rndr::WindowsApplication::GetMonitorForWindow(const GenericWindow& window) const
{
    HWND hwnd = reinterpret_cast<HWND>(window.GetNativeHandle());
    HMONITOR hmonitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    return MonitorInfoFromHMONITOR(hmonitor, 0);
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
