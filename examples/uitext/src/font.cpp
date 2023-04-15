#include "font.h"

#include "rndr/core/fileutils.h"

bool Font::Init(const std::string& InName, const std::string& AssetPath)
{
    Name = InName;
    Id = GenerateId();

    FontFileContents = Rndr::file::ReadEntireFile(AssetPath);
    if (FontFileContents.empty())
    {
        return false;
    }
    const int Offset = stbtt_GetFontOffsetForIndex(FontFileContents.data(), 0);
    if (!stbtt_InitFont(&TTInfo, FontFileContents.data(), Offset))
    {
        return false;
    }
    stbtt_GetFontVMetrics(&TTInfo, &Ascent, &Descent, &LineGap);
    return true;
}

int Font::MakeGlyphHash(int CodePoint, int SizeInPixels, int FontId)
{
    // CodePoint: 16-bit
    // SizeInPixels: 10-bit
    // FontId: 6-bit
    return (CodePoint << 16) | (SizeInPixels << 6) | FontId;
}

int Font::GetCodePointFromHash(int Hash)
{
    constexpr int CodePointMask = 0xFFFF0000;
    return (Hash & CodePointMask) >> 16;
}

int Font::GetSizeInPixelsFromHash(int Hash)
{
    constexpr int SizeMask = 0x0000FFC0;
    return (Hash & SizeMask) >> 6;
}

int Font::GetFontIdFromHash(int Hash)
{
    constexpr int FontIdMask = 0x0000003F;
    return Hash & FontIdMask;
}

int Font::GenerateId()
{
    static int s_Generator = 1;
    return s_Generator++;
}
