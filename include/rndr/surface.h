#pragma once

#include <functional>

#include "rndr/math/math.h"
#include "rndr/rndr.h"

namespace rndr
{

class Window;
struct Color;

struct PixelShaderInfo
{
    Point2r Position;
    real Barycentric[3];
};

using PixelShaderCallback = std::function<Color(const PixelShaderInfo&)>;

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
     * @param Color The format of color needs to be 0xXXRRGGBB. Color needs to be in linear space.
     */
    void SetPixel(const Point2i& Location, uint32_t Color);

    /**
     * Colors a pixel at specified location.
     *
     * @param Location Location of a pixel in screen space.
     * @param Color Color object in linear space.
     */
    void SetPixel(const Point2i& Location, rndr::Color Color);

    /**
     * Colors a pixel at (X, Y) location.
     *
     * @param X Coordinate along the X axis.
     * @param Y Coordinate along the Y axis.
     * @param Color The format of color needs to be 0xXXRRGGBB. Color needs to be in linear space.
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
     * Color a block that starts at BottomLeft of size Size with Color.
     */
    void RenderBlock(const Vector2i& BottomLeft, const Vector2i& Size, uint32_t Color);

    /**
     * Render triangle defined by Points. Note that points should be specified in counter-clockwise
     * direction.
     *
     * @param Points Array of three points in the surface's coordinate space.
     * @param Callback Function that is invoked for every pixel that is inside the triangle.
     * Function should return the calculated color of a pixel.
     */
    void RenderTriangle(const Point2r (&Points)[3], PixelShaderCallback Callback);

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