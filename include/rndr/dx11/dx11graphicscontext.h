#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <d3d11.h>
#include <d3dcompiler.h>

#include "rndr/core/graphicstypes.h"

namespace rndr
{

class Window;
class FrameBuffer;

class GraphicsContext
{
public:
    GraphicsContext(Window* Window, GraphicsContextProperties Props = GraphicsContextProperties{});
    ~GraphicsContext();

    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetDeviceContext();
    IDXGISwapChain* GetSwapchain();
    D3D_FEATURE_LEVEL GetFeatureLevel();

    FrameBuffer* GetWindowFrameBuffer();

private:
    void WindowResize(Window* Window, int Width, int Height);

private:
    Window* m_Window;
    GraphicsContextProperties m_Props;

    ID3D11Device* m_Device;
    ID3D11DeviceContext* m_DeviceContext;
    IDXGISwapChain* m_Swapchain;
    D3D_FEATURE_LEVEL m_FeatureLevel;

    std::unique_ptr<FrameBuffer> m_WindowFrameBuffer;
};

}  // namespace rndr

#endif  // RNDR_DX11
