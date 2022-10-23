#include "rndr/render/dx11/dx11buffer.h"

#if defined RNDR_DX11

#include <d3d.h>
#include <d3d11.h>

#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"

rndr::Buffer::~Buffer()
{
    DX11SafeRelease(DX11Buffer);
}

bool rndr::Buffer::Init(GraphicsContext* Context, const BufferProperties& Props, ByteSpan InitialData)
{
    D3D11_BUFFER_DESC Desc;
    Desc.BindFlags = DX11FromBufferBindFlag(Props.BindFlag);
    Desc.Usage = DX11FromUsage(Props.Usage);
    Desc.CPUAccessFlags = DX11FromUsageToCPUAccess(Props.Usage);
    Desc.ByteWidth = Props.Size;
    Desc.MiscFlags = 0;
    Desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA DescData;
    DescData.pSysMem = InitialData ? InitialData.Data : nullptr;
    DescData.SysMemPitch = 0;
    DescData.SysMemSlicePitch = 0;
    ID3D11Device* Device = Context->GetDevice();
    HRESULT Result = Device->CreateBuffer(&Desc, &DescData, &DX11Buffer);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

bool rndr::Buffer::Update(GraphicsContext* Context, ByteSpan Data, int StartOffset) const
{
    if (Props.Usage == Usage::Readback)
    {
        RNDR_LOG_ERROR("Buffer::Update: Can't update readback buffer!");
        return false;
    }
    if (!Data)
    {
        RNDR_LOG_ERROR("Buffer::Update: Data is not valid!");
        return false;
    }
    if (StartOffset < 0 || StartOffset >= Props.Size)
    {
        RNDR_LOG_ERROR("Buffer::Update: StartOffset is outside the buffer range!");
        return false;
    }
    if (StartOffset + Data.Size > Props.Size)
    {
        RNDR_LOG_ERROR("Buffer::Update: Data is too large for the buffer!");
        return false;
    }

    ID3D11DeviceContext* DeviceContext = Context->GetDeviceContext();
    if (Props.Usage == Usage::Dynamic)
    {
        D3D11_MAPPED_SUBRESOURCE Subresource;
        HRESULT Result = DeviceContext->Map(DX11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Subresource);
        if (Context->WindowsHasFailed(Result))
        {
            std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }

        memcpy((uint8_t*)Subresource.pData + StartOffset, Data.Data, Data.Size);

        DeviceContext->Unmap(DX11Buffer, 0);
        if (Context->WindowsHasFailed())
        {
            std::string ErrorMessage = Context->WindowsGetErrorMessage();
            RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
            return false;
        }

        return true;
    }

    D3D11_BOX* DestRegionPtr = nullptr;
    D3D11_BOX DestRegion;
    DestRegion.left = StartOffset;
    DestRegion.right = StartOffset + Data.Size;
    DestRegion.top = 0;
    DestRegion.bottom = 1;
    DestRegion.front = 0;
    DestRegion.back = 1;
    DestRegionPtr = &DestRegion;

    DeviceContext->UpdateSubresource(DX11Buffer, 0, DestRegionPtr, Data.Data, Data.Size, 0);
    if (Context->WindowsHasFailed())
    {
        std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

#endif  // RNDR_DX11
