#include "rndr/render/dx11/dx11swapchain.h"

#if RNDR_DX11

#include <dxgi.h>

#include "rndr/core/log.h"
#include "rndr/core/memory.h"

#include "rndr/render/dx11/dx11framebuffer.h"
#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"

rndr::SwapChain::~SwapChain()
{
    DX11SafeRelease(DX11SwapChain);
    RNDR_DELETE(rndr::FrameBuffer, FrameBuffer);
}

bool rndr::SwapChain::Init(GraphicsContext* Context, NativeWindowHandle Handle, int Width, int Height, const SwapChainProperties& Props)
{
    this->Props = Props;
    this->Width = Width;
    this->Height = Height;

    HWND WindowHandle = reinterpret_cast<HWND>(Handle);
    if (!IsWindow(WindowHandle))
    {
        RNDR_LOG_ERROR("SwapChain::Init: Native window handle is invalid!");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC SwapChainDesc;
    ZeroMemory(&SwapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));
    SwapChainDesc.BufferDesc.Width = Width;
    SwapChainDesc.BufferDesc.Height = Height;
    SwapChainDesc.BufferDesc.Format = DX11FromPixelFormat(Props.ColorFormat);
    SwapChainDesc.BufferDesc.RefreshRate = DXGI_RATIONAL{0, 1};  // Zero
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.Windowed = Props.bWindowed;
    SwapChainDesc.OutputWindow = WindowHandle;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;

    IDXGIDevice* DXGIDevice = nullptr;
    HRESULT Result = Context->GetDevice()->QueryInterface(__uuidof(IDXGIDevice), (void**)&DXGIDevice);
    if (Context->WindowsHasFailed(Result))
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("SwapChain::Init: %s", ErrorMessage.c_str());
        return false;
    }

    IDXGIAdapter* DXGIAdapter = nullptr;
    Result = DXGIDevice->GetAdapter(&DXGIAdapter);
    if (FAILED(Result))
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("SwapChain::Init: %s", ErrorMessage.c_str());
        return false;
    }

    IDXGIFactory* DXGIFactory = nullptr;
    Result = DXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&DXGIFactory);
    if (FAILED(Result))
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("SwapChain::Init: %s", ErrorMessage.c_str());
        return false;
    }

    Result = DXGIFactory->CreateSwapChain(Context->GetDevice(), &SwapChainDesc, &DX11SwapChain);
    if (FAILED(Result))
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("SwapChain::Init: %s", ErrorMessage.c_str());
        return false;
    }

    FrameBuffer = Context->CreateFrameBufferForSwapChain(Width, Height, this);
    if (!FrameBuffer)
    {
        RNDR_LOG_ERROR("Failed to create a framebuffer for swapchain!");
        return false;
    }

    DX11SafeRelease(DXGIFactory);
    DX11SafeRelease(DXGIAdapter);
    DX11SafeRelease(DXGIDevice);

    return true;
}

#endif  // RNDR_DX11
