#pragma once

#include "rndr/core/base.h"
#include "rndr/core/math.h"

#if defined RNDR_RASTER

#include "rndr/core/pipeline.h"

#include "rndr/raster/rasterizer.h"
#include "rndr/raster/rasterframebuffer.h"

namespace rndr
{

class Window;
class Model;

/**
 * Encapuslates the graphics API. Unique per window. Use this to render stuff and present to the window.
 */
class GraphicsContext
{
public:
    GraphicsContext(Window* Window, const GraphicsContextProperties& Props = GraphicsContextProperties{});

    FrameBuffer* GetWindowFrameBuffer();

    // Clear the buffer of a specified framebuffer with a desired value. If null is used this will be applied to the window framebuffer.
    void ClearColor(FrameBuffer* FrameBuffer, const Vector4r& Color, int Index = 0);
    void ClearDepth(FrameBuffer* FrameBuffer, real Depth);
    void ClearStencil(FrameBuffer* FrameBuffer, uint8_t Value);

    void Render(Model* Model);

    void Present(bool bVerticalSync);

private:
    void WindowResize(Window* Window, int Width, int Height);

private:
    GraphicsContextProperties m_Props;
    Window* m_Window;
    std::unique_ptr<FrameBuffer> m_WindowFrameBuffer;

    std::unique_ptr<Rasterizer> m_Rasterizer;
};

}  // namespace rndr

#endif  // RNDR_RASTER