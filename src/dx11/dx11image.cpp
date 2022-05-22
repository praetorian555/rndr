#include "rndr/dx11/dx11image.h"

#if defined RNDR_DX11

#include <Windows.h>
#include <d3d11.h>

#include "stb_image/stb_image.h"

#include "rndr/core/fileutils.h"
#include "rndr/core/graphicscontext.h"
#include "rndr/core/log.h"

#include "rndr/dx11/dx11helpers.h"

rndr::Image::Image(GraphicsContext* Context, int Width, int Height, const ImageProperties& Props)
    : Width(Width), Height(Height), Props(Props), bSwapchainBackBuffer(false)
{
    Create(Context);
}

rndr::Image::Image(GraphicsContext* Context) : bSwapchainBackBuffer(true)
{
    IDXGISwapChain* Swapchain = Context->GetSwapchain();
    DXGI_SWAP_CHAIN_DESC Desc;
    Swapchain->GetDesc(&Desc);
    Width = Desc.BufferDesc.Width;
    Height = Desc.BufferDesc.Height;
    Props.PixelFormat = DX11ToPixelFormat(Desc.BufferDesc.Format);
    Props.ImageBindFlags = ImageBindFlags::RenderTarget;
    Props.CPUAccess = CPUAccess::None;
    Props.Usage = Usage::GPUReadWrite;
    Props.bUseMips = false;

    Create(Context);
}

rndr::Image::Image(GraphicsContext* Context, const std::string& FilePath, const ImageProperties& P) : Props(P), bSwapchainBackBuffer(false)
{
    const ImageFileFormat FileFormat = rndr::GetImageFileFormat(FilePath);
    assert(FileFormat != ImageFileFormat::NotSupported);

    // stb_image library loads data starting from the top left-most pixel while our engine expects
    // data from lower left corner first
    const bool bShouldFlip = true;
    stbi_set_flip_vertically_on_load(bShouldFlip);

    // Note that this one will always return data in a form:
    // 1 channel: Gray
    // 2 channel: Gray Alpha
    // 3 channel: Red Green Blue
    // 4 channel: Red Green Blue Alpha
    //
    // Note that we always request 4 channels.
    int ChannelNumber;
    const int DesiredChannelNumber = 4;
    ByteSpan Data;

    Data.Data = stbi_load(FilePath.c_str(), &Width, &Height, &ChannelNumber, DesiredChannelNumber);
    if (!Data.Data)
    {
        RNDR_LOG_ERROR_OR_ASSERT("Image: stbi_load_from_file failed with error: %s", stbi_failure_reason());
        assert(Data);
    }

    // TODO(mkostic): How to know if image uses 16 or 8 bits per channel??

    // TODO(mkostic): Add support for grayscale images.
    assert(ChannelNumber == 3 || ChannelNumber == 4);

    const int PixelSize = GetPixelSize(Props.PixelFormat);
    const int PixelCount = Width * Height;
    const int BufferSize = Width * Height * PixelSize;
    Data.Size = BufferSize;

    Create(Context, Data);

    free(Data.Data);
}

void rndr::Image::Create(GraphicsContext* Context, ByteSpan Data)
{
    ID3D11Device* Device = Context->GetDevice();
    HRESULT Result;

    if (!bSwapchainBackBuffer)
    {
        D3D11_TEXTURE2D_DESC Desc;
        ZeroMemory(&Desc, sizeof(D3D11_TEXTURE2D_DESC));
        Desc.BindFlags = DX11FromImageBindFlags(Props.ImageBindFlags);
        Desc.CPUAccessFlags = DX11FromCPUAccess(Props.CPUAccess);
        Desc.Format = DX11FromPixelFormat(Props.PixelFormat);
        Desc.Usage = DX11FromUsage(Props.Usage);
        Desc.Width = Width;
        Desc.Height = Height;
        Desc.ArraySize = Props.ArraySize;
        Desc.MiscFlags = 0;
        Desc.MipLevels = 1;
        Desc.SampleDesc.Count = 1;  // TODO(mkostic): Add options for multisampling in props
        Desc.SampleDesc.Quality = 0;

        const int PixelSize = GetPixelSize(Props.PixelFormat);
        const int PitchSize = Width * PixelSize;
        D3D11_SUBRESOURCE_DATA InitialData;
        InitialData.pSysMem = Data.Data;
        InitialData.SysMemPitch = PitchSize;
        InitialData.SysMemSlicePitch = 0;
        D3D11_SUBRESOURCE_DATA* InitialDataPtr = Data ? &InitialData : nullptr;

        Result = Device->CreateTexture2D(&Desc, InitialDataPtr, &DX11Texture);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR_OR_ASSERT("Failed to create ID3D11Texture2D!");
            return;
        }
    }
    else
    {
        IDXGISwapChain* Swapchain = Context->GetSwapchain();
        HRESULT Result = Swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&DX11Texture);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR_OR_ASSERT("Failed to get back buffer from the swapchain!");
            return;
        }
    }

    if (Props.ImageBindFlags & ImageBindFlags::ShaderResource)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC ResourceDesc;
        ResourceDesc.Format = DX11FromPixelFormat(Props.PixelFormat);
        ResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        ResourceDesc.Texture2D.MipLevels = Props.bUseMips ? -1 : 1;
        ResourceDesc.Texture2D.MostDetailedMip = 0;
        Result = Device->CreateShaderResourceView(DX11Texture, &ResourceDesc, &DX11ShaderResourceView);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR_OR_ASSERT("Failed to create ID3D11ShaderResourceView!");
        }
    }
    if (Props.ImageBindFlags & ImageBindFlags::RenderTarget)
    {
        Result = Device->CreateRenderTargetView(DX11Texture, nullptr, &DX11RenderTargetView);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR_OR_ASSERT("Failed to create ID3D11RenderTargetView!");
        }
    }
    if (Props.ImageBindFlags & ImageBindFlags::DepthStencil)
    {
        Result = Device->CreateDepthStencilView(DX11Texture, nullptr, &DX11DepthStencilView);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR_OR_ASSERT("Failed to create ID3D11DepthStencilView!");
        }
    }
}

rndr::Image::~Image()
{
    DX11SafeRelease(DX11Texture);
    DX11SafeRelease(DX11ShaderResourceView);
    DX11SafeRelease(DX11RenderTargetView);
    DX11SafeRelease(DX11DepthStencilView);
}

#endif  // RNDR_DX11
