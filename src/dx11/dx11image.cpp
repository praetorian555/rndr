#include "rndr/render/dx11/dx11image.h"

#if defined RNDR_DX11

#include <Windows.h>
#include <d3d11.h>

#include "stb_image/stb_image.h"

#include "rndr/core/fileutils.h"
#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/graphicscontext.h"

bool rndr::Image::Init(GraphicsContext* Context, int Width, int Height, const ImageProperties& Props, ByteSpan InitData)
{
    this->Props = Props;
    this->Width = Width;
    this->Height = Height;
    this->ArraySize = 1;

    if (Width == 0 || Height == 0)
    {
        RNDR_LOG_ERROR("Image::Init: Width and Height can't be zero!");
        return false;
    }

    ByteSpan DataArray[1] = {InitData};
    Span<ByteSpan> Data{DataArray, 1};
    return InitInternal(Context, InitData ? Data : Span<ByteSpan>{});
}

bool rndr::Image::InitArray(GraphicsContext* Context,
                            int Width,
                            int Height,
                            int ArraySize,
                            const ImageProperties& Props,
                            Span<ByteSpan> InitData)
{
    this->Props = Props;
    this->Width = Width;
    this->Height = Height;
    this->ArraySize = ArraySize;

    if (ArraySize <= 1)
    {
        RNDR_LOG_ERROR("Image::InitArray: Invalid array size - %d", ArraySize);
        return false;
    }
    if (InitData && InitData.Size != ArraySize)
    {
        RNDR_LOG_ERROR("Image::InitArray: There is init data but the size doesn't match the array size!");
        return false;
    }
    if (Width == 0 || Height == 0)
    {
        RNDR_LOG_ERROR("Image::InitArray: Width and Height can't be zero!");
        return false;
    }

    return InitInternal(Context, InitData);
}

bool rndr::Image::InitCubeMap(GraphicsContext* Context, int Width, int Height, const ImageProperties& Props, Span<ByteSpan> InitData)
{
    this->Props = Props;
    this->Width = Width;
    this->Height = Height;
    this->ArraySize = 6;

    if (InitData && InitData.Size != this->ArraySize)
    {
        RNDR_LOG_ERROR("Image::InitCubeMap: There is init data but the size doesn't match the array size!");
        return false;
    }
    if (Width == 0 || Height == 0)
    {
        RNDR_LOG_ERROR("Image::InitCubeMap: Width and Height can't be zero!");
        return false;
    }

    return InitInternal(Context, InitData, true);
}

bool rndr::Image::InitSwapchainBackBuffer(GraphicsContext* Context)
{
    // TODO: Remove
    return true;
}

bool rndr::Image::InitInternal(GraphicsContext* Context, Span<ByteSpan> InitData, bool bCubeMap)
{
    D3D11_TEXTURE2D_DESC Desc;
    ZeroMemory(&Desc, sizeof(D3D11_TEXTURE2D_DESC));
    Desc.BindFlags = DX11FromImageBindFlags(Props.ImageBindFlags);
    Desc.CPUAccessFlags = DX11FromCPUAccess(Props.CPUAccess);
    Desc.Format = DX11FromPixelFormat(Props.PixelFormat);
    Desc.Usage = DX11FromUsage(Props.Usage);
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.ArraySize = ArraySize;
    Desc.MipLevels = Props.bUseMips ? 0 : 1;
    Desc.MiscFlags = bCubeMap ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

    // TODO(mkostic): Add options for multisampling in props
    Desc.SampleDesc.Count = 1;
    Desc.SampleDesc.Quality = 0;

    const int PixelSize = GetPixelSize(Props.PixelFormat);
    const int PitchSize = Width * PixelSize;

    D3D11_SUBRESOURCE_DATA* InitialDataPtr = nullptr;
    if (InitData)
    {
        InitialDataPtr = new D3D11_SUBRESOURCE_DATA[ArraySize];
        for (int i = 0; i < ArraySize; i++)
        {
            ZeroMemory(&InitialDataPtr[i], sizeof(D3D11_SUBRESOURCE_DATA));
            InitialDataPtr[i].pSysMem = InitData[i].Data;
            InitialDataPtr[i].SysMemPitch = PitchSize;
        }
    }

    ID3D11Device* Device = Context->GetDevice();
    HRESULT Result = Device->CreateTexture2D(&Desc, InitialDataPtr, &DX11Texture);
    delete[] InitialDataPtr;
    if (FAILED(Result))
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    if (Props.ImageBindFlags & ImageBindFlags::ShaderResource)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC ResourceDesc;
        ZeroMemory(&ResourceDesc, sizeof(ResourceDesc));
        ResourceDesc.Format = DX11FromPixelFormat(Props.PixelFormat);
        if (!bCubeMap)
        {
            ResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            ResourceDesc.Texture2DArray.MipLevels = Props.bUseMips ? -1 : 1;
            ResourceDesc.Texture2DArray.ArraySize = ArraySize;
        }
        else
        {
            ResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            ResourceDesc.TextureCube.MipLevels = Props.bUseMips ? -1 : 1;
        }
        Result = Device->CreateShaderResourceView(DX11Texture, &ResourceDesc, &DX11ShaderResourceView);
        if (FAILED(Result))
        {
            std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }
    }
    if (Props.ImageBindFlags & ImageBindFlags::RenderTarget)
    {
        D3D11_RENDER_TARGET_VIEW_DESC Desc;
        ZeroMemory(&Desc, sizeof(Desc));
        Desc.Format = DX11FromPixelFormat(Props.PixelFormat);
        Desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        Result = Device->CreateRenderTargetView(DX11Texture, &Desc, &DX11RenderTargetView);
        if (FAILED(Result))
        {
            std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }
    }
    if (Props.ImageBindFlags & ImageBindFlags::DepthStencil)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC Desc;
        ZeroMemory(&Desc, sizeof(Desc));
        Desc.Format = DX11FromPixelFormat(Props.PixelFormat);
        Desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        Result = Device->CreateDepthStencilView(DX11Texture, &Desc, &DX11DepthStencilView);
        if (FAILED(Result))
        {
            std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }
    }

    return true;
}

void rndr::Image::Update(GraphicsContext* Context, int ArrayIndex, ByteSpan Contents, int BoxWidth, int BoxHeight) const
{
    D3D11_BOX* DestRegionPtr = nullptr;
    D3D11_BOX DestRegion;
    DestRegion.left = 0;
    DestRegion.right = BoxWidth;
    DestRegion.top = 0;
    DestRegion.bottom = BoxHeight;
    DestRegion.front = 0;
    DestRegion.back = 1;
    DestRegionPtr = &DestRegion;

    ID3D11DeviceContext* DeviceContext = Context->GetDeviceContext();
    // TODO(mkostic): Handle case for multiple mip maps
    const uint32_t SubresourceIndex = D3D11CalcSubresource(0, ArrayIndex, 1);
    const int PixelSize = GetPixelSize(Props.PixelFormat);
    DeviceContext->UpdateSubresource(DX11Texture, SubresourceIndex, DestRegionPtr, Contents.Data, BoxWidth * PixelSize, 0);
}

rndr::Image::~Image()
{
    DX11SafeRelease(DX11Texture);
    DX11SafeRelease(DX11ShaderResourceView);
    DX11SafeRelease(DX11RenderTargetView);
    DX11SafeRelease(DX11DepthStencilView);
}

#endif  // RNDR_DX11
