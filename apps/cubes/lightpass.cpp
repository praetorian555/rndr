#include "lightpass.h"

void LightRenderPass::Init(rndr::Camera* Camera)
{
    m_Pipeline = std::make_unique<rndr::Pipeline>();
    m_Pipeline->WindingOrder = rndr::WindingOrder::CCW;
    m_Pipeline->VertexShader = RNDR_BIND_TWO_PARAM(this, &LightRenderPass::VertexShader);
    m_Pipeline->FragmentShader = RNDR_BIND_THREE_PARAM(this, &LightRenderPass::FragmentShader);

    m_Pipeline->DepthTestOperator = rndr::DepthTest::Less;
    m_Pipeline->MinDepth = 0.01;
    m_Pipeline->MaxDepth = 100;
    m_Pipeline->bChangesDepth = false;

    m_Pipeline->ColorBlendOperator = rndr::BlendOperator::Add;
    m_Pipeline->SrcColorBlendFactor = rndr::BlendFactor::SrcAlpha;
    m_Pipeline->DstColorBlendFactor = rndr::BlendFactor::OneMinusSrcAlpha;
    m_Pipeline->AlphaBlendOperator = rndr::BlendOperator::Add;
    m_Pipeline->SrcAlphaBlendFactor = rndr::BlendFactor::One;
    m_Pipeline->DstAlphaBlendFactor = rndr::BlendFactor::OneMinusSrcAlpha;

    m_Camera = Camera;

    rndr::Transform ShaderConstant;
    m_Model = std::make_unique<rndr::Model>(m_Pipeline.get(), (rndr::ByteSpan)rndr::Cube::GetVertexPositions(), sizeof(rndr::Point3r), 0,
                                            rndr::Cube::GetIndices(), rndr::ByteSpan((uint8_t*)&ShaderConstant, sizeof(ShaderConstant)));

    m_LightPosition = rndr::Point3r(0, 0, -45);
}

void LightRenderPass::ShutDown() {}

void LightRenderPass::Render(rndr::Rasterizer& Renderer, real DeltaSeconds)
{
    const real LightSize = 0.3;
    rndr::Transform LightTransform = rndr::Translate((rndr::Vector3r)m_LightPosition) * rndr::Scale(LightSize, LightSize, LightSize);
    LightTransform = m_Camera->FromWorldToNDC() * LightTransform;

    rndr::Transform* ShaderConstant = (rndr::Transform*)m_Model->GetShaderConstants().Data;
    *ShaderConstant = LightTransform;

    Renderer.Draw(m_Model.get());
}

void LightRenderPass::SetTargetImages(rndr::Image* ColorImage, rndr::Image* DepthImage)
{
    m_Pipeline->ColorImage = ColorImage;
    m_Pipeline->DepthImage = DepthImage;
}

rndr::Point3r LightRenderPass::GetLightPosition() const
{
    return m_LightPosition;
}

void LightRenderPass::VertexShader(const rndr::InVertexInfo& InInfo, rndr::OutVertexInfo& OutInfo)
{
    rndr::Transform* FullTransform = (rndr::Transform*)InInfo.UserConstants;
    rndr::Point3r* Position = (rndr::Point3r*)InInfo.UserVertexData;
    OutInfo.PositionNDCNonEucliean = (*FullTransform)(rndr::Point4r(*Position));
}

void LightRenderPass::FragmentShader(const rndr::Triangle& T, const rndr::InFragmentInfo& InInfo, rndr::OutFragmentInfo& OutInfo)
{
    OutInfo.Color = rndr::Color::White;
}
