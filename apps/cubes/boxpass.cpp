#include "boxpass.h"

#include "rndr/core/debug.h"

void BoxRenderPass::Init(rndr::Camera* Camera)
{
    std::shared_ptr<rndr::VertexShader> VertexShader = std::make_shared<rndr::VertexShader>();
    VertexShader->Callback = RNDR_BIND_TWO_PARAM(this, &BoxRenderPass::VertexShader);

    std::shared_ptr<rndr::FragmentShader> FragShader = std::make_shared<rndr::FragmentShader>();
    FragShader->Callback = RNDR_BIND_THREE_PARAM(this, &BoxRenderPass::FragmentShader);

    m_Pipeline = std::make_unique<rndr::Pipeline>();
    m_Pipeline->WindingOrder = rndr::WindingOrder::CCW;
    m_Pipeline->VertexShader = VertexShader;
    m_Pipeline->FragmentShader = FragShader;
    m_Pipeline->DepthTest = rndr::DepthTest::LesserThen;

    std::vector<BoxVertex> Vertices;
    auto& CubePositions = rndr::Cube::GetVertexPositions();
    auto& CubeTexCoords = rndr::Cube::GetVertexTextureCoordinates();
    auto& CubeNormals = rndr::Cube::GetNormals();
    for (int i = 0; i < CubePositions.Size; i++)
    {
        Vertices.push_back(BoxVertex{CubePositions[i], CubeTexCoords[i], CubeNormals[i]});
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
    rndr::ImageConfig TextureConfig;
    TextureConfig.MagFilter = rndr::ImageFiltering::BilinearInterpolation;
    TextureConfig.MinFilter = rndr::ImageFiltering::TrilinearInterpolation;
    m_Texture = std::make_unique<rndr::Image>(WallTexturePath, TextureConfig);

    m_Camera = Camera;

    rndr::ByteSpan VertexData((uint8_t*)Vertices.data(), Vertices.size() * sizeof(BoxVertex));
    rndr::ByteSpan InstanceData((uint8_t*)m_Instances.data(), m_Instances.size() * sizeof(BoxInstance));
    const int VertexStride = sizeof(BoxVertex);
    const int OutVertexStride = sizeof(OutBoxVertex);
    const int InstanceStride = sizeof(BoxInstance);
    const int InstanceCount = m_Instances.size();
    rndr::ByteSpan EmptySpan;
    rndr::IntSpan Indices = rndr::Cube::GetIndices();
     m_Model = std::make_unique<rndr::Model>(m_Pipeline.get(), VertexData, VertexStride, OutVertexStride, Indices,
                                            EmptySpan, InstanceCount, InstanceData, InstanceStride);
}

void BoxRenderPass::ShutDown() {}

void BoxRenderPass::Render(rndr::Rasterizer& Renderer, real DeltaSeconds)
{
    Renderer.Draw(m_Model.get());
}

void BoxRenderPass::SetTargetImages(rndr::Image* ColorImage, rndr::Image* DepthImage)
{
    m_Pipeline->ColorImage = ColorImage;
    m_Pipeline->DepthImage = DepthImage;
}

void BoxRenderPass::SetLightPosition(rndr::Point3r LightPosition)
{
    m_LightPosition = LightPosition;
}

void BoxRenderPass::VertexShader(const rndr::InVertexInfo& InInfo, rndr::OutVertexInfo& OutInfo)
{
    BoxVertex* InVertexData = (BoxVertex*)InInfo.UserVertexData;
    BoxInstance* InInstanceData = (BoxInstance*)InInfo.UserInstanceData;

    rndr::Point3r WorldPosition = InInstanceData->FromModelToWorld(InVertexData->Position);

    OutInfo.PositionNDCNonEucliean = m_Camera->FromWorldToNDC()(rndr::Point4r(WorldPosition));
    OutBoxVertex* OutVertexData = (OutBoxVertex*)OutInfo.UserVertexData;
    OutVertexData->Normal = InInstanceData->FromModelToWorld(InVertexData->Normal);
    OutVertexData->TexCoords = InVertexData->TexCoords;
    OutVertexData->PositionWorld = WorldPosition;
}

void BoxRenderPass::FragmentShader(const rndr::Triangle& T, const rndr::InFragmentInfo& InInfo, rndr::OutFragmentInfo& OutInfo)
{
     const rndr::Point2r TexCoords = RNDR_INTERPOLATE(T, OutBoxVertex, rndr::Point2r, TexCoords, InInfo);
     const rndr::Vector2r duvdx = RNDR_DX(T, OutBoxVertex, rndr::Point2r, TexCoords, rndr::Vector2r, InInfo);
     const rndr::Vector2r duvdy = RNDR_DY(T, OutBoxVertex, rndr::Point2r, TexCoords, rndr::Vector2r, InInfo);

     rndr::Color Result = m_Texture->Sample(TexCoords, duvdx, duvdy);
     assert(Result.GammaSpace == rndr::GammaSpace::Linear);

     const rndr::Point3r FragmentPosition = RNDR_INTERPOLATE(T, OutBoxVertex, rndr::Point3r, PositionWorld, InInfo);
     rndr::Normal3r Normal = RNDR_INTERPOLATE(T, OutBoxVertex, rndr::Normal3r, Normal, InInfo);
     Normal = rndr::Normalize(Normal);
     rndr::Vector3r LightDirection = m_LightPosition - FragmentPosition;
     LightDirection = rndr::Normalize(LightDirection);

     const real AmbientStrength = 0.1;
     const real DiffuseStrength = std::max(rndr::Dot(Normal, LightDirection), (real)0.0);

     Result *= (AmbientStrength + DiffuseStrength);

     OutInfo.Color = Result;
}
