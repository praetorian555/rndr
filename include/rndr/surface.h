#pragma once

#include "rndr/rndr.h"
#include "rndr/math/math.h"

namespace rndr
{

class Window;

/**
 * Represents a memory buffer to which user can render.
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

private:
    uint32_t m_Width = 0, m_Height = 0;
    uint32_t m_PixelSize = 4;
    uint8_t* m_ColorBuffer = nullptr;
};

}  // namespace rndr