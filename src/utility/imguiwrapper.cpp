#include "rndr/utility/imguiwrapper.h"

#ifdef RNDR_IMGUI

#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui.h"

#include "rndr/core/log.h"
#include "rndr/core/window.h"

#include "rndr/render/graphicscontext.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND Handle,
                                                             UINT MsgCode,
                                                             WPARAM ParamW,
                                                             LPARAM ParamL);

rndr::ScopePtr<rndr::ImGuiWrapper> rndr::ImGuiWrapper::Create(Window& Window,
                                                              GraphicsContext& Context,
                                                              const ImGuiProperties& Props)
{
    ImGuiWrapper* Wrapper = New<ImGuiWrapper>("ImGuiWrapper");
    if (Wrapper != nullptr && Wrapper->Init(Window, Context, Props))
    {
        return ScopePtr{Wrapper};
    }

    return {};
}

bool rndr::ImGuiWrapper::Init(Window& Window,
                              GraphicsContext& Context,
                              const ImGuiProperties& Props)
{
    m_Window = &Window;
    m_Context = &Context;
    m_Props = Props;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= m_Props.EnableKeyboardNavigation ? ImGuiConfigFlags_NavEnableKeyboard
                                                       : ImGuiConfigFlags_None;
    IO.ConfigFlags |=
        m_Props.EnableGamepadNavigation ? ImGuiConfigFlags_NavEnableGamepad : ImGuiConfigFlags_None;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    Window::NativeWindowEventDelegate Delegate;
    Delegate.Set(
        [](NativeWindowHandle Handle, uint32_t MsgCode, uint64_t ParamW, int64_t ParamL)
        {
            return ImGui_ImplWin32_WndProcHandler(reinterpret_cast<HWND>(Handle), MsgCode, ParamW,
                                                  ParamL);
        });
    Window.SetNativeWindowEventDelegate(Delegate);

    // Setup Platform/Renderer backends
    const NativeWindowHandle WindowHandle = Window.GetNativeWindowHandle();  // NOLINT
    ImGui_ImplWin32_Init(reinterpret_cast<void*>(WindowHandle));
    ImGui_ImplDX11_Init(Context.DX11Device, Context.DX11DeviceContext);

    return true;
}

bool rndr::ImGuiWrapper::ShutDown()  // NOLINT
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    return true;
}

void rndr::ImGuiWrapper::StartFrame()
{
    if (m_FrameStarted)
    {
        RNDR_LOG_ERROR("ImGuiWrapper::StartFrame: StartFrame already called!");
        return;
    }
    m_FrameStarted = true;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (m_Props.DisplayDemoWindow && m_DemoWindowOpened)
    {
        ImGui::ShowDemoWindow(&m_DemoWindowOpened);
    }
}

void rndr::ImGuiWrapper::EndFrame()
{
    if (!m_FrameStarted)
    {
        RNDR_LOG_ERROR("ImGuiWrapper::EndFrame: StartFrame was never called!");
        return;
    }
    m_FrameStarted = false;

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

#endif  // RNDR_IMGUI
