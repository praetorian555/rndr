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

bool rndr::Buffer::Init(GraphicsContext* Context,
                        const BufferProperties& InProps,
                        ByteSpan InitialData)
{
    if (InProps.Stride == 0)
    {
        RNDR_LOG_ERROR("Buffer::Init: Stride can't be 0!");
        return false;
    }

    Props = InProps;

    D3D11_BUFFER_DESC Desc;
    Desc.BindFlags = DX11FromBufferTypeToBindFlag(Props.Type);
    Desc.Usage = DX11FromUsage(Props.Usage);
    Desc.CPUAccessFlags = DX11FromUsageToCPUAccess(Props.Usage);
    Desc.ByteWidth = Props.Size;
    Desc.MiscFlags = 0;
    Desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA DescData;
    DescData.pSysMem = InitialData.Data;
    DescData.SysMemPitch = 0;
    DescData.SysMemSlicePitch = 0;
    D3D11_SUBRESOURCE_DATA* DescDataPtr = InitialData ? &DescData : nullptr;
    const HRESULT Result = Context->DX11Device->CreateBuffer(&Desc, DescDataPtr, &DX11Buffer);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("Buffer::Init: %s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

bool rndr::Buffer::Update(GraphicsContext* Context, ByteSpan Data, uint32_t StartOffset) const
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

    ID3D11DeviceContext* DeviceContext = Context->DX11DeviceContext;
    if (Props.Usage == Usage::Dynamic)
    {
        D3D11_MAPPED_SUBRESOURCE Subresource;
        const HRESULT Result =
            DeviceContext->Map(DX11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Subresource);
        if (Context->WindowsHasFailed(Result))
        {
            const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
            RNDR_LOG_ERROR("Buffer::Update: %s", ErrorMessage.c_str());
            return false;
        }

        uint8_t* SubresourceData = reinterpret_cast<uint8_t*>(Subresource.pData);
        memcpy(&SubresourceData[StartOffset], Data.Data, Data.Size);

        DeviceContext->Unmap(DX11Buffer, 0);
        if (Context->WindowsHasFailed())
        {
            const std::string ErrorMessage = Context->WindowsGetErrorMessage();
            RNDR_LOG_ERROR("Buffer::Update: %s", ErrorMessage.c_str());
            return false;
        }

        return true;
    }

    D3D11_BOX* DestRegionPtr = nullptr;
    const uint32_t DataSize = static_cast<uint32_t>(Data.Size);
    if (Props.Type != BufferType::Constant)
    {
        D3D11_BOX DestRegion;
        DestRegion.left = StartOffset;
        DestRegion.right = StartOffset + DataSize;
        DestRegion.top = 0;
        DestRegion.bottom = 1;
        DestRegion.front = 0;
        DestRegion.back = 1;
        DestRegionPtr = &DestRegion;
    }

    DeviceContext->UpdateSubresource(DX11Buffer, 0, DestRegionPtr, Data.Data, DataSize, 0);
    if (Context->WindowsHasFailed())
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("Buffer::Update: %s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

bool rndr::Buffer::Read(rndr::GraphicsContext* Context, ByteSpan OutData, uint32_t ReadOffset) const
{
    if (Props.Usage != Usage::Readback)
    {
        RNDR_LOG_ERROR("Buffer::Read: Can only read from readback buffer!");
        return false;
    }
    if (!OutData)
    {
        RNDR_LOG_ERROR("Buffer::Read: OutData is not valid!");
        return false;
    }
    if (ReadOffset < 0 || ReadOffset >= Props.Size)
    {
        RNDR_LOG_ERROR("Buffer::Read: ReadOffset is outside the buffer range!");
        return false;
    }
    if (ReadOffset + OutData.Size > Props.Size)
    {
        RNDR_LOG_ERROR("Buffer::Read: Data is too large for the buffer!");
        return false;
    }

    ID3D11DeviceContext* DeviceContext = Context->DX11DeviceContext;
    D3D11_MAPPED_SUBRESOURCE Subresource;
    const HRESULT Result = DeviceContext->Map(DX11Buffer, 0, D3D11_MAP_READ, 0, &Subresource);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    uint8_t* SubresourceData = reinterpret_cast<uint8_t*>(Subresource.pData);
    memcpy(OutData.Data, &SubresourceData[ReadOffset], OutData.Size);

    DeviceContext->Unmap(DX11Buffer, 0);
    if (Context->WindowsHasFailed())
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage();
        RNDR_LOG_ERROR("%s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

#endif  // RNDR_DX11
