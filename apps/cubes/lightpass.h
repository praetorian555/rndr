#pragma once

#include "rndr/rndr.h"

class LightRenderPass
{
public:
    void Init(rndr::GraphicsContext* GraphicsContext, rndr::Camera* Camera);
    void ShutDown();

    void Render(real DeltaSeconds);

    rndr::Point3r GetLightPosition() const;

private:
    void VertexShader(const rndr::InVertexInfo& InInfo, rndr::OutVertexInfo& OutInfo);
    void FragmentShader(const rndr::Triangle& T, const rndr::InFragmentInfo& InInfo, rndr::OutFragmentInfo& OutInfo);

private:
    rndr::GraphicsContext* m_GraphicsContext;
    std::unique_ptr<rndr::Pipeline> m_Pipeline;
    std::unique_ptr<rndr::Model> m_Model;

    rndr::Camera* m_Camera = nullptr;

    rndr::Point3r m_LightPosition;
};