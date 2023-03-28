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
    bool enable_keyboard_navigation = false;
    bool enable_gamepad_navigation = false;
    bool display_demo_window = true;
};

class ImGuiWrapper
{
public:
    [[nodiscard]] static ScopePtr<ImGuiWrapper> Create(
        Window& window,
        GraphicsContext& context,
        const ImGuiProperties& props = ImGuiProperties{});

    bool Init(Window& window,
              GraphicsContext& context,
              const ImGuiProperties& props = ImGuiProperties{});
    bool ShutDown();

    void StartFrame();
    void EndFrame();

    [[nodiscard]] const ImGuiProperties& GetProps() const { return m_props; }

private:
    Window* m_window;
    GraphicsContext* m_context;
    ImGuiProperties m_props;

    bool m_frame_started = false;

    bool m_demo_window_opened = true;
};
}  // namespace rndr

#endif
