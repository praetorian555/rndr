#pragma once

#include "rndr/core/base.h"

#ifdef RNDR_IMGUI

namespace rndr
{
class ImGuiWrapper
{
public:
    bool Init(class Window& Window, class GraphicsContext& Context);
    bool ShutDown();

    void StartFrame();

    void Render();

private:
    class Window* m_Window;
    class GraphicsContext* m_Context;
};
}  // namespace rndr

#endif
