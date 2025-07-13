#pragma once

#include "opal/container/scope-ptr.h"

#include "rndr/renderer-base.hpp"

namespace Rndr
{

/**
 * A renderer that draws color lines in 3D space.
 */
class LineRenderer : public RendererBase
{
public:
    LineRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc);
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

    Opal::DynamicArray<VertexData> m_vertex_data;

    Opal::ScopePtr<Shader> m_vertex_shader;
    Opal::ScopePtr<Shader> m_fragment_shader;
    Opal::ScopePtr<Pipeline> m_pipeline;
    Opal::ScopePtr<Buffer> m_vertex_buffer;
    Opal::ScopePtr<Buffer> m_constant_buffer;

    static constexpr i32 k_max_lines_count = 65536;
    static constexpr u64 k_vertex_data_size = sizeof(VertexData) * k_max_lines_count * 2;
};

}  // namespace Rndr