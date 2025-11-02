#pragma once

#include "rndr/generic-window.hpp"
#include "rndr/render-api.hpp"
#include "system-message-handler.hpp"

namespace Rndr
{

struct ImGuiContextDesc
{
    bool enable_keyboard_navigation = true;
    bool enable_gamepad_navigation = false;
    Opal::StringUtf8 font_path;
    f32 font_size_in_pixels = 16;
    f32 alpha_multiplier = 1.5f;
};

class ImGuiContext : public SystemMessageHandler
{
public:
    ImGuiContext(GenericWindow& window, GraphicsContext& context, const ImGuiContextDesc& desc = {});
    virtual ~ImGuiContext();

    bool Destroy();

    void StartFrame();
    void EndFrame();

    bool OnWindowClose(GenericWindow&) override { return false; }
    void OnWindowSizeChanged(const GenericWindow& window, i32 width, i32 height) override;
    bool OnButtonDown(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnButtonUp(const GenericWindow& window, InputPrimitive key_code, bool is_repeated) override;
    bool OnCharacter(const GenericWindow& window, uchar32 character, bool is_repeated) override;
    bool OnMouseButtonDown(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseButtonUp(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseDoubleClick(const GenericWindow& window, InputPrimitive primitive, const Vector2i& cursor_position) override;
    bool OnMouseWheel(const GenericWindow& window, f32 wheel_delta, const Vector2i& cursor_position) override;
    bool OnMouseMove(const GenericWindow& window, f32 delta_x, f32 delta_y) override;

private:
    Opal::Ref<GenericWindow> m_window;
    Opal::Ref<GraphicsContext> m_context;
    ImGuiContextDesc m_desc;
    bool m_is_initialized = false;
    bool m_frame_started = false;
    bool m_is_demo_window_opened = false;
};

};  // namespace Rndr