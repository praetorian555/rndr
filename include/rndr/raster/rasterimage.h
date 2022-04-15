#pragma once

#include <functional>

#include "rndr/core/base.h"
#include "rndr/core/bounds2.h"
#include "rndr/core/colors.h"
#include "rndr/core/math.h"
#include "rndr/core/pipeline.h"

#if defined RNDR_RASTER

namespace rndr
{

class Window;
struct Color;
class Sampler2D;

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
     * Set value of a pixel.
     * @tparam T Value type. Use Vector4r for colors, real for depth and uint32_t for stencil value.
     * @param Location Pixel position in screen space where origin is in bottom left corner.
     * @param Value Value to set in pixel. If this is a color value make sure that it is in linear space.
     */
    template <typename T>
    void SetPixelValue(const Point2i& Location, const T& Value);
    template <typename T>
    void SetPixelValue(int X, int Y, const T& Value);

    /**
     * Get color of specified pixel in linear space.
     */
    Vector4r GetPixelColor(const Point2i& Location) const;
    Vector4r GetPixelColor(int X, int Y) const;

    /**
     * Get depth of a specified pixel.
     */
    real GetPixelDepth(const Point2i& Location) const;
    real GetPixelDepth(int X, int Y) const;

    /**
     * Get stencil value of a specified pixel.
     */
    uint8_t GetStencilValue(const Point2i& Location) const;
    uint8_t GetStencilValue(int X, int Y) const;

    /**
     * Clear the image with specified value.
     * @tparam T Value type. Use Vector4r for colors, real for depth and uint32_t for stencil value.
     * @param Value Value to set in pixel. If this is a color value make sure that it is in linear space.
     */
    template <typename T>
    void Clear(const T& Value);

protected:
    void SetPixelColor(const Point2i& Location, const Vector4r& Color);
    void SetPixelColor(const Point2i& Position, uint32_t Color);
    void SetPixelDepth(const Point2i& Location, real Depth);
    void SetPixelStencilValue(const Point2i& Location, uint8_t Value);

    void ClearColor(const Vector4r& Color);
    void ClearDepth(real Depth);
    void ClearStencil(uint8_t Value);

    void GenerateMipMaps();

private:
    friend struct Sampler2D;

private:
    ImageConfig m_Config;

    Bounds2i m_Bounds;
    std::vector<uint8_t> m_Buffer;

    std::vector<Image*> m_MipMaps;
};

// Implementations
template <typename T>
void Image::SetPixelValue(const Point2i& Location, const T& Value)
{
    assert(false);
}

template <typename T>
void Image::SetPixelValue(int X, int Y, const T& Value)
{
    assert(false);
}

template <typename T>
void Image::Clear(const T& Value)
{
    assert(false);
}

template <>
void Image::SetPixelValue<Vector4r>(const Point2i& Location, const Vector4r& Value);
template <>
void Image::SetPixelValue<real>(const Point2i& Location, const real& Value);
template <>
void Image::SetPixelValue<uint8_t>(const Point2i& Location, const uint8_t& Value);

template <>
void Image::SetPixelValue<Vector4r>(int X, int Y, const Vector4r& Value);
template <>
void Image::SetPixelValue<real>(int X, int Y, const real& Value);
template <>
void Image::SetPixelValue<uint8_t>(int X, int Y, const uint8_t& Value);

template <>
void Image::Clear<Vector4r>(const Vector4r& Value);
template <>
void Image::Clear<real>(const real& Value);
template <>
void Image::Clear<uint8_t>(const uint8_t& Value);


}  // namespace rndr

#endif  // RNDR_RASTER
