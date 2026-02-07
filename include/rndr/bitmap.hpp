#pragma once

#include "rndr/graphics-types.hpp"
#include "rndr/pixel-format.hpp"

namespace Rndr
{

/**
 * Helper class to store image data in the CPU memory including the mips if provided. It can also be used to read values of individual
 * pixels as well as to change them.
 */
class Bitmap
{
public:
    /**
     * Create an empty bitmap.
     */
    Bitmap() = default;

    /**
     * Create a bitmap with the specified width, height, pixel format and optional data.
     * @param width Width of the bitmap. Must be greater than 0.
     * @param height Height of the bitmap. Must be greater than 0.
     * @param depth Depth of the bitmap. Must be greater than 0.
     * @param pixel_format Pixel format of the bitmap. Must be supported.
     * @param mip_count How many mip maps there are for this bitmap. Default is 1.
     * @param data Optional data to initialize the bitmap with. If not specified, the bitmap will be
     * initialized with zeros.
     */
    Bitmap(i32 width, i32 height, i32 depth, PixelFormat pixel_format, i32 mip_count, const Opal::ArrayView<u8>& data = {});

    void GenerateMips();

    /**
     * Check if bitmap is valid.
     * @return Returns true if bitmap is valid, false otherwise.
     */
    [[nodiscard]] bool IsValid() const { return m_width > 0 && m_height > 0 && m_depth > 0; }

    [[nodiscard]] i32 GetWidth() const { return m_width; }
    [[nodiscard]] i32 GetHeight() const { return m_height; }
    [[nodiscard]] i32 GetDepth() const { return m_depth; }
    [[nodiscard]] PixelFormat GetPixelFormat() const { return m_pixel_format; }
    [[nodiscard]] u32 GetMipCount() const { return m_mip_count; }

    /**
     * Get number of components per pixel.
     * @return Returns number of components per pixel.
     */
    [[nodiscard]] i32 GetComponentCount() const { return m_comp_count; }

    /**
     * Get size of pixel in bytes.
     * @return Returns size of pixel in bytes.
     */
    [[nodiscard]] u64 GetPixelSize() const;

    /**
     * Get size of row in bytes.
     * @return Returns size of row in bytes.
     */
    [[nodiscard]] u64 GetRowSize(i32 mip_level = 0) const;

    /**
     * Get size of the bitmap in bytes but only of the first plane.
     * @return Returns size in bytes.
     */
    [[nodiscard]] u64 GetSize2D(i32 mip_level = 0) const;

    /**
     * Get size of the bitmap in bytes including depth.
     * @return Returns size in bytes.
     */
    [[nodiscard]] u64 GetSize3D(i32 mip_level = 0) const;

    /**
     * Get bitmap size including depth and all mip levels.
     * @return Return size in bytes.
     */
    [[nodiscard]] u64 GetTotalSize() const;

    /**
     * Offset in the pixel array where the specified mip level starts.
     * @param mip_level Mip level.
     * @return Offset in bytes.
     */
    [[nodiscard]] u64 GetMipLevelOffset(i32 mip_level = 0) const;

    [[nodiscard]] u8* GetData() { return m_data.GetData(); }
    [[nodiscard]] const u8* GetData() const { return m_data.GetData(); }

    [[nodiscard]] Vector4f GetPixel(i32 x, i32 y, i32 z = 0, i32 mip_level = 0) const;
    void SetPixel(i32 x, i32 y, i32 z, i32 mip_level, const Vector4f& pixel);

    /**
     * Helper function used to check if current pixel format can be used to individually access pixels on CPU side.
     * @param pixel_format Pixel format to check.
     * @return Returns true if pixel format is supported, false otherwise.
     */
    [[nodiscard]] static bool IsPixelFormatSupported(PixelFormat pixel_format);

private:
    [[nodiscard]] Vector4f GetPixelUnsignedByte(i32 x, i32 y, i32 z, i32 mip_level) const;
    void SetPixelUnsignedByte(i32 x, i32 y, i32 z, i32 mip_level, const Vector4f& pixel);

    [[nodiscard]] Vector4f GetPixelUnsignedShort(i32 x, i32 y, i32 z, i32 mip_level) const;
    void SetPixelUnsignedShort(i32 x, i32 y, i32 z, i32 mip_level, const Vector4f& pixel);

    [[nodiscard]] Vector4f GetPixelFloat(i32 x, i32 y, i32 z, i32 mip_level) const;
    void SetPixelFloat(i32 x, i32 y, i32 z, i32 mip_level, const Vector4f& pixel);

    using GetPixelFunc = Vector4f (Bitmap::*)(i32 x, i32 y, i32 z, i32 mip_level) const;
    using SetPixelFunc = void (Bitmap::*)(i32 x, i32 y, i32 z, i32 mip_level, const Vector4f& pixel);

    i32 m_width = 0;
    i32 m_height = 0;
    i32 m_depth = 0;
    i32 m_mip_count = 1;
    i32 m_comp_count = 0;
    PixelFormat m_pixel_format = PixelFormat::Undefined;
    Opal::DynamicArray<u8> m_data;
    GetPixelFunc m_get_pixel_func = nullptr;
    SetPixelFunc m_set_pixel_func = nullptr;
};

}  // namespace Rndr
