#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <d3d.h>

#include "rndr/core/graphicscontext.h"
#include "rndr/core/graphicstypes.h"
#include "rndr/core/span.h"

namespace rndr
{

/**
 * Encapsulates model's geometry and instance data.
 */
class Mesh
{
public:
    Mesh(GraphicsContext* Context,
         ByteSpan VertexData,
         int VertexStride,
         ByteSpan InstanceData,
         int InstanceStride,
         IntSpan Indices,
         PrimitiveTopology Topology);
    ~Mesh();

    void Render();

private:
    GraphicsContext* m_GraphicsContext;
    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11Buffer* m_InstanceBuffer = nullptr;
    ID3D11Buffer* m_IndexBuffer = nullptr;
    int m_VertexCount = 0;
    int m_VertexStride = 0;
    int m_InstanceCount = 0;
    int m_InstanceStride = 0;
    D3D11_PRIMITIVE_TOPOLOGY m_Topology;
};

}  // namespace rndr

#endif  // RNDR_DX11
