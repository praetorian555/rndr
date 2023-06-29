#include "rndr/render/dx11/dx11image.h"

#if defined RNDR_DX11

#include <Windows.h>
#include <d3d11.h>

#include "stb_image/stb_image.h"

#include "math/bounds2.h"
#include "math/point2.h"
#include "math/vector2.h"

#include "rndr/core/file.h"
#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11swapchain.h"

bool rndr::Image::Init(GraphicsContext* Context,
                       int InWidth,
                       int InHeight,
                       const ImageProperties& InProps,
                       ByteSpan InitData)
{
    Props = InProps;
    Width = InWidth;
    Height = InHeight;
    ArraySize = 1;

    if (Width == 0 || Height == 0)
    {
        RNDR_LOG_ERROR("Image::Init: Width and Height can't be zero!");
        return false;
    }

    StackArray<ByteSpan, 1> DataArray = {InitData};
    const Span<ByteSpan> Data{DataArray.data(), 1};
    return InitInternal(Context, InitData ? Data : Span<ByteSpan>{});
}

bool rndr::Image::Init(GraphicsContext* Context,
                       int InWidth,
                       int InHeight,
                       int InArraySize,
                       const ImageProperties& InProps,
                       Span<ByteSpan> InitData)
{
    this->Props = InProps;
    this->Width = InWidth;
    this->Height = InHeight;
    this->ArraySize = InArraySize;

    if (ArraySize <= 1)
    {
        RNDR_LOG_ERROR("Image::InitArray: Invalid array size - %d", ArraySize);
        return false;
    }
    if (InitData && InitData.Size != ArraySize)
    {
        RNDR_LOG_ERROR(
            "Image::InitArray: There is init data but the size doesn't match the array size!");
        return false;
    }
    if (Width == 0 || Height == 0)
    {
        RNDR_LOG_ERROR("Image::InitArray: Width and Height can't be zero!");
        return false;
    }

    return InitInternal(Context, InitData);
}

bool rndr::Image::Init(GraphicsContext* Context,
                       int InWidth,
                       int InHeight,
                       const ImageProperties& InProps,
                       Span<ByteSpan> InitData)
{
    constexpr int kCubeImageCount = 6;

    Props = InProps;
    Width = InWidth;
    Height = InHeight;
    ArraySize = kCubeImageCount;

    if (InitData && InitData.Size != this->ArraySize)
    {
        RNDR_LOG_ERROR(
            "Image::InitCubeMap: There is init data but the size doesn't match the array size!");
        return false;
    }
    if (Width == 0 || Height == 0)
    {
        RNDR_LOG_ERROR("Image::InitCubeMap: Width and Height can't be zero!");
        return false;
    }

    return InitInternal(Context, InitData, true);
}

bool rndr::Image::Init(GraphicsContext* Context, rndr::SwapChain* SwapChain, int BufferIndex)
{
    this->Props.UseMips = false;
    this->Props.SampleCount = 1;
    this->Props.ImageBindFlags = ImageBindFlags::RenderTarget;
    this->Props.Usage = Usage::Default;
    this->Props.PixelFormat = SwapChain->Props.ColorFormat;
    this->Width = SwapChain->Width;
    this->Height = SwapChain->Height;
    this->ArraySize = 1;
    this->BackBufferIndex = BufferIndex;

    return InitInternal(Context, Span<ByteSpan>{}, false, SwapChain);
}

bool rndr::Image::InitInternal(GraphicsContext* Context,
                               Span<ByteSpan> InitData,
                               bool IsCubeMap,
                               SwapChain* SwapChain)
{
    D3D11_TEXTURE2D_DESC Desc;
    ZeroMemory(&Desc, sizeof(D3D11_TEXTURE2D_DESC));
    Desc.BindFlags = DX11FromImageBindFlags(Props.ImageBindFlags);
    Desc.CPUAccessFlags = DX11FromUsageToCPUAccess(Props.Usage);
    Desc.Format = DX11FromPixelFormat(Props.PixelFormat);
    Desc.Usage = DX11FromUsage(Props.Usage);
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.ArraySize = ArraySize;
    Desc.MipLevels = Props.UseMips ? 0 : 1;
    Desc.MiscFlags = IsCubeMap ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
    Desc.SampleDesc.Count = Props.SampleCount;
    Desc.SampleDesc.Quality = Props.SampleCount > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0;

    const int PixelSize = GetPixelSize(Props.PixelFormat);
    const int PitchSize = Width * PixelSize;

    D3D11_SUBRESOURCE_DATA* InitialDataPtr = nullptr;
    if (InitData)
    {
        InitialDataPtr = new D3D11_SUBRESOURCE_DATA[ArraySize];
        for (int Index = 0; Index < ArraySize; Index++)
        {
            ZeroMemory(&InitialDataPtr[Index], sizeof(D3D11_SUBRESOURCE_DATA));
            InitialDataPtr[Index].pSysMem = InitData[Index].Data;
            InitialDataPtr[Index].SysMemPitch = PitchSize;
        }
    }

    ID3D11Device* Device = Context->DX11Device;
    HRESULT Result = S_OK;

    if (SwapChain == nullptr)
    {
        Result = Device->CreateTexture2D(&Desc, InitialDataPtr, &DX11Texture);
        delete[] InitialDataPtr;
        if (Context->WindowsHasFailed(Result))
        {
            const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("Image::InitInternal: %s", ErrorMessage.c_str());
            return false;
        }
    }
    else
    {
        Result = SwapChain->DX11SwapChain->GetBuffer(BackBufferIndex, __uuidof(ID3D11Texture2D),
                                                     reinterpret_cast<LPVOID*>(&DX11Texture));
        if (Context->WindowsHasFailed(Result))
        {
            const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("Image::InitInternal: %s", ErrorMessage.c_str());
            return false;
        }
    }

    if ((Props.ImageBindFlags & ImageBindFlags::ShaderResource) != 0)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC ResourceDesc;
        ZeroMemory(&ResourceDesc, sizeof(ResourceDesc));
        ResourceDesc.Format = DX11FromPixelFormat(Props.PixelFormat);
        if (!IsCubeMap)
        {
            if (ArraySize == 1)
            {
                if (Props.SampleCount > 1)
                {
                    ResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
                }
                else
                {
                    ResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                    ResourceDesc.Texture2D.MipLevels = Props.UseMips ? -1 : 1;
                }
            }
            else
            {
                if (Props.SampleCount > 1)
                {
                    ResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
                    ResourceDesc.Texture2DMSArray.ArraySize = ArraySize;
                }
                else
                {
                    ResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                    ResourceDesc.Texture2DArray.MipLevels = Props.UseMips ? -1 : 1;
                    ResourceDesc.Texture2DArray.ArraySize = ArraySize;
                }
            }
        }
        else
        {
            ResourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            ResourceDesc.TextureCube.MipLevels = Props.UseMips ? -1 : 1;
        }
        Result =
            Device->CreateShaderResourceView(DX11Texture, &ResourceDesc, &DX11ShaderResourceView);
        if (Context->WindowsHasFailed(Result))
        {
            const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }
    }
    if ((Props.ImageBindFlags & ImageBindFlags::RenderTarget) != 0)
    {
        D3D11_RENDER_TARGET_VIEW_DESC ResourceDesc;
        ZeroMemory(&ResourceDesc, sizeof(ResourceDesc));
        ResourceDesc.Format = DX11FromPixelFormat(Props.PixelFormat);
        if (ArraySize == 1)
        {
            if (Props.SampleCount > 1)
            {
                ResourceDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
            }
            else
            {
                ResourceDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            }
        }
        else
        {
            if (Props.SampleCount > 1)
            {
                ResourceDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
                ResourceDesc.Texture2DMSArray.ArraySize = ArraySize;
            }
            else
            {
                ResourceDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                ResourceDesc.Texture2DArray.ArraySize = ArraySize;
            }
        }
        Result = Device->CreateRenderTargetView(DX11Texture, &ResourceDesc, &DX11RenderTargetView);
        if (Context->WindowsHasFailed(Result))
        {
            const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }
    }
    if ((Props.ImageBindFlags & ImageBindFlags::DepthStencil) != 0)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC ResourceDesc;
        ZeroMemory(&ResourceDesc, sizeof(ResourceDesc));
        ResourceDesc.Format = DX11FromPixelFormat(Props.PixelFormat);
        if (ArraySize == 1)
        {
            if (Props.SampleCount > 1)
            {
                ResourceDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
            }
            else
            {
                ResourceDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            }
        }
        else
        {
            if (Props.SampleCount > 1)
            {
                ResourceDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
                ResourceDesc.Texture2DMSArray.ArraySize = ArraySize;
            }
            else
            {
                ResourceDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                ResourceDesc.Texture2DArray.ArraySize = ArraySize;
            }
        }
        Result = Device->CreateDepthStencilView(DX11Texture, &ResourceDesc, &DX11DepthStencilView);
        if (Context->WindowsHasFailed(Result))
        {
            const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }
    }

    return true;
}

// FIXME: Refactor this to support both CommandList and GraphicsContext
bool rndr::Image::Update(GraphicsContext* Context,
                         int ArrayIndex,
                         const math::Point2& Start,
                         const math::Vector2& Size,
                         ByteSpan Contents) const
{
    if (Props.Usage == Usage::Readback)
    {
        RNDR_LOG_ERROR("Image::Update: Can't update readback image!");
        return false;
    }
    if (Props.UseMips)
    {
        RNDR_LOG_ERROR("Image::Update: Update of image with mips not supported!");
        return false;
    }
    if (ArrayIndex < 0 || ArrayIndex >= ArraySize)
    {
        RNDR_LOG_ERROR("Image::Update: Invalid image index!");
        return false;
    }
    const math::Point2 ImageSize{static_cast<float>(Width), static_cast<float>(Height)};
    const math::Bounds2 ImageBounds({0, 0}, ImageSize);
    const math::Point2 End = Start + Size;
    if (!math::InsideInclusive(Start, ImageBounds))
    {
        RNDR_LOG_ERROR("Image::Update: Invalid Start point!");
        return false;
    }
    if (!math::InsideInclusive(Start, ImageBounds))
    {
        RNDR_LOG_ERROR("Image::Update: Invalid End point!");
        return false;
    }
    const int PixelSize = GetPixelSize(Props.PixelFormat);
    const int SizeX = static_cast<int>(Size.X);
    const int SizeY = static_cast<int>(Size.Y);
    if (!Contents || Contents.Size != SizeX * SizeY * PixelSize)
    {
        RNDR_LOG_ERROR("Image::Update: Invalid contents!");
        return false;
    }

    ID3D11DeviceContext* DeviceContext = Context->DX11DeviceContext;
    const uint32_t SubresourceIndex = D3D11CalcSubresource(0, ArrayIndex, 1);
    if (Context->WindowsHasFailed())
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }
    if (Props.Usage == Usage::Dynamic)
    {
        D3D11_MAPPED_SUBRESOURCE Subresource;
        const HRESULT Result = DeviceContext->Map(DX11Texture, SubresourceIndex,
                                                  D3D11_MAP_WRITE_DISCARD, 0, &Subresource);
        if (Context->WindowsHasFailed(Result))
        {
            const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }

        for (int Row = 0; Row < SizeY; Row++)
        {
            memcpy(reinterpret_cast<uint8_t*>(Subresource.pData) + Row * Subresource.RowPitch,
                   Contents.Data + Row * SizeX * PixelSize, SizeX * PixelSize);
        }

        DeviceContext->Unmap(DX11Texture, ArrayIndex);
        if (Context->WindowsHasFailed())
        {
            const std::string ErrorMessage = Context->WindowsGetErrorMessage();
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }

        return true;
    }

    // Image has default usage

    D3D11_BOX* DestRegionPtr = nullptr;
    D3D11_BOX DestRegion;
    DestRegion.left = static_cast<uint32_t>(Start.X);
    DestRegion.right = static_cast<uint32_t>(End.X);
    DestRegion.top = static_cast<uint32_t>(Start.Y);
    DestRegion.bottom = static_cast<uint32_t>(End.Y);
    DestRegion.front = 0;
    DestRegion.back = 1;
    DestRegionPtr = &DestRegion;
    const int BoxWidth = static_cast<int>(End.X - Start.X);

    DeviceContext->UpdateSubresource(DX11Texture, SubresourceIndex, DestRegionPtr, Contents.Data,
                                     BoxWidth * PixelSize, 0);
    if (Context->WindowsHasFailed())
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

// TODO(Marko): Move this to GraphicsContext.
bool rndr::Image::Read(GraphicsContext* Context,
                       int ArrayIndex,
                       const math::Point2& Start,
                       const math::Vector2& Size,
                       ByteSpan OutContents) const
{
    if (Props.Usage != Usage::Readback)
    {
        RNDR_LOG_ERROR("Image::Read: Only image with readback usage can be read from!");
        return false;
    }
    if (Props.UseMips)
    {
        RNDR_LOG_ERROR("Image::Read: Update of image with mips not supported!");
        return false;
    }
    if (ArrayIndex < 0 || ArrayIndex >= ArraySize)
    {
        RNDR_LOG_ERROR("Image::Read: Invalid image index!");
        return false;
    }
    const math::Point2 ImageSize{static_cast<float>(Width), static_cast<float>(Height)};
    const math::Bounds2 ImageBounds({0, 0}, ImageSize);
    if (!math::InsideInclusive(Start, ImageBounds))
    {
        RNDR_LOG_ERROR("Image::Read: Invalid Start point!");
        return false;
    }
    if (!math::InsideInclusive(ImageSize, ImageBounds))
    {
        RNDR_LOG_ERROR("Image::Read: Invalid End point!");
        return false;
    }
    const int PixelSize = GetPixelSize(Props.PixelFormat);
    const int SizeX = static_cast<int>(Size.X);
    const int SizeY = static_cast<int>(Size.Y);
    const int StartX = static_cast<int>(Start.X);
    const int StartY = static_cast<int>(Start.Y);
    if (!OutContents || OutContents.Size != SizeX * SizeY * PixelSize)
    {
        RNDR_LOG_ERROR("Image::Read: Invalid contents!");
        return false;
    }

    ID3D11DeviceContext* DeviceContext = Context->DX11DeviceContext;
    const uint32_t SubresourceIndex = D3D11CalcSubresource(0, ArrayIndex, 1);
    if (Context->WindowsHasFailed())
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("Image::Read: %s", ErrorMessage.c_str());
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE Subresource;
    const HRESULT Result =
        DeviceContext->Map(DX11Texture, SubresourceIndex, D3D11_MAP_READ, 0, &Subresource);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("Image::Read: %s", ErrorMessage.c_str());
        return false;
    }

    uint8_t* SrcData = reinterpret_cast<uint8_t*>(Subresource.pData);
    for (int Row = StartY; Row < StartY + SizeY; Row++)
    {
        const int RowPitch = static_cast<int>(Subresource.RowPitch);
        const int ReadSize = SizeX * PixelSize;
        const int DstOffset = (Row - StartY) * SizeX * PixelSize;
        const int SrcOffset = Row * RowPitch + StartX * PixelSize;
        memcpy(OutContents.Data + DstOffset, SrcData + SrcOffset, ReadSize);
    }

    DeviceContext->Unmap(DX11Texture, ArrayIndex);
    if (Context->WindowsHasFailed())
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("Image::Read: %s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

// TODO(Marko): Check if we can do this with CommandList
bool rndr::Image::Copy(GraphicsContext* Context, Image* Src, Image* Dest)
{
    if (Src == nullptr)
    {
        RNDR_LOG_ERROR("Image::Copy: Source image is invalid!");
        return false;
    }
    if (Dest == nullptr)
    {
        RNDR_LOG_ERROR("Image::Copy: Destination image is invalid!");
        return false;
    }

    ID3D11DeviceContext* DeviceContext = Context->DX11DeviceContext;
    DeviceContext->CopyResource(Dest->DX11Texture, Src->DX11Texture);
    if (Context->WindowsHasFailed())
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

rndr::Image::~Image()
{
    DX11SafeRelease(DX11Texture);
    DX11SafeRelease(DX11ShaderResourceView);
    DX11SafeRelease(DX11RenderTargetView);
    DX11SafeRelease(DX11DepthStencilView);
}

#endif  // RNDR_DX11
