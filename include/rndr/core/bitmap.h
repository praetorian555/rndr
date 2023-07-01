#pragma once

#include "rndr/core/graphics-types.h"

namespace Rndr
{

/**
 * Helper class to store image data in the CPU memory.It can also be used to read values of
 * individual pixels as well as to change them.
 */
class Bitmap
{
public:
    /**
     * Create a bitmap with the specified width, height, pixel format and optional data.
     * @param width Width of the bitmap. Must be greater than 0.
     * @param height Height of the bitmap. Must be greater than 0.
     * @param depth Depth of the bitmap. Must be greater than 0.
     * @param pixel_format Pixel format of the bitmap. Must be supported.
     * @param data Optional data to initialize the bitmap with. If not specified, the bitmap will be
     * initialized with zeros.
     */
    Bitmap(int width, int height, int depth, PixelFormat pixel_format, const ByteSpan& data = ByteSpan());

    /**
     * Check if bitmap is valid.
     * @return Returns true if bitmap is valid, false otherwise.
     */
    [[nodiscard]] bool IsValid() const { return m_width > 0 && m_height > 0 && m_depth > 0; }

    [[nodiscard]] int GetWidth() const { return m_width; }
    [[nodiscard]] int GetHeight() const { return m_height; }
    [[nodiscard]] int GetDepth() const { return m_depth; }
    [[nodiscard]] PixelFormat GetPixelFormat() const { return m_pixel_format; }

    /**
     * Get number of components per pixel.
     * @return Returns number of components per pixel.
     */
    [[nodiscard]] int GetComponentCount() const { return m_comp_count; }

    /**
     * Get size of pixel in bytes.
     * @return Returns size of pixel in bytes.
     */
    [[nodiscard]] size_t GetPixelSize() const;

    /**
     * Get size of row in bytes.
     * @return Returns size of row in bytes.
     */
    [[nodiscard]] size_t GetRowSize() const { return m_width * GetPixelSize(); }

    /**
     * Get size of the bitmap in bytes but only of the first plane.
     * @return Returns size in bytes.
     */
    [[nodiscard]] size_t GetSize2D() const { return m_width * m_height * GetPixelSize(); }

    /**
     * Get size of the bitmap in bytes including depth.
     * @return Returns size in bytes.
     */
    [[nodiscard]] size_t GetSize3D() const { return m_width * m_height * m_depth * GetPixelSize(); }

    [[nodiscard]] uint8_t* GetData() { return m_data.data(); }
    [[nodiscard]] const uint8_t* GetData() const { return m_data.data(); }

    [[nodiscard]] math::Vector4 GetPixel(int x, int y, int z = 0) const;
    void SetPixel(int x, int y, int z, const math::Vector4& pixel);

    /**
     * Helper function used to check if given pixel format can be used for creating a bitmap.
     * @param pixel_format Pixel format to check.
     * @return Returns true if pixel format is supported, false otherwise.
     */
    [[nodiscard]] static bool IsPixelFormatSupported(PixelFormat pixel_format);

private:
    [[nodiscard]] math::Vector4 GetPixelUnsignedByte(int x, int y, int z) const;
    void SetPixelUnsignedByte(int x, int y, int z, const math::Vector4& pixel);

    [[nodiscard]] math::Vector4 GetPixelFloat(int x, int y, int z) const;
    void SetPixelFloat(int x, int y, int z, const math::Vector4& pixel);

    using GetPixelFunc = math::Vector4 (Bitmap::*)(int x, int y, int z) const;
    using SetPixelFunc = void (Bitmap::*)(int x, int y, int z, const math::Vector4& pixel);

    int m_width = 0;
    int m_height = 0;
    int m_depth = 0;
    int m_comp_count = 0;
    PixelFormat m_pixel_format = PixelFormat::R32_TYPELESS;
    Array<uint8_t> m_data;
    GetPixelFunc m_get_pixel_func = nullptr;
    SetPixelFunc m_set_pixel_func = nullptr;
};

}  // namespace Rndr