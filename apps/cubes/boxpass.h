#pragma once

#include "math/point3.h"
#include "math/vector4.h"

#include "rndr/rndr.h"

class BoxRenderPass
{
public:
    void Init(rndr::GraphicsContext* GraphicsContext, rndr::ProjectionCamera* Camera);
    void ShutDown();

    void Render(real DeltaSeconds);

    void SetLightPosition(math::Point3 LightPosition);
    void SetViewerPosition(math::Point3 ViewerPosition);

private:
#if defined RNDR_RASTER
    void VertexShader(const rndr::InVertexInfo& InInfo, rndr::OutVertexInfo& OutInfo);
    void FragmentShader(const rndr::Triangle& T, const rndr::InFragmentInfo& InInfo, rndr::OutFragmentInfo& OutInfo);
#endif  // RNDR_RASTER

    struct InInstance
    {
        math::Transform FromModelToWorld;
    };

    RNDR_ALIGN(16) struct ShaderConstants
    {
        math::Transform FromWorldToNDC;
        math::Point3 LightPosition;
        math::Vector4 LightColor;
        math::Point3 ViewerPosition;
    };

private:
    rndr::GraphicsContext* m_GraphicsContext;

    rndr::InputLayout* m_InputLayout;
    rndr::RasterizerState* m_RasterizerState;
    rndr::DepthStencilState* m_DepthStencilState;
    rndr::BlendState* m_BlendState;

    rndr::Buffer* m_VertexBuffer;
    rndr::Buffer* m_InstanceBuffer;
    rndr::Buffer* m_IndexBuffer;
    rndr::Buffer* m_ConstantBuffer;

    rndr::Image* m_Texture;
    rndr::Sampler* m_Sampler;

#if defined RNDR_RASTER
    rndr::PhongShader m_Shader;
    std::vector<rndr::PhongShader::InInstance> m_Instances;
#else
    rndr::Shader* m_VertexShader;
    rndr::Shader* m_FragmentShader;
    std::vector<InInstance> m_Instances;
#endif RNDR_RASTER

    rndr::ProjectionCamera* m_Camera;
    math::Point3 m_LightPosition;
    math::Point3 m_ViewerPosition;

    rndr::Mesh* m_Mesh;

    int m_IndexCount;
    int m_InstanceCount;
};
