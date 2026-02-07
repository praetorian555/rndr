#pragma once

#include <cstdio>

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "rndr/bitmap.hpp"
#include "rndr/graphics-types.hpp"
#include "rndr/material.hpp"
#include "rndr/mesh.hpp"

struct aiMaterial;
struct aiScene;

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
 * Save bitmap to the disc. The data is stored from top to bottom, left to right, row by row.
 *
 * @param bitmap Image in the CPU memory to save to the disc.
 * @param file_path Absolute or relative path to the file on the disc. Should include both the file
 * name and extension. Extension is used to figure out the format.
 *
 * @return Returns true in case of a success, false otherwise.
 */
bool SaveImage(const Bitmap& bitmap, const Opal::StringUtf8& file_path);

/**
 * Load an image from a file. Supports PNG, JPEG (via stbi) and KTX/KTX2 (via libktx).
 * For PNG/JPEG, the channel bit depth is auto-detected (8-bit, 16-bit, or 32-bit HDR) and the
 * image is always loaded as RGBA. For KTX files, the format is read from the file metadata.
 *
 * @param file_path Absolute or relative path to the image file.
 * @param flip_vertically If true, the image will be flipped vertically. Only applies to PNG/JPEG.
 * @param generate_mips If true, mip maps will be generated. For KTX files, mips are not generated
 * if they are already present in the file.
 *
 * @return Returns a valid Bitmap.
 * @throw Opal::Exception if file does not exist or if there was a problem loading the data.
 */
[[nodiscard]] Bitmap LoadImage(const Opal::StringUtf8& file_path, bool flip_vertically, bool generate_mips);

void LoadMeshAndMaterialDescription(const Opal::StringUtf8& file_path, Mesh& out_mesh, MaterialDesc& out_material_desc);
void LoadMesh(const aiScene& ai_scene, const Opal::StringUtf8& mesh_name, Mesh& out_mesh, u32& out_material_index);
void LoadMaterialDescription(const aiScene& ai_scene, u32 material_index, const Opal::StringUtf8& parent_path, MaterialDesc& out_material_desc);

}  // namespace File

}  // namespace Rndr
