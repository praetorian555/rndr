#pragma once

#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/renderer-base.h"

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

    void AddLine(const Point3f& start, const Point3f& end, const Vector4f& color);

    void SetCameraTransform(const Matrix4x4f& transform);

    bool Render() override;

protected:
    struct VertexData
    {
        Point3f position;
        Vector4f color;
    };

    Opal::Array<VertexData> m_vertex_data;

    ScopePtr<Shader> m_vertex_shader;
    ScopePtr<Shader> m_fragment_shader;
    ScopePtr<Pipeline> m_pipeline;
    ScopePtr<Buffer> m_vertex_buffer;
    ScopePtr<Buffer> m_constant_buffer;

    static constexpr i32 k_max_lines_count = 65536;
    static constexpr u64 k_vertex_data_size = sizeof(VertexData) * k_max_lines_count * 2;
};

}  // namespace Rndr