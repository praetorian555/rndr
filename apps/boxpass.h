#pragma once

#include "rndr/rndr.h"

class BoxRenderPass
{
public:
    struct BoxConstants
    {
        rndr::Camera* Camera;
        rndr::Image* Texture;
    };

    struct BoxVertex
    {
        rndr::Point3r Position;
        rndr::Point2r TexCoords;
    };

    struct BoxInstance
    {
        rndr::Transform FromModelToWorld;
    };

public:
    void Init(rndr::Image* ColorImage, rndr::Image* DepthImage, rndr::Camera* Camera);
    void ShutDown();

    void Render(rndr::Rasterizer& Renderer, real DeltaSeconds);

private:
    rndr::Point3r VertexShader(const rndr::PerVertexInfo& Info, real& W);
    rndr::Color FragmentShader(const rndr::PerPixelInfo& Info, real& Depth);

private:
    std::unique_ptr<rndr::Pipeline> m_Pipeline;
    std::unique_ptr<rndr::Model> m_Model;
    std::unique_ptr<rndr::Image> m_Texture;

    std::vector<BoxInstance> m_Instances;

    rndr::Camera* m_Camera;
};
