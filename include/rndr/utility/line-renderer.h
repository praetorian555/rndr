#pragma once

#include "rndr/core/renderer-base.h"
#include "rndr/core/scope-ptr.h"

namespace Rndr
{

/**
 * A renderer that draws color lines in 3D space.
 */
class LineRenderer : public RendererBase
{
public:
    LineRenderer(const String& name, const RendererBaseDesc& desc);
    ~LineRenderer() override = default;

    void AddLine(const math::Point3& start, const math::Point3& end, const math::Vector4& color);

    void SetCameraTransform(const math::Transform& transform);

    bool Render() override;

protected:
    struct VertexData
    {
        math::Point3 position;
        math::Vector4 color;
    };

    Array<VertexData> m_vertex_data;

    ScopePtr<Shader> m_vertex_shader;
    ScopePtr<Shader> m_fragment_shader;
    ScopePtr<Pipeline> m_pipeline;
    ScopePtr<Buffer> m_vertex_buffer;
    ScopePtr<Buffer> m_constant_buffer;

    static constexpr int32_t k_max_lines_count = 65536;
    static constexpr size_t k_vertex_data_size = sizeof(VertexData) * k_max_lines_count * 2;
};

}  // namespace Rndr