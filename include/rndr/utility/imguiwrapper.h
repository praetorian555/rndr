#pragma once

#include "rndr/core/base.h"

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
    RNDR_DEFAULT_BODY(ImGuiWrapper);

    // TODO(Marko): Make this part of the RNDR_DEFAULT_BODY macro.
    ~ImGuiWrapper() = default;

public:
    /**
     * Initializes ImGui backend.
     * @param window Window to attach to.
     * @param context Graphics context to attach to.
     * @param props ImGui properties.
     * @return True if initialization was successful.
     */
    static bool Init(Window& window,
                     GraphicsContext& context,
                     const ImGuiProperties& props = ImGuiProperties{});

    /**
     * Shuts down ImGui backend.
     * @return True if shutdown was successful.
     */
    static bool ShutDown();

    /**
     * Starts a new ImGui frame.
     */
    static void StartFrame();

    /**
     * Ends the current ImGui frame.
     */
    static void EndFrame();

    /**
     * Returns the ImGui properties.
     * @return ImGui properties.
     */
    [[nodiscard]] static const ImGuiProperties& GetProps();

private:
    static ImGuiWrapper& Get();

    Window* m_window = nullptr;
    GraphicsContext* m_context = nullptr;
    ImGuiProperties m_props;

    bool m_frame_started = false;
    bool m_demo_window_opened = true;
};
}  // namespace rndr

#endif
