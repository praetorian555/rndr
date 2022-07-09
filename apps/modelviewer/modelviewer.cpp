#include <vector>

#include "math/normal3.h"
#include "math/point3.h"
#include "math/rotator.h"
#include "math/transform.h"
#include "math/vector2.h"
#include "math/vector3.h"

#include "rndr/core/fileutils.h"
#include "rndr/rndr.h"
#include "rndr/ui/uisystem.h"

RNDR_ALIGN(16) struct VertexShaderConstants
{
    math::Transform FromWorldToNDC;
};

RNDR_ALIGN(16) struct FragmentShaderConstants
{
    math::Point3 ViewerPosition;
    float Padding1 = 0;
    math::Vector3 LightDirection;
    float Padding2 = 0;
    math::Vector3 LightColor;
    float AmbientStrength = 0.1;
    float Shininess = 32;
};

struct InInstance
{
    math::Transform FromModelToWorld;
    math::Transform FromWorldToModel;
};

std::string g_ModelPath;

rndr::RndrApp* g_App = nullptr;
rndr::GraphicsContext* g_Context = nullptr;
rndr::FirstPersonCamera* g_Camera = nullptr;

rndr::Shader* g_VertexShader = nullptr;
rndr::Shader* g_FragmentShader = nullptr;
rndr::InputLayout* g_InputLayout = nullptr;
rndr::RasterizerState* g_RasterizerState = nullptr;
rndr::DepthStencilState* g_DepthStencilState = nullptr;
rndr::BlendState* g_BlendState = nullptr;

std::vector<rndr::Buffer*> g_VertexBuffers;
std::vector<rndr::Buffer*> g_IndexBuffers;
rndr::Buffer* g_InstanceBuffer = nullptr;
rndr::Buffer* g_VertexConstantBuffer = nullptr;
rndr::Buffer* g_FragmentConstantBuffer = nullptr;

rndr::Image* g_DiffuseImage = nullptr;
rndr::Image* g_NormalImage = nullptr;
rndr::Image* g_SpecularImage = nullptr;
rndr::Sampler* g_Sampler = nullptr;

rndr::Model* g_Model = nullptr;
math::Rotator g_MeshRotation;
math::Vector3 g_MeshRotationState;

math::Vector3 g_LightDirection;
math::Vector4 g_LightColor;

rndr::ui::FontHandle Font;

void Init();
void InitRenderPrimitives();
void CleanUp();

void Loop(float DeltaSeconds);
void Update(float DeltaSeconds);
void Render(float DeltaSeconds);
void Present(bool bVSync);

void RotateAroundX(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real Value);
void RotateAroundY(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real Value);

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        g_ModelPath = RNDR_ASSET_DIR "/models/cube_left.obj";
    }
    else
    {
        g_ModelPath = argv[1];
    }

    Init();
    g_App->Run();
    CleanUp();
}

void Init()
{
    g_App = rndr::Init();
    assert(g_App);
    g_App->OnTickDelegate.Add(Loop);
    g_Context = g_App->GetWindow()->GetGraphicsContext();

    bool Result = rndr::ui::Init(g_Context);
    assert(Result);
    Font = rndr::ui::AddFont("C:/Windows/Fonts/arial.ttf", 36);
    assert(Font != rndr::ui::kInvalidFontHandle);

    rndr::ProjectionCameraProperties CameraProps;
    CameraProps.Projection = rndr::ProjectionType::Perspective;
    CameraProps.VerticalFOV = 60;
    CameraProps.ScreenWidth = g_App->GetWindow()->GetWidth();
    CameraProps.ScreenHeight = g_App->GetWindow()->GetHeight();
    CameraProps.Near = 0.01f;
    CameraProps.Far = 100.0f;
    rndr::ProjectionCamera* ProjCamera = new rndr::ProjectionCamera(math::Transform{}, CameraProps);
    g_Camera = new rndr::FirstPersonCamera(ProjCamera, math::Point3{0, 0, -20}, 10);

    rndr::WindowDelegates::OnResize.Add(
        [](rndr::Window* Window, int Width, int Height)
        {
            if (g_App->GetWindow() == Window)
            {
                g_Camera->GetProjectionCamera()->SetScreenSize(Width, Height);
            }
        });

    rndr::InputContext* InputContext = g_App->GetInputContext();
    InputContext->CreateMapping("RotateAroundY", RotateAroundY);
    InputContext->AddBinding("RotateAroundY", rndr::InputPrimitive::Keyboard_J, rndr::InputTrigger::ButtonDown, 1);
    InputContext->AddBinding("RotateAroundY", rndr::InputPrimitive::Keyboard_L, rndr::InputTrigger::ButtonDown, -1);
    InputContext->AddBinding("RotateAroundY", rndr::InputPrimitive::Keyboard_J, rndr::InputTrigger::ButtonUp, 0);
    InputContext->AddBinding("RotateAroundY", rndr::InputPrimitive::Keyboard_L, rndr::InputTrigger::ButtonUp, 0);
    InputContext->CreateMapping("RotateAroundX", RotateAroundX);
    InputContext->AddBinding("RotateAroundX", rndr::InputPrimitive::Keyboard_I, rndr::InputTrigger::ButtonDown, 1);
    InputContext->AddBinding("RotateAroundX", rndr::InputPrimitive::Keyboard_K, rndr::InputTrigger::ButtonDown, -1);
    InputContext->AddBinding("RotateAroundX", rndr::InputPrimitive::Keyboard_I, rndr::InputTrigger::ButtonUp, 0);
    InputContext->AddBinding("RotateAroundX", rndr::InputPrimitive::Keyboard_K, rndr::InputTrigger::ButtonUp, 0);

    InitRenderPrimitives();
}

void InitRenderPrimitives()
{
    {
        rndr::ShaderProperties VertexShaderProps;
        VertexShaderProps.bCompilationNeeded = true;
        VertexShaderProps.Type = rndr::ShaderType::Vertex;
        VertexShaderProps.FilePath = RNDR_ASSET_DIR L"/shaders/MaterialVertexShader.hlsl";
        VertexShaderProps.EntryPoint = "MaterialVertexShader";
        g_VertexShader = g_Context->CreateShader(VertexShaderProps);
        assert(g_VertexShader);

        rndr::ShaderProperties FragmentShaderProps;
        FragmentShaderProps.bCompilationNeeded = true;
        FragmentShaderProps.Type = rndr::ShaderType::Fragment;
        FragmentShaderProps.FilePath = RNDR_ASSET_DIR L"/shaders/MaterialFragmentShader.hlsl";
        FragmentShaderProps.EntryPoint = "MaterialFragmentShader";
        g_FragmentShader = g_Context->CreateShader(FragmentShaderProps);
        assert(g_FragmentShader);
    }

    {
        rndr::InputLayoutProperties ILProps[13];
        ILProps[0].SemanticName = "POSITION";
        ILProps[0].SemanticIndex = 0;
        ILProps[0].InputSlot = 0;
        ILProps[0].Format = rndr::PixelFormat::R32G32B32_FLOAT;
        ILProps[0].OffsetInVertex = 0;
        ILProps[0].Repetition = rndr::DataRepetition::PerVertex;
        ILProps[0].InstanceStepRate = 0;
        ILProps[1].SemanticName = "TEXCOORD";
        ILProps[1].SemanticIndex = 0;
        ILProps[1].InputSlot = 0;
        ILProps[1].Format = rndr::PixelFormat::R32G32_FLOAT;
        ILProps[1].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[1].Repetition = rndr::DataRepetition::PerVertex;
        ILProps[1].InstanceStepRate = 0;
        ILProps[2].SemanticName = "NORMAL";
        ILProps[2].SemanticIndex = 0;
        ILProps[2].InputSlot = 0;
        ILProps[2].Format = rndr::PixelFormat::R32G32B32_FLOAT;
        ILProps[2].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[2].Repetition = rndr::DataRepetition::PerVertex;
        ILProps[2].InstanceStepRate = 0;
        ILProps[3].SemanticName = "TANGENT";
        ILProps[3].SemanticIndex = 0;
        ILProps[3].InputSlot = 0;
        ILProps[3].Format = rndr::PixelFormat::R32G32B32_FLOAT;
        ILProps[3].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[3].Repetition = rndr::DataRepetition::PerVertex;
        ILProps[3].InstanceStepRate = 0;
        ILProps[4].SemanticName = "BINORMAL";
        ILProps[4].SemanticIndex = 0;
        ILProps[4].InputSlot = 0;
        ILProps[4].Format = rndr::PixelFormat::R32G32B32_FLOAT;
        ILProps[4].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[4].Repetition = rndr::DataRepetition::PerVertex;
        ILProps[4].InstanceStepRate = 0;
        ILProps[5].SemanticName = "ROWX";
        ILProps[5].SemanticIndex = 0;
        ILProps[5].InputSlot = 1;
        ILProps[5].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
        ILProps[5].OffsetInVertex = 0;
        ILProps[5].Repetition = rndr::DataRepetition::PerInstance;
        ILProps[5].InstanceStepRate = 1;
        ILProps[6].SemanticName = "ROWY";
        ILProps[6].SemanticIndex = 0;
        ILProps[6].InputSlot = 1;
        ILProps[6].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
        ILProps[6].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[6].Repetition = rndr::DataRepetition::PerInstance;
        ILProps[6].InstanceStepRate = 1;
        ILProps[7].SemanticName = "ROWZ";
        ILProps[7].SemanticIndex = 0;
        ILProps[7].InputSlot = 1;
        ILProps[7].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
        ILProps[7].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[7].Repetition = rndr::DataRepetition::PerInstance;
        ILProps[7].InstanceStepRate = 1;
        ILProps[8].SemanticName = "ROWW";
        ILProps[8].SemanticIndex = 0;
        ILProps[8].InputSlot = 1;
        ILProps[8].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
        ILProps[8].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[8].Repetition = rndr::DataRepetition::PerInstance;
        ILProps[8].InstanceStepRate = 1;
        ILProps[9].SemanticName = "ROWX";
        ILProps[9].SemanticIndex = 1;
        ILProps[9].InputSlot = 1;
        ILProps[9].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
        ILProps[9].OffsetInVertex = 0;
        ILProps[9].Repetition = rndr::DataRepetition::PerInstance;
        ILProps[9].InstanceStepRate = 1;
        ILProps[10].SemanticName = "ROWY";
        ILProps[10].SemanticIndex = 1;
        ILProps[10].InputSlot = 1;
        ILProps[10].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
        ILProps[10].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[10].Repetition = rndr::DataRepetition::PerInstance;
        ILProps[10].InstanceStepRate = 1;
        ILProps[11].SemanticName = "ROWZ";
        ILProps[11].SemanticIndex = 1;
        ILProps[11].InputSlot = 1;
        ILProps[11].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
        ILProps[11].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[11].Repetition = rndr::DataRepetition::PerInstance;
        ILProps[11].InstanceStepRate = 1;
        ILProps[12].SemanticName = "ROWW";
        ILProps[12].SemanticIndex = 1;
        ILProps[12].InputSlot = 1;
        ILProps[12].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
        ILProps[12].OffsetInVertex = rndr::AppendAlignedElement;
        ILProps[12].Repetition = rndr::DataRepetition::PerInstance;
        ILProps[12].InstanceStepRate = 1;
        g_InputLayout = g_Context->CreateInputLayout(rndr::Span(ILProps, 13), g_VertexShader);
        assert(g_InputLayout);
    }

    {
        rndr::RasterizerProperties RasterProps;
        RasterProps.bAntialiasedLineEnable = false;
        RasterProps.bDepthClipEnable = true;
        RasterProps.bMultisampleEnable = false;
        RasterProps.bScissorEnable = false;
        RasterProps.CullFace = rndr::Face::Back;
        RasterProps.FrontFaceWindingOrder = rndr::WindingOrder::CW;
        RasterProps.FillMode = rndr::FillMode::Solid;
        g_RasterizerState = g_Context->CreateRasterizerState(RasterProps);

        rndr::DepthStencilProperties DSProps;
        DSProps.bDepthEnable = true;
        DSProps.bStencilEnable = false;
        DSProps.DepthComparator = rndr::Comparator::Less;
        DSProps.DepthMask = rndr::DepthMask::All;
        g_DepthStencilState = g_Context->CreateDepthStencilState(DSProps);

        rndr::BlendProperties BlendProps;
        BlendProps.bBlendEnable = true;
        BlendProps.SrcColorFactor = rndr::BlendFactor::SrcAlpha;
        BlendProps.DstColorFactor = rndr::BlendFactor::OneMinusSrcAlpha;
        BlendProps.ColorOperator = rndr::BlendOperator::Add;
        BlendProps.SrcAlphaFactor = rndr::BlendFactor::One;
        BlendProps.DstAlphaFactor = rndr::BlendFactor::OneMinusSrcAlpha;
        BlendProps.AlphaOperator = rndr::BlendOperator::Add;
        g_BlendState = g_Context->CreateBlendState(BlendProps);
    }

    {
        struct InVertex
        {
            math::Point3 Position;
            math::Vector2 TexCoords;
            math::Normal3 Normal;
            math::Vector3 Tangent;
            math::Vector3 Bitangent;
        };

        RNDR_LOG_INFO("Loding model from file %s", g_ModelPath.c_str());
        g_Model = rndr::ObjParser::Parse(g_ModelPath);

        for (int i = 0; i < g_Model->GetMeshes().Size; i++)
        {
            rndr::Mesh* Mesh = g_Model->GetMeshes()[i];

            std::vector<InVertex> Vertices;
            auto& CubePositions = Mesh->GetPositions();
            auto& CubeTexCoords = Mesh->GetTexCoords();
            auto& CubeNormals = Mesh->GetNormals();
            for (int i = 0; i < CubePositions.Size; i++)
            {
                math::Normal3 Normal = CubeNormals[i];
                Vertices.push_back(InVertex{CubePositions[i], CubeTexCoords[i], Normal});
            }

            rndr::BufferProperties VertexBufferProps;
            VertexBufferProps.BindFlag = rndr::BufferBindFlag::Vertex;
            VertexBufferProps.CPUAccess = rndr::CPUAccess::None;
            VertexBufferProps.Usage = rndr::Usage::GPUReadWrite;
            VertexBufferProps.Size = Vertices.size() * sizeof(InVertex);
            VertexBufferProps.Stride = sizeof(InVertex);
            rndr::Buffer* VertexBuffer = g_Context->CreateBuffer(VertexBufferProps, (rndr::ByteSpan)Vertices);
            g_VertexBuffers.push_back(VertexBuffer);
        }
    }

    {
        InInstance Instance;
        rndr::BufferProperties InstanceBufferProps;
        InstanceBufferProps.BindFlag = rndr::BufferBindFlag::Vertex;
        InstanceBufferProps.CPUAccess = rndr::CPUAccess::None;
        InstanceBufferProps.Usage = rndr::Usage::GPUReadWrite;
        InstanceBufferProps.Size = sizeof(InInstance);
        InstanceBufferProps.Stride = sizeof(InInstance);
        g_InstanceBuffer = g_Context->CreateBuffer(InstanceBufferProps, (rndr::ByteSpan)&Instance);
    }

    for (int i = 0; i < g_Model->GetMeshes().Size; i++)
    {
        rndr::Mesh* Mesh = g_Model->GetMeshes()[i];
        rndr::BufferProperties IndexBufferProps;
        IndexBufferProps.BindFlag = rndr::BufferBindFlag::Index;
        IndexBufferProps.CPUAccess = rndr::CPUAccess::None;
        IndexBufferProps.Usage = rndr::Usage::GPUReadWrite;
        IndexBufferProps.Size = Mesh->GetIndices().Size * sizeof(int);
        IndexBufferProps.Stride = sizeof(int);
        rndr::Buffer* IndexBuffer = g_Context->CreateBuffer(IndexBufferProps, rndr::ByteSpan(Mesh->GetIndices()));
        g_IndexBuffers.push_back(IndexBuffer);
    }

    {
        VertexShaderConstants Constants;
        Constants.FromWorldToNDC = math::Transpose(g_Camera->GetProjectionCamera()->FromWorldToNDC());
        rndr::BufferProperties ConstBufferProps;
        ConstBufferProps.BindFlag = rndr::BufferBindFlag::Constant;
        ConstBufferProps.CPUAccess = rndr::CPUAccess::None;
        ConstBufferProps.Usage = rndr::Usage::GPUReadWrite;
        ConstBufferProps.Size = sizeof(Constants);
        ConstBufferProps.Stride = sizeof(Constants);
        g_VertexConstantBuffer = g_Context->CreateBuffer(ConstBufferProps, (rndr::ByteSpan)&Constants);
    }

    {
        FragmentShaderConstants Constants;
        rndr::BufferProperties ConstBufferProps;
        ConstBufferProps.BindFlag = rndr::BufferBindFlag::Constant;
        ConstBufferProps.CPUAccess = rndr::CPUAccess::None;
        ConstBufferProps.Usage = rndr::Usage::GPUReadWrite;
        ConstBufferProps.Size = sizeof(Constants);
        ConstBufferProps.Stride = sizeof(Constants);
        g_FragmentConstantBuffer = g_Context->CreateBuffer(ConstBufferProps, (rndr::ByteSpan)&Constants);
    }

    {
        const std::string TexturePath = RNDR_ASSET_DIR "/textures/templategrid_albedo.png";
        rndr::CPUImage ImageInfo = rndr::ReadEntireImage(TexturePath);
        rndr::ImageProperties TextureProps;
        TextureProps.ImageBindFlags = rndr::ImageBindFlags::ShaderResource;
        TextureProps.ArraySize = 1;
        TextureProps.bUseMips = true;
        TextureProps.CPUAccess = rndr::CPUAccess::None;
        TextureProps.Usage = rndr::Usage::GPUReadWrite;
        TextureProps.PixelFormat = ImageInfo.Format;
        g_DiffuseImage = g_Context->CreateImage(ImageInfo.Width, ImageInfo.Height, TextureProps, ImageInfo.Data);
    }

    {
        const std::string TexturePath = RNDR_ASSET_DIR "/textures/templategrid_normal.png";
        rndr::CPUImage ImageInfo = rndr::ReadEntireImage(TexturePath);
        rndr::ImageProperties TextureProps;
        TextureProps.ImageBindFlags = rndr::ImageBindFlags::ShaderResource;
        TextureProps.ArraySize = 1;
        TextureProps.bUseMips = true;
        TextureProps.CPUAccess = rndr::CPUAccess::None;
        TextureProps.Usage = rndr::Usage::GPUReadWrite;
        TextureProps.PixelFormat = ImageInfo.Format;
        g_NormalImage = g_Context->CreateImage(ImageInfo.Width, ImageInfo.Height, TextureProps, ImageInfo.Data);
    }

    {
        const std::string TexturePath = RNDR_ASSET_DIR "/textures/templategrid_specular.png";
        rndr::CPUImage ImageInfo = rndr::ReadEntireImage(TexturePath);
        rndr::ImageProperties TextureProps;
        TextureProps.ImageBindFlags = rndr::ImageBindFlags::ShaderResource;
        TextureProps.ArraySize = 1;
        TextureProps.bUseMips = true;
        TextureProps.CPUAccess = rndr::CPUAccess::None;
        TextureProps.Usage = rndr::Usage::GPUReadWrite;
        TextureProps.PixelFormat = ImageInfo.Format;
        g_SpecularImage = g_Context->CreateImage(ImageInfo.Width, ImageInfo.Height, TextureProps, ImageInfo.Data);
    }

    {
        rndr::SamplerProperties SamplerProps;
        SamplerProps.AddressingU = rndr::ImageAddressing::Repeat;
        SamplerProps.AddressingV = rndr::ImageAddressing::Repeat;
        SamplerProps.AddressingW = rndr::ImageAddressing::Repeat;
        SamplerProps.Filter = rndr::ImageFiltering::MinMagMipLinear;
        SamplerProps.LODBias = 0;
        g_Sampler = g_Context->CreateSampler(SamplerProps);
    }
}

void CleanUp()
{
    delete g_Camera;
    delete g_Model;

    g_Context->DestroyShader(g_VertexShader);
    g_Context->DestroyShader(g_FragmentShader);
    g_Context->DestroyInputLayout(g_InputLayout);
    g_Context->DestroyRasterizerState(g_RasterizerState);
    g_Context->DestroyDepthStencilState(g_DepthStencilState);
    g_Context->DestroyBlendState(g_BlendState);

    for (rndr::Buffer* Buff : g_VertexBuffers)
    {
        g_Context->DestroyBuffer(Buff);
    }
    g_Context->DestroyBuffer(g_InstanceBuffer);
    for (rndr::Buffer* Buff : g_IndexBuffers)
    {
        g_Context->DestroyBuffer(Buff);
    }
    g_Context->DestroyBuffer(g_VertexConstantBuffer);
    g_Context->DestroyBuffer(g_FragmentConstantBuffer);

    g_Context->DestroyImage(g_DiffuseImage);
    g_Context->DestroyImage(g_NormalImage);
    g_Context->DestroyImage(g_SpecularImage);
    g_Context->DestroySampler(g_Sampler);

    rndr::ui::ShutDown();
    rndr::ShutDown();
}

void Loop(float DeltaSeconds)
{
    Update(DeltaSeconds);
    Render(DeltaSeconds);
    Present(true);
}

void Update(float DeltaSeconds)
{
    // Update instance transforms
    {
        const float RotationSpeed = 10;
        g_MeshRotation.Roll += g_MeshRotationState.X * RotationSpeed * DeltaSeconds;
        g_MeshRotation.Yaw += g_MeshRotationState.Y * RotationSpeed * DeltaSeconds;
        InInstance Instance;
        Instance.FromModelToWorld = math::Scale(2, 2, 2) * math::Rotate(g_MeshRotation);
        Instance.FromWorldToModel = math::Transpose((math::Transform)Instance.FromModelToWorld.GetInverse());
        Instance.FromModelToWorld = math::Transpose(Instance.FromModelToWorld);
        Instance.FromWorldToModel = math::Transpose(Instance.FromWorldToModel);
        g_InstanceBuffer->Update((rndr::ByteSpan)&Instance);
    }

    // Update vertex shader constants
    {
        VertexShaderConstants Constants;
        Constants.FromWorldToNDC = math::Transpose(g_Camera->GetProjectionCamera()->FromWorldToNDC());
        g_VertexConstantBuffer->Update((rndr::ByteSpan)&Constants);
    }

    // Update fragment shader constants
    {
        g_LightColor = rndr::Colors::Pink;
        g_LightDirection = math::Point3{} - math::Point3{-50, 50, 0};
        g_LightDirection = math::Normalize(g_LightDirection);
        FragmentShaderConstants Constants;
        Constants.ViewerPosition = math::Point3{};
        Constants.LightDirection = g_LightDirection;
        Constants.LightColor = g_LightColor.XYZ();
        Constants.AmbientStrength = 0.01f;
        Constants.Shininess = 64;
        g_FragmentConstantBuffer->Update((rndr::ByteSpan)&Constants);
    }

    g_Camera->Update(DeltaSeconds);
}

void Render(float DeltaSeconds)
{
    g_Context->ClearColor(nullptr, rndr::Colors::Black);
    g_Context->ClearDepth(nullptr, 1.0);

    g_Context->BindShader(g_VertexShader);
    g_Context->BindShader(g_FragmentShader);
    g_Context->BindInputLayout(g_InputLayout);
    g_Context->BindRasterizerState(g_RasterizerState);
    g_Context->BindDepthStencilState(g_DepthStencilState);
    g_Context->BindBlendState(g_BlendState);

    g_Context->BindBuffer(g_InstanceBuffer, 1);

    g_Context->BindBuffer(g_VertexConstantBuffer, 0, g_VertexShader);
    g_Context->BindBuffer(g_FragmentConstantBuffer, 0, g_FragmentShader);
    g_Context->BindImageAsShaderResource(g_DiffuseImage, 0, g_FragmentShader);
    g_Context->BindImageAsShaderResource(g_NormalImage, 1, g_FragmentShader);
    g_Context->BindImageAsShaderResource(g_SpecularImage, 2, g_FragmentShader);
    g_Context->BindSampler(g_Sampler, 0, g_FragmentShader);

    g_Context->BindFrameBuffer(g_Context->GetWindowFrameBuffer());

    for (int i = 0; i < g_VertexBuffers.size(); i++)
    {
        g_Context->BindBuffer(g_VertexBuffers[i], 0);
        g_Context->BindBuffer(g_IndexBuffers[i], 0);
        rndr::Mesh* Mesh = g_Model->GetMeshes()[i];
        g_Context->DrawIndexed(rndr::PrimitiveTopology::TriangleList, Mesh->GetIndices().Size);
    }

    rndr::ui::StartFrame();

    rndr::ui::TextBoxProperties TBProps;
    TBProps.BaseLineStart = math::Point2(150, 650);
    TBProps.Scale = 2.0f;
    TBProps.Font = Font;
    TBProps.Color = rndr::Colors::White;
    //rndr::ui::DrawTextBox("Hello there general!\nWill you fight with me?", TBProps);
    rndr::ui::DrawTextBox("Will you fight with me?\nComrade", TBProps);

    rndr::ui::BoxProperties Props;
    Props.BottomLeft = math::Point2{100, 100};
    Props.Size = math::Vector2{300, 100};
    Props.Color = math::Vector4{0, 1, 0, 1};
    rndr::ui::StartBox(Props);
    Props.BottomLeft = math::Point2{110, 110};
    Props.Size = math::Vector2{50, 50};
    Props.Color = math::Vector4{0, 0, 1, 1};
    rndr::ui::StartBox(Props);

    static float AnimTime = 0;
    static float AnimDuration = 0.2;
    static float FinalAlpha = 0.5;

    if (rndr::ui::MouseHovers())
    {
        AnimTime += DeltaSeconds;
        AnimTime = math::Clamp(AnimTime, 0, AnimDuration);
        float Alpha = math::Lerp(AnimTime / AnimDuration, 1, FinalAlpha);
        rndr::ui::SetColor(math::Vector4(0, 0, 1, Alpha));
    }
    else
    {
        AnimTime -= DeltaSeconds;
        AnimTime = math::Clamp(AnimTime, 0, AnimDuration);
        float Alpha = math::Lerp(AnimTime / AnimDuration, 1, FinalAlpha);
        rndr::ui::SetColor(math::Vector4(0, 0, 1, Alpha));
    }
    rndr::ui::EndBox();
    rndr::ui::EndBox();

    const math::Vector4 FirstColor = math::Vector4{1, 0, 0, 1};
    const math::Vector4 SecondColor = math::Vector4{1, 0, 0, 0.75};
    static math::Vector4 NextColor = SecondColor;
    static math::Vector4 Color = FirstColor;

    Props.BottomLeft = math::Point2{500, 100};
    Props.Size = math::Vector2{100, 100};
    Props.Color = Color;
    rndr::ui::StartBox(Props);
    if (rndr::ui::LeftMouseButtonClicked())
    {
        Color = NextColor;
        NextColor = NextColor == FirstColor ? SecondColor : FirstColor;
    }
    rndr::ui::EndBox();

    rndr::ui::EndFrame();
}

void Present(bool bVSync)
{
    rndr::GraphicsContext* GC = g_App->GetWindow()->GetGraphicsContext();
    GC->Present(bVSync);
}

void RotateAroundX(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real Value)
{
    g_MeshRotationState.X = Value;
}

void RotateAroundY(rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger, real Value)
{
    g_MeshRotationState.Y = Value;
}
