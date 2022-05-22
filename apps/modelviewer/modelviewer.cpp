#include "rndr/rndr.h"

#include <vector>

RNDR_ALIGN(16) struct VertexShaderConstants
{
    math::Transform FromWorldToNDC;
};

RNDR_ALIGN(16) struct FragmentShaderConstants
{
    rndr::Vector3r LightDirection;
    rndr::Vector4r LightColor;
    rndr::Point3r ViewerPosition;
};

struct InInstance
{
    math::Transform FromModelToWorld;
    math::Transform FromWorldToModel;
};

rndr::RndrApp* g_App = nullptr;
rndr::GraphicsContext* g_Context = nullptr;
rndr::ProjectionCamera* g_Camera = nullptr;

rndr::Shader* g_VertexShader = nullptr;
rndr::Shader* g_FragmentShader = nullptr;
rndr::InputLayout* g_InputLayout = nullptr;
rndr::RasterizerState* g_RasterizerState = nullptr;
rndr::DepthStencilState* g_DepthStencilState = nullptr;
rndr::BlendState* g_BlendState = nullptr;

rndr::Buffer* g_VertexBuffer = nullptr;
rndr::Buffer* g_InstanceBuffer = nullptr;
rndr::Buffer* g_IndexBuffer = nullptr;
rndr::Buffer* g_VertexConstantBuffer = nullptr;
rndr::Buffer* g_FragmentConstantBuffer = nullptr;

rndr::Image* g_DiffuseImage = nullptr;
rndr::Image* g_NormalImage = nullptr;
rndr::Image* g_SpecularImage = nullptr;
rndr::Sampler* g_Sampler = nullptr;

rndr::Mesh* g_CubeMesh = nullptr;

void Init();
void InitRenderPrimitives();
void CleanUp();

void Loop(float DeltaSeconds);
void Update(float DeltaSeconds);
void Render(float DeltaSeconds);
void Present(bool bVSync);

int main()
{
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

    rndr::ProjectionCameraProperties CameraProps;
    CameraProps.Projection = rndr::ProjectionType::Perspective;
    CameraProps.VerticalFOV = 60;
    CameraProps.ScreenWidth = g_App->GetWindow()->GetWidth();
    CameraProps.ScreenHeight = g_App->GetWindow()->GetHeight();
    CameraProps.Near = 0.01f;
    CameraProps.Far = 100.0f;
    g_Camera = new rndr::ProjectionCamera(math::Translate(rndr::Vector3r(0, 0, 0)), CameraProps);

    rndr::WindowDelegates::OnResize.Add(
        [](rndr::Window* Window, int Width, int Height)
        {
            if (g_App->GetWindow() == Window)
            {
                g_Camera->SetScreenSize(Width, Height);
            }
        });

    InitRenderPrimitives();
}

void InitRenderPrimitives()
{
    {
        rndr::ShaderProperties VertexShaderProps;
        VertexShaderProps.bCompilationNeeded = true;
        VertexShaderProps.Type = rndr::ShaderType::Vertex;
        VertexShaderProps.FilePath = ASSET_DIR L"/shaders/MaterialVertexShader.hlsl";
        VertexShaderProps.EntryPoint = "MaterialVertexShader";
        g_VertexShader = g_Context->CreateShader(VertexShaderProps);
        assert(g_VertexShader);

        rndr::ShaderProperties FragmentShaderProps;
        FragmentShaderProps.bCompilationNeeded = true;
        FragmentShaderProps.Type = rndr::ShaderType::Fragment;
        FragmentShaderProps.FilePath = ASSET_DIR L"/shaders/MaterialFragmentShader.hlsl";
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
            rndr::Point3r Position;
            rndr::Vector2r TexCoords;
            rndr::Normal3r Normal;
            rndr::Vector3r Tangent;
            rndr::Vector3r Bitangent;
        };

        std::string CubeObjPath = ASSET_DIR "/models/cube.obj";
        g_CubeMesh = rndr::ObjParser::Parse(CubeObjPath);

        std::vector<InVertex> Vertices;
        auto& CubePositions = g_CubeMesh->GetPositions();
        auto& CubeTexCoords = g_CubeMesh->GetTexCoords();
        auto& CubeNormals = g_CubeMesh->GetNormals();
        for (int i = 0; i < CubePositions.Size; i++)
        {
            Vertices.push_back(InVertex{CubePositions[i], CubeTexCoords[i], CubeNormals[i]});
        }

        rndr::BufferProperties VertexBufferProps;
        VertexBufferProps.BindFlag = rndr::BufferBindFlag::Vertex;
        VertexBufferProps.CPUAccess = rndr::CPUAccess::None;
        VertexBufferProps.Usage = rndr::Usage::GPUReadWrite;
        VertexBufferProps.Size = Vertices.size() * sizeof(InVertex);
        VertexBufferProps.Stride = sizeof(InVertex);
        g_VertexBuffer = g_Context->CreateBuffer(VertexBufferProps, Vertices);
    }

    {
        InInstance Instance;
        rndr::BufferProperties InstanceBufferProps;
        InstanceBufferProps.BindFlag = rndr::BufferBindFlag::Vertex;
        InstanceBufferProps.CPUAccess = rndr::CPUAccess::None;
        InstanceBufferProps.Usage = rndr::Usage::GPUReadWrite;
        InstanceBufferProps.Size = sizeof(InInstance);
        InstanceBufferProps.Stride = sizeof(InInstance);
        g_InstanceBuffer = g_Context->CreateBuffer(InstanceBufferProps, &Instance);
    }

    {
        rndr::BufferProperties IndexBufferProps;
        IndexBufferProps.BindFlag = rndr::BufferBindFlag::Index;
        IndexBufferProps.CPUAccess = rndr::CPUAccess::None;
        IndexBufferProps.Usage = rndr::Usage::GPUReadWrite;
        IndexBufferProps.Size = g_CubeMesh->GetIndices().Size * sizeof(int);
        IndexBufferProps.Stride = sizeof(int);
        g_IndexBuffer = g_Context->CreateBuffer(IndexBufferProps, rndr::ByteSpan(g_CubeMesh->GetIndices()));
    }

    {
        VertexShaderConstants Constants;
        Constants.FromWorldToNDC = math::Transpose(g_Camera->FromWorldToNDC());
        rndr::BufferProperties ConstBufferProps;
        ConstBufferProps.BindFlag = rndr::BufferBindFlag::Constant;
        ConstBufferProps.CPUAccess = rndr::CPUAccess::None;
        ConstBufferProps.Usage = rndr::Usage::GPUReadWrite;
        ConstBufferProps.Size = sizeof(Constants);
        ConstBufferProps.Stride = sizeof(Constants);
        g_VertexConstantBuffer = g_Context->CreateBuffer(ConstBufferProps, &Constants);
    }

    {
        FragmentShaderConstants Constants;
        rndr::BufferProperties ConstBufferProps;
        ConstBufferProps.BindFlag = rndr::BufferBindFlag::Constant;
        ConstBufferProps.CPUAccess = rndr::CPUAccess::None;
        ConstBufferProps.Usage = rndr::Usage::GPUReadWrite;
        ConstBufferProps.Size = sizeof(Constants);
        ConstBufferProps.Stride = sizeof(Constants);
        g_FragmentConstantBuffer = g_Context->CreateBuffer(ConstBufferProps, &Constants);
    }

    {
        const std::string TexturePath = ASSET_DIR "/textures/templategrid_albedo.png";
        rndr::ImageProperties TextureProps;
        TextureProps.ImageBindFlags = rndr::ImageBindFlags::ShaderResource;
        TextureProps.ArraySize = 1;
        TextureProps.bUseMips = true;
        TextureProps.CPUAccess = rndr::CPUAccess::None;
        TextureProps.Usage = rndr::Usage::GPUReadWrite;
        TextureProps.PixelFormat = rndr::PixelFormat::R8G8B8A8_UNORM_SRGB;
        g_DiffuseImage = g_Context->CreateImage(TexturePath.c_str(), TextureProps);
    }

    {
        const std::string TexturePath = ASSET_DIR "/textures/templategrid_normal.png";
        rndr::ImageProperties TextureProps;
        TextureProps.ImageBindFlags = rndr::ImageBindFlags::ShaderResource;
        TextureProps.ArraySize = 1;
        TextureProps.bUseMips = true;
        TextureProps.CPUAccess = rndr::CPUAccess::None;
        TextureProps.Usage = rndr::Usage::GPUReadWrite;
        TextureProps.PixelFormat = rndr::PixelFormat::R8G8B8A8_UNORM_SRGB;
        g_NormalImage = g_Context->CreateImage(TexturePath.c_str(), TextureProps);
    }

    {
        const std::string TexturePath = ASSET_DIR "/textures/templategrid_specular.png";
        rndr::ImageProperties TextureProps;
        TextureProps.ImageBindFlags = rndr::ImageBindFlags::ShaderResource;
        TextureProps.ArraySize = 1;
        TextureProps.bUseMips = true;
        TextureProps.CPUAccess = rndr::CPUAccess::None;
        TextureProps.Usage = rndr::Usage::GPUReadWrite;
        TextureProps.PixelFormat = rndr::PixelFormat::R8G8B8A8_UNORM_SRGB;
        g_SpecularImage = g_Context->CreateImage(TexturePath.c_str(), TextureProps);
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
    delete g_CubeMesh;

    g_Context->DestroyShader(g_VertexShader);
    g_Context->DestroyShader(g_FragmentShader);
    g_Context->DestroyInputLayout(g_InputLayout);
    g_Context->DestroyRasterizerState(g_RasterizerState);
    g_Context->DestroyDepthStencilState(g_DepthStencilState);
    g_Context->DestroyBlendState(g_BlendState);

    g_Context->DestroyBuffer(g_VertexBuffer);
    g_Context->DestroyBuffer(g_InstanceBuffer);
    g_Context->DestroyBuffer(g_IndexBuffer);
    g_Context->DestroyBuffer(g_VertexConstantBuffer);
    g_Context->DestroyBuffer(g_FragmentConstantBuffer);

    g_Context->DestroyImage(g_DiffuseImage);
    g_Context->DestroyImage(g_NormalImage);
    g_Context->DestroyImage(g_SpecularImage);
    g_Context->DestroySampler(g_Sampler);

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
        InInstance Instance;
        Instance.FromModelToWorld = math::Translate(rndr::Vector3r(0, 0, 20)) * math::Scale(5, 5, 5);
        Instance.FromWorldToModel = Instance.FromModelToWorld.GetInverse();
        Instance.FromModelToWorld = math::Transpose(Instance.FromModelToWorld);
        Instance.FromWorldToModel = math::Transpose(Instance.FromWorldToModel); 
        g_InstanceBuffer->Update(&Instance);
    }

    // Update vertex shader constants
    {
        VertexShaderConstants Constants;
        Constants.FromWorldToNDC = math::Transpose(g_Camera->FromWorldToNDC());
        g_VertexConstantBuffer->Update(&Constants);
    }

    // Update fragment shader constants
    {
        FragmentShaderConstants Constants;
        g_FragmentConstantBuffer->Update(&Constants);
    }
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

    g_Context->BindBuffer(g_VertexBuffer, 0);
    g_Context->BindBuffer(g_InstanceBuffer, 1);
    g_Context->BindBuffer(g_IndexBuffer, 0);

    g_Context->BindBuffer(g_VertexConstantBuffer, 0, g_VertexShader);
    g_Context->BindBuffer(g_FragmentConstantBuffer, 0, g_FragmentShader);
    g_Context->BindImageAsShaderResource(g_DiffuseImage, 0, g_FragmentShader);
    g_Context->BindImageAsShaderResource(g_NormalImage, 1, g_FragmentShader);
    g_Context->BindImageAsShaderResource(g_SpecularImage, 2, g_FragmentShader);
    g_Context->BindSampler(g_Sampler, 0, g_FragmentShader);

    g_Context->BindFrameBuffer(g_Context->GetWindowFrameBuffer());

    g_Context->DrawIndexed(rndr::PrimitiveTopology::TriangleList, g_CubeMesh->GetIndices().Size);
}

void Present(bool bVSync)
{
    rndr::GraphicsContext* GC = g_App->GetWindow()->GetGraphicsContext();
    GC->Present(bVSync);
}
