#include "rndr/dx11/dx11buffer.h"

#if defined RNDR_DX11

#include <d3d.h>
#include <d3d11.h>

#include "rndr/core/log.h"

#include "rndr/dx11/dx11graphicscontext.h"
#include "rndr/dx11/dx11helpers.h"

rndr::Buffer::Buffer(rndr::GraphicsContext* Context, const BufferProperties& P, ByteSpan InitialData) : GraphicsContext(Context), Props(P)
{
    D3D11_BUFFER_DESC Desc;
    Desc.BindFlags = DX11FromBufferBindFlag(Props.BindFlag);
    Desc.Usage = DX11FromUsage(Props.Usage);
    Desc.CPUAccessFlags = DX11FromCPUAccess(Props.CPUAccess);
    Desc.ByteWidth = Props.Size;
    Desc.MiscFlags = 0;
    Desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA DescData;
    DescData.pSysMem = InitialData ? InitialData.Data : nullptr;
    DescData.SysMemPitch = 0;
    DescData.SysMemSlicePitch = 0;
    ID3D11Device* Device = Context->GetDevice();
    HRESULT Result = Device->CreateBuffer(&Desc, &DescData, &DX11Buffer);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to create a buffer resource!");
        return;
    }
}

rndr::Buffer::~Buffer()
{
    DX11SafeRelease(DX11Buffer);
}

void rndr::Buffer::Update(ByteSpan Data) const
{
    assert(Data.Size <= Props.Size);

    D3D11_BOX* DestRegionPtr = nullptr;
    D3D11_BOX DestRegion;
    if (Props.BindFlag != BufferBindFlag::Constant)
    {
        DestRegion.left = 0;
        DestRegion.right = Data.Size;
        DestRegion.top = 0;
        DestRegion.bottom = 1;
        DestRegion.front = 0;
        DestRegion.back = 1;
        DestRegionPtr = &DestRegion;
    }

    ID3D11DeviceContext* DeviceContext = GraphicsContext->GetDeviceContext();
    DeviceContext->UpdateSubresource(DX11Buffer, 0, DestRegionPtr, Data.Data, 0, 0);
}

#endif  // RNDR_DX11
