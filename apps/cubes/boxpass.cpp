#include "boxpass.h"

#include "math/rng.h"

#include "rndr/core/debug.h"

void BoxRenderPass::Init(rndr::GraphicsContext* GraphicsContext, rndr::ProjectionCamera* Camera)
{
    m_GraphicsContext = GraphicsContext;
    m_Camera = Camera;

    rndr::ShaderProperties VertexShaderProps;
    VertexShaderProps.bCompilationNeeded = true;
    VertexShaderProps.Type = rndr::ShaderType::Vertex;
    VertexShaderProps.FilePath = RNDR_ASSET_DIR L"/shaders/PhongVertexShader.hlsl";
    VertexShaderProps.EntryPoint = "PhongVertexShader";
    m_VertexShader = m_GraphicsContext->CreateShader(VertexShaderProps);

    rndr::ShaderProperties FragmentShaderProps;
    FragmentShaderProps.bCompilationNeeded = true;
    FragmentShaderProps.Type = rndr::ShaderType::Fragment;
    FragmentShaderProps.FilePath = RNDR_ASSET_DIR L"/shaders/PhongFragmentShader.hlsl";
    FragmentShaderProps.EntryPoint = "PhongFragmentShader";
    m_FragmentShader = m_GraphicsContext->CreateShader(FragmentShaderProps);

    rndr::InputLayoutProperties ILProps[7];
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
    ILProps[3].SemanticName = "ROWX";
    ILProps[3].SemanticIndex = 0;
    ILProps[3].InputSlot = 1;
    ILProps[3].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
    ILProps[3].OffsetInVertex = 0;
    ILProps[3].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[3].InstanceStepRate = 1;
    ILProps[4].SemanticName = "ROWY";
    ILProps[4].SemanticIndex = 0;
    ILProps[4].InputSlot = 1;
    ILProps[4].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
    ILProps[4].OffsetInVertex = rndr::AppendAlignedElement;
    ILProps[4].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[4].InstanceStepRate = 1;
    ILProps[5].SemanticName = "ROWZ";
    ILProps[5].SemanticIndex = 0;
    ILProps[5].InputSlot = 1;
    ILProps[5].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
    ILProps[5].OffsetInVertex = rndr::AppendAlignedElement;
    ILProps[5].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[5].InstanceStepRate = 1;
    ILProps[6].SemanticName = "ROWW";
    ILProps[6].SemanticIndex = 0;
    ILProps[6].InputSlot = 1;
    ILProps[6].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
    ILProps[6].OffsetInVertex = rndr::AppendAlignedElement;
    ILProps[6].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[6].InstanceStepRate = 1;
    m_InputLayout = m_GraphicsContext->CreateInputLayout(rndr::Span(ILProps, 7), m_VertexShader);

    rndr::RasterizerProperties RasterProps;
    RasterProps.bAntialiasedLineEnable = false;
    RasterProps.bDepthClipEnable = true;
    RasterProps.bMultisampleEnable = false;
    RasterProps.bScissorEnable = false;
    RasterProps.CullFace = rndr::Face::Back;
    RasterProps.FrontFaceWindingOrder = rndr::WindingOrder::CW;
    RasterProps.FillMode = rndr::FillMode::Solid;
    m_RasterizerState = m_GraphicsContext->CreateRasterizerState(RasterProps);

    rndr::DepthStencilProperties DSProps;
    DSProps.bDepthEnable = true;
    DSProps.bStencilEnable = false;
    DSProps.DepthComparator = rndr::Comparator::Less;
    DSProps.DepthMask = rndr::DepthMask::All;
    m_DepthStencilState = m_GraphicsContext->CreateDepthStencilState(DSProps);

    rndr::BlendProperties BlendProps;
    BlendProps.bBlendEnable = true;
    BlendProps.SrcColorFactor = rndr::BlendFactor::SrcAlpha;
    BlendProps.DstColorFactor = rndr::BlendFactor::OneMinusSrcAlpha;
    BlendProps.ColorOperator = rndr::BlendOperator::Add;
    BlendProps.SrcAlphaFactor = rndr::BlendFactor::One;
    BlendProps.DstAlphaFactor = rndr::BlendFactor::OneMinusSrcAlpha;
    BlendProps.AlphaOperator = rndr::BlendOperator::Add;
    m_BlendState = m_GraphicsContext->CreateBlendState(BlendProps);

    struct InVertex
    {
        math::Point3 Position;
        math::Vector2 TexCoords;
        math::Normal3 Normal;
    };

    std::string CubeObjPath = RNDR_ASSET_DIR "/models/cube.obj";
    m_Mesh = rndr::ObjParser::Parse(CubeObjPath)->GetMeshes()[0];

    std::vector<InVertex> Vertices;
    for (int i = 0; i < m_Mesh->GetPositions().Size; i++)
    {
        Vertices.push_back(InVertex{m_Mesh->GetPositions()[i], m_Mesh->GetTexCoords()[i], m_Mesh->GetNormals()[i]});
    }

    rndr::BufferProperties VertexBufferProps;
    VertexBufferProps.BindFlag = rndr::BufferBindFlag::Vertex;
    VertexBufferProps.CPUAccess = rndr::CPUAccess::None;
    VertexBufferProps.Usage = rndr::Usage::GPUReadWrite;
    VertexBufferProps.Size = Vertices.size() * sizeof(Vertices);
    VertexBufferProps.Stride = sizeof(InVertex);
    m_VertexBuffer = m_GraphicsContext->CreateBuffer(VertexBufferProps, (rndr::ByteSpan)Vertices);

    math::RNG RandomGen;
    const int BoxCount = 1000;
    m_Instances.resize(BoxCount);

    for (int i = 0; i < BoxCount; i++)
    {
        const real ScaleFactor = RandomGen.UniformRealInRange(0.8, 2.0);
        math::Rotator Angles;
        Angles.Pitch = RandomGen.UniformRealInRange(-90, 90);
        Angles.Yaw = RandomGen.UniformRealInRange(-90, 90);
        Angles.Roll = RandomGen.UniformRealInRange(-90, 90);
        math::Vector3 Position{0, 0, 10};
        Position.X = RandomGen.UniformRealInRange(-100, 100);
        Position.Y = RandomGen.UniformRealInRange(-100, 100);
        Position.Z = RandomGen.UniformRealInRange(30, 60);

        m_Instances[i].FromModelToWorld =
            math::Translate(Position) * math::Rotate(Angles) * math::Scale(ScaleFactor, ScaleFactor, ScaleFactor);
        m_Instances[i].FromModelToWorld = math::Transpose(m_Instances[i].FromModelToWorld);
    }

    rndr::BufferProperties InstanceBufferProps;
    InstanceBufferProps.BindFlag = rndr::BufferBindFlag::Vertex;
    InstanceBufferProps.CPUAccess = rndr::CPUAccess::None;
    InstanceBufferProps.Usage = rndr::Usage::GPUReadWrite;
    InstanceBufferProps.Size = m_Instances.size() * sizeof(InInstance);
    InstanceBufferProps.Stride = sizeof(InInstance);
    m_InstanceBuffer = m_GraphicsContext->CreateBuffer(InstanceBufferProps, (rndr::ByteSpan)m_Instances);
    m_InstanceCount = m_Instances.size();

    rndr::IntSpan Indices = m_Mesh->GetIndices();
    rndr::BufferProperties IndexBufferProps;
    IndexBufferProps.BindFlag = rndr::BufferBindFlag::Index;
    IndexBufferProps.CPUAccess = rndr::CPUAccess::None;
    IndexBufferProps.Usage = rndr::Usage::GPUReadWrite;
    IndexBufferProps.Size = Indices.Size * sizeof(int);
    IndexBufferProps.Stride = sizeof(int);
    m_IndexBuffer = m_GraphicsContext->CreateBuffer(IndexBufferProps, rndr::ByteSpan(Indices));
    m_IndexCount = Indices.Size;

    ShaderConstants Constants;
    Constants.FromWorldToNDC = math::Transpose(m_Camera->FromWorldToNDC());
    Constants.LightPosition = m_LightPosition;
    Constants.ViewerPosition = m_ViewerPosition;
    rndr::BufferProperties ConstBufferProps;
    ConstBufferProps.BindFlag = rndr::BufferBindFlag::Constant;
    ConstBufferProps.CPUAccess = rndr::CPUAccess::None;
    ConstBufferProps.Usage = rndr::Usage::GPUReadWrite;
    ConstBufferProps.Size = sizeof(Constants);
    ConstBufferProps.Stride = sizeof(Constants);
    m_ConstantBuffer = m_GraphicsContext->CreateBuffer(ConstBufferProps, (rndr::ByteSpan)&Constants);

    const std::string WallTexturePath = RNDR_ASSET_DIR "/textures/bricked-wall.png";
    rndr::CPUImage ImageInfo = rndr::ReadEntireImage(WallTexturePath);
    rndr::ImageProperties TextureProps;
    TextureProps.ImageBindFlags = rndr::ImageBindFlags::ShaderResource;
    TextureProps.ArraySize = 1;
    TextureProps.bUseMips = true;
    TextureProps.CPUAccess = rndr::CPUAccess::None;
    TextureProps.Usage = rndr::Usage::GPUReadWrite;
    TextureProps.PixelFormat = ImageInfo.Format;
    m_Texture = m_GraphicsContext->CreateImage(ImageInfo.Width, ImageInfo.Height, TextureProps, ImageInfo.Data);

    rndr::SamplerProperties SamplerProps;
    SamplerProps.AddressingU = rndr::ImageAddressing::Repeat;
    SamplerProps.AddressingV = rndr::ImageAddressing::Repeat;
    SamplerProps.AddressingW = rndr::ImageAddressing::Repeat;
    SamplerProps.Filter = rndr::ImageFiltering::MinMagMipLinear;
    SamplerProps.LODBias = 0;
    m_Sampler = m_GraphicsContext->CreateSampler(SamplerProps);
}

void BoxRenderPass::ShutDown()
{
    m_GraphicsContext->DestroyShader(m_VertexShader);
    m_GraphicsContext->DestroyShader(m_FragmentShader);
    m_GraphicsContext->DestroyInputLayout(m_InputLayout);
    m_GraphicsContext->DestroyRasterizerState(m_RasterizerState);
    m_GraphicsContext->DestroyDepthStencilState(m_DepthStencilState);
    m_GraphicsContext->DestroyBlendState(m_BlendState);

    m_GraphicsContext->DestroyBuffer(m_VertexBuffer);
    m_GraphicsContext->DestroyBuffer(m_InstanceBuffer);
    m_GraphicsContext->DestroyBuffer(m_IndexBuffer);
    m_GraphicsContext->DestroyBuffer(m_ConstantBuffer);

    m_GraphicsContext->DestroyImage(m_Texture);
    m_GraphicsContext->DestroySampler(m_Sampler);
}

void BoxRenderPass::Render(real DeltaSeconds)
{
    m_GraphicsContext->ClearColor(nullptr, rndr::Colors::Blue);
    m_GraphicsContext->ClearDepth(nullptr, 1);

    m_GraphicsContext->BindShader(m_VertexShader);
    m_GraphicsContext->BindShader(m_FragmentShader);
    m_GraphicsContext->BindInputLayout(m_InputLayout);
    m_GraphicsContext->BindRasterizerState(m_RasterizerState);
    m_GraphicsContext->BindDepthStencilState(m_DepthStencilState);
    m_GraphicsContext->BindBlendState(m_BlendState);

    m_GraphicsContext->BindBuffer(m_VertexBuffer, 0);
    m_GraphicsContext->BindBuffer(m_InstanceBuffer, 1);
    m_GraphicsContext->BindBuffer(m_IndexBuffer, 0);

    m_GraphicsContext->BindBuffer(m_ConstantBuffer, 0, m_VertexShader);
    m_GraphicsContext->BindBuffer(m_ConstantBuffer, 0, m_FragmentShader);
    ShaderConstants Constants;
    Constants.FromWorldToNDC = math::Transpose(m_Camera->FromWorldToNDC());
    Constants.LightPosition = m_LightPosition;
    Constants.ViewerPosition = m_ViewerPosition;
    m_ConstantBuffer->Update((rndr::ByteSpan)&Constants);

    m_GraphicsContext->BindImageAsShaderResource(m_Texture, 0, m_FragmentShader);
    m_GraphicsContext->BindSampler(m_Sampler, 0, m_FragmentShader);

    m_GraphicsContext->BindFrameBuffer(m_GraphicsContext->GetWindowFrameBuffer());

    m_GraphicsContext->DrawIndexedInstanced(rndr::PrimitiveTopology::TriangleList, m_IndexCount, m_InstanceCount);
}

void BoxRenderPass::SetLightPosition(math::Point3 LightPosition)
{
    m_LightPosition = LightPosition;
}

void BoxRenderPass::SetViewerPosition(math::Point3 ViewerPosition)
{
    m_ViewerPosition = ViewerPosition;
}
