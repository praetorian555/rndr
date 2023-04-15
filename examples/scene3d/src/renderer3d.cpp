#include "renderer3d.h"

#include "math/projections.h"

Renderer::Renderer(Rndr::GraphicsContext* Ctx,
                   int32_t MaxVertices,
                   int32_t MaxFaces,
                   int32_t MaxInstances,
                   int32_t ScreenWidth,
                   int32_t ScreenHeight)
    : m_Ctx(Ctx),
      m_MaxVertices(MaxVertices),
      m_MaxFaces(MaxFaces),
      m_MaxInstances(MaxInstances),
      m_ScreenWidth(ScreenWidth),
      m_ScreenHeight(ScreenHeight)
{
    assert(m_MaxVertices > 0);
    assert(m_MaxFaces > 0);
    assert(m_MaxInstances > 0);

    const std::string VertexShaderPath = SCENE3D_ASSET_DIR "/scene3dvert.hlsl";
    const std::string FragmentShaderPath = SCENE3D_ASSET_DIR "/scene3dfrag.hlsl";

    Rndr::ByteArray VertexShaderContents = Rndr::file::ReadEntireFile(VertexShaderPath);
    assert(!VertexShaderContents.empty());
    Rndr::ByteArray FragmentShaderContents = Rndr::file::ReadEntireFile(FragmentShaderPath);
    assert(!FragmentShaderContents.empty());

    const Rndr::PipelineProperties PipelineProps{
        .InputLayout = Rndr::InputLayoutBuilder()
                           .AddBuffer(0, Rndr::DataRepetition::PerVertex, 1)
                           .AddBuffer(1, Rndr::DataRepetition::PerInstance, 1)
                           .AppendElement(0, "POSITION", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(0, "COLOR", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(0, "NORMAL", Rndr::PixelFormat::R32G32B32_FLOAT)
                           .AppendElement(1, "ROWX", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWY", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWZ", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWW", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWX", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWY", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWZ", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWW", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .Build(),
        .VertexShader = {.Type = Rndr::ShaderType::Vertex, .EntryPoint = "Main"},
        .VertexShaderContents = Rndr::ByteSpan(VertexShaderContents),
        .PixelShader = {.Type = Rndr::ShaderType::Fragment, .EntryPoint = "Main"},
        .PixelShaderContents = Rndr::ByteSpan(FragmentShaderContents),
        .Rasterizer = {.FrontFaceWindingOrder = Rndr::WindingOrder::CCW,
                       .CullFace = Rndr::Face::None},
        .DepthStencil = {.DepthEnable = true, .StencilEnable = false},
    };
    m_Pipeline = m_Ctx->CreatePipeline(PipelineProps);
    assert(m_Pipeline.IsValid());

    Rndr::BufferProperties BufferProps;
    BufferProps.Size = m_MaxVertices * sizeof(VertexData);
    BufferProps.Stride = sizeof(VertexData);
    BufferProps.Type = Rndr::BufferType::Vertex;
    m_VertexBuffer = m_Ctx->CreateBuffer(BufferProps, Rndr::ByteSpan{});

    BufferProps.Size = m_MaxInstances * sizeof(InstanceData);
    BufferProps.Stride = sizeof(InstanceData);
    BufferProps.Type = Rndr::BufferType::Vertex;
    m_InstanceBuffer = m_Ctx->CreateBuffer(BufferProps, Rndr::ByteSpan{});
    assert(m_InstanceBuffer.IsValid());

    BufferProps.Size = sizeof(ConstantData);
    BufferProps.Stride = sizeof(ConstantData);
    BufferProps.Type = Rndr::BufferType::Constant;
    m_ConstantBuffer = m_Ctx->CreateBuffer(BufferProps, Rndr::ByteSpan{});
    assert(m_ConstantBuffer.IsValid());

    BufferProps.Size = m_MaxFaces * 3 * sizeof(int32_t);
    BufferProps.Stride = sizeof(int32_t);
    BufferProps.Type = Rndr::BufferType::Index;
    m_IndexBuffer = m_Ctx->CreateBuffer(BufferProps, Rndr::ByteSpan{});
    assert(m_IndexBuffer.IsValid());
}

void Renderer::SetScreenSize(int Width, int Height)
{
    m_ScreenWidth = Width;
    m_ScreenHeight = Height;
    m_Camera->SetScreenSize(Width, Height);

    ConstantData ConstData;
    ConstData.WorldToNDC = math::Transpose(m_Camera->FromWorldToNDC()).GetMatrix();
    ConstData.CameraPositionWorld = math::Point3{0, 0, 0};
    ConstData.Shininess = m_Shininess;
    m_ConstantBuffer->Update(m_Ctx, Rndr::ByteSpan{&ConstData});
}

void Renderer::SetRenderTarget(Rndr::FrameBuffer& Target)
{
    m_Target = &Target;
}

void Renderer::SetShininess(float Shininess)
{
    m_Shininess = Shininess;

    ConstantData ConstData;
    ConstData.WorldToNDC = math::Transpose(m_Camera->FromWorldToNDC()).GetMatrix();
    ConstData.CameraPositionWorld = math::Point3{0, 0, 0};
    ConstData.Shininess = m_Shininess;
    m_ConstantBuffer->Update(m_Ctx, Rndr::ByteSpan{&ConstData});
}

void Renderer::SetProjectionCamera(Rndr::ProjectionCamera* Camera)
{
    m_Camera = Camera;

    ConstantData ConstData;
    ConstData.WorldToNDC = math::Transpose(m_Camera->FromWorldToNDC()).GetMatrix();
    ConstData.CameraPositionWorld = math::Point3{0, 0, 0};
    ConstData.Shininess = m_Shininess;
    m_ConstantBuffer->Update(m_Ctx, Rndr::ByteSpan{&ConstData});
}

void Renderer::RenderModel(Rndr::Model& Model, const Rndr::Span<math::Transform>& Instances)
{
    assert(Model.Positions.size() <= m_MaxVertices);
    assert(Model.Indices.size() <= m_MaxFaces * 3);
    assert(Instances.Size <= m_MaxInstances);

    Rndr::Array<VertexData> Vertices(Model.Positions.size());
    for (int VertexIndex = 0; VertexIndex < Model.Positions.size(); VertexIndex++)
    {
        Vertices[VertexIndex].Position = math::Point4{Model.Positions[VertexIndex]};
        Vertices[VertexIndex].Color = Rndr::Colors::kRed;
        Vertices[VertexIndex].Normal = Model.Normals[VertexIndex];
    }
    m_VertexBuffer->Update(m_Ctx, Rndr::ByteSpan{Vertices});

    m_IndexBuffer->Update(m_Ctx, Rndr::ByteSpan{Model.Indices});

    Rndr::Array<InstanceData> InstancesTransposed(Instances.Size);
    for (int InstanceIndex = 0; InstanceIndex < Instances.Size; InstanceIndex++)
    {
        InstancesTransposed[InstanceIndex].ObjectToWorld =
            math::Transpose(Instances[InstanceIndex]).GetMatrix();
        // Here we need to do a transpose of transpose of inverse, but two transposes cancel each
        // other
        InstancesTransposed[InstanceIndex].NormalTransform = Instances[InstanceIndex].GetInverse();
    }
    m_InstanceBuffer->Update(m_Ctx, Rndr::ByteSpan{InstancesTransposed});

    m_Ctx->BindBuffer(m_ConstantBuffer.Get(), 0, m_Pipeline->VertexShader.Get());
    m_Ctx->BindBuffer(m_ConstantBuffer.Get(), 0, m_Pipeline->PixelShader.Get());
    m_Ctx->BindBuffer(m_VertexBuffer.Get(), 0);
    m_Ctx->BindBuffer(m_InstanceBuffer.Get(), 1);
    m_Ctx->BindBuffer(m_IndexBuffer.Get(), 0);
    m_Ctx->BindPipeline(m_Pipeline.Get());
    m_Ctx->BindFrameBuffer(m_Target);

    const int32_t IndexCount = static_cast<int32_t>(Model.Indices.size());
    const int32_t InstanceCount = static_cast<int32_t>(Instances.Size);
    m_Ctx->DrawIndexedInstanced(Rndr::PrimitiveTopology::TriangleList, IndexCount, InstanceCount);
}
