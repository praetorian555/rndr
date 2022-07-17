#include "rndr/ui/uifont.h"

#include <filesystem>

#include <stb_truetype/stb_truetype.h>

#include "rndr/core/fileutils.h"
#include "rndr/core/log.h"
#include "rndr/core/span.h"

#include "rndr/ui/uisystem.h"

#define SQUARE(x) ((x) * (x))

namespace rndr
{
namespace ui
{

// Private types

struct GlyphInfo
{
    uint32_t Codepoint = 0;
    int Width = 0;
    int Height = 0;
    int OffsetX = 0;
    int OffsetY = 0;
    int Advance = 0;
    math::Point2 UVStart;
    math::Point2 UVEnd;
};

struct AtlasInfo
{
    std::string FontName;
    float FontSizeInPixels;

    FontId Id;

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
static constexpr int kStartCodepointASCII = 33;
static constexpr int kEndCodepointASCII = 127;

// Module private data
extern UIProperties g_UIProps;

// Private data
static Span<AtlasInfo*> g_Atlases;

// Main module functions
RenderId AllocateRenderId();
void FreeRenderId(RenderId Id);
void UpdateRenderResource(int RenderId, ByteSpan Contents, int Width, int Height);

// Module private functions
bool InitFont();
void ShutDownFont();

// Private functions
static AtlasInfo* GetAtlas(const char* FontName, float FontSize);
static AtlasInfo* GetAtlas(FontId Id);
static AtlasInfo* CreateAtlas(const std::string& FontPath, float SizeInPixels);
static int GetMinGlyphCountPerSize(int GlyphSize, int GlyphCount, int SideInPixels);
static bool ContainsCodepoint(FontId Font, int Codepoint);

}  // namespace ui
}  // namespace rndr

bool rndr::ui::InitFont()
{
    assert(g_UIProps.MaxAtlasCount > 0);
    g_Atlases.Size = g_UIProps.MaxAtlasCount;
    g_Atlases.Data = new AtlasInfo*[g_Atlases.Size];

    for (int i = 0; i < g_Atlases.Size; i++)
    {
        g_Atlases[i] = nullptr;
    }

    return true;
}

void rndr::ui::ShutDownFont()
{
    for (int i = 0; i < g_Atlases.Size; i++)
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
    delete[] g_Atlases.Data;
}

rndr::ui::FontId rndr::ui::AddFont(const char* FilePath, float FontSizeInPixels)
{
    std::filesystem::path FontPath(FilePath);
    if (FontPath.extension() != ".ttf")
    {
        RNDR_LOG_ERROR("We only support .ttf font files!");
        return kInvalidFontId;
    }
    std::string FontName = FontPath.stem().string();
    AtlasInfo* Atlas = GetAtlas(FontName.c_str(), FontSizeInPixels);
    if (Atlas)
    {
        RNDR_LOG_INFO("Font with a name %s and a pixel size %d already present...", FontName.c_str(), FontSizeInPixels);
        return Atlas->Id;
    }
    FontId FreeId = kInvalidFontId;
    for (int i = 0; i < g_Atlases.Size; i++)
    {
        if (!g_Atlases[i])
        {
            FreeId = i;
            break;
        }
    }
    if (FreeId == kInvalidFontId)
    {
        RNDR_LOG_ERROR("Max number of supported fonts reached, supported %d fonts!", g_Atlases.Size);
        return kInvalidFontId;
    }
    Atlas = CreateAtlas(FontPath.string(), FontSizeInPixels);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Failed to create a font atlas from font file %s!", FilePath);
        return kInvalidFontId;
    }
    Atlas->Id = FreeId;
    g_Atlases[FreeId] = Atlas;
    RNDR_LOG_INFO("Successfully added font %s with pixel size %d with font id %d", FontName.c_str(), (int)FontSizeInPixels, FreeId);
    return Atlas->Id;
}

void rndr::ui::RemoveFont(FontId Id)
{
    if (Id < 0 || Id >= g_Atlases.Size)
    {
        return;
    }
    AtlasInfo* Atlas = g_Atlases[Id];
    if (!Atlas)
    {
        return;
    }
    FreeRenderId(Atlas->RenderId);
    delete[] Atlas->Contents.Data;
    delete[] Atlas->Glyphs.Data;
    delete[] Atlas->FontInfo.data;
    delete Atlas;
    g_Atlases[Id] = nullptr;
}

bool rndr::ui::ContainsFont(const char* FontName, float FontSizeInPixels)
{
    return GetAtlas(FontName, FontSizeInPixels) != nullptr;
}

bool rndr::ui::ContainsFont(FontId Font)
{
    return GetAtlas(Font) != nullptr;
}

int rndr::ui::GetFontSize(FontId Font)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font id %d doesn't correspond to any font, did you call AddFont?");
        return 0;
    }
    return Atlas->Ascent + std::abs(Atlas->Descent);
}

int rndr::ui::GetGlyphAdvance(FontId Font, int CurrentCodepoint, int NextCodepoint)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font id %d doesn't correspond to any font, did you call AddFont?");
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

int rndr::ui::GetFontVerticalAdvance(FontId Font)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font id %d doesn't correspond to any font, did you call AddFont?");
        return 0;
    }
    return Atlas->Ascent + std::abs(Atlas->Descent) + Atlas->LineGap;
}

int rndr::ui::GetFontDescent(FontId Font)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font id %d doesn't correspond to any font, did you call AddFont?");
        return 0;
    }
    return Atlas->Descent;
}

int rndr::ui::GetFontAscent(FontId Font)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font id %d doesn't correspond to any font, did you call AddFont?");
        return 0;
    }
    return Atlas->Ascent;
}

math::Vector2 rndr::ui::GetGlyphSize(FontId Font, int Codepoint)
{
    if (!ContainsCodepoint(Font, Codepoint))
    {
        return math::Vector2{};
    }
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font id %d doesn't correspond to any font, did you call AddFont?");
        return math::Vector2{};
    }
    GlyphInfo* Info = &Atlas->Glyphs[Codepoint - Atlas->StartCodepoint];
    return math::Vector2{(float)Info->Width, (float)Info->Height};
}

math::Vector2 rndr::ui::GetGlyphBearing(FontId Font, int Codepoint)
{
    if (!ContainsCodepoint(Font, Codepoint))
    {
        return math::Vector2{};
    }
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font id %d doesn't correspond to any font, did you call AddFont?");
        return math::Vector2{};
    }
    GlyphInfo* Info = &Atlas->Glyphs[Codepoint - Atlas->StartCodepoint];
    return math::Vector2{(float)Info->OffsetX, (float)-(Info->Height + Info->OffsetY)};
}

void rndr::ui::GetGlyphTexCoords(FontId Font, int Codepoint, math::Point2* BottomLeft, math::Point2* TopRight)
{
    if (!ContainsCodepoint(Font, Codepoint))
    {
        return;
    }
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font id %d doesn't correspond to any font, did you call AddFont?");
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

int rndr::ui::GetFontRenderId(FontId Id)
{
    AtlasInfo* Atlas = GetAtlas(Id);
    if (!Atlas)
    {
        RNDR_LOG_ERROR("Font id %d doesn't correspond to any font, did you call AddFont?");
        return kInvalidRenderId;
    }
    return Atlas->RenderId;
}

rndr::ui::AtlasInfo* rndr::ui::GetAtlas(const char* FontName, float FontSize)
{
    for (int i = 0; i < g_Atlases.Size; i++)
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

rndr::ui::AtlasInfo* rndr::ui::GetAtlas(FontId Id)
{
    if (Id < 0 || Id >= g_Atlases.Size)
    {
        RNDR_LOG_ERROR("Invalid id!");
        return nullptr;
    }
    return g_Atlases[Id];
}

rndr::ui::AtlasInfo* rndr::ui::CreateAtlas(const std::string& FontPath, float SizeInPixels)
{
    constexpr int kGlyphCount = kEndCodepointASCII - kStartCodepointASCII;

    int SideCount = GetMinGlyphCountPerSize(SizeInPixels, kGlyphCount, g_UIProps.MaxImageSideSize);
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
    Atlas->Contents.Size = SQUARE(g_UIProps.MaxImageSideSize) * kPixelSize;
    Atlas->Contents.Data = new uint8_t[Atlas->Contents.Size];
    assert(Atlas->Contents.Data);

    Atlas->RenderId = AllocateRenderId();
    if (Atlas->RenderId == -1)
    {
        RNDR_LOG_ERROR("Failed to obtain render id!");
        return nullptr;
    }

    constexpr uint32_t kFullyTransparentWhite = 0x00FFFFFF;
    for (int i = 0; i < SQUARE(g_UIProps.MaxImageSideSize); i++)
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
                const int Index = (OffsetY * (int)SizeInPixels + Y) * g_UIProps.MaxImageSideSize + (OffsetX * (int)SizeInPixels + X);
                Ptr[Index] = kFullyTransparentWhite | (Alpha << 24);
            }
        }

        stbtt_GetCodepointHMetrics(&Atlas->FontInfo, Codepoint, &Info.Advance, nullptr);
        Info.Advance *= Atlas->Scale;

        Info.UVStart.X = (OffsetX * SizeInPixels) / g_UIProps.MaxImageSideSize;
        Info.UVStart.Y = (OffsetY * SizeInPixels) / g_UIProps.MaxImageSideSize;
        Info.UVEnd.X = (OffsetX * SizeInPixels + Info.Width) / g_UIProps.MaxImageSideSize;
        Info.UVEnd.Y = (OffsetY * SizeInPixels + Info.Height) / g_UIProps.MaxImageSideSize;

        stbtt_FreeBitmap(MonoData, nullptr);
    }

    stbtt_GetCodepointHMetrics(&Atlas->FontInfo, ' ', &Atlas->SpaceAdvance, nullptr);
    Atlas->SpaceAdvance *= Atlas->Scale;

    UpdateRenderResource(Atlas->RenderId, Atlas->Contents, g_UIProps.MaxImageSideSize, g_UIProps.MaxImageSideSize);

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

bool rndr::ui::ContainsCodepoint(FontId Font, int Codepoint)
{
    AtlasInfo* Atlas = GetAtlas(Font);
    if (!Atlas)
    {
        return false;
    }
    return Codepoint >= Atlas->StartCodepoint && Codepoint < Atlas->EndCodepoint;
}
