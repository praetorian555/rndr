#pragma once

#include <string>
#include <vector>

#include "stb_truetype/stb_truetype.h"

#include "math/point2.h"

#include "rndr/core/span.h"

struct Glyph
{
    int CodePoint;

    float Scale;

    int Width;
    int Height;
    int OffsetX;
    int OffsetY;

    math::Point2 TexBottomLeft;
    math::Point2 TexTopRight;

    rndr::ByteSpan SDF;
};

struct Font
{
    Font() = default;

    bool Init(const std::string& Name, const std::string& AssetPath);

    static int MakeGlyphHash(int CodePoint, int SizeInPixels, int FontId);
    static int GetCodePointFromHash(int Hash);
    static int GetSizeInPixelsFromHash(int Hash);
    static int GetFontIdFromHash(int Hash);

    std::string Name;
    int Id;

    stbtt_fontinfo TTInfo;
    int Ascent;
    int Descent;
    int LineGap;

private:
    static int GenerateId();

    rndr::ByteArray FontFileContents;
};
