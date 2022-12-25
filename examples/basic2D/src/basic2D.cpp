#include "rndr/rndr.h"

#include "stb_truetype/stb_truetype.h"

struct IntPoint
{
    int X = 0;
    int Y = 0;
};

class AtlasPacker
{
public:
    struct RectIn
    {
        IntPoint Size;
        uintptr_t UserData;
    };

    struct RectOut
    {
        IntPoint BottomLeft;
        IntPoint Size;
        uintptr_t UserData = 0;
    };

    enum class SortCriteria
    {
        Height,
        Width,
        Area,
        PathologicalMultiplier  // max(w, h) / min(w, h) * w * h
    };

public:
    AtlasPacker(const int AtlasWidth, const int AtlasHeight, SortCriteria SortCrit)
        : m_AtlasSize{AtlasWidth, AtlasHeight}, m_SortCriteria(SortCrit)
    {
    }

    static float PathologicMult(const RectIn& R)
    {
        return std::max(R.Size.X, R.Size.Y) / std::min(R.Size.X, R.Size.Y) * R.Size.X * R.Size.Y;
    }

    // Returns true if A is greater than B
    bool GreaterThan(const RectIn& A, const RectIn& B) const
    {
        switch (m_SortCriteria)
        {
            case SortCriteria::Height:
            {
                return A.Size.Y > B.Size.Y;
            }
            case SortCriteria::Width:
            {
                return A.Size.X > B.Size.X;
            }
            case SortCriteria::Area:
            {
                return A.Size.X * A.Size.Y > B.Size.X * B.Size.Y;
            }
            case SortCriteria::PathologicalMultiplier:
            {
                return PathologicMult(A) > PathologicMult(B);
            }
        }
        assert(false);
        return true;
    }

    bool GreaterThan(const RectOut& A, const RectOut& B)
    {
        RectIn AA{A.Size};
        RectIn BB{B.Size};
        return GreaterThan(AA, BB);
    }

    std::vector<RectOut> Pack(std::vector<RectIn>& InRects)
    {
        // Sort input rectangles based on the sorting criteria
        std::sort(InRects.begin(), InRects.end(),
                  [this](const RectIn& A, const RectIn& B) { return GreaterThan(A, B); });

        // Initialize array of free slots and populate it by one slot which has the size of the
        // whole atlas
        std::vector<RectOut> Slots;
        RectOut StartSlot{IntPoint{0, 0}, m_AtlasSize, 0};
        Slots.push_back(StartSlot);

        std::vector<RectOut> OutRects;
        for (const RectIn& InRect : InRects)
        {
            // No more free space left
            if (Slots.empty())
            {
                break;
            }

            // Go through free space slots, from smaller to bigger
            for (int i = Slots.size() - 1; i >= 0; i--)
            {
                RectOut& Slot = Slots[i];

                // This slot is too small, skip it
                if (Slot.Size.X < InRect.Size.X || Slot.Size.Y < InRect.Size.Y)
                {
                    continue;
                }

                // We found a free slot for the input rect, add info the output rect list
                RectOut OutRect;
                OutRect.BottomLeft = Slot.BottomLeft;
                OutRect.Size = InRect.Size;
                OutRect.UserData = InRect.UserData;
                OutRects.push_back(OutRect);

                const int DiffX = Slot.Size.X - InRect.Size.X;
                const int DiffY = Slot.Size.Y - InRect.Size.Y;

                RectOut SmallerSlot;
                SmallerSlot.BottomLeft =
                    IntPoint{Slot.BottomLeft.X + InRect.Size.X, Slot.BottomLeft.Y};
                SmallerSlot.Size = IntPoint{Slot.Size.X - InRect.Size.X, InRect.Size.Y};

                RectOut BiggerSlot;
                BiggerSlot.BottomLeft = IntPoint{Slot.BottomLeft.X, InRect.Size.Y};
                BiggerSlot.Size = IntPoint{Slot.Size.X, Slot.Size.Y - InRect.Size.Y};

                if (GreaterThan(SmallerSlot, BiggerSlot))
                {
                    std::swap(SmallerSlot, BiggerSlot);
                }

                // Move the current slot to the back and remove it from the list
                std::swap(Slot, Slots.back());
                Slots.pop_back();

                if (BiggerSlot.Size.X != 0 && BiggerSlot.Size.Y != 0)
                {
                    Slots.push_back(BiggerSlot);
                }
                if (SmallerSlot.Size.X != 0 && SmallerSlot.Size.Y != 0)
                {
                    Slots.push_back(SmallerSlot);
                }

                break;
            }
        }

        return OutRects;
    }

    static void Test()
    {
        AtlasPacker P(100, 100, SortCriteria::Height);
        std::vector<RectIn> InRects;
        InRects.push_back(RectIn{{10, 20}, 0});
        InRects.push_back(RectIn{{5, 5}, 1});
        InRects.push_back(RectIn{{20, 10}, 2});
        InRects.push_back(RectIn{{30, 30}, 3});

        std::vector<RectOut> OutRects = P.Pack(InRects);
        assert(OutRects.size() == 4);

        assert(OutRects[0].BottomLeft.X == 0);
        assert(OutRects[0].BottomLeft.Y == 0);
        assert(OutRects[0].UserData == 3);

        assert(OutRects[1].BottomLeft.X == 30);
        assert(OutRects[1].BottomLeft.Y == 0);
        assert(OutRects[1].UserData == 0);

        assert(OutRects[2].BottomLeft.X == 30);
        assert(OutRects[2].BottomLeft.Y == 20);
        assert(OutRects[2].UserData == 2);

        assert(OutRects[3].BottomLeft.X == 50);
        assert(OutRects[3].BottomLeft.Y == 20);
        assert(OutRects[3].UserData == 1);
    }

private:
    IntPoint m_AtlasSize;
    SortCriteria m_SortCriteria;
};

class Renderer
{
public:
    RNDR_ALIGN(16) struct InstanceData
    {
        math::Point2 BottomLeft;
        math::Point2 TopRight;
        math::Point2 TexBottomLeft = math::Point2{0.0f, 0.0f};
        math::Point2 TexTopRight = math::Point2{1.0f, 1.0f};
        math::Vector4 Color;
        float AtlasIndex;
    };

    RNDR_ALIGN(16) struct ConstantData
    {
        math::Vector2 ScreenSize;
    };

    static constexpr float AtlasWidth = 1024.0f;
    static constexpr float AtlasHeight = 1024.0f;

    static constexpr int InvalidSpriteId = 0;
    static constexpr int InvalidFontId = 0;

private:
    struct Sprite
    {
        int Id = InvalidSpriteId;
        math::Point2 TexBottomLeft;
        math::Point2 TexTopRight;
        int AtlasIndex = 0;
        rndr::CPUImage ImageData;
    };

    struct Glyph
    {
        int CodePoint = 0;
        int SpriteId = InvalidSpriteId;

        float ScaledYOffset = 0.0f;
        float ScaledWidth = 0.0f;
        float ScaledHeight = 0.0f;
        float ScaledAdvanceWidth = 0.0f;
        float ScaledLeftSideBearing = 0.0f;
    };

    struct Font
    {
        int Id = InvalidFontId;
        std::vector<Glyph> Glyphs;

        stbtt_fontinfo StbFontInfo;
        float Scale = 0.0f;
        float ScaledAscent = 0.0f;
        float ScaledDescent = 0.0f;
        float ScaledLineGap = 0.0f;
    };

public:
    Renderer(rndr::GraphicsContext* Ctx,
             int32_t MaxInstances,
             int32_t MaxAtlasCount,
             const math::Vector2& ScreenSize)
        : m_Ctx(Ctx), m_MaxInstances(MaxInstances), m_ScreenSize(ScreenSize)
    {
        const std::string VertexShaderPath = BASIC2D_ASSET_DIR "/basic2Dvertex.hlsl";
        const std::string FragmentShaderPath = BASIC2D_ASSET_DIR "/basic2Dfragment.hlsl";

        const rndr::ByteSpan VertexShaderContents = rndr::file::ReadEntireFile(VertexShaderPath);
        assert(VertexShaderContents);
        const rndr::ByteSpan FragmentShaderContents =
            rndr::file::ReadEntireFile(FragmentShaderPath);
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
        AtlasProps.PixelFormat = rndr::PixelFormat::R8G8B8A8_UNORM;
        AtlasProps.ImageBindFlags = rndr::ImageBindFlags::ShaderResource;
        std::vector<uint8_t> WhiteTextureData(AtlasWidth * AtlasHeight *
                                              rndr::GetPixelSize(AtlasProps.PixelFormat));
        memset(WhiteTextureData.data(), 0xFF, WhiteTextureData.size());
        std::vector<rndr::ByteSpan> InitData(MaxAtlasCount);
        for (rndr::ByteSpan& S : InitData)
        {
            S.Data = WhiteTextureData.data();
            S.Size = WhiteTextureData.size();
        }
        rndr::Span<rndr::ByteSpan> InitDataSpan(InitData);
        m_TextureAtlas = m_Ctx->CreateImageArray(AtlasWidth, AtlasHeight, MaxAtlasCount, AtlasProps,
                                                 InitDataSpan);
        assert(m_TextureAtlas.IsValid());

        m_TextureAtlasSampler = m_Ctx->CreateSampler();
        assert(m_TextureAtlasSampler.IsValid());

        m_Instances.reserve(m_MaxInstances);
    }

    ~Renderer() = default;

    // In production code this would be done offline
    int RegisterSprite(const std::string& AssetPath)
    {
        rndr::CPUImage Im = rndr::file::ReadEntireImage(AssetPath);
        if (!Im.Data)
        {
            return InvalidSpriteId;
        }

        Sprite S;
        S.Id = m_Sprites.size() + 1;
        S.ImageData = Im;
        m_Sprites.push_back(S);
    }

    int RegisterSprite(const rndr::CPUImage& Image)
    {
        Sprite S;
        S.Id = m_Sprites.size() + 1;
        S.ImageData = Image;
        m_Sprites.push_back(S);
        return S.Id;
    }

    int RegisterFont(const std::string& FontPath, int FontSizeInPixels)
    {
        rndr::ByteSpan Contents = rndr::file::ReadEntireFile(FontPath);
        if (!Contents)
        {
            return InvalidFontId;
        }

        int Offset = stbtt_GetFontOffsetForIndex(Contents.Data, 0);

        Font F;
        if (!stbtt_InitFont(&F.StbFontInfo, Contents.Data, Offset))
        {
            return InvalidFontId;
        }

        F.Scale = stbtt_ScaleForPixelHeight(&F.StbFontInfo, FontSizeInPixels);
        int Ascent, Descent, LineGap;
        stbtt_GetFontVMetrics(&F.StbFontInfo, &Ascent, &Descent, &LineGap);
        F.ScaledAscent = roundf(F.Scale * Ascent);
        F.ScaledDescent = roundf(F.Scale * Descent);
        F.ScaledLineGap = roundf(F.Scale * LineGap);

        constexpr int ASCIICodePointStart = 32;
        constexpr int ASCIICodePointEnd = 127;
        for (int CodePoint = ASCIICodePointStart; CodePoint < ASCIICodePointEnd; CodePoint++)
        {
            Glyph G;

            int AdvanceWidth, LeftSideBearing;
            stbtt_GetCodepointHMetrics(&F.StbFontInfo, CodePoint, &AdvanceWidth, &LeftSideBearing);
            G.ScaledAdvanceWidth = roundf(AdvanceWidth * F.Scale);
            G.ScaledLeftSideBearing = roundf(LeftSideBearing * F.Scale);

            int x0, x1;
            int y0, y1;
            stbtt_GetCodepointBitmapBox(&F.StbFontInfo, CodePoint, F.Scale, F.Scale, &x0, &y0, &x1,
                                        &y1);
            G.ScaledYOffset = static_cast<float>(-y1);
            G.ScaledWidth = x1 - x0;
            G.ScaledHeight = y1 - y0;

            rndr::ByteSpan GlyphData;
            GlyphData.Size =
                G.ScaledWidth * G.ScaledHeight * rndr::GetPixelSize(rndr::PixelFormat::R8_UNORM);
            GlyphData.Data = new uint8_t[GlyphData.Size];
            stbtt_MakeCodepointBitmap(&F.StbFontInfo, GlyphData.Data, G.ScaledWidth, G.ScaledHeight,
                                      G.ScaledWidth, F.Scale, F.Scale, CodePoint);

            rndr::CPUImage Im;
            Im.Width = G.ScaledWidth;
            Im.Height = G.ScaledHeight;
            Im.Format = rndr::PixelFormat::R8_UNORM;
            Im.Data = GlyphData;
            G.SpriteId = RegisterSprite(Im);
            G.CodePoint = CodePoint;

            F.Glyphs.push_back(G);
        }

        F.Id = m_Fonts.size() + 1;
        m_Fonts.push_back(F);

        return true;
    }

    void WriteToAtlas(Sprite& S, AtlasPacker::RectOut& Rect, rndr::ByteSpan& Atlas)
    {
        S.AtlasIndex = 1;
        S.TexBottomLeft.X = Rect.BottomLeft.X / AtlasWidth;
        S.TexBottomLeft.Y = Rect.BottomLeft.Y / AtlasHeight;
        S.TexTopRight.X = (Rect.BottomLeft.X + Rect.Size.X) / AtlasWidth;
        S.TexTopRight.Y = (Rect.BottomLeft.Y + Rect.Size.Y) / AtlasHeight;

        const int DstPixelSize = rndr::GetPixelSize(rndr::PixelFormat::R8G8B8A8_UNORM_SRGB);
        assert(DstPixelSize == sizeof(uint32_t));
        const int SrcPixelSize = rndr::GetPixelSize(S.ImageData.Format);
        for (int i = 0; i < Rect.Size.Y; i++)
        {
            for (int j = 0; j < Rect.Size.X; j++)
            {
                uint8_t SrcPixel = S.ImageData.Data.Data[(i * Rect.Size.X + j) * SrcPixelSize];
                const int DstPixelAddr =
                    ((Rect.BottomLeft.Y + i) * AtlasWidth + Rect.BottomLeft.X + j) * DstPixelSize;
                uint32_t* DstPixel = reinterpret_cast<uint32_t*>(Atlas.Data + DstPixelAddr);
                *DstPixel = 0xFFFFFFFF;
                *DstPixel &= static_cast<uint32_t>(SrcPixel) << 24;
            }
        }

        delete[] S.ImageData.Data.Data;
        S.ImageData.Data.Data = nullptr;
    }

    void PackSprites()
    {
        AtlasPacker Packer(AtlasWidth, AtlasHeight, AtlasPacker::SortCriteria::Height);

        std::vector<AtlasPacker::RectIn> InRects;
        for (int i = 0; i < m_Sprites.size(); i++)
        {
            Sprite& S = m_Sprites[i];
            AtlasPacker::RectIn Rect;
            Rect.Size = IntPoint{S.ImageData.Width, S.ImageData.Height};
            Rect.UserData = static_cast<uintptr_t>(S.Id);
            InRects.push_back(Rect);
        }

        std::vector<AtlasPacker::RectOut> OutRects = Packer.Pack(InRects);
        assert(OutRects.size() == InRects.size());

        rndr::ByteSpan AtlasData;
        AtlasData.Size =
            AtlasWidth * AtlasHeight * rndr::GetPixelSize(rndr::PixelFormat::R8G8B8A8_UNORM_SRGB);
        AtlasData.Data = new uint8_t[AtlasData.Size];
        memset(AtlasData.Data, 0, AtlasData.Size);

        for (int i = 0; i < OutRects.size(); i++)
        {
            int SpriteId = static_cast<int>(OutRects[i].UserData);
            Sprite& S = *std::find_if(m_Sprites.begin(), m_Sprites.end(),
                                      [SpriteId](const Sprite& S) { return S.Id == SpriteId; });
            WriteToAtlas(S, OutRects[i], AtlasData);
        }

        m_TextureAtlas->Update(m_Ctx, 1, {0, 0}, {AtlasWidth, AtlasHeight}, AtlasData);
    }

    bool AddElement(const math::Point2& BottomLeft,
                    const math::Vector2& Size,
                    const math::Vector4& Color)
    {
        if (m_Instances.size() == m_MaxInstances)
        {
            return false;
        }

        InstanceData Data{.BottomLeft = BottomLeft,
                          .TopRight = BottomLeft + Size,
                          .Color = Color,
                          .AtlasIndex = 0};

        m_Instances.push_back(Data);
        return true;
    }

    void AddText(const std::string& Text,
                 int FontId,
                 const math::Point2& BaseLineStart,
                 const math::Vector4& Color)
    {
        Font* Font = nullptr;
        for (Renderer::Font& F : m_Fonts)
        {
            if (F.Id == FontId)
            {
                Font = &F;
                break;
            }
        }
        if (!Font)
        {
            return;
        }

        math::Point2 Cursor = BaseLineStart;
        for (int i = 0; i < Text.size(); i++)
        {
            char CodePoint = Text[i];

            Glyph& G =
                *std::find_if(Font->Glyphs.begin(), Font->Glyphs.end(),
                              [CodePoint](const Glyph& G) { return G.CodePoint == CodePoint; });

            Sprite& S = *std::find_if(m_Sprites.begin(), m_Sprites.end(),
                                      [Id = G.SpriteId](const Sprite& S) { return Id == S.Id; });

            math::Point2 BottomLeft{Cursor.X + G.ScaledLeftSideBearing,
                                    Cursor.Y + G.ScaledYOffset};
            InstanceData Data{.BottomLeft = BottomLeft,
                              .TopRight = BottomLeft + math::Vector2{G.ScaledWidth, G.ScaledHeight},
                              .TexBottomLeft = S.TexBottomLeft,
                              .TexTopRight = S.TexTopRight,
                              .Color = Color,
                              .AtlasIndex = static_cast<float>(S.AtlasIndex)};
            m_Instances.push_back(Data);

            const int Kern = stbtt_GetCodepointKernAdvance(&Font->StbFontInfo, CodePoint, Text[i + 1]);
            const float ScaledKern = std::roundf(Kern * Font->Scale);

            Cursor.X += G.ScaledAdvanceWidth + ScaledKern;
        }
    }

    bool Render(rndr::FrameBuffer* FrameBuffer)
    {
        m_InstanceBuffer->Update(m_Ctx, rndr::ByteSpan{m_Instances});
        m_Ctx->BindBuffer(m_InstanceBuffer.Get(), 0);
        m_Ctx->BindBuffer(m_ConstantBuffer.Get(), 0, m_Pipeline->VertexShader.Get());
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

private:
    rndr::GraphicsContext* m_Ctx;

    rndr::ScopePtr<rndr::Pipeline> m_Pipeline;
    rndr::ScopePtr<rndr::Buffer> m_InstanceBuffer;
    rndr::ScopePtr<rndr::Buffer> m_ConstantBuffer;
    rndr::ScopePtr<rndr::Buffer> m_IndexBuffer;
    rndr::ScopePtr<rndr::Image> m_TextureAtlas;
    rndr::ScopePtr<rndr::Sampler> m_TextureAtlasSampler;

    const int m_MaxInstances;
    math::Vector2 m_ScreenSize;

    std::vector<InstanceData> m_Instances;
    std::vector<Sprite> m_Sprites;
    std::vector<Font> m_Fonts;
};

class App
{
public:
    constexpr static int MaxInstancesCount = 100;
    constexpr static int MaxAtlasCount = 2;

public:
    App(int WindowWidth, int WindowHeight)
        : m_WindowWidth(WindowWidth), m_WindowHeight(WindowHeight)
    {
        m_RndrCtx = std::make_unique<rndr::RndrContext>();
        assert(m_RndrCtx.get());
        m_GraphicsCtx = m_RndrCtx->CreateGraphicsContext();
        assert(m_GraphicsCtx.IsValid());
        m_Window = m_RndrCtx->CreateWin(m_WindowWidth, m_WindowHeight);
        assert(m_Window.IsValid());

        rndr::NativeWindowHandle Handle = m_Window->GetNativeWindowHandle();
        m_SwapChain = m_GraphicsCtx->CreateSwapChain(Handle, m_WindowWidth, m_WindowHeight);
        assert(m_SwapChain.IsValid());

        m_Renderer = std::make_unique<Renderer>(m_GraphicsCtx.Get(), MaxInstancesCount,
                                                MaxAtlasCount, GetScreenSize());
        assert(m_Renderer.get());

        m_Renderer->RegisterFont(BASIC2D_ASSET_DIR "/yumin.ttf", 24);
        m_Renderer->PackSprites();
    }

    ~App() = default;

    void RunLoop()
    {
        while (!m_Window->IsClosed())
        {
            m_Window->ProcessEvents();

            math::Vector4 ClearColor{0.2f, 0.5f, 0.4f, 1.0f};
            rndr::FrameBuffer* DefaultFB = m_SwapChain->FrameBuffer;
            m_GraphicsCtx->ClearColor(DefaultFB->ColorBuffers[0], ClearColor);

            m_Renderer->AddElement({100, 100}, {100, 100}, {1, 0, 0, 1});
            m_Renderer->AddText("The Five Boxing Wizards Jump Quickly!", 1, {100, 300}, {1, 1, 0, 1});
            m_Renderer->Render(m_SwapChain->FrameBuffer);

            m_GraphicsCtx->Present(m_SwapChain.Get(), true);
        }
    }

    rndr::GraphicsContext* GetGraphicsContext() { return m_GraphicsCtx.Get(); }
    math::Vector2 GetScreenSize() const { return math::Vector2{m_WindowWidth, m_WindowHeight}; }

private:
    std::unique_ptr<rndr::RndrContext> m_RndrCtx;

    rndr::ScopePtr<rndr::GraphicsContext> m_GraphicsCtx;
    rndr::ScopePtr<rndr::Window> m_Window;
    rndr::ScopePtr<rndr::SwapChain> m_SwapChain;

    std::unique_ptr<Renderer> m_Renderer;

    float m_WindowWidth = 800.0f;
    float m_WindowHeight = 600.0f;
};

int main()
{
    AtlasPacker::Test();

    App MainApp(800, 600);
    MainApp.RunLoop();
    return 0;
}