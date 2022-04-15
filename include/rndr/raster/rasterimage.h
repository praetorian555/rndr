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
    PixelLayout PixelLayout = PixelLayout::A8R8G8B8;
    GammaSpace GammaSpace = GammaSpace::GammaCorrected;

    ImageFiltering MagFilter = ImageFiltering::Point;
    ImageFiltering MinFilter = ImageFiltering::Point;
    ImageFiltering MipFilter = ImageFiltering::Linear;
    bool bUseMips = false;

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
    Image(int Width, int Height, const ImageConfig& Options = ImageConfig{});
    Image(const std::string& FilePath, const ImageConfig& Options = ImageConfig{});

    ~Image();

    /**
     * Change the ordering and/or a size of channels in a pixel as well as gamma space.
     */
    void SetPixelFormat(rndr::GammaSpace Space, rndr::PixelLayout Layout);

    // Getters
    uint8_t* GetBuffer() { return m_Buffer.data(); }
    const ImageConfig& GetConfig() const { return m_Config; }
    const Bounds2i& GetBounds() const { return m_Bounds; }
    real GetAspectRatio() const;
    uint32_t GetPixelSize() const;
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

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
     * Get pixel color in linear space.
     */
    Vector4r GetPixelColor(const Point2i& Location) const;
    Vector4r GetPixelColor(int X, int Y) const;

    real GetPixelDepth(const Point2i& Location) const;
    real GetPixelDepth(int X, int Y) const;

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
    int m_Width, m_Height;

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
