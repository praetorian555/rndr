#include "boxpass.h"

#include "rndr/core/debug.h"

void BoxRenderPass::Init(rndr::GraphicsContext* GraphicsContext, rndr::ProjectionCamera* Camera)
{
    m_GraphicsContext = GraphicsContext;

    m_Pipeline = std::make_unique<rndr::Pipeline>();

    m_Pipeline->bEnableCulling = true;
    m_Pipeline->FrontFaceWindingOrder = rndr::WindingOrder::CCW;
    m_Pipeline->CullFace = rndr::Face::Back;

    m_Pipeline->VertexShader = RNDR_BIND_TWO_PARAM(&m_Shader, &rndr::PhongShader::VertexShader);
    m_Pipeline->FragmentShader = RNDR_BIND_THREE_PARAM(&m_Shader, &rndr::PhongShader::FragmentShader);

    m_Pipeline->bUseDepthTest = true;
    m_Pipeline->DepthTestOperator = rndr::DepthTest::Less;
    m_Pipeline->bFragmentShaderChangesDepth = false;

    m_Pipeline->ColorBlendOperator = rndr::BlendOperator::Add;
    m_Pipeline->SrcColorBlendFactor = rndr::BlendFactor::SrcAlpha;
    m_Pipeline->DstColorBlendFactor = rndr::BlendFactor::OneMinusSrcAlpha;
    m_Pipeline->AlphaBlendOperator = rndr::BlendOperator::Add;
    m_Pipeline->SrcAlphaBlendFactor = rndr::BlendFactor::One;
    m_Pipeline->DstAlphaBlendFactor = rndr::BlendFactor::OneMinusSrcAlpha;

    m_Pipeline->RenderTarget = m_GraphicsContext->GetWindowFrameBuffer();

    std::vector<rndr::PhongShader::InVertex> Vertices;
    auto& CubePositions = rndr::Cube::GetVertexPositions();
    auto& CubeTexCoords = rndr::Cube::GetVertexTextureCoordinates();
    auto& CubeNormals = rndr::Cube::GetNormals();
    for (int i = 0; i < CubePositions.Size; i++)
    {
        Vertices.push_back(rndr::PhongShader::InVertex{CubePositions[i], CubeTexCoords[i], CubeNormals[i]});
    }

    rndr::RNG RandomGen;
    const int BoxCount = 100;
    m_Instances.resize(BoxCount);

    for (int i = 0; i < BoxCount; i++)
    {
        const real ScaleFactor = RandomGen.UniformRealInRange(0.8, 2.0);
        rndr::Rotator Angles;
        Angles.Pitch = RandomGen.UniformRealInRange(-90, 90);
        Angles.Yaw = RandomGen.UniformRealInRange(-90, 90);
        Angles.Roll = RandomGen.UniformRealInRange(-90, 90);
        rndr::Vector3r Position;
        Position.X = RandomGen.UniformRealInRange(-30, 30);
        Position.Y = RandomGen.UniformRealInRange(-30, 30);
        Position.Z = RandomGen.UniformRealInRange(-60, -30);

        m_Instances[i].FromModelToWorld =
            rndr::Translate(Position) * rndr::Rotate(Angles) * rndr::Scale(ScaleFactor, ScaleFactor, ScaleFactor);
    }

    const std::string WallTexturePath = ASSET_DIR "/bricked-wall.png";
    rndr::ImageProperties TextureProps;
    TextureProps.MagFilter = rndr::ImageFiltering::Linear;
    TextureProps.MinFilter = rndr::ImageFiltering::Linear;
    TextureProps.MipFilter = rndr::ImageFiltering::Linear;
    TextureProps.bUseMips = true;
    m_Texture = std::make_unique<rndr::Image>(WallTexturePath, TextureProps);

    m_Camera = Camera;

    rndr::ByteSpan VertexData((uint8_t*)Vertices.data(), Vertices.size() * sizeof(rndr::PhongShader::InVertex));
    rndr::ByteSpan InstanceData((uint8_t*)m_Instances.data(), m_Instances.size() * sizeof(rndr::PhongShader::InInstance));
    const int VertexStride = sizeof(rndr::PhongShader::InVertex);
    const int OutVertexStride = sizeof(rndr::PhongShader::OutVertex);
    const int InstanceStride = sizeof(rndr::PhongShader::InInstance);
    const int InstanceCount = m_Instances.size();
    rndr::ByteSpan EmptySpan;
    rndr::IntSpan Indices = rndr::Cube::GetIndices();
    m_Model = std::make_unique<rndr::Model>(m_Pipeline.get(), VertexData, VertexStride, OutVertexStride, Indices, EmptySpan, InstanceCount,
                                            InstanceData, InstanceStride);

    m_Shader.SetCamera(Camera);
    m_Shader.SetDiffuseImage(m_Texture.get());
    m_Shader.SetSpecularColor(rndr::Colors::White.XYZ());
}

void BoxRenderPass::ShutDown() {}

void BoxRenderPass::Render(real DeltaSeconds)
{
    m_Shader.ClearLights();

    m_Shader.SetViewPosition(m_ViewerPosition);
    m_Shader.AddPointLight(m_LightPosition, rndr::Colors::White.XYZ());

    m_GraphicsContext->Render(m_Model.get());
}

void BoxRenderPass::SetLightPosition(rndr::Point3r LightPosition)
{
    m_LightPosition = LightPosition;
}

void BoxRenderPass::SetViewerPosition(rndr::Point3r ViewerPosition)
{
    m_ViewerPosition = ViewerPosition;
}
