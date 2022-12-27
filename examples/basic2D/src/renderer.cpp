#include "renderer.h"

namespace SDF
{
constexpr int Padding = 5;
constexpr uint8_t OneEdgeValue = 180;
constexpr float PixelDistScale = static_cast<float>(OneEdgeValue) / static_cast<float>(Padding);
}  // namespace SDF

Renderer::Renderer(rndr::GraphicsContext* Ctx,
                   int32_t MaxInstances,
                   int32_t MaxAtlasCount,
                   const math::Vector2& ScreenSize)
    : m_Ctx(Ctx),
      m_MaxInstances(MaxInstances),
      m_ScreenSize(ScreenSize),
      m_Packer(AtlasWidth, AtlasHeight, AtlasPacker::SortCriteria::Height)
{
    m_Atlas.resize(AtlasWidth * AtlasHeight);
    memset(m_Atlas.data(), 0, m_Atlas.size());

    const std::string VertexShaderPath = BASIC2D_ASSET_DIR "/basic2Dvertex.hlsl";
    const std::string FragmentShaderPath = BASIC2D_ASSET_DIR "/basic2Dfragment.hlsl";

    const rndr::ByteSpan VertexShaderContents = rndr::file::ReadEntireFile(VertexShaderPath);
    assert(VertexShaderContents);
    const rndr::ByteSpan FragmentShaderContents = rndr::file::ReadEntireFile(FragmentShaderPath);
    assert(FragmentShaderContents);

    rndr::PipelineProperties PipelineProps{
        .InputLayout = rndr::InputLayoutBuilder()
                           .AddBuffer(0, rndr::DataRepetition::PerInstance, 1)
                           .AppendElement(0, "POSITION", rndr::PixelFormat::R32G32_FLOAT)
                           .AppendElement(0, "POSITION", rndr::PixelFormat::R32G32_FLOAT)
                           .AppendElement(0, "TEXCOORD", rndr::PixelFormat::R32G32_FLOAT)
                           .AppendElement(0, "TEXCOORD", rndr::PixelFormat::R32G32_FLOAT)
                           .AppendElement(0, "COLOR", rndr::PixelFormat::R32G32B32A32_FLOAT)
                           .AppendElement(0, "PSIZE", rndr::PixelFormat::R32_FLOAT)
                           .AppendElement(0, "PSIZE", rndr::PixelFormat::R32_FLOAT)
                           .Build(),
        .VertexShader = {.Type = rndr::ShaderType::Vertex, .EntryPoint = "Main"},
        .VertexShaderContents = VertexShaderContents,
        .PixelShader = {.Type = rndr::ShaderType::Fragment, .EntryPoint = "Main"},
        .PixelShaderContents = FragmentShaderContents,
        .DepthStencil = {.bDepthEnable = false, .bStencilEnable = false},
    };
    m_Pipeline = m_Ctx->CreatePipeline(PipelineProps);
    assert(m_Pipeline.IsValid());

    rndr::BufferProperties BufferProps;
    BufferProps.Size = m_MaxInstances * sizeof(InstanceData);
    BufferProps.Stride = sizeof(InstanceData);
    BufferProps.Type = rndr::BufferType::Vertex;
    m_InstanceBuffer = m_Ctx->CreateBuffer(BufferProps, rndr::ByteSpan{});
    assert(m_InstanceBuffer.IsValid());

    ConstantData ConstData{m_ScreenSize};
    BufferProps.Size = sizeof(ConstantData);
    BufferProps.Stride = sizeof(ConstantData);
    BufferProps.Type = rndr::BufferType::Constant;
    m_ConstantBuffer = m_Ctx->CreateBuffer(BufferProps, rndr::ByteSpan{&ConstData});
    assert(m_ConstantBuffer.IsValid());

    std::vector Indices{0, 1, 2, 1, 3, 2};
    BufferProps.Size = Indices.size() * sizeof(int32_t);
    BufferProps.Stride = sizeof(int32_t);
    BufferProps.Type = rndr::BufferType::Index;
    m_IndexBuffer = m_Ctx->CreateBuffer(BufferProps, rndr::ByteSpan{Indices});
    assert(m_IndexBuffer.IsValid());

    rndr::ImageProperties AtlasProps;
    AtlasProps.PixelFormat = rndr::PixelFormat::R8_UNORM;
    AtlasProps.ImageBindFlags = rndr::ImageBindFlags::ShaderResource;
    std::vector<uint8_t> InitTextureData(AtlasWidth * AtlasHeight *
                                         rndr::GetPixelSize(AtlasProps.PixelFormat));
    memset(InitTextureData.data(), 0x00, InitTextureData.size());
    std::vector<rndr::ByteSpan> InitData(MaxAtlasCount);
    m_TextureAtlas =
        m_Ctx->CreateImage(AtlasWidth, AtlasHeight, AtlasProps, rndr::ByteSpan(InitTextureData));
    assert(m_TextureAtlas.IsValid());

    rndr::SamplerProperties SamplerProps;
    //SamplerProps.AddressingU = rndr::ImageAddressing::Clamp;
    //SamplerProps.AddressingV = rndr::ImageAddressing::Clamp;
    m_TextureAtlasSampler = m_Ctx->CreateSampler(SamplerProps);
    assert(m_TextureAtlasSampler.IsValid());

    m_Instances.reserve(m_MaxInstances);
}

bool Renderer::AddFont(const std::string& FontName, const std::string& AssetPath)
{
    Font NewFont;
    if (!NewFont.Init(FontName, AssetPath))
    {
        return false;
    }
    m_Fonts.insert(std::make_pair(FontName, NewFont));
    m_IdToFonts.insert(std::make_pair(NewFont.Id, NewFont));

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

    Font* F = &FontIter->second;

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

        InstanceData Quad;
        Quad.SDFThresholdTop =
            math::Lerp(Props.Bolden, static_cast<float>(SDF::OneEdgeValue) / 255.0f, 0.0f);
        Quad.SDFThresholdBottom = math::Lerp(Props.Smoothness, Quad.SDFThresholdTop, 0.0f);
        Quad.Color = Props.Color;
        Quad.TexBottomLeft = G.TexBottomLeft;
        Quad.TexTopRight = G.TexTopRight;
        Quad.BottomLeft =
            Cursor + math::Vector2{static_cast<float>(G.OffsetX), static_cast<float>(G.OffsetY)};
        Quad.TopRight = Quad.BottomLeft +
                        math::Vector2{static_cast<float>(G.Width), static_cast<float>(G.Height)};
        m_Instances.push_back(Quad);

        Cursor.X += std::roundf(G.Scale * AdvanceWidth);

        int Kern = stbtt_GetCodepointKernAdvance(&F->TTInfo, CodePoint, Text[i + 1]);
        Cursor.X += std::roundf(G.Scale * Kern);
    }
}

bool Renderer::Present(rndr::FrameBuffer* FrameBuffer)
{
    m_InstanceBuffer->Update(m_Ctx, rndr::ByteSpan{m_Instances});
    m_Ctx->BindBuffer(m_InstanceBuffer.Get(), 0);
    m_Ctx->BindBuffer(m_ConstantBuffer.Get(), 0, m_Pipeline->VertexShader.Get());
    m_Ctx->BindBuffer(m_ConstantBuffer.Get(), 0, m_Pipeline->PixelShader.Get());
    m_Ctx->BindBuffer(m_IndexBuffer.Get(), 0);
    // Should we merge image and sampler binding into one call? Maybe even one class
    m_Ctx->BindImageAsShaderResource(m_TextureAtlas.Get(), 0, m_Pipeline->PixelShader.Get());
    m_Ctx->BindSampler(m_TextureAtlasSampler.Get(), 0, m_Pipeline->PixelShader.Get());
    m_Ctx->BindPipeline(m_Pipeline.Get());
    m_Ctx->BindFrameBuffer(FrameBuffer);

    m_Ctx->DrawIndexedInstanced(rndr::PrimitiveTopology::TriangleList, 6, m_Instances.size());

    m_Instances.clear();
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

    const float Scale = stbtt_ScaleForPixelHeight(&F->TTInfo, FontSize);
    const float ScaledAscent = std::roundf(F->Ascent * Scale);
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

    for (const AtlasPacker::RectOut& OutRect : OutRects)
    {
        const int GlyphHash = static_cast<int>(OutRect.UserData);
        Glyph& G = m_Glyphs[GlyphHash];

        Bitmap B;
        B.BottomLeft = {OutRect.BottomLeft.X + 1, OutRect.BottomLeft.Y + 1};
        B.Size = {G.Width, G.Height};
        B.Data = G.SDF;

        WriteToAtlas(B);

        G.TexBottomLeft.X = B.BottomLeft.X / AtlasWidth;
        G.TexBottomLeft.Y = B.BottomLeft.Y / AtlasHeight;
        G.TexTopRight.X = (B.BottomLeft.X + B.Size.X) / AtlasWidth;
        G.TexTopRight.Y = (B.BottomLeft.Y + B.Size.Y) / AtlasHeight;
    }

    m_TextureAtlas->Update(m_Ctx, 0, {0, 0}, {AtlasWidth, AtlasHeight}, rndr::ByteSpan(m_Atlas));
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
