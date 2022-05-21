#pragma once

#include "rndr/rndr.h"

class BoxRenderPass
{
public:
    void Init(rndr::GraphicsContext* GraphicsContext, rndr::ProjectionCamera* Camera);
    void ShutDown();

    void Render(real DeltaSeconds);

    void SetLightPosition(rndr::Point3r LightPosition);
    void SetViewerPosition(rndr::Point3r ViewerPosition);

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
        rndr::Point3r LightPosition;
        rndr::Vector4r LightColor;
        rndr::Point3r ViewerPosition;
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
    rndr::Point3r m_LightPosition;
    rndr::Point3r m_ViewerPosition;

    int m_IndexCount;
    int m_InstanceCount;
};
