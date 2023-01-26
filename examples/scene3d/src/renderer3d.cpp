#include "renderer3d.h"

#include "math/projections.h"

Renderer::Renderer(rndr::GraphicsContext* Ctx,
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
      m_ScreenHeight(ScreenHeight),
      m_Camera(math::Transform{}, ScreenWidth, ScreenHeight, rndr::ProjectionCameraProperties{})
{
    assert(m_MaxVertices > 0);
    assert(m_MaxFaces > 0);
    assert(m_MaxInstances > 0);

    const std::string VertexShaderPath = SCENE3D_ASSET_DIR "/scene3dvert.hlsl";
    const std::string FragmentShaderPath = SCENE3D_ASSET_DIR "/scene3dfrag.hlsl";

    rndr::ByteArray VertexShaderContents = rndr::file::ReadEntireFile(VertexShaderPath);
    assert(!VertexShaderContents.empty());
    rndr::ByteArray FragmentShaderContents = rndr::file::ReadEntireFile(FragmentShaderPath);
    assert(!FragmentShaderContents.empty());

    const rndr::PipelineProperties PipelineProps{
        .InputLayout = rndr::InputLayoutBuilder()
                           .AddBuffer(0, rndr::DataRepetition::PerVertex, 1)
                           .AddBuffer(1, rndr::DataRepetition::PerInstance, 1)
                           .AppendElement(0, "POSITION", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(0, "COLOR", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(0, "NORMAL", rndr::PixelFormat::R32G32B32_FLOAT)
                           .AppendElement(1, "ROWX", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWY", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWZ", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWW", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWX", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWY", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWZ", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(1, "ROWW", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .Build(),
        .VertexShader = {.Type = rndr::ShaderType::Vertex, .EntryPoint = "Main"},
        .VertexShaderContents = rndr::ByteSpan(VertexShaderContents),
        .PixelShader = {.Type = rndr::ShaderType::Fragment, .EntryPoint = "Main"},
        .PixelShaderContents = rndr::ByteSpan(FragmentShaderContents),
        .Rasterizer = {.FrontFaceWindingOrder = rndr::WindingOrder::CCW,
                       .CullFace = rndr::Face::None},
        .DepthStencil = {.DepthEnable = true, .StencilEnable = false},
    };
    m_Pipeline = m_Ctx->CreatePipeline(PipelineProps);
    assert(m_Pipeline.IsValid());

    rndr::BufferProperties BufferProps;
    BufferProps.Size = m_MaxVertices * sizeof(VertexData);
    BufferProps.Stride = sizeof(VertexData);
    BufferProps.Type = rndr::BufferType::Vertex;
    m_VertexBuffer = m_Ctx->CreateBuffer(BufferProps, rndr::ByteSpan{});

    BufferProps.Size = m_MaxInstances * sizeof(InstanceData);
    BufferProps.Stride = sizeof(InstanceData);
    BufferProps.Type = rndr::BufferType::Vertex;
    m_InstanceBuffer = m_Ctx->CreateBuffer(BufferProps, rndr::ByteSpan{});
    assert(m_InstanceBuffer.IsValid());

    m_Camera.SetScreenSize(m_ScreenWidth, m_ScreenHeight);
    ConstantData ConstData;
    ConstData.WorldToNDC = math::Transpose(m_Camera.FromWorldToNDC()).GetMatrix();
    ConstData.CameraPositionWorld = math::Point3{};
    ConstData.Shininess = m_Shininess;
    BufferProps.Size = sizeof(ConstantData);
    BufferProps.Stride = sizeof(ConstantData);
    BufferProps.Type = rndr::BufferType::Constant;
    m_ConstantBuffer = m_Ctx->CreateBuffer(BufferProps, rndr::ByteSpan{&ConstData});
    assert(m_ConstantBuffer.IsValid());

    BufferProps.Size = m_MaxFaces * 3 * sizeof(int32_t);
    BufferProps.Stride = sizeof(int32_t);
    BufferProps.Type = rndr::BufferType::Index;
    m_IndexBuffer = m_Ctx->CreateBuffer(BufferProps, rndr::ByteSpan{});
    assert(m_IndexBuffer.IsValid());
}

void Renderer::SetScreenSize(int Width, int Height)
{
    m_ScreenWidth = Width;
    m_ScreenHeight = Height;
    m_Camera.SetScreenSize(Width, Height);

    ConstantData ConstData;
    ConstData.WorldToNDC = math::Transpose(m_Camera.FromWorldToNDC()).GetMatrix();
    ConstData.CameraPositionWorld = math::Point3{0, 0, 0};
    ConstData.Shininess = m_Shininess;
    m_ConstantBuffer->Update(m_Ctx, rndr::ByteSpan{&ConstData});
}

void Renderer::SetRenderTarget(rndr::FrameBuffer& Target)
{
    m_Target = &Target;
}

void Renderer::SetShininess(float Shininess)
{
    m_Shininess = Shininess;

    ConstantData ConstData;
    ConstData.WorldToNDC = math::Transpose(m_Camera.FromWorldToNDC()).GetMatrix();
    ConstData.CameraPositionWorld = math::Point3{0, 0, 0};
    ConstData.Shininess = m_Shininess;
    m_ConstantBuffer->Update(m_Ctx, rndr::ByteSpan{&ConstData});
}

void Renderer::RenderModel(rndr::Model& Model, const rndr::Span<math::Transform>& Instances)
{
    assert(Model.Positions.size() <= m_MaxVertices);
    assert(Model.Indices.size() <= m_MaxFaces * 3);
    assert(Instances.Size <= m_MaxInstances);

    rndr::Array<VertexData> Vertices(Model.Positions.size());
    for (int VertexIndex = 0; VertexIndex < Model.Positions.size(); VertexIndex++)
    {
        Vertices[VertexIndex].Position = math::Point4{Model.Positions[VertexIndex]};
        Vertices[VertexIndex].Color = rndr::Colors::kRed;
        Vertices[VertexIndex].Normal = Model.Normals[VertexIndex];
    }
    m_VertexBuffer->Update(m_Ctx, rndr::ByteSpan{Vertices});

    m_IndexBuffer->Update(m_Ctx, rndr::ByteSpan{Model.Indices});

    rndr::Array<InstanceData> InstancesTransposed(Instances.Size);
    for (int InstanceIndex = 0; InstanceIndex < Instances.Size; InstanceIndex++)
    {
        InstancesTransposed[InstanceIndex].ObjectToWorld =
            math::Transpose(Instances[InstanceIndex]).GetMatrix();
        // Here we need to do a transpose of transpose of inverse, but two transposes cancel each
        // other
        InstancesTransposed[InstanceIndex].NormalTransform = Instances[InstanceIndex].GetInverse();
    }
    m_InstanceBuffer->Update(m_Ctx, rndr::ByteSpan{InstancesTransposed});

    m_Ctx->BindBuffer(m_ConstantBuffer.Get(), 0, m_Pipeline->VertexShader.Get());
    m_Ctx->BindBuffer(m_ConstantBuffer.Get(), 0, m_Pipeline->PixelShader.Get());
    m_Ctx->BindBuffer(m_VertexBuffer.Get(), 0);
    m_Ctx->BindBuffer(m_InstanceBuffer.Get(), 1);
    m_Ctx->BindBuffer(m_IndexBuffer.Get(), 0);
    m_Ctx->BindPipeline(m_Pipeline.Get());
    m_Ctx->BindFrameBuffer(m_Target);

    const int32_t IndexCount = static_cast<int32_t>(Model.Indices.size());
    const int32_t InstanceCount = static_cast<int32_t>(Instances.Size);
    m_Ctx->DrawIndexedInstanced(rndr::PrimitiveTopology::TriangleList, IndexCount, InstanceCount);
}
