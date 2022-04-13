#include "rndr/core/model.h"

rndr::Model::Model(Pipeline* Pipeline,
                   ByteSpan VertexData,
                   int VertexStride,
                   int OutVertexStride,
                   IntSpan Indices,
                   ByteSpan ShaderConstants,
                   int InstanceCount,
                   ByteSpan InstanceData,
                   int InstanceStride)
{
    m_Pipeline = Pipeline;

    assert(VertexData);
    m_VertexData.resize(VertexData.Size);
    memcpy(m_VertexData.data(), VertexData.Data, VertexData.Size);
    m_VertexStride = VertexStride;
    m_VertexCount = VertexData.Size / VertexStride;
    assert(VertexData.Size % VertexStride == 0);

    m_OutVertexStride = OutVertexStride;

    assert(Indices);
    m_Indices.resize(Indices.Size);
    memcpy(m_Indices.data(), Indices.Data, Indices.Size * sizeof(int));

    if (ShaderConstants)
    {
        m_ShaderConstants.resize(ShaderConstants.Size);
        memcpy(m_ShaderConstants.data(), ShaderConstants.Data, ShaderConstants.Size);
    }

    m_InstanceCount = InstanceCount;

    if (InstanceData)
    {
        m_InstanceData.resize(InstanceData.Size);
        memcpy(m_InstanceData.data(), InstanceData.Data, InstanceData.Size);
        m_InstanceStride = InstanceStride;
        assert(InstanceData.Size % InstanceStride == 0);
        assert(InstanceData.Size / InstanceStride == InstanceCount);
    }
}

void rndr::Model::SetShaderConstants(ByteSpan ShaderConstants)
{
    if (ShaderConstants)
    {
        m_ShaderConstants.resize(ShaderConstants.Size);
        memcpy(m_ShaderConstants.data(), ShaderConstants.Data, ShaderConstants.Size);
    }
    else
    {
        m_ShaderConstants.clear();
    }
}
