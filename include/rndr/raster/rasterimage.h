#pragma once

#include <functional>

#include "rndr/core/base.h"
#include "rndr/core/bounds2.h"
#include "rndr/core/color.h"
#include "rndr/core/math.h"
#include "rndr/core/pipeline.h"

#if defined RNDR_RASTER

namespace rndr
{

class Window;
struct Color;

struct ImageConfig
{
    int Width = 1024;
    int Height = 768;

    rndr::PixelLayout PixelLayout = rndr::PixelLayout::A8R8G8B8;
    rndr::GammaSpace GammaSpace = rndr::GammaSpace::GammaCorrected;

    rndr::ImageFiltering MagFilter = rndr::ImageFiltering::NearestNeighbor;
    rndr::ImageFiltering MinFilter = rndr::ImageFiltering::NearestNeighbor;

    int LODBias = 0;
};

/**
 * Represents a memory buffer to which user can render.
 *
 * Image uses coordinate system where origin is at the bottom-left point of the screen, X grows to
 * the right and Y grows upward.
 */
class Image
{
public:
    Image(const ImageConfig& Options = ImageConfig{});
    Image(const std::string& FilePath, const ImageConfig& Options = ImageConfig{});

    ~Image();

    /**
     * Change the ordering and/or a size of channels in a pixel as well as gamma space.
     */
    void SetPixelFormat(rndr::GammaSpace Space, rndr::PixelLayout Layout);

    /**
     * Get data buffer.
     *
     * @return Returns a pointer to the byte array.
     */
    uint8_t* GetBuffer() { return m_Buffer.data(); }

    /**
     * Get image configurations.
     *
     * @return Returns read-only image config.
     */
    const ImageConfig& GetConfig() const { return m_Config; }

    /**
     * Get Bounds2 object consisting of (0, 0) and (Width - 1, Height - 1) points.
     */
    const Bounds2i& GetBounds() const { return m_Bounds; }

    /**
     * Get aspect ratio of a surface. If the height is zero the ratio will be 1.
     */
    real GetAspectRatio() const;

    /**
     * Get the size of a pixel.
     *
     * @return Returns the size of a pixel in bytes.
     */
    uint32_t GetPixelSize() const;

    /**
     * Get color of specified pixel.
     */
    rndr::Color GetPixelColor(const Point2i& Location) const;
    rndr::Color GetPixelColor(int X, int Y) const;

    /**
     * Get depth of a specified pixel.
     */
    real GetPixelDepth(const Point2i& Location) const;
    real GetPixelDepth(int X, int Y) const;

    /**
     * Colors a pixel at specified location.
     *
     * @param Location Location of a pixel in screen space.
     * @param Color New pixel color.
     */
    void SetPixelColor(const Point2i& Location, rndr::Color Color);

    /**
     * Colors a pixel at (X, Y) location.
     *
     * @param X Coordinate along the X axis.
     * @param Y Coordinate along the Y axis.
     * @param Color New pixel color.
     */
    void SetPixelColor(int X, int Y, rndr::Color Color);

    /**
     * Set pixel depth to the specified value.
     */
    void SetPixelDepth(const Point2i& Location, real Depth);
    void SetPixelDepth(int X, int Y, real Depth);

    /**
     * Copies content from Source to this image.
     *
     * @param Source Source image.
     * @param BottomLeft Bottom left point to which to copy the Source image.
     */
    void RenderImage(const rndr::Image& Source, const Point2i& BottomLeft);

    /**
     * Clear the buffer with specified value.
     *
     * @note Valid if image is used a color buffer.
     */
    void ClearColorBuffer(rndr::Color Color);

    /**
     * Clear the buffer with specified value.
     *
     * @note Valid if image is used a depth buffer.
     */
    void ClearDepthBuffer(real ClearValue);

    // This is done by default if we use TrilinearInterpolation for minification filter but any
    // change in data will not trigger the generation again.
    void GenerateMipMaps();

    /**
     * Calculate image sample based on UV coordinates.
     */
    rndr::Color Sample(const Point2r& TexCoord, const Vector2r& duvdx, const Vector2r& duvdy);

    static Color SampleNearestNeighbor(const Image* I, const Point2r& TexCoord);
    static Color SampleBilinear(const Image* I, const Point2r& TexCoord);
    static Color SampleTrilinear(const Image* I, const Point2r& TexCoord, real LOD);

protected:
    void SetPixelColor(const Point2i& Position, uint32_t Color);
    void SetPixelColor(int X, int Y, uint32_t Color);

private:
    ImageConfig m_Config;

    Bounds2i m_Bounds;
    std::vector<uint8_t> m_Buffer;

    std::vector<Image*> m_MipMaps;
};

}  // namespace rndr

#endif // RNDR_RASTER
