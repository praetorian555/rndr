#include "rndr/utility/imguiwrapper.h"

#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui.h"

#include "rndr/core/window.h"

#include "rndr/render/graphicscontext.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

bool rndr::ImGuiWrapper::Init(Window& Window, GraphicsContext& Context)
{
    m_Window = &Window;
    m_Context = &Context;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& IO = ImGui::GetIO();
    RNDR_UNUSED(IO);
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    Window::NativeWindowEventDelegate Delegate;
    Delegate.Set(
        [](NativeWindowHandle Handle, uint32_t MsgCode, uint64_t ParamW, int64_t ParamL)
        {
            return ImGui_ImplWin32_WndProcHandler(reinterpret_cast<HWND>(Handle), MsgCode, ParamW,
                                                  ParamL);
        });
    Window.SetNativeWindowEventDelegate(Delegate);

    // Setup Platform/Renderer backends
    NativeWindowHandle WindowHandle = Window.GetNativeWindowHandle();
    ImGui_ImplWin32_Init(reinterpret_cast<void*>(WindowHandle));
    ImGui_ImplDX11_Init(Context.DX11Device, Context.DX11DeviceContext);

    return true;
}

bool rndr::ImGuiWrapper::ShutDown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    return true;
}

void rndr::ImGuiWrapper::StartFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    static bool ShowWindow = true;
    if (ShowWindow)
    {
        ImGui::ShowDemoWindow(&ShowWindow);
    }
}

void rndr::ImGuiWrapper::Render()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
