#include "rndr/ui/uifont.h"

#include <filesystem>

#include <stb_truetype/stb_truetype.h>

#include "rndr/core/fileutils.h"
#include "rndr/core/log.h"
#include "rndr/core/span.h"

#define SQUARE(x) ((x) * (x))

namespace rndr
{
namespace ui
{

// Private types

struct AtlasInfo
{
    std::string FontName;
    float FontSizeInPixels;

    FontHandle Handle;

    stbtt_fontinfo FontInfo;

    // Array of info on ASCII glyphs
    Span<GlyphInfo> Glyphs;
    // Factor to convert unscaled units to pixels
    float Scale;
    // Distance between baseline and the highest point of a font can have in pixels
    int Ascent;
    // Distance between baseline and the lowest point of a font, negative number in pixels
    int Descent;
    // Distance between font descent of one line and font ascent of the line beneath
    int LineGap;
    // First unicode value of the character range
    int StartCodepoint;
    // Unicode value after the last value in the character range
    int EndCodepoint;
    // Atlas image
    ByteSpan Contents;
    // Number of glyphs per side
    int SideCount;
    int SpaceAdvance;
    int RenderId;
};

// Private constants
static constexpr int kMaxAtlasCount = 16;
static constexpr int kMaxAtlasSideSizeInPixels = 36 * 10;
static constexpr int kStartCodepointASCII = 33;
static constexpr int kEndCodepointASCII = 127;

// Private data
static AtlasInfo* g_Atlases[kMaxAtlasCount];

// Main module functions
int GetRenderId();
void UpdateRenderResource(int RenderId, ByteSpan Contents);

// Module private functions
void InitFont();
void ShutDownFont();
int GetMaxAtlasSideSizeInPixels();

// Private functions
static AtlasInfo* GetAtlas(const char* FontName, float FontSize);
static AtlasInfo* GetAtlas(FontHandle Handle);
static AtlasInfo* CreateAtlas(const std::string& FontPath, float SizeInPixels);
static int GetMinGlyphCountPerSize(int GlyphSize, int GlyphCount, int SideInPixels);
static bool ContainsCodepoint(FontHandle Font, int Codepoint);

}  // namespace ui
}  // namespace rndr

void rndr::ui::InitFont()
{
    for (int i = 0; i < kMaxAtlasCount; i++)
    {
        g_Atlases[i] = nullptr;
    }
}

void rndr::ui::ShutDownFont()
{
    for (int i = 0; i < kMaxAtlasCount; i++)
    {
        if (!g_Atlases[i])
        {
            continue;
        }

        delete[] g_Atlases[i]->Contents.Data;
        delete[] g_Atlases[i]->Glyphs.Data;
        delete g_Atlases[i];
        g_Atlases[i] = nullptr;
    }
}

int rndr::ui::GetMaxAtlasSideSizeInPixels()
{
    return kMaxAtlasSideSizeInPixels;
}

rndr::ui::FontHandle rndr::ui::AddFont(const char* FilePath, float FontSizeInPixels)
{
    std::filesystem::path FontPath(FilePath);
    if (FontPath.extension() != ".ttf")
    {
        RNDR_LOG_ERROR("We only support .ttf font files!");
        return kInvalidFontHandle;
    }
    std::string FontName = FontPath.stem().string();
    AtlasInfo* Atlas = GetAtlas(FontName.c_str(), FontSizeInPixels);
    if (Atlas)
    {
        RNDR_LOG_INFO("Font with a name %s and a pixel size %d already present...", FontName.c_str(), FontSizeInPixels);
        return Atlas->Handle;
    }
    FontHandle FreeHandle = kInvalidFontHandle;
    for (int i = 0; i < kMaxAtlasCount; i++)
    {
        if (!g_Atlases[i])
        {
            FreeHandle = i;
            break;
        }
    }
    if (FreeHandle == kInvalidFontHandle)
    {
        RNDR_LOG_ERROR("Max number of supported fonts reached, supported %d fonts!", kMaxAtlasCount);
        return kInvalidFontHandle;
    }
    Atlas = CreateAtlas(FontPath.string(), FontSizeInPixels);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Failed to create a font atlas from font file %s!", FilePath);
        return kInvalidFontHandle;
    }
    Atlas->Handle = FreeHandle;
    g_Atlases[FreeHandle] = Atlas;
    RNDR_LOG_INFO("Successfully added font %s with pixel size %d with font handle %d", FontName.c_str(), (int)FontSizeInPixels, FreeHandle);
    return Atlas->Handle;
}

void rndr::ui::RemoveFont(FontHandle Handle)
{
    if (Handle < 0 || Handle >= kMaxAtlasCount)
    {
        return;
    }
    AtlasInfo* Atlas = g_Atlases[Handle];
    if (!Atlas)
    {
        return;
    }
    delete[] Atlas->Contents.Data;
    delete[] Atlas->Glyphs.Data;
    delete[] Atlas->FontInfo.data;
    delete Atlas;
    g_Atlases[Handle] = nullptr;
}

bool rndr::ui::ContainsFont(const char* FontName, float FontSizeInPixels)
{
    return GetAtlas(FontName, FontSizeInPixels) != nullptr;
}

bool rndr::ui::ContainsFont(FontHandle Font)
{
    return GetAtlas(Font) != nullptr;
}

int rndr::ui::GetFontSize(FontHandle Font)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font handle %d doesn't correspond to any font, did you call AddFont?");
        return 0;
    }
    return Atlas->Ascent + std::abs(Atlas->Descent);
}

int rndr::ui::GetGlyphAdvance(FontHandle Font, int CurrentCodepoint, int NextCodepoint)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font handle %d doesn't correspond to any font, did you call AddFont?");
        return 0;
    }
    int KernAdvance = stbtt_GetCodepointKernAdvance(&Atlas->FontInfo, CurrentCodepoint, NextCodepoint);
    KernAdvance = std::roundf(Atlas->Scale * KernAdvance);
    int Advance = 0;
    if (ContainsCodepoint(Font, CurrentCodepoint))
    {
        Advance = Atlas->Glyphs[CurrentCodepoint - Atlas->StartCodepoint].Advance;
    }
    else if (CurrentCodepoint == ' ')
    {
        Advance = Atlas->SpaceAdvance;
    }
    return Advance + KernAdvance;
}

int rndr::ui::GetFontVerticalAdvance(FontHandle Font)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font handle %d doesn't correspond to any font, did you call AddFont?");
        return 0;
    }
    return Atlas->Ascent + std::abs(Atlas->Descent) + Atlas->LineGap;
}

math::Vector2 rndr::ui::GetGlyphSize(FontHandle Font, int Codepoint)
{
    if (!ContainsCodepoint(Font, Codepoint))
    {
        return math::Vector2{};
    }
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font handle %d doesn't correspond to any font, did you call AddFont?");
        return math::Vector2{};
    }
    GlyphInfo* Info = &Atlas->Glyphs[Codepoint - Atlas->StartCodepoint];
    return math::Vector2{(float)Info->Width, (float)Info->Height};
}

math::Vector2 rndr::ui::GetGlyphBearing(FontHandle Font, int Codepoint)
{
    if (!ContainsCodepoint(Font, Codepoint))
    {
        return math::Vector2{};
    }
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font handle %d doesn't correspond to any font, did you call AddFont?");
        return math::Vector2{};
    }
    GlyphInfo* Info = &Atlas->Glyphs[Codepoint - Atlas->StartCodepoint];
    return math::Vector2{(float)Info->OffsetX, (float)-(Info->Height + Info->OffsetY)};
}

void rndr::ui::GetGlyphTexCoords(FontHandle Font, int Codepoint, math::Point2* BottomLeft, math::Point2* TopRight)
{
    if (!ContainsCodepoint(Font, Codepoint))
    {
        return;
    }
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font handle %d doesn't correspond to any font, did you call AddFont?");
        return;
    }
    GlyphInfo* Info = &Atlas->Glyphs[Codepoint - Atlas->StartCodepoint];
    if (BottomLeft)
    {
        *BottomLeft = Info->UVStart;
    }
    if (TopRight)
    {
        *TopRight = Info->UVEnd;
    }
}

int rndr::ui::GetFontRenderId(FontHandle Handle)
{
    AtlasInfo* Atlas = GetAtlas(Handle);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font handle %d doesn't correspond to any font, did you call AddFont?");
        return -1;
    }
    return Atlas->RenderId;
}

rndr::ui::AtlasInfo* rndr::ui::GetAtlas(const char* FontName, float FontSize)
{
    for (int i = 0; i < kMaxAtlasCount; i++)
    {
        if (!g_Atlases[i])
        {
            continue;
        }
        if (g_Atlases[i]->FontName == FontName && g_Atlases[i]->FontSizeInPixels == FontSize)
        {
            return g_Atlases[i];
        }
    }
    return nullptr;
}

rndr::ui::AtlasInfo* rndr::ui::GetAtlas(FontHandle Handle)
{
    if (Handle < 0 || Handle >= kMaxAtlasCount)
    {
        RNDR_LOG_ERROR("Invalid handle!");
        return nullptr;
    }
    return g_Atlases[Handle];
}

rndr::ui::AtlasInfo* rndr::ui::CreateAtlas(const std::string& FontPath, float SizeInPixels)
{
    constexpr int kGlyphCount = kEndCodepointASCII - kStartCodepointASCII;

    int SideCount = GetMinGlyphCountPerSize(SizeInPixels, kGlyphCount, kMaxAtlasSideSizeInPixels);
    if (SideCount == 0)
    {
        RNDR_LOG_ERROR("Not enough space for glyphs!");
        return nullptr;
    }
    ByteSpan FileContents = ReadEntireFile(FontPath);
    if (!FileContents)
    {
        return nullptr;
    }
    AtlasInfo* Atlas = new AtlasInfo{};
    int FontOffset = stbtt_GetFontOffsetForIndex(FileContents.Data, 0);
    int Result = stbtt_InitFont(&Atlas->FontInfo, FileContents.Data, FontOffset);
    if (!Result)
    {
        RNDR_LOG_ERROR("stbtt_InitFont failed!");
        return nullptr;
    }

    Atlas->SideCount = SideCount;
    Atlas->FontInfo = Atlas->FontInfo;
    Atlas->Scale = stbtt_ScaleForPixelHeight(&Atlas->FontInfo, SizeInPixels);
    Atlas->StartCodepoint = kStartCodepointASCII;
    Atlas->EndCodepoint = kEndCodepointASCII;

    stbtt_GetFontVMetrics(&Atlas->FontInfo, &Atlas->Ascent, &Atlas->Descent, &Atlas->LineGap);
    Atlas->Ascent *= Atlas->Scale;
    Atlas->Descent *= Atlas->Scale;
    Atlas->LineGap *= Atlas->Scale;

    Atlas->Glyphs.Size = kGlyphCount;
    Atlas->Glyphs.Data = new GlyphInfo[kGlyphCount];

    constexpr int kPixelSize = 4;
    Atlas->Contents.Size = SQUARE(kMaxAtlasSideSizeInPixels) * kPixelSize;
    Atlas->Contents.Data = new uint8_t[Atlas->Contents.Size];
    assert(Atlas->Contents.Data);

    Atlas->RenderId = GetRenderId();
    if (Atlas->RenderId == -1)
    {
        RNDR_LOG_ERROR("Failed to obtain render id!");
        return nullptr;
    }

    constexpr uint32_t kFullyTransparentWhite = 0x00FFFFFF;
    for (int i = 0; i < SQUARE(kMaxAtlasSideSizeInPixels); i++)
    {
        uint32_t* Ptr = (uint32_t*)Atlas->Contents.Data;
        Ptr[i] = kFullyTransparentWhite;
    }

    uint32_t* Ptr = (uint32_t*)Atlas->Contents.Data;
    for (int Codepoint = Atlas->StartCodepoint; Codepoint < Atlas->EndCodepoint; Codepoint++)
    {
        const int GlyphIndex = Codepoint - Atlas->StartCodepoint;
        assert(GlyphIndex >= 0);
        GlyphInfo& Info = Atlas->Glyphs[GlyphIndex];
        Info.Codepoint = Codepoint;
        uint8_t* MonoData =
            stbtt_GetCodepointBitmap(&Atlas->FontInfo, 0, Atlas->Scale, Codepoint, &Info.Width, &Info.Height, &Info.OffsetX, &Info.OffsetY);
        assert(MonoData);
        int OffsetX = GlyphIndex % SideCount;
        int OffsetY = GlyphIndex / SideCount;

        for (int Y = 0; Y < Info.Height; Y++)
        {
            for (int X = 0; X < Info.Width; X++)
            {
                uint32_t Alpha = MonoData[Y * Info.Width + X];
                const int Index = (OffsetY * (int)SizeInPixels + Y) * kMaxAtlasSideSizeInPixels + (OffsetX * (int)SizeInPixels + X);
                Ptr[Index] = kFullyTransparentWhite | (Alpha << 24);
            }
        }

        stbtt_GetCodepointHMetrics(&Atlas->FontInfo, Codepoint, &Info.Advance, nullptr);
        Info.Advance *= Atlas->Scale;

        Info.UVStart.X = (OffsetX * SizeInPixels) / kMaxAtlasSideSizeInPixels;
        Info.UVStart.Y = (OffsetY * SizeInPixels) / kMaxAtlasSideSizeInPixels;
        Info.UVEnd.X = (OffsetX * SizeInPixels + Info.Width) / kMaxAtlasSideSizeInPixels;
        Info.UVEnd.Y = (OffsetY * SizeInPixels + Info.Height) / kMaxAtlasSideSizeInPixels;

        stbtt_FreeBitmap(MonoData, nullptr);
    }

    stbtt_GetCodepointHMetrics(&Atlas->FontInfo, ' ', &Atlas->SpaceAdvance, nullptr);
    Atlas->SpaceAdvance *= Atlas->Scale;

    UpdateRenderResource(Atlas->RenderId, Atlas->Contents);

    return Atlas;
}

int rndr::ui::GetMinGlyphCountPerSize(int GlyphSize, int GlyphCount, int SideInPixels)
{
    for (int i = 1;; i++)
    {
        if (GlyphCount <= SQUARE(i))
        {
            if (GlyphSize * i <= SideInPixels)
            {
                return i;
            }
            else
            {
                return 0;
            }
        }
    }
}

bool rndr::ui::ContainsCodepoint(FontHandle Font, int Codepoint)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        return false;
    }
    return Codepoint >= Atlas->StartCodepoint && Codepoint < Atlas->EndCodepoint;
}
