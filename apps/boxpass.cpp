#include "boxpass.h"

#define BIND_SHADER(Func, This) std::bind(&Func, This, std::placeholders::_1, std::placeholders::_2)

void BoxRenderPass::Init(rndr::Image* ColorImage, rndr::Image* DepthImage, rndr::Camera* Camera)
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
    m_Pipeline->ColorImage = ColorImage;
    m_Pipeline->DepthImage = DepthImage;

    std::vector<BoxVertex> Vertices;
    auto& CubePositions = rndr::Cube::GetVertexPositions();
    auto& CubeTexCoords = rndr::Cube::GetVertexTextureCoordinates();
    for (int i = 0; i < CubePositions.size(); i++)
    {
        Vertices.push_back(BoxVertex{CubePositions[i], CubeTexCoords[i]});
    }

    m_Model = std::make_unique<rndr::Model>();
    m_Model->SetPipeline(m_Pipeline.get());
    m_Model->SetVertexData(Vertices);
    m_Model->SetIndices(rndr::Cube::GetIndices());

    const std::string WallTexturePath = ASSET_DIR "/bricked-wall.png";
    rndr::ImageConfig TextureConfig;
    TextureConfig.MagFilter = rndr::ImageFiltering::BilinearInterpolation;
    TextureConfig.MinFilter = rndr::ImageFiltering::TrilinearInterpolation;
    m_Texture = std::make_unique<rndr::Image>(WallTexturePath, TextureConfig);

    m_Camera = Camera;
}

void BoxRenderPass::ShutDown() {}

void BoxRenderPass::Render(rndr::Rasterizer& Renderer, real DeltaSeconds)
{
    rndr::Transform ModelTransform = rndr::Translate(rndr::Vector3r{0, 0, -4});
    BoxConstants Constants{&ModelTransform, m_Camera, m_Texture.get()};
    m_Model->SetConstants(Constants);

    Renderer.Draw(m_Model.get());
}

rndr::Point3r BoxRenderPass::VertexShader(const rndr::PerVertexInfo& Info, real& W)
{
    BoxConstants* Constants = (BoxConstants*)Info.Constants;
    BoxVertex* Data = (BoxVertex*)Info.VertexData;

    rndr::Point3r WorldSpace = (*Constants->FromModelToWorld)(Data->Position, W);
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
