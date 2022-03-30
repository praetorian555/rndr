#pragma once

#include "rndr/rndr.h"

class LightRenderPass
{
public:
    void Init(rndr::Camera* Camera);
    void ShutDown();

    void Render(rndr::Rasterizer& Renderer, real DeltaSeconds);

    void SetTargetImages(rndr::Image* ColorImage, rndr::Image* DepthImage);

    rndr::Point3r GetLightPosition() const;

private:
    rndr::Point4r VertexShader(const rndr::PerVertexInfo& Info);
    rndr::Color FragmentShader(const rndr::PerPixelInfo& Info, real& Depth);

private:
    std::unique_ptr<rndr::Pipeline> m_Pipeline;
    std::unique_ptr<rndr::Model> m_Model;

    rndr::Camera* m_Camera = nullptr;

    rndr::Point3r m_LightPosition;
};