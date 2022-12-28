#pragma once

#include <string>

#include "rndr/core/base.h"
#include "rndr/render/graphicstypes.h"

namespace rndr
{

// Helper struct to store image data in the CPU memory and loaded from a file.
struct CPUImage
{
    int Width = 0;
    int Height = 0;
    PixelFormat Format = PixelFormat::R32_TYPELESS;
    ByteSpan Data;
};

namespace file
{

/**
 * Reads the contents of the entire file in the binary form.
 *
 * @param FilePath Absolute or relative path to the file on the disc.
 *
 * @return Returns a valid ByteSpan object containing the array of bytes representing the file
 * contents. If the ByteSpan object is invalid then there was an error reading the file. To release
 * memory use RNDR_DELETE_ARRAY.
 */
ByteSpan ReadEntireFile(const std::string& FilePath);

/**
 * Reads the contents of the entire image file into CPU memory.
 *
 * @param FilePath Absolute or relative path to the file on the disc.
 *
 * @return Returns a valid CPUImage object containing the image info and data in the CPU memory. You
 * can check for CPUImage validity by checking the validity of Data field. In case of an error
 * returned object will be invalid. To release the memory use FreeImage API.
 */
CPUImage ReadEntireImage(const std::string& FilePath);

/**
 * Releases the image data memory in the CPUImage object.
 *
 * @param Image CPUImage object to be released from CPU memory.
 */
void FreeImage(const CPUImage& Image);

/**
 * Save image to the disc.
 *
 * @param Image Image in the CPU memory to saved to the disc.
 * @param FilePath Absolute or relative path to the file on the disc. Should include both the file
 * name and extension. Extension is used to figure out the format.
 * 
 * @return Returns true in case of a success, false otherwise.
 */
bool SaveImage(const CPUImage& Image, const std::string& FilePath);

}  // namespace file

}  // namespace rndr
