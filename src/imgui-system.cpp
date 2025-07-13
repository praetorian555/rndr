#include "rndr/imgui-system.hpp"

#if RNDR_WINDOWS
#include "backends/imgui_impl_win32.h"
#endif

#include "imgui.h"

#if RNDR_OPENGL
#include "backends/imgui_impl_opengl3.h"
#endif

#include "opal/container/hash-map.h"
#include "opal/container/scope-ptr.h"

#include "rndr/log.hpp"
#include "rndr/trace.hpp"

namespace
{
Opal::HashMap<Rndr::InputPrimitive, ImGuiKey> g_primitive_to_imgui_key{
    {Rndr::InputPrimitive::Backspace, ImGuiKey_Backspace},
    {Rndr::InputPrimitive::Tab, ImGuiKey_Tab},
    {Rndr::InputPrimitive::Return, ImGuiKey_Enter},
    {Rndr::InputPrimitive::Shift, ImGuiKey_LeftShift},
    {Rndr::InputPrimitive::Control, ImGuiKey_LeftCtrl},
    {Rndr::InputPrimitive::Alt, ImGuiKey_LeftAlt},
    {Rndr::InputPrimitive::Pause, ImGuiKey_Pause},
    {Rndr::InputPrimitive::CapsLock, ImGuiKey_CapsLock},
    {Rndr::InputPrimitive::Escape, ImGuiKey_Escape},
    {Rndr::InputPrimitive::Space, ImGuiKey_Space},
    {Rndr::InputPrimitive::PageUp, ImGuiKey_PageUp},
    {Rndr::InputPrimitive::PageDown, ImGuiKey_PageDown},
    {Rndr::InputPrimitive::End, ImGuiKey_End},
    {Rndr::InputPrimitive::Home, ImGuiKey_Home},
    {Rndr::InputPrimitive::LeftArrow, ImGuiKey_LeftArrow},
    {Rndr::InputPrimitive::UpArrow, ImGuiKey_UpArrow},
    {Rndr::InputPrimitive::RightArrow, ImGuiKey_RightArrow},
    {Rndr::InputPrimitive::DownArrow, ImGuiKey_DownArrow},
    {Rndr::InputPrimitive::Insert, ImGuiKey_Insert},
    {Rndr::InputPrimitive::Delete, ImGuiKey_Delete},
    {Rndr::InputPrimitive::Digit_0, ImGuiKey_0},
    {Rndr::InputPrimitive::Digit_1, ImGuiKey_1},
    {Rndr::InputPrimitive::Digit_2, ImGuiKey_2},
    {Rndr::InputPrimitive::Digit_3, ImGuiKey_3},
    {Rndr::InputPrimitive::Digit_4, ImGuiKey_4},
    {Rndr::InputPrimitive::Digit_5, ImGuiKey_5},
    {Rndr::InputPrimitive::Digit_6, ImGuiKey_6},
    {Rndr::InputPrimitive::Digit_7, ImGuiKey_7},
    {Rndr::InputPrimitive::Digit_8, ImGuiKey_8},
    {Rndr::InputPrimitive::Digit_9, ImGuiKey_9},
    {Rndr::InputPrimitive::A, ImGuiKey_A},
    {Rndr::InputPrimitive::B, ImGuiKey_B},
    {Rndr::InputPrimitive::C, ImGuiKey_C},
    {Rndr::InputPrimitive::D, ImGuiKey_D},
    {Rndr::InputPrimitive::E, ImGuiKey_E},
    {Rndr::InputPrimitive::F, ImGuiKey_F},
    {Rndr::InputPrimitive::G, ImGuiKey_G},
    {Rndr::InputPrimitive::H, ImGuiKey_H},
    {Rndr::InputPrimitive::I, ImGuiKey_I},
    {Rndr::InputPrimitive::J, ImGuiKey_J},
    {Rndr::InputPrimitive::K, ImGuiKey_K},
    {Rndr::InputPrimitive::L, ImGuiKey_L},
    {Rndr::InputPrimitive::M, ImGuiKey_M},
    {Rndr::InputPrimitive::N, ImGuiKey_N},
    {Rndr::InputPrimitive::O, ImGuiKey_O},
    {Rndr::InputPrimitive::P, ImGuiKey_P},
    {Rndr::InputPrimitive::Q, ImGuiKey_Q},
    {Rndr::InputPrimitive::R, ImGuiKey_R},
    {Rndr::InputPrimitive::S, ImGuiKey_S},
    {Rndr::InputPrimitive::T, ImGuiKey_T},
    {Rndr::InputPrimitive::U, ImGuiKey_U},
    {Rndr::InputPrimitive::V, ImGuiKey_V},
    {Rndr::InputPrimitive::W, ImGuiKey_W},
    {Rndr::InputPrimitive::X, ImGuiKey_X},
    {Rndr::InputPrimitive::Y, ImGuiKey_Y},
    {Rndr::InputPrimitive::Z, ImGuiKey_Z},
    {Rndr::InputPrimitive::Numpad_0, ImGuiKey_Keypad0},
    {Rndr::InputPrimitive::Numpad_1, ImGuiKey_Keypad1},
    {Rndr::InputPrimitive::Numpad_2, ImGuiKey_Keypad2},
    {Rndr::InputPrimitive::Numpad_3, ImGuiKey_Keypad3},
    {Rndr::InputPrimitive::Numpad_4, ImGuiKey_Keypad4},
    {Rndr::InputPrimitive::Numpad_5, ImGuiKey_Keypad5},
    {Rndr::InputPrimitive::Numpad_6, ImGuiKey_Keypad6},
    {Rndr::InputPrimitive::Numpad_7, ImGuiKey_Keypad7},
    {Rndr::InputPrimitive::Numpad_8, ImGuiKey_Keypad8},
    {Rndr::InputPrimitive::Numpad_9, ImGuiKey_Keypad9},
    {Rndr::InputPrimitive::Multiply, ImGuiKey_KeypadMultiply},
    {Rndr::InputPrimitive::Add, ImGuiKey_KeypadAdd},
    {Rndr::InputPrimitive::Subtract, ImGuiKey_KeypadSubtract},
    {Rndr::InputPrimitive::Decimal, ImGuiKey_KeypadDecimal},
    {Rndr::InputPrimitive::Divide, ImGuiKey_KeypadDivide},
    {Rndr::InputPrimitive::F1, ImGuiKey_F1},
    {Rndr::InputPrimitive::F2, ImGuiKey_F2},
    {Rndr::InputPrimitive::F3, ImGuiKey_F3},
    {Rndr::InputPrimitive::F4, ImGuiKey_F4},
    {Rndr::InputPrimitive::F5, ImGuiKey_F5},
    {Rndr::InputPrimitive::F6, ImGuiKey_F6},
    {Rndr::InputPrimitive::F7, ImGuiKey_F7},
    {Rndr::InputPrimitive::F8, ImGuiKey_F8},
    {Rndr::InputPrimitive::F9, ImGuiKey_F9},
    {Rndr::InputPrimitive::F10, ImGuiKey_F10},
    {Rndr::InputPrimitive::F11, ImGuiKey_F11},
    {Rndr::InputPrimitive::F12, ImGuiKey_F12},
    {Rndr::InputPrimitive::NumLock, ImGuiKey_NumLock},
    {Rndr::InputPrimitive::ScrollLock, ImGuiKey_ScrollLock},
    {Rndr::InputPrimitive::LeftShift, ImGuiKey_LeftShift},
    {Rndr::InputPrimitive::RightShift, ImGuiKey_RightShift},
    {Rndr::InputPrimitive::LeftCtrl, ImGuiKey_LeftCtrl},
    {Rndr::InputPrimitive::RightCtrl, ImGuiKey_RightCtrl},
    {Rndr::InputPrimitive::LeftAlt, ImGuiKey_LeftAlt},
    {Rndr::InputPrimitive::RightAlt, ImGuiKey_RightAlt},
    {Rndr::InputPrimitive::Semicolon, ImGuiKey_Semicolon},
    {Rndr::InputPrimitive::Plus, ImGuiKey_Equal},
    {Rndr::InputPrimitive::Comma, ImGuiKey_Comma},
    {Rndr::InputPrimitive::Minus, ImGuiKey_Minus},
    {Rndr::InputPrimitive::Period, ImGuiKey_Period},
    {Rndr::InputPrimitive::Slash, ImGuiKey_Slash},
    {Rndr::InputPrimitive::Tilde, ImGuiKey_GraveAccent},
    {Rndr::InputPrimitive::OpenBracket, ImGuiKey_LeftBracket},
    {Rndr::InputPrimitive::CloseBracket, ImGuiKey_RightBracket},
    {Rndr::InputPrimitive::Backslash, ImGuiKey_Backslash},
    {Rndr::InputPrimitive::Apostrophe, ImGuiKey_Apostrophe},
};

Opal::HashMap<Rndr::InputPrimitive, Rndr::i32> g_mouse_primitive_to_imgui_key{{Rndr::InputPrimitive::Mouse_LeftButton, 0},
                                                                              {Rndr::InputPrimitive::Mouse_RightButton, 1},
                                                                              {Rndr::InputPrimitive::Mouse_MiddleButton, 2},
                                                                              {Rndr::InputPrimitive::Mouse_XButton1, 3},
                                                                              {Rndr::InputPrimitive::Mouse_XButton2, 4}

};
}  // namespace

Rndr::ImGuiContext::ImGuiContext(GenericWindow& window, GraphicsContext& context, const ImGuiContextDesc& desc)
    : m_window(window), m_context(context), m_desc(desc)
{

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= m_desc.enable_keyboard_navigation ? ImGuiConfigFlags_NavEnableKeyboard : ImGuiConfigFlags_None;
    io.ConfigFlags |= m_desc.enable_gamepad_navigation ? ImGuiConfigFlags_NavEnableGamepad : ImGuiConfigFlags_None;

    i32 x, y, w, h;
    window.GetPositionAndSize(x, y, w, h);
    io.DisplaySize = ImVec2(static_cast<f32>(w), static_cast<f32>(h));

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

#if RNDR_WINDOWS
    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_window->GetNativeHandle());
#else
    RNDR_ASSERT(false, "Platform not supported!");
#endif

#if RNDR_OPENGL
    ImGui_ImplOpenGL3_Init("#version 330");
#else
    RNDR_ASSERT(false, "Graphics API not supported!");
#endif
}

Rndr::ImGuiContext::~ImGuiContext()
{
    Destroy();
}

bool Rndr::ImGuiContext::Destroy()
{
#if RNDR_OPENGL
    ImGui_ImplOpenGL3_Shutdown();
#else
    RNDR_ASSERT(false, "Platform not supported!");
#endif

#if RNDR_WINDOWS
    ImGui_ImplWin32_Shutdown();
#else
    RNDR_ASSERT(false, "Platform not supported!");
#endif

    ImGui::DestroyContext();

    m_window = nullptr;
    m_context = nullptr;

    return true;
}

void Rndr::ImGuiContext::StartFrame()
{
    if (m_frame_started)
    {
        RNDR_LOG_WARNING("Frame already started!");
        return;
    }

    RNDR_GPU_EVENT_BEGIN("ImGui Context");

#if RNDR_WINDOWS
    ImGui_ImplWin32_NewFrame();
#else
    RNDR_ASSERT(false, "Platform not supported!");
#endif

#if RNDR_OPENGL
    ImGui_ImplOpenGL3_NewFrame();
#else
    RNDR_ASSERT(false, "Platform not supported!");
#endif

    ImGui::NewFrame();

    m_frame_started = true;
}

void Rndr::ImGuiContext::EndFrame()
{
    if (!m_frame_started)
    {
        RNDR_LOG_WARNING("Frame not started!");
        return;
    }

    ImGui::Render();

#if RNDR_OPENGL
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#else
    RNDR_ASSERT(false, "Platform not supported!");
#endif

    RNDR_GPU_EVENT_END("ImGui System");

    m_frame_started = false;
}

void Rndr::ImGuiContext::OnWindowSizeChanged(const GenericWindow&, i32, i32) {}

bool Rndr::ImGuiContext::OnButtonDown(const GenericWindow& window, InputPrimitive primitive, bool)
{
    if (m_window.GetPtr() != &window)
    {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (g_primitive_to_imgui_key.contains(primitive))
    {
        io.AddKeyEvent(g_primitive_to_imgui_key[primitive], true);
    }
    return true;
}

bool Rndr::ImGuiContext::OnButtonUp(const GenericWindow& window, InputPrimitive primitive, bool)
{
    if (m_window.GetPtr() != &window)
    {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    if (g_primitive_to_imgui_key.contains(primitive))
    {
        io.AddKeyEvent(g_primitive_to_imgui_key[primitive], false);
    }
    return true;
}

bool Rndr::ImGuiContext::OnCharacter(const GenericWindow& window, uchar32 character, bool)
{
    if (m_window.GetPtr() != &window)
    {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacter(character);
    return true;
}

bool Rndr::ImGuiContext::OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i&)
{
    if (m_window.GetPtr() != &window)
    {
        return false;
    }
    if (g_mouse_primitive_to_imgui_key.contains(primitive))
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseButtonEvent(g_mouse_primitive_to_imgui_key[primitive], true);
    }
    return true;
}

bool Rndr::ImGuiContext::OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i&)
{
    if (m_window.GetPtr() != &window)
    {
        return false;
    }

    if (g_mouse_primitive_to_imgui_key.contains(primitive))
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseButtonEvent(g_mouse_primitive_to_imgui_key[primitive], false);
    }
    return true;
}

bool Rndr::ImGuiContext::OnMouseDoubleClick(const GenericWindow&, InputPrimitive, const Vector2i&)
{
    return true;
}

bool Rndr::ImGuiContext::OnMouseWheel(const GenericWindow& window, f32 delta, const Vector2i&)
{
    if (m_window.GetPtr() != &window)
    {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent(0.0f, delta);
    return true;
}

bool Rndr::ImGuiContext::OnMouseMove(const GenericWindow&, f32, f32)
{
    return true;
}
