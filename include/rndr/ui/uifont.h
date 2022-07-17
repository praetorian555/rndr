#pragma once

#include "math/point2.h"

namespace rndr
{
namespace ui
{

using FontId = int;
constexpr FontId kInvalidFontId = -1;

FontId AddFont(const char* FilePath, float FontSizeInPixels);
void RemoveFont(FontId Id);

bool ContainsFont(const char* FontName, float FontSizeInPixels);
bool ContainsFont(FontId Font);

// Returns the size in pixels of a specified font. If font is invalid function will return 0.
// This value is equal to the ascent + abs(descent).
int GetFontSize(FontId Font);

// Returns, in pixels how much to move the baseline origin after rendering glyph corresponding to the CurrentCodepoint.
// If data is not found returns 0. Its equal to the Advance + KernAdvance, where Advance is the default advance for CurrentCodepoint and
// KernAdvance is advance between CurrentCodepoint and NextCodepoint.
int GetGlyphAdvance(FontId Font, int CurrentCodepoint, int NextCodepoint);

// Returns how much to advance the baseline vertically. Returns 0 in case that Font is invalid.
int GetFontVerticalAdvance(FontId Font);

// Get distance between baseline and the lowest point of the font. Returns negative value or 0.
int GetFontDescent(FontId Font);

// Get distance between baseline and the highest point of the font. Returns positive value or 0.
int GetFontAscent(FontId Font);

// Returns the dimensions of the glyph image.
math::Vector2 GetGlyphSize(FontId Font, int Codepoint);

// Returns the offset of where the draw bottom left point of the glyph image relative to the baseline origin.
math::Vector2 GetGlyphBearing(FontId Font, int Codepoint);

void GetGlyphTexCoords(FontId Font, int Codepoint, math::Point2* BottomLeft, math::Point2* TopRight);

// The id of this font when it registered with the UI Render module.
int GetFontRenderId(FontId Font);

}  // namespace ui
}  // namespace rndr
