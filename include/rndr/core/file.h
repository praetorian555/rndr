#pragma once

//#include <cstdio>

#include "rndr/core/base.h"
#include "rndr/core/bitmap.h"
#include "rndr/core/containers/array.h"
#include "rndr/core/containers/scope-ptr.h"
#include "rndr/core/containers/string.h"
#include "rndr/core/graphics-types.h"

namespace Rndr
{

struct FileDeleter
{
    void operator()(FILE* file) const;
};

using ScopeFilePtr = ScopePtr<FILE, FileDeleter>;

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
 *
 * @param file_path Absolute or relative path to the file on the disc.
 *
 * @return Returns a valid String object containing the text representing the file contents.
 */
[[nodiscard]] String ReadEntireTextFile(const String& file_path);

/**
 * Reads the contents of the text file that contains a shader. The function will also resolve the includes
 * in the shader file.
 *
 * @param ref_path Reference path compared to which both shader_path and include paths will be resolved.
 * @param shader_path Path relative to the ref_path to the shader file.
 *
 * @return Returns a valid String object containing the text representing the shader contents. Returns empty string in case of an error.
 */
[[nodiscard]] String ReadShader(const String& ref_path, const String& shader_path);

/**
 * Prints the shader contents to the console.
 *
 * @param shader_contents String containing the shader contents.
 */
void PrintShader(const String& shader_contents);

/**
 * Reads the contents of the entire image file into CPU memory. The data is stored from top to
 * bottom, left to right, row by row.
 *
 * @param file_path Absolute or relative path to the file on the disc.
 * @param desired_format Format in which the image should be read. If the format is not supported,
 * the function will fail. Please use Bitmap::IsPixelFormatSupported to check if the format is
 * supported.
 * @param flip_vertically If true, the image will be flipped vertically.
 *
 * @return Returns a valid CPUImage object containing the image info and data in the CPU memory. You
 * can check for CPUImage validity by calling IsValid on it.
 */
[[nodiscard]] Bitmap ReadEntireImage(const String& file_path, PixelFormat desired_format, bool flip_vertically = false);

/**
 * Save bitmap to the disc. The data is stored from top to bottom, left to right, row by row.
 *
 * @param bitmap Image in the CPU memory to save to the disc.
 * @param file_path Absolute or relative path to the file on the disc. Should include both the file
 * name and extension. Extension is used to figure out the format.
 *
 * @return Returns true in case of a success, false otherwise.
 */
bool SaveImage(const Bitmap& bitmap, const String& file_path);

}  // namespace File

}  // namespace Rndr::File
