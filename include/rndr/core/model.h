#pragma once

#include <vector>

#include "rndr/core/base.h"
#include "rndr/core/span.h"

#if defined RNDR_RASTER
#include "rndr/render/rasterpipeline.h"
#else
#error Pipeline implementation is missing!
#endif

namespace rndr
{

/**
 * This class encapsulates all data needed to render a model described by triangles. It stores geometry of the mesh and material info
 * (pipeline config and textures).
 */
class Model
{
public:
    /**
     * Constructor.
     *
     * @param Pipeline Pipeline object used to configure the rendering pipeline.
     * @param VertexData Data representing the geometry of a mesh in a form of byte array.
     * @param VertexStride Size in bytes of one vertex.
     * @param OutVertexStride Size in bytes of a structure that is the output structure of the vertex shader.
     * @param Indices Array of vertex indices that form triangles of the mesh.
     * @param ShaderConstants Array of bytes that represent data that is constant during the pipeline execution.
     * @param InstanceCount Number of instances of the given mesh.
     * @param InstanceData Array of bytes reprenting data specific to the instance.
     * @param InstanceStride Size in bytes of data for one instance.
     */
    Model(Pipeline* Pipeline,
          ByteSpan VertexData,
          int VertexStride,
          int OutVertexStride,
          IntSpan Indices,
          ByteSpan ShaderConstants,
          int InstanceCount = 1,
          ByteSpan InstanceData = ByteSpan(),
          int InstanceStride = 1);
    ~Model() = default;

    void SetShaderConstants(ByteSpan ShaderConstants);

    Pipeline* GetPipeline() { return m_Pipeline; }
    const Pipeline* GetPipeline() const { return m_Pipeline; }

    ByteSpan GetVertexData() const { return ByteSpan(m_VertexData); }
    int GetVertexStride() const { return m_VertexStride; }
    int GetVertexCount() const { return m_VertexCount; }

    int GetOutVertexStride() const { return m_OutVertexStride; }

    IntSpan GetIndices() const { return IntSpan(m_Indices); }

    ByteSpan GetInstanceData() const { return !m_InstanceData.empty() ? ByteSpan(m_InstanceData) : ByteSpan(); }
    int GetInstanceStride() const { return m_InstanceStride; }
    int GetInstanceCount() const { return m_InstanceCount; }

    ByteSpan GetShaderConstants() { return !m_ShaderConstants.empty() ? ByteSpan(m_ShaderConstants) : ByteSpan(); }

private:
    Pipeline* m_Pipeline;  // Just a reference, we don't own this object

    std::vector<uint8_t> m_VertexData;
    int m_VertexStride = 0;
    int m_VertexCount = 0;

    int m_OutVertexStride = 0;

    std::vector<int> m_Indices;

    std::vector<uint8_t> m_InstanceData;
    int m_InstanceStride = 0;
    int m_InstanceCount = 0;

    std::vector<uint8_t> m_ShaderConstants;
};

}  // namespace rndr