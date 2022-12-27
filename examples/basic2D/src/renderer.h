#pragma once

#include "rndr/rndr.h"

#include "atlaspacker.h"
#include "font.h"

struct TextProperties
{
    math::Vector4 Color;
    float Bolden = 0.0f;      // Value between [0, 1].
    float Smoothness = 0.0f;  // Value between [0, 1].
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
        float SDFThresholdBottom;
        float SDFThresholdTop;
    };

    RNDR_ALIGN(16) struct ConstantData
    {
        math::Vector2 ScreenSize;
    };

    static constexpr float AtlasWidth = 1024.0f;
    static constexpr float AtlasHeight = 1024.0f;

    static constexpr int InvalidFontId = 0;

public:
    Renderer(rndr::GraphicsContext* Ctx,
             int32_t MaxInstances,
             int32_t MaxAtlasCount,
             const math::Vector2& ScreenSize);
    ~Renderer() = default;

    bool AddFont(const std::string& FontName, const std::string& AssetPath);

    void RenderText(const std::string& Text,
                    const std::string& FontName,
                    int FontSize,
                    const math::Point2 BaseLineStart,
                    const TextProperties& Props);

    bool Present(rndr::FrameBuffer* FrameBuffer);

private:
    struct Bitmap
    {
        IntPoint BottomLeft;
        IntPoint Size;
        rndr::ByteSpan Data;
    };

    bool IsGlyphSupported(int CodePoint, Font* F, int FontSize);
    void UpdateAtlas(int CodePointStart, int CodePointEnd, Font* F, int FontSize);
    void WriteToAtlas(const Bitmap& Bitmap);

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

    std::unordered_map<std::string, Font> m_Fonts;
    std::unordered_map<int, Font> m_IdToFonts;

    std::unordered_map<int, Glyph> m_Glyphs;

    AtlasPacker m_Packer;
    std::vector<uint8_t> m_Atlas;
};
