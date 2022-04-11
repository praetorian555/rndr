#pragma once

#include "rndr/rndr.h"

class BoxRenderPass
{
public:
    void Init(rndr::Camera* Camera);
    void ShutDown();

    void Render(rndr::Rasterizer& Renderer, real DeltaSeconds);

    void SetTargetImages(rndr::Image* ColorImage, rndr::Image* DepthImage);
    void SetLightPosition(rndr::Point3r LightPosition);
    void SetViewerPosition(rndr::Point3r ViewerPosition);

private:
    void VertexShader(const rndr::InVertexInfo& InInfo, rndr::OutVertexInfo& OutInfo);
    void FragmentShader(const rndr::Triangle& T, const rndr::InFragmentInfo& InInfo, rndr::OutFragmentInfo& OutInfo);

private:
    std::unique_ptr<rndr::Pipeline> m_Pipeline;
    std::unique_ptr<rndr::Model> m_Model;
    std::unique_ptr<rndr::Image> m_Texture;

    rndr::PhongShader m_Shader;

    std::vector<rndr::PhongShader::InInstance> m_Instances;

    rndr::Camera* m_Camera;
    rndr::Point3r m_LightPosition;
    rndr::Point3r m_ViewerPosition;
};
