#pragma once

#include <functional>

#include "rndr/math/math.h"
#include "rndr/rndr.h"

namespace rndr
{

class Window;
struct Color;

/**
 * Represents a memory buffer to which user can render.
 *
 * Surface uses coordinate system where origin is at the bottom-left point of the screen, X grows to
 * the right and Y grows upward.
 */
class Surface
{
public:
    Surface() = default;

    /**
     * Will recreate internal memory buffer if Width or Height are different from existing ones.
     */
    void UpdateSize(int Width, int Height);

    /**
     * Get color buffer.
     *
     * @return Returns a pointer to the color buffer.
     */
    uint8_t* GetColorBuffer() { return m_ColorBuffer; }

    /**
     * Get the width of the surface.
     *
     * @return Returns width.
     */
    uint32_t GetWidth() const { return m_Width; }

    /**
     * Get the height of the surface.
     *
     * @return Returns height.
     */
    uint32_t GetHeight() const { return m_Height; }

    /**
     * Get the size of a pixel.
     *
     * @return Returns the size of a pixel in bytes.
     */
    uint32_t GetPixelSize() const { return m_PixelSize; }

    /**
     * Colors a pixel at specified location.
     *
     * @param Location Location of a pixel in screen space.
     * @param Color New pixel color.
     */
    void SetPixel(const Point2i& Location, rndr::Color Color);

    /**
     * Colors a pixel at (X, Y) location.
     *
     * @param X Coordinate along the X axis.
     * @param Y Coordinate along the Y axis.
     * @param Color New pixel color.
     */
    void SetPixel(int X, int Y, rndr::Color Color);

    /**
     * Render a line between Start and End pixel locations.
     *
     * @param Start Location of a starting pixel.
     * @param End Location of a end pixel.
     * @param Color The format of color needs to be 0xXXRRGGBB.
     */
    void RenderLine(const Point2i& Start, const Point2i& End, rndr::Color Color);

    /**
     * Color a block that starts at BottomLeft of size Size with Color.
     */
    void RenderBlock(const Point2i& BottomLeft, const Point2i& Size, rndr::Color Color);

    /**
     * Clear the color buffer with specified color.
     *
     * @param Color The new color of the color buffer.
     */
    void ClearColorBuffer(rndr::Color Color);

private:
    uint32_t m_Width = 0, m_Height = 0;
    uint32_t m_PixelSize = 4;
    uint8_t* m_ColorBuffer = nullptr;
};

}  // namespace rndr