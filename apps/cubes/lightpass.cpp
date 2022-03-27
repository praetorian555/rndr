#include "lightpass.h"

#define BIND_SHADER(Func, This) std::bind(&Func, This, std::placeholders::_1, std::placeholders::_2)

void LightRenderPass::Init(rndr::Camera* Camera)
{
    std::shared_ptr<rndr::VertexShader> VertexShader = std::make_shared<rndr::VertexShader>();
    VertexShader->Callback = BIND_SHADER(LightRenderPass::VertexShader, this);

    std::shared_ptr<rndr::PixelShader> PixelShader = std::make_shared<rndr::PixelShader>();
    PixelShader->Callback = BIND_SHADER(LightRenderPass::FragmentShader, this);

    m_Pipeline = std::make_unique<rndr::Pipeline>();
    m_Pipeline->WindingOrder = rndr::WindingOrder::CCW;
    m_Pipeline->VertexShader = VertexShader;
    m_Pipeline->PixelShader = PixelShader;
    m_Pipeline->DepthTest = rndr::DepthTest::LesserThen;

    m_Camera = Camera;

    m_Model = std::make_unique<rndr::Model>();
    m_Model->SetPipeline(m_Pipeline.get());
    m_Model->SetVertexData(rndr::Cube::GetVertexPositions());
    m_Model->SetIndices(rndr::Cube::GetIndices());
}

void LightRenderPass::ShutDown() {}

void LightRenderPass::Render(rndr::Rasterizer& Renderer, real DeltaSeconds)
{
    const real LightSize = 0.3;
    const rndr::Vector3r LightPosition(0, 0, -45);
    rndr::Transform LightTransform =
        rndr::Translate(LightPosition) * rndr::Scale(LightSize, LightSize, LightSize);
    LightTransform = m_Camera->FromWorldToNDC() * LightTransform;

    m_Model->SetConstants(LightTransform);

    Renderer.Draw(m_Model.get());
}

void LightRenderPass::SetTargetImages(rndr::Image* ColorImage, rndr::Image* DepthImage)
{
    m_Pipeline->ColorImage = ColorImage;
    m_Pipeline->DepthImage = DepthImage;
}

rndr::Point3r LightRenderPass::VertexShader(const rndr::PerVertexInfo& Info, real& W)
{
    rndr::Transform* FullTransform = (rndr::Transform*)Info.Constants;
    rndr::Point3r* Position = (rndr::Point3r*)Info.VertexData;
    return (*FullTransform)(*Position, W);
}

rndr::Color LightRenderPass::FragmentShader(const rndr::PerPixelInfo& Info, real& Depth)
{
    return rndr::Color::White;
}
