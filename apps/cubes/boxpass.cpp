#include "boxpass.h"

void BoxRenderPass::Init(rndr::Camera* Camera)
{
    std::shared_ptr<rndr::VertexShader> VertexShader = std::make_shared<rndr::VertexShader>();
    VertexShader->Callback = RNDR_BIND_ONE_PARAM(this, &BoxRenderPass::VertexShader);

    std::shared_ptr<rndr::PixelShader> PixelShader = std::make_shared<rndr::PixelShader>();
    PixelShader->Callback = RNDR_BIND_TWO_PARAM(this, &BoxRenderPass::FragmentShader);

    m_Pipeline = std::make_unique<rndr::Pipeline>();
    m_Pipeline->WindingOrder = rndr::WindingOrder::CCW;
    m_Pipeline->VertexShader = VertexShader;
    m_Pipeline->PixelShader = PixelShader;
    m_Pipeline->DepthTest = rndr::DepthTest::LesserThen;

    std::vector<BoxVertex> Vertices;
    auto& CubePositions = rndr::Cube::GetVertexPositions();
    auto& CubeTexCoords = rndr::Cube::GetVertexTextureCoordinates();
    auto& CubeNormals = rndr::Cube::GetNormals();
    for (int i = 0; i < CubePositions.size(); i++)
    {
        Vertices.push_back(
            BoxVertex{CubePositions[i], CubeTexCoords[i], CubeNormals[i], rndr::Point3r()});
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

        m_Instances[i].FromModelToWorld = rndr::Translate(Position) * rndr::Rotate(Angles) *
                                          rndr::Scale(ScaleFactor, ScaleFactor, ScaleFactor);
    }

    const std::string WallTexturePath = ASSET_DIR "/bricked-wall.png";
    rndr::ImageConfig TextureConfig;
    TextureConfig.MagFilter = rndr::ImageFiltering::BilinearInterpolation;
    TextureConfig.MinFilter = rndr::ImageFiltering::TrilinearInterpolation;
    m_Texture = std::make_unique<rndr::Image>(WallTexturePath, TextureConfig);

    m_Camera = Camera;

    m_Model = std::make_unique<rndr::Model>();
    m_Model->SetPipeline(m_Pipeline.get());
    m_Model->SetVertexData(Vertices);
    m_Model->SetInstanceData(m_Instances);
    m_Model->SetIndices(rndr::Cube::GetIndices());
}

void BoxRenderPass::ShutDown() {}

void BoxRenderPass::Render(rndr::Rasterizer& Renderer, real DeltaSeconds)
{
    Renderer.Draw(m_Model.get(), m_Instances.size());
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

rndr::Point4r BoxRenderPass::VertexShader(const rndr::PerVertexInfo& Info)
{
    BoxVertex* VertexData = (BoxVertex*)Info.VertexData;
    BoxInstance* InstanceData = (BoxInstance*)Info.InstanceData;

    rndr::Point3r WorldPosition = InstanceData->FromModelToWorld(VertexData->Position);
    VertexData->WorldPosition = WorldPosition;
    const rndr::Point4r NDCSpace = m_Camera->FromWorldToNDC()(rndr::Point4r(WorldPosition));
    return NDCSpace;
}

rndr::Color BoxRenderPass::FragmentShader(const rndr::PerPixelInfo& Info, real& Depth)
{
    BoxInstance* InstanceData = (BoxInstance*)Info.InstanceData;

    const size_t TextureCoordsOffset = offsetof(BoxVertex, TexCoords);
    const rndr::Point2r TexCoord = Info.Interpolate<rndr::Point2r, BoxVertex>(TextureCoordsOffset);

    const rndr::Vector2r duvdx =
        Info.DerivativeX<rndr::Point2r, BoxVertex, rndr::Vector2r>(TextureCoordsOffset) *
        Info.NextXMult;
    const rndr::Vector2r duvdy =
        Info.DerivativeY<rndr::Point2r, BoxVertex, rndr::Vector2r>(TextureCoordsOffset) *
        Info.NextYMult;

    rndr::Color Result = m_Texture->Sample(TexCoord, duvdx, duvdy);
    assert(Result.GammaSpace == rndr::GammaSpace::Linear);

    const real AmbientStrength = 0.1;

    const size_t PositionOffset = offsetof(BoxVertex, WorldPosition);
    const rndr::Point3r FragmentPos = Info.Interpolate<rndr::Point3r, BoxVertex>(PositionOffset);
    rndr::Vector3r LightDir = m_LightPosition - FragmentPos;
    LightDir = rndr::Normalize(LightDir);
    const size_t NormalOffset = offsetof(BoxVertex, Normal);
    rndr::Normal3r Normal = Info.Interpolate<rndr::Normal3r, BoxVertex>(NormalOffset);
    Normal = InstanceData->FromModelToWorld(Normal);
    Normal = rndr::Normalize(Normal);
    const real DiffuseStrength = std::max(rndr::Dot(Normal, LightDir), (real)0.0);

    if (Info.Position.X == 506 && Info.Position.Y == 408)
    {
        RNDR_LOG_INFO("FragmentPos=(%f, %f, %f), Normal=(%f, %f, %f), DiffuseStrength=%f",
                      FragmentPos.X, FragmentPos.Y, FragmentPos.Z, Normal.X, Normal.Y, Normal.Z, DiffuseStrength);
    }

    const real A = Result.A;
    Result *= (AmbientStrength + DiffuseStrength);
    Result.A = A;

    return Result;
}
