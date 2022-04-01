#pragma once

#include "rndr/rndr.h"

class BoxRenderPass
{
public:
    struct BoxVertex
    {
        rndr::Point3r Position;
        rndr::Point2r TexCoords;
        rndr::Normal3r Normal;
    };

    struct OutBoxVertex
    {
        rndr::Point2r TexCoords;
        rndr::Normal3r Normal;
        rndr::Point3r PositionWorld;
    };

    struct BoxInstance
    {
        rndr::Transform FromModelToWorld;
    };

public:
    void Init(rndr::Camera* Camera);
    void ShutDown();

    void Render(rndr::Rasterizer& Renderer, real DeltaSeconds);

    void SetTargetImages(rndr::Image* ColorImage, rndr::Image* DepthImage);
    void SetLightPosition(rndr::Point3r LightPosition);

private:
    void VertexShader(const rndr::InVertexInfo& InInfo, rndr::OutVertexInfo& OutInfo);
    void FragmentShader(const rndr::Triangle& T, const rndr::InFragmentInfo& InInfo, rndr::OutFragmentInfo& OutInfo);

private:
    std::unique_ptr<rndr::Pipeline> m_Pipeline;
    std::unique_ptr<rndr::Model> m_Model;
    std::unique_ptr<rndr::Image> m_Texture;

    std::vector<BoxInstance> m_Instances;

    rndr::Camera* m_Camera;
    rndr::Point3r m_LightPosition;
};
