#pragma once

#include "rndr/core/base.h"
#include "rndr/core/memory.h"

#ifdef RNDR_IMGUI

namespace rndr
{
class Window;
struct GraphicsContext;

struct ImGuiProperties
{
    bool EnableKeyboardNavigation = false;
    bool EnableGamepadNavigation = false;
    bool DisplayDemoWindow = true;
};

class ImGuiWrapper
{
public:
    [[nodiscard]] static ScopePtr<ImGuiWrapper> Create(
        Window& Window,
        GraphicsContext& Context,
        const ImGuiProperties& Props = ImGuiProperties{});

    bool Init(Window& Window,
              GraphicsContext& Context,
              const ImGuiProperties& Props = ImGuiProperties{});
    bool ShutDown();

    void StartFrame();
    void EndFrame();

    [[nodiscard]] const ImGuiProperties& GetProps() const { return m_Props; }

private:
    Window* m_Window;
    GraphicsContext* m_Context;
    ImGuiProperties m_Props;

    bool m_FrameStarted = false;

    bool m_DemoWindowOpened = true;
};
}  // namespace rndr

#endif
