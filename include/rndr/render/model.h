#pragma once

#include <vector>

#include "rndr/core/rndr.h"

namespace rndr
{

class Pipeline;

/**
 * This class encapsulates all data needed to render a model described by triangles.
 */
class Model
{
public:
    Model() = default;

    void SetPipeline(const Pipeline* PipelineConfig) { m_PipelineConfig = PipelineConfig; }

    template <typename T>
    void SetVertexData(const std::vector<T>& VertexData)
    {
        m_VertexDataStride = sizeof(T);
        m_VertexData.reserve(VertexData.size() * m_VertexDataStride);
        memcpy(m_VertexData.data(), VertexData.data(), m_VertexData.capacity());
    }

    void SetIndices(const std::vector<int>& Indices) { m_Indices = Indices; }

    template <typename T>
    void SetInstanceData(const std::vector<T>& InstanceData)
    {
        m_InstanceDataStride = sizeof(T);
        m_InstanceData.reserve(InstanceData.size() * m_InstanceDataStride);
        memcpy(m_InstanceData.data(), InstanceData.data(), m_InstanceData.capacity());
    }

    const Pipeline* GetPipeline() const { return m_PipelineConfig; }
    
    std::vector<uint8_t>& GetVertexData() { return m_VertexData; }
    const std::vector<uint8_t>& GetVertexData() const { return m_VertexData; }
    int GetVertexDataStride() const { return m_VertexDataStride; }

    std::vector<int>& GetIndices() { return m_Indices; }
    const std::vector<int>& GetIndices() const { return m_Indices; }

    std::vector<uint8_t>& GetInstanceData() { return m_InstanceData; }
    const std::vector<uint8_t>& GetInstanceData() const { return m_InstanceData; }
    int GetInstanceDataStride() const { return m_InstanceDataStride; }

private:
    const Pipeline* m_PipelineConfig; // Just a reference, we don't own this object

    std::vector<uint8_t> m_VertexData;
    int m_VertexDataStride = 0;

    std::vector<int> m_Indices;

    std::vector<uint8_t> m_InstanceData;
    int m_InstanceDataStride = 0;
};

}  // namespace rndr