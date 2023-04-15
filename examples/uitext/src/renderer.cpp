#include "renderer.h"

namespace SDF
{
constexpr int Padding = 5;
constexpr uint8_t OneEdgeValue = 180;
constexpr float PixelDistScale = static_cast<float>(OneEdgeValue) / static_cast<float>(Padding);
}  // namespace SDF

Renderer::Renderer(Rndr::GraphicsContext* Ctx,
                   int32_t MaxInstances,
                   const math::Vector2& ScreenSize)
    : m_Ctx(Ctx),
      m_MaxInstances(MaxInstances),
      m_ScreenSize(ScreenSize),
      m_Packer(AtlasWidth, AtlasHeight, AtlasPacker::SortCriteria::Height)
{
    m_Atlas.resize(AtlasWidth * AtlasHeight);
    memset(m_Atlas.data(), 0, m_Atlas.size());

    const std::string VertexShaderPath = BASIC2D_ASSET_DIR "/textvert.hlsl";
    const std::string FragmentShaderPath = BASIC2D_ASSET_DIR "/textfrag.hlsl";

    Rndr::ByteArray VertexShaderContents = Rndr::file::ReadEntireFile(VertexShaderPath);
    assert(!VertexShaderContents.empty());
    Rndr::ByteArray FragmentShaderContents = Rndr::file::ReadEntireFile(FragmentShaderPath);
    assert(!FragmentShaderContents.empty());

    const Rndr::PipelineProperties PipelineProps{
        .InputLayout = Rndr::InputLayoutBuilder()
                           .AddBuffer(0, Rndr::DataRepetition::PerInstance, 1)
                           .AppendElement(0, "POSITION", Rndr::PixelFormat::R32G32_FLOAT)
                           .AppendElement(0, "POSITION", Rndr::PixelFormat::R32G32_FLOAT)
                           .AppendElement(0, "TEXCOORD", Rndr::PixelFormat::R32G32_FLOAT)
                           .AppendElement(0, "TEXCOORD", Rndr::PixelFormat::R32G32_FLOAT)
                           .AppendElement(0, "COLOR", Rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(0, "PSIZE", Rndr::PixelFormat::R32_FLOAT)
                           .AppendElement(0, "PSIZE", Rndr::PixelFormat::R32_FLOAT)
                           .Build(),
        .VertexShader = {.Type = Rndr::ShaderType::Vertex, .EntryPoint = "Main"},
        .VertexShaderContents = Rndr::ByteSpan(VertexShaderContents),
        .PixelShader = {.Type = Rndr::ShaderType::Fragment, .EntryPoint = "Main"},
        .PixelShaderContents = Rndr::ByteSpan(FragmentShaderContents),
        .DepthStencil = {.DepthEnable = false, .StencilEnable = false},
    };
    m_Pipeline = m_Ctx->CreatePipeline(PipelineProps);
    assert(m_Pipeline.IsValid());

    Rndr::BufferProperties BufferProps;
    BufferProps.Size = m_MaxInstances * sizeof(InstanceData);
    BufferProps.Stride = sizeof(InstanceData);
    BufferProps.Type = Rndr::BufferType::Vertex;
    m_InstanceBuffer = m_Ctx->CreateBuffer(BufferProps, Rndr::ByteSpan{});
    assert(m_InstanceBuffer.IsValid());

    BufferProps.Size = m_MaxInstances * sizeof(InstanceData);
    BufferProps.Stride = sizeof(InstanceData);
    BufferProps.Type = Rndr::BufferType::Vertex;
    m_ShadowBuffer = m_Ctx->CreateBuffer(BufferProps, Rndr::ByteSpan{});
    assert(m_ShadowBuffer.IsValid());

    ConstantData ConstData{m_ScreenSize};
    BufferProps.Size = sizeof(ConstantData);
    BufferProps.Stride = sizeof(ConstantData);
    BufferProps.Type = Rndr::BufferType::Constant;
    m_ConstantBuffer = m_Ctx->CreateBuffer(BufferProps, Rndr::ByteSpan{&ConstData});
    assert(m_ConstantBuffer.IsValid());

    std::vector Indices{0, 1, 2, 1, 3, 2};
    BufferProps.Size = static_cast<uint32_t>(Indices.size() * sizeof(int32_t));
    BufferProps.Stride = sizeof(int32_t);
    BufferProps.Type = Rndr::BufferType::Index;
    m_IndexBuffer = m_Ctx->CreateBuffer(BufferProps, Rndr::ByteSpan{Indices});
    assert(m_IndexBuffer.IsValid());

    Rndr::ImageProperties AtlasProps;
    AtlasProps.PixelFormat = Rndr::PixelFormat::R8_UNORM;
    AtlasProps.ImageBindFlags = Rndr::ImageBindFlags::ShaderResource;
    std::vector<uint8_t> InitTextureData(AtlasWidth * AtlasHeight * Rndr::GetPixelSize(AtlasProps.PixelFormat));
    memset(InitTextureData.data(), 0x00, InitTextureData.size());
    m_TextureAtlas =
        m_Ctx->CreateImage(AtlasWidth, AtlasHeight, AtlasProps, Rndr::ByteSpan(InitTextureData));
    assert(m_TextureAtlas.IsValid());

    Rndr::SamplerProperties SamplerProps;
    SamplerProps.AddressingU = Rndr::ImageAddressing::Clamp;
    SamplerProps.AddressingV = Rndr::ImageAddressing::Clamp;
    SamplerProps.AddressingW = Rndr::ImageAddressing::Clamp;
    SamplerProps.Filter = Rndr::ImageFiltering::MinMagMipLinear;
    m_TextureAtlasSampler = m_Ctx->CreateSampler(SamplerProps);
    assert(m_TextureAtlasSampler.IsValid());

    m_Instances.reserve(m_MaxInstances);
}

bool Renderer::AddFont(const std::string& FontName, const std::string& AssetPath)
{
    Font* NewFont = new Font{};
    if (!NewFont->Init(FontName, AssetPath))
    {
        return false;
    }
    m_Fonts.insert(std::make_pair(FontName, NewFont));
    m_IdToFonts.insert(std::make_pair(NewFont->Id, NewFont));

    return true;
}

void Renderer::RenderText(const std::string& Text,
                          const std::string& FontName,
                          int FontSize,
                          const math::Point2 BaseLineStart,
                          const TextProperties& Props)
{
    if (Text.empty())
    {
        RNDR_LOG_TRACE("Empty string, nothing to do");
        return;
    }
    auto FontIter = m_Fonts.find(FontName);
    if (FontIter == m_Fonts.end())
    {
        RNDR_LOG_ERROR("Font name not recognized, make sure you add it through AddFont API!");
        return;
    }
    if (FontSize >= 1024)
    {
        RNDR_LOG_ERROR("Invalid font size, value above 1024 pixels are not supported!");
        return;
    }

    Font* F = FontIter->second;

    if (!IsGlyphSupported(Text[0], F, FontSize))
    {
        // We don't have glyphs for this combo, add them to atlas
        constexpr int ASCIICodePointStart = 32;
        constexpr int ASCIICodePointEnd = 127;
        UpdateAtlas(ASCIICodePointStart, ASCIICodePointEnd, F, FontSize);
    }

    math::Point2 Cursor = BaseLineStart;
    for (int i = 0; i < Text.size(); i++)
    {
        const char& CodePoint = Text[i];
        const int GlyphHash = Font::MakeGlyphHash(CodePoint, FontSize, F->Id);
        Glyph& G = m_Glyphs[GlyphHash];

        int AdvanceWidth;
        int LSB;
        stbtt_GetCodepointHMetrics(&F->TTInfo, CodePoint, &AdvanceWidth, &LSB);

        InstanceData ShadowQuad;
        ShadowQuad.ThresholdBottom = Props.ShadowThresholdBottom;
        ShadowQuad.ThresholdTop = Props.ShadowThresholdTop;
        ShadowQuad.Color = Props.ShadowColor;
        ShadowQuad.TexBottomLeft = G.TexBottomLeft;
        ShadowQuad.TexTopRight = G.TexTopRight;
        ShadowQuad.BottomLeft = Cursor + math::Vector2{static_cast<float>(G.OffsetX) * Props.Scale,
                                                       static_cast<float>(G.OffsetY) * Props.Scale};
        ShadowQuad.TopRight =
            ShadowQuad.BottomLeft + math::Vector2{static_cast<float>(G.Width) * Props.Scale,
                                                  static_cast<float>(G.Height) * Props.Scale};
        if (Props.bShadow)
        {
            m_Shadows.push_back(ShadowQuad);
        }

        InstanceData Quad = ShadowQuad;
        Quad.ThresholdBottom = Props.Threshold;
        Quad.ThresholdTop = Props.Threshold;
        Quad.Color = Props.Color;
        m_Instances.push_back(Quad);

        Cursor.X += std::roundf(G.Scale * AdvanceWidth * Props.Scale);

        int Kern = stbtt_GetCodepointKernAdvance(&F->TTInfo, CodePoint, Text[i + 1]);
        Cursor.X += std::roundf(G.Scale * Kern * Props.Scale);
    }
}

bool Renderer::Present(Rndr::FrameBuffer* FrameBuffer)
{
    m_InstanceBuffer->Update(m_Ctx, Rndr::ByteSpan{m_Instances});
    m_ShadowBuffer->Update(m_Ctx, Rndr::ByteSpan{m_Shadows});

    m_Ctx->BindBuffer(m_ConstantBuffer.Get(), 0, m_Pipeline->VertexShader.Get());
    m_Ctx->BindBuffer(m_ConstantBuffer.Get(), 0, m_Pipeline->PixelShader.Get());
    m_Ctx->BindBuffer(m_IndexBuffer.Get(), 0);
    // Should we merge image and sampler binding into one call? Maybe even one class
    m_Ctx->BindImageAsShaderResource(m_TextureAtlas.Get(), 0, m_Pipeline->PixelShader.Get());
    m_Ctx->BindSampler(m_TextureAtlasSampler.Get(), 0, m_Pipeline->PixelShader.Get());
    m_Ctx->BindPipeline(m_Pipeline.Get());
    m_Ctx->BindFrameBuffer(FrameBuffer);

    m_Ctx->BindBuffer(m_ShadowBuffer.Get(), 0);
    m_Ctx->DrawIndexedInstanced(Rndr::PrimitiveTopology::TriangleList, 6,
                                static_cast<uint32_t>(m_Shadows.size()));

    m_Ctx->BindBuffer(m_InstanceBuffer.Get(), 0);
    m_Ctx->DrawIndexedInstanced(Rndr::PrimitiveTopology::TriangleList, 6,
                                static_cast<uint32_t>(m_Instances.size()));

    m_Instances.clear();
    m_Shadows.clear();
    return true;
}

bool Renderer::IsGlyphSupported(int CodePoint, Font* F, int FontSize)
{
    int GlyphHash = Font::MakeGlyphHash(CodePoint, FontSize, F->Id);
    return m_Glyphs.contains(GlyphHash);
}

void Renderer::UpdateAtlas(int CodePointStart, int CodePointEnd, Font* F, int FontSize)
{
    std::vector<AtlasPacker::RectIn> InRects;

    const float Scale = stbtt_ScaleForPixelHeight(&F->TTInfo, static_cast<float>(FontSize));
    for (int CodePoint = CodePointStart; CodePoint < CodePointEnd; CodePoint++)
    {
        const int GlyphHash = Font::MakeGlyphHash(CodePoint, FontSize, F->Id);
        Glyph& G = m_Glyphs[GlyphHash];
        G.CodePoint = CodePoint;
        G.Scale = Scale;

        uint8_t* Data =
            stbtt_GetCodepointSDF(&F->TTInfo, Scale, CodePoint, SDF::Padding, SDF::OneEdgeValue,
                                  SDF::PixelDistScale, &G.Width, &G.Height, &G.OffsetX, &G.OffsetY);
        G.SDF.Size = G.Width * G.Height;
        G.SDF.Data = new uint8_t[G.SDF.Size];
        memcpy(G.SDF.Data, Data, G.SDF.Size);
        stbtt_FreeSDF(Data, nullptr);

        G.OffsetY = -(G.OffsetY + G.Height);

        // We store the glyphs with a bit of space around them
        AtlasPacker::RectIn InRect{{G.Width + 2, G.Height + 2}, static_cast<uintptr_t>(GlyphHash)};
        InRects.push_back(InRect);
    }

    std::vector<AtlasPacker::RectOut> OutRects = m_Packer.Pack(InRects);
    assert(OutRects.size() == InRects.size());

    math::Vector2 AtlasSize(static_cast<float>(AtlasWidth), static_cast<float>(AtlasHeight));
    for (const AtlasPacker::RectOut& OutRect : OutRects)
    {
        const int GlyphHash = static_cast<int>(OutRect.UserData);
        Glyph& G = m_Glyphs[GlyphHash];

        Bitmap B;
        B.BottomLeft = {OutRect.BottomLeft.X + 1, OutRect.BottomLeft.Y + 1};
        B.Size = {G.Width, G.Height};
        B.Data = G.SDF;

        WriteToAtlas(B);

        G.TexBottomLeft.X = B.BottomLeft.X / AtlasSize.X;
        G.TexBottomLeft.Y = B.BottomLeft.Y / AtlasSize.Y;
        G.TexTopRight.X = (B.BottomLeft.X + B.Size.X) / AtlasSize.X;
        G.TexTopRight.Y = (B.BottomLeft.Y + B.Size.Y) / AtlasSize.Y;
    }

    m_TextureAtlas->Update(m_Ctx, 0, {0, 0}, AtlasSize, Rndr::ByteSpan(m_Atlas));
}

void Renderer::WriteToAtlas(const Bitmap& B)
{
    for (int i = 0; i < B.Size.Y; i++)
    {
        const int DstOffset = (B.BottomLeft.Y + i) * AtlasWidth + B.BottomLeft.X;
        const int SrcOffset = i * B.Size.X;
        memcpy(m_Atlas.data() + DstOffset, B.Data.Data + SrcOffset, B.Size.X);
    }
}
