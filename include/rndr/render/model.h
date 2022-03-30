#pragma once

#include <memory>
#include <vector>

#include "rndr/core/base.h"

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
    virtual ~Model() { delete m_Constants; }

    void SetPipeline(Pipeline* PipelineConfig) { m_PipelineConfig = PipelineConfig; }

    template <typename T>
    void SetVertexData(const std::vector<T>& VertexData)
    {
        m_VertexDataStride = sizeof(T);
        m_VertexData.resize(VertexData.size() * m_VertexDataStride);
        memcpy(m_VertexData.data(), VertexData.data(), m_VertexData.capacity());
        m_VertexCount = VertexData.size();
    }

    void SetIndices(const std::vector<int>& Indices) { m_Indices = Indices; }

    template <typename T>
    void SetInstanceData(const std::vector<T>& InstanceData)
    {
        m_InstanceDataStride = sizeof(T);
        m_InstanceData.resize(InstanceData.size() * m_InstanceDataStride);
        memcpy(m_InstanceData.data(), InstanceData.data(), m_InstanceData.capacity());
        m_InstanceCount = InstanceData.size();
    }

    template <typename T>
    void SetConstants(const T& Constants)
    {
        T* Tmp = new T(Constants);
        m_Constants = (void*)Tmp;
    }

    Pipeline* GetPipeline() { return m_PipelineConfig; }
    const Pipeline* GetPipeline() const { return m_PipelineConfig; }

    std::vector<uint8_t>& GetVertexData() { return m_VertexData; }
    const std::vector<uint8_t>& GetVertexData() const { return m_VertexData; }
    int GetVertexDataStride() const { return m_VertexDataStride; }
    int GetVertexCount() const { return m_VertexCount; }

    std::vector<int>& GetIndices() { return m_Indices; }
    const std::vector<int>& GetIndices() const { return m_Indices; }

    std::vector<uint8_t>& GetInstanceData() { return m_InstanceData; }
    const std::vector<uint8_t>& GetInstanceData() const { return m_InstanceData; }
    int GetInstanceDataStride() const { return m_InstanceDataStride; }
    int GetInstanceCount() const { return m_InstanceCount; }

    void* GetConstants() { return m_Constants; }
    const void* GetConstants() const { return m_Constants; }

private:
    Pipeline* m_PipelineConfig;  // Just a reference, we don't own this object

    std::vector<uint8_t> m_VertexData;
    int m_VertexDataStride = 0;
    int m_VertexCount = 0;

    std::vector<int> m_Indices;

    std::vector<uint8_t> m_InstanceData;
    int m_InstanceDataStride = 0;
    int m_InstanceCount = 0;

    void* m_Constants = nullptr;
};

}  // namespace rndr