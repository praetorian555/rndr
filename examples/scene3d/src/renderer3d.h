#pragma once

#include "rndr/rndr.h"

#include "math/point4.h"
#include "math/transform.h"
#include "math/vector4.h"

class Renderer
{
public:
    RNDR_ALIGN(16) struct VertexData
    {
        math::Point4 Position;
        math::Vector4 Color;
    };

    RNDR_ALIGN(16) struct InstanceData
    {
        math::Transform ObjectToWorld;
    };

    RNDR_ALIGN(16) struct ConstantData
    {
        math::Transform WorldToNDC;
    };

public:
    Renderer(rndr::GraphicsContext* Ctx,
             int32_t MaxVertices,
             int32_t MaxFaces,
             int32_t MaxInstances,
             int32_t ScreenWidth,
             int32_t ScreenHeight);
    ~Renderer() = default;

    void SetScreenSize(int Width, int Height);
    void SetRenderTarget(rndr::FrameBuffer& Target);

    void RenderModel(rndr::Model& Model, const rndr::Span<math::Transform>& Instances);

private:
    rndr::GraphicsContext* m_Ctx;

    rndr::ScopePtr<rndr::Pipeline> m_Pipeline;
    rndr::ScopePtr<rndr::Buffer> m_VertexBuffer;
    rndr::ScopePtr<rndr::Buffer> m_InstanceBuffer;
    rndr::ScopePtr<rndr::Buffer> m_ConstantBuffer;
    rndr::ScopePtr<rndr::Buffer> m_IndexBuffer;
    rndr::FrameBuffer* m_Target;

    const int m_MaxVertices;
    const int m_MaxFaces;
    const int m_MaxInstances;

    int m_ScreenWidth;
    int m_ScreenHeight;
    rndr::ProjectionCamera m_Camera;
};
