#pragma once

#include "rndr/core/base.h"

#if defined RNDR_RASTER

#include <memory>
#include <vector>

#include "rndr/core/pipeline.h"

#include "rndr/raster/rasterimage.h"

namespace rndr
{

class GraphicsContext;

/**
 * Encapsulates render targets. It supports multiple color buffers as well as depth and stencil buffers.
 */
class FrameBuffer
{
public:
    FrameBuffer(GraphicsContext* Context, int Width, int Height, const FrameBufferProperties& Props = FrameBufferProperties{});

    void SetSize(int Width, int Height);

    Image* GetColorBuffer(int Index = 0);
    Image* GetDepthBuffer();
    Image* GetStencilBuffer();

    bool IsUsingDepthStencil() const;
    int GetColorBufferCount() const;
    int GetWidth() const;
    int GetHeight() const;
    GraphicsContext* GetContext() const;

private:
    GraphicsContext* m_Context;
    int m_Width, m_Height;
    FrameBufferProperties m_Props;

    std::vector<std::unique_ptr<Image>> m_ColorBuffers;
    std::unique_ptr<Image> m_DepthBuffer;
    std::unique_ptr<Image> m_StencilBuffer;
};

}  // namespace rndr

#endif  // RNDR_RASTER
