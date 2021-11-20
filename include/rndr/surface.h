#pragma once

#include "rndr/math/math.h"
#include "rndr/rndr.h"

namespace rndr
{

class Window;

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
     * Colors a pixel at (X, Y) location.
     *
     * @param Location Pixel coordinates.
     * @param Color The format of color needs to be 0xXXRRGGBB.
     */
    void SetPixel(const Vector2i& Location, uint32_t Color);

    /**
     * Colors a pixel at (X, Y) location.
     *
     * @param X Coordinate along the X axis.
     * @param Y Coordinate along the Y axis.
     * @param Color The format of color needs to be 0xXXRRGGBB.
     */
    void SetPixel(int X, int Y, uint32_t Color);

    /**
     * Render a line between Start and End pixel locations.
     *
     * @param Start Location of a starting pixel.
     * @param End Location of a end pixel.
     * @param Color The format of color needs to be 0xXXRRGGBB.
     */
    void RenderLine(const Vector2i& Start, const Vector2i& End, uint32_t Color);

    /**
     * Clear the color buffer with specified color.
     *
     * @param Color The new color of the color buffer.
     */
    void ClearColorBuffer(uint32_t Color);

private:
    uint32_t m_Width = 0, m_Height = 0;
    uint32_t m_PixelSize = 4;
    uint8_t* m_ColorBuffer = nullptr;
};

}  // namespace rndr