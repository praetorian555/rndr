#pragma once

#include "rndr/core/color.h"
#include "rndr/core/math.h"
#include "rndr/core/rndr.h"

namespace rndr
{

struct ImageOptions
{
    PixelFormat PixelFormat;
    PixelLayout PixelLayout;
    uint32_t Width;
    uint32_t Height;
};

/**
 * Represents any image data loaded from a file.
 */
class Image
{
public:
    Image(const ImageOptions& Config, const uint8_t* Data);

    const uint8_t* GetData() const;
    uint8_t* GetData();

    const Color& GetTexel(const Point2i& Position) const;
    Color& GetTexel(const Point2i& Position);

    void SetTexel(const Point2i& Position, const rndr::Color& Color);

private:
    PixelFormat m_PixelFormat;
    PixelLayout m_PixelLayout;
    uint32_t m_Width;
    uint32_t m_Height;
    uint32_t m_PixelSize;

    uint8_t* m_Data;
};

}  // namespace rndr