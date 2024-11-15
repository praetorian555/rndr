#pragma once

#include <cstdio>

#include "opal/container/string.h"
#include "opal/container/dynamic-array.h"

#include "rndr/bitmap.h"
#include "rndr/graphics-types.h"

namespace Rndr
{

class FileHandler
{
public:
    FileHandler(const char* file_path, const char* mode);
    ~FileHandler();

    FileHandler(const FileHandler&) = delete;
    FileHandler& operator=(const FileHandler&) = delete;

    FileHandler(FileHandler&&) = delete;
    FileHandler&& operator=(FileHandler&&) = delete;

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] bool IsEOF() const;

    bool Read(void* buffer, size_t element_size, size_t element_count);
    bool Write(const void* buffer, size_t element_size, size_t element_count);

private:
    struct _iobuf* m_file_handle = 0;
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
[[nodiscard]] Opal::DynamicArray<u8> ReadEntireFile(const Opal::StringUtf8& file_path);

/**
 * Reads the contents of the entire file in the text form.
 *
 * @param file_path Absolute or relative path to the file on the disc.
 *
 * @return Returns a valid String object containing the text representing the file contents.
 */
[[nodiscard]] Opal::StringUtf8 ReadEntireTextFile(const Opal::StringUtf8& file_path);

/**
 * Reads the contents of the text file that contains a shader. The function will also resolve the includes
 * in the shader file.
 *
 * @param ref_path Reference path compared to which both shader_path and include paths will be resolved.
 * @param shader_path Path relative to the ref_path to the shader file.
 *
 * @return Returns a valid String object containing the text representing the shader contents. Returns empty string in case of an error.
 */
[[nodiscard]] Opal::StringUtf8 ReadShader(const Opal::StringUtf8& ref_path, const Opal::StringUtf8& shader_path);

/**
 * Prints the shader contents to the console.
 *
 * @param shader_contents String containing the shader contents.
 */
void PrintShader(const Opal::StringUtf8& shader_contents);

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
[[nodiscard]] Bitmap ReadEntireImage(const Opal::StringUtf8& file_path, PixelFormat desired_format, bool flip_vertically = false);

/**
 * Save bitmap to the disc. The data is stored from top to bottom, left to right, row by row.
 *
 * @param bitmap Image in the CPU memory to save to the disc.
 * @param file_path Absolute or relative path to the file on the disc. Should include both the file
 * name and extension. Extension is used to figure out the format.
 *
 * @return Returns true in case of a success, false otherwise.
 */
bool SaveImage(const Bitmap& bitmap, const Opal::StringUtf8& file_path);

}  // namespace File

}  // namespace Rndr
