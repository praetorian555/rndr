#include "rndr/dx11/dx11buffer.h"

#if defined RNDR_DX11

#include <d3d.h>
#include <d3d11.h>

#include "rndr/core/log.h"

#include "rndr/dx11/dx11graphicscontext.h"
#include "rndr/dx11/dx11helpers.h"

rndr::Buffer::Buffer(rndr::GraphicsContext* Context, const BufferProperties& P, ByteSpan InitialData) : GraphicsContext(Context), Props(P)
{
    assert(Props.Size == InitialData.Size);

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
        RNDR_LOG_ERROR_OR_ASSERT("Failed to create a buffer resource!", );
        return;
    }
}

rndr::Buffer::~Buffer() {}

void rndr::Buffer::Update(ByteSpan Data) const
{
    ID3D11DeviceContext* DeviceContext = GraphicsContext->GetDeviceContext();

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    HRESULT Result = DeviceContext->Map(DX11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to map a buffer resource!");
        return;
    }
    memcpy(MappedResource.pData, Data.Data, Data.Size);
    DeviceContext->Unmap(DX11Buffer, 0);
}

#endif  // RNDR_DX11
