#include "rndr/dx11/dx11graphicscontext.h"

#if defined RNDR_DX11

#include <Windows.h>

#include "rndr/core/framebuffer.h"
#include "rndr/core/window.h"

#include "rndr/dx11/dx11helpers.h"

rndr::GraphicsContext::GraphicsContext(Window* Window, GraphicsContextProperties Props) : m_Window(Window), m_Props(Props)
{

    HWND WindowHandle = reinterpret_cast<HWND>(m_Window->GetNativeWindowHandle());

    DXGI_SWAP_CHAIN_DESC SwapChainDesc;
    ZeroMemory(&SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
    SwapChainDesc.BufferCount = 1;
    SwapChainDesc.BufferDesc.Width = m_Window->GetWidth();
    SwapChainDesc.BufferDesc.Height = m_Window->GetHeight();
    SwapChainDesc.BufferDesc.Format = FromPixelFormat(m_Props.FrameBuffer.ColorBufferProperties[0].PixelFormat);
    SwapChainDesc.BufferDesc.RefreshRate = DXGI_RATIONAL{0, 1};  // TODO(mkostic): Figure this out
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.OutputWindow = WindowHandle;
    // If you want no multisamling use count=1 and quality=0
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.Windowed = TRUE;
    UINT CreateDeviceFlags = 0;
#if _DEBUG
    CreateDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif
    // These are the feature levels that we will accept.
    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
                                         D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1};
    // This will be the feature level that is used to create our device and swap chain.
    D3D_FEATURE_LEVEL FeatureLevel;
    IDXGIAdapter* Adapter = nullptr;  // Use default adapter
    HMODULE SoftwareRasterizerModule = nullptr;
    HRESULT Result = D3D11CreateDeviceAndSwapChain(Adapter, D3D_DRIVER_TYPE_HARDWARE, SoftwareRasterizerModule, CreateDeviceFlags,
                                                   FeatureLevels, _countof(FeatureLevels), D3D11_SDK_VERSION, &SwapChainDesc, &m_Swapchain,
                                                   &m_Device, &FeatureLevel, &m_DeviceContext);

    m_WindowFrameBuffer = std::make_unique<FrameBuffer>(this, m_Window->GetWidth(), m_Window->GetHeight(), m_Props.FrameBuffer);

    WindowDelegates::OnResize.Add(RNDR_BIND_THREE_PARAM(this, &GraphicsContext::WindowResize));
}

rndr::GraphicsContext::~GraphicsContext()
{
    DX11SafeRelease(m_Swapchain);
    DX11SafeRelease(m_Device);
    DX11SafeRelease(m_DeviceContext);
}

void rndr::GraphicsContext::WindowResize(Window* Window, int Width, int Height)
{
    if (Window != m_Window)
    {
        return;
    }

    m_WindowFrameBuffer->SetSize(Width, Height);
}

ID3D11Device* rndr::GraphicsContext::GetDevice()
{
    return m_Device;
}

ID3D11DeviceContext* rndr::GraphicsContext::GetDeviceContext()
{
    return m_DeviceContext;
}

IDXGISwapChain* rndr::GraphicsContext::GetSwapchain()
{
    return m_Swapchain;
}

rndr::FrameBuffer* rndr::GraphicsContext::GetWindowFrameBuffer()
{
    return m_WindowFrameBuffer.get();
}

#endif  // RNDR_DX11
