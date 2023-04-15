#pragma once

#include "rndr/rndr.h"

#include "atlaspacker.h"
#include "font.h"

struct TextProperties
{
    float Scale = 1.0f;

    math::Vector4 Color;
    float Threshold = 0.7f;  // Value in range [0, 1]. Higher value means skinnier text.

    bool bShadow = true;
    math::Vector4 ShadowColor = {0, 0, 0, 0x64 / 255.0f};
    float ShadowThresholdBottom = 0.4f;  // Value in range [0, 1].
    float ShadowThresholdTop = 0.7f;     // Value in range [0, 1].
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
        float ThresholdBottom;
        float ThresholdTop;
        float Padding[2];
    };

    RNDR_ALIGN(16) struct ConstantData
    {
        math::Vector2 ScreenSize;
        float Padding[2];
    };

    static constexpr int AtlasWidth = 1024;
    static constexpr int AtlasHeight = 1024;

    static constexpr int InvalidFontId = 0;

public:
    Renderer(Rndr::GraphicsContext* Ctx, int32_t MaxInstances, const math::Vector2& ScreenSize);
    ~Renderer() = default;

    bool AddFont(const std::string& FontName, const std::string& AssetPath);

    void RenderText(const std::string& Text,
                    const std::string& FontName,
                    int FontSize,
                    const math::Point2 BaseLineStart,
                    const TextProperties& Props);

    bool Present(Rndr::FrameBuffer* FrameBuffer);

private:
    struct Bitmap
    {
        IntPoint BottomLeft;
        IntPoint Size;
        Rndr::ByteSpan Data;
    };

    bool IsGlyphSupported(int CodePoint, Font* F, int FontSize);
    void UpdateAtlas(int CodePointStart, int CodePointEnd, Font* F, int FontSize);
    void WriteToAtlas(const Bitmap& Bitmap);

private:
    Rndr::GraphicsContext* m_Ctx;

    Rndr::ScopePtr<Rndr::Pipeline> m_Pipeline;
    Rndr::ScopePtr<Rndr::Buffer> m_InstanceBuffer;
    Rndr::ScopePtr<Rndr::Buffer> m_ShadowBuffer;
    Rndr::ScopePtr<Rndr::Buffer> m_ConstantBuffer;
    Rndr::ScopePtr<Rndr::Buffer> m_IndexBuffer;
    Rndr::ScopePtr<Rndr::Image> m_TextureAtlas;
    Rndr::ScopePtr<Rndr::Sampler> m_TextureAtlasSampler;

    const int m_MaxInstances;
    math::Vector2 m_ScreenSize;

    std::vector<InstanceData> m_Instances;
    std::vector<InstanceData> m_Shadows;

    std::unordered_map<std::string, Font*> m_Fonts;
    std::unordered_map<int, Font*> m_IdToFonts;

    std::unordered_map<int, Glyph> m_Glyphs;

    AtlasPacker m_Packer;
    std::vector<uint8_t> m_Atlas;
};
