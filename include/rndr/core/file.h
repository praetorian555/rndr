#pragma once

#include "rndr/core/graphics-types.h"
#include "rndr/core/base.h"
#include "rndr/core/array.h"
#include "rndr/core/string.h"

namespace Rndr
{

/**
 * Helper struct to store image data in the CPU memory.
 */
struct CPUImage
{
    int width = 0;
    int height = 0;
    PixelFormat pixel_format = PixelFormat::R32_TYPELESS;
    Array<uint8_t> data;

    [[nodiscard]] bool IsValid() const { return !data.empty(); }
};

namespace File
{

/**
 * Reads the contents of the entire file in the binary form.
 *
 * @param file_path Absolute or relative path to the file on the disc.
 *
 * @return Returns a valid ByteArray object containing the file contents.
 */
[[nodiscard]] ByteArray ReadEntireFile(const String& file_path);

/**
 * Reads the contents of the entire file in the text form.
 * @param file_path Absolute or relative path to the file on the disc.
 * @return Returns a valid String object containing the text representing the file contents.
 */
[[nodiscard]] String ReadEntireTextFile(const String& file_path);

/**
 * Reads the contents of the entire image file into CPU memory.
 *
 * @param file_path Absolute or relative path to the file on the disc.
 *
 * @return Returns a valid CPUImage object containing the image info and data in the CPU memory. You
 * can check for CPUImage validity by calling IsValid on it.
 * @note Currently this supports regular image formats, not HDR. It will store pixels as RGBA in
 * gamma correct space (SRGB).
 */
[[nodiscard]] CPUImage ReadEntireImage(const String& file_path);

/**
 * Save image to the disc.
 *
 * @param image Image in the CPU memory to saved to the disc.
 * @param file_path Absolute or relative path to the file on the disc. Should include both the file
 * name and extension. Extension is used to figure out the format.
 *
 * @return Returns true in case of a success, false otherwise.
 */
bool SaveImage(const CPUImage& image, const String& file_path);

}  // namespace File

}  // namespace Rndr
