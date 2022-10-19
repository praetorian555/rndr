#include "rndr/render/dx11/dx11swapchain.h"

#if RNDR_DX11

#include <dxgi.h>

#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/graphicscontext.h"

bool rndr::SwapChain::Init(GraphicsContext* Context, void* NativeWindowHandle, const SwapChainProperties& P)
{
    Props = P;

    HWND WindowHandle = reinterpret_cast<HWND>(NativeWindowHandle);
    if (!IsWindow(WindowHandle))
    {
        RNDR_LOG_ERROR("Native window handle is invalid!");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC SwapChainDesc;
    ZeroMemory(&SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
    SwapChainDesc.BufferDesc.Width = Props.Width;
    SwapChainDesc.BufferDesc.Height = Props.Height;
    SwapChainDesc.BufferDesc.Format = DX11FromPixelFormat(Props.FrameBuffer.ColorBufferProperties[0].PixelFormat);
    SwapChainDesc.BufferDesc.RefreshRate = DXGI_RATIONAL{0, 1};  // Zero
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    SwapChainDesc.Windowed = Props.bWindowed;
    SwapChainDesc.OutputWindow = WindowHandle;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;

    IDXGIDevice* DXGIDevice = nullptr;
    HRESULT Result = Context->GetDevice()->QueryInterface(__uuidof(IDXGIDevice), (void**)&DXGIDevice);
    if (FAILED(Result))
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    IDXGIAdapter* DXGIAdapter = nullptr;
    Result = DXGIDevice->GetAdapter(&DXGIAdapter);
    if (FAILED(Result))
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    IDXGIFactory* IDXGIFactory = nullptr;
    Result = DXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&IDXGIFactory);
    if (FAILED(Result))
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    Result = IDXGIFactory->CreateSwapChain(Context->GetDevice(), &SwapChainDesc, &DX11SwapChain);
    if (FAILED(Result))
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    // FrameBuffer = Context->CreateFrameBufferForSwapChain(this, Props.Width, Props.Height, Props.FrameBuffer);
    // if (!FrameBuffer)
    //{
    //     RNDR_LOG_ERROR("Failed to create a framebuffer for swapchain!");
    //     return false;
    // }

    return true;
}

#endif  // RNDR_DX11
