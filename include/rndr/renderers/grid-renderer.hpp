#pragma once

#include "rndr/render-api.hpp"
#include "rndr/renderers/renderer-base.hpp"

namespace Rndr
{

class GridRenderer : public RendererBase
{
public:
    GridRenderer(const Opal::StringUtf8& name, const RendererBaseDesc& desc, Opal::Ref<FrameBuffer> target);
    ~GridRenderer() override;

    void Destroy();

    void SetFrameBufferTarget(Opal::Ref<FrameBuffer> target);
    void SetTransforms(const Matrix4x4f& view, const Matrix4x4f& projection);

    bool Render(f32 delta_seconds, CommandList& command_list) override;

private:
    Buffer m_uniform_buffer;
    Shader m_vertex_shader;
    Shader m_fragment_shader;
    Pipeline m_pipeline;
    Opal::Ref<FrameBuffer> m_target;
    Matrix4x4f m_view;
    Matrix4x4f m_projection;
};

}  // namespace Rndr