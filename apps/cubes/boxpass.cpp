#include "boxpass.h"

#define BIND_SHADER(Func, This) std::bind(&Func, This, std::placeholders::_1, std::placeholders::_2)

void BoxRenderPass::Init(rndr::Camera* Camera)
{
    std::shared_ptr<rndr::VertexShader> VertexShader = std::make_shared<rndr::VertexShader>();
    VertexShader->Callback = BIND_SHADER(BoxRenderPass::VertexShader, this);

    std::shared_ptr<rndr::PixelShader> PixelShader = std::make_shared<rndr::PixelShader>();
    PixelShader->Callback = BIND_SHADER(BoxRenderPass::FragmentShader, this);

    m_Pipeline = std::make_unique<rndr::Pipeline>();
    m_Pipeline->WindingOrder = rndr::WindingOrder::CCW;
    m_Pipeline->VertexShader = VertexShader;
    m_Pipeline->PixelShader = PixelShader;
    m_Pipeline->DepthTest = rndr::DepthTest::LesserThen;

    std::vector<BoxVertex> Vertices;
    auto& CubePositions = rndr::Cube::GetVertexPositions();
    auto& CubeTexCoords = rndr::Cube::GetVertexTextureCoordinates();
    for (int i = 0; i < CubePositions.size(); i++)
    {
        Vertices.push_back(BoxVertex{CubePositions[i], CubeTexCoords[i]});
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

    BoxConstants Constants{m_Camera, m_Texture.get()};

    m_Model = std::make_unique<rndr::Model>();
    m_Model->SetPipeline(m_Pipeline.get());
    m_Model->SetVertexData(Vertices);
    m_Model->SetInstanceData(m_Instances);
    m_Model->SetIndices(rndr::Cube::GetIndices());
    m_Model->SetConstants(Constants);
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

rndr::Point3r BoxRenderPass::VertexShader(const rndr::PerVertexInfo& Info, real& W)
{
    BoxConstants* Constants = (BoxConstants*)Info.Constants;
    BoxVertex* VertexData = (BoxVertex*)Info.VertexData;
    BoxInstance* InstanceData = (BoxInstance*)Info.InstanceData;

    rndr::Point3r WorldSpace = InstanceData->FromModelToWorld(VertexData->Position, W);
    rndr::Point3r NDCSpace = Constants->Camera->FromWorldToNDC()(WorldSpace, W);
    return NDCSpace;
}

rndr::Color BoxRenderPass::FragmentShader(const rndr::PerPixelInfo& Info, real& Depth)
{
    const BoxConstants* const Constants = (BoxConstants*)Info.Constants;

    const size_t TextureCoordsOffset = offsetof(BoxVertex, TexCoords);
    const rndr::Point2r TexCoord = Info.Interpolate<rndr::Point2r, BoxVertex>(TextureCoordsOffset);

    const rndr::Vector2r duvdx =
        Info.DerivativeX<rndr::Point2r, BoxVertex, rndr::Vector2r>(TextureCoordsOffset) *
        Info.NextXMult;
    const rndr::Vector2r duvdy =
        Info.DerivativeY<rndr::Point2r, BoxVertex, rndr::Vector2r>(TextureCoordsOffset) *
        Info.NextYMult;

    rndr::Color Result = Constants->Texture->Sample(TexCoord, duvdx, duvdy);
    assert(Result.GammaSpace == rndr::GammaSpace::Linear);

    return Result;
}
