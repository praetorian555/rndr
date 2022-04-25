#include "rndr/dx11/dx11mesh.h"

#if defined RNDR_DX11

#include "rndr/core/log.h"

#include "rndr/dx11/dx11helpers.h"

rndr::Mesh::Mesh(GraphicsContext* Context,
                 ByteSpan VertexData,
                 int VertexStride,
                 ByteSpan InstanceData,
                 int InstanceStride,
                 IntSpan Indices,
                 PrimitiveTopology Topology)
    : m_GraphicsContext(Context)
{
    assert(VertexData && VertexStride > 0);
    assert(Indices);

    m_VertexCount = VertexData.Size / VertexStride;
    m_InstanceCount = InstanceData && InstanceStride > 0 ? InstanceData.Size / InstanceStride : 0;
    m_VertexStride = VertexStride;
    m_InstanceStride = InstanceStride;

    m_Topology = FromPrimitiveTopology(Topology);

    D3D11_BUFFER_DESC VertexDesc;
    D3D11_SUBRESOURCE_DATA VertexInitialData;
    VertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    VertexDesc.Usage = D3D11_USAGE_DEFAULT;
    VertexDesc.ByteWidth = VertexData.Size;
    VertexDesc.CPUAccessFlags = 0;
    VertexDesc.MiscFlags = 0;
    VertexDesc.StructureByteStride = 0;
    VertexInitialData.pSysMem = VertexData.Data;
    VertexInitialData.SysMemPitch = 0;
    VertexInitialData.SysMemSlicePitch = 0;
    ID3D11Device* Device = m_GraphicsContext->GetDevice();
    HRESULT Result = Device->CreateBuffer(&VertexDesc, &VertexInitialData, &m_VertexBuffer);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR("Failed to create vertex buffer!");
        return;
    }

    if (InstanceData)
    {
        D3D11_BUFFER_DESC InstanceDesc;
        D3D11_SUBRESOURCE_DATA InstanceInitialData;
        InstanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        InstanceDesc.Usage = D3D11_USAGE_DEFAULT;
        InstanceDesc.ByteWidth = VertexData.Size;
        InstanceDesc.CPUAccessFlags = 0;
        InstanceDesc.MiscFlags = 0;
        InstanceDesc.StructureByteStride = 0;
        InstanceInitialData.pSysMem = VertexData.Data;
        InstanceInitialData.SysMemPitch = 0;
        InstanceInitialData.SysMemSlicePitch = 0;
        Result = Device->CreateBuffer(&InstanceDesc, &InstanceInitialData, &m_InstanceBuffer);
        if (FAILED(Result))
        {
            RNDR_LOG_ERROR("Failed to create instance buffer!");
            return;
        }
    }

    D3D11_BUFFER_DESC IndicesDesc;
    D3D11_SUBRESOURCE_DATA IndicesInitialData;
    IndicesDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    IndicesDesc.Usage = D3D11_USAGE_DEFAULT;
    IndicesDesc.ByteWidth = VertexData.Size;
    IndicesDesc.CPUAccessFlags = 0;
    IndicesDesc.MiscFlags = 0;
    IndicesDesc.StructureByteStride = 0;
    IndicesInitialData.pSysMem = VertexData.Data;
    IndicesInitialData.SysMemPitch = 0;
    IndicesInitialData.SysMemSlicePitch = 0;
    Result = Device->CreateBuffer(&IndicesDesc, &IndicesInitialData, &m_IndexBuffer);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR("Failed to create index buffer!");
        return;
    }
}

rndr::Mesh::~Mesh()
{
    DX11SafeRelease(m_VertexBuffer);
    DX11SafeRelease(m_InstanceBuffer);
    DX11SafeRelease(m_IndexBuffer);
}

void rndr::Mesh::Render()
{
    ID3D11DeviceContext* DeviceContext = m_GraphicsContext->GetDeviceContext();

    ID3D11Buffer* Buffers[2];
    uint32_t Strides[2];
    uint32_t Offsets[2];

    Buffers[0] = m_VertexBuffer;
    Strides[0] = m_VertexStride;
    Offsets[0] = 0;

    int BufferCount = 1;
    if (m_InstanceBuffer)
    {
        Buffers[1] = m_InstanceBuffer;
        Strides[1] = m_InstanceStride;
        Offsets[0] = 0;
        BufferCount = 2;
    }

    DeviceContext->IASetVertexBuffers(0, BufferCount, Buffers, Strides, Offsets);
    DeviceContext->IASetIndexBuffer(m_IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    DeviceContext->IASetPrimitiveTopology(m_Topology);
}

#endif  // RNDR_DX11
