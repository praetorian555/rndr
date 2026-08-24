#include "rndr/file.hpp"

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

#if RNDR_FORGE && defined(RNDR_KTX)
#include "ktx.h"
#include "ktxvulkan.h"
#endif

#include "opal/exceptions.h"
#include "opal/file-system.h"
#include "opal/paths.h"

#include "rndr/log.hpp"

namespace
{

FILE* OpenFile(const char* file_path, const char* mode)
{
#if RNDR_WINDOWS
    FILE* file = nullptr;
    fopen_s(&file, file_path, mode);
    return file;
#else
    return fopen(file_path, mode);
#endif
}

}  // namespace

Rndr::FileHandler::FileHandler(const char* file_path, const char* mode)
{
    m_file_handle = OpenFile(file_path, mode);
}

Rndr::FileHandler::~FileHandler()
{
    if (m_file_handle != 0)
    {
        fclose(m_file_handle);
    }
}

bool Rndr::FileHandler::IsValid() const
{
    return m_file_handle != 0;
}

bool Rndr::FileHandler::IsEOF() const
{
    return feof(m_file_handle) != 0;
}

bool Rndr::FileHandler::Read(void* buffer, size_t element_size, size_t element_count)
{
    const size_t read_elements = fread(buffer, element_size, element_count, m_file_handle);
    RNDR_ASSERT(read_elements == element_count, "Expected to read element_count elements!");
    return read_elements == element_count;
}

bool Rndr::FileHandler::Write(const void* buffer, size_t element_size, size_t element_count)
{
    const size_t written_elements = fwrite(buffer, element_size, element_count, m_file_handle);
    RNDR_ASSERT(written_elements == element_count, "Expected to write element_count elements!");
    return written_elements == element_count;
}

Opal::DynamicArray<Rndr::u8> Rndr::File::ReadEntireFile(const Opal::StringUtf8& file_path)
{
    Opal::StringLocale file_path_locale;
    file_path_locale.Resize(300);
    const Opal::ErrorCode err = Opal::Transcode(file_path, file_path_locale);
    if (err != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Failed to transcode file path!");
        return {};
    }
    FILE* file = OpenFile(file_path_locale.GetData(), "rb");
    if (file == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file {}", file_path.GetData());
        return {};
    }

    fseek(file, 0, SEEK_END);
    const int contents_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    Opal::DynamicArray<u8> contents(contents_size);
    const u64 read_bytes = fread(contents.GetData(), 1, contents.GetSize(), file);
    if (read_bytes != static_cast<u64>(contents_size))
    {
        RNDR_LOG_WARNING("Failed to read all bytes from the file!");
    }

    fclose(file);

    return contents;
}

Opal::StringUtf8 Rndr::File::ReadEntireTextFile(const Opal::StringUtf8& file_path)
{
    Opal::DynamicArray<u8> contents = ReadEntireFile(file_path);
    return {reinterpret_cast<char8*>(contents.GetData()), contents.GetSize()};
}

Opal::StringUtf8 Rndr::File::ReadShader(const Opal::StringUtf8& ref_path, const Opal::StringUtf8& shader_path)
{
    const Opal::StringUtf8 full_path = Opal::Paths::Combine(ref_path, shader_path).GetValue();
    if (!Opal::Exists(full_path))
    {
        RNDR_LOG_ERROR("Shader file {} does not exist!", full_path.GetData());
        return {};
    }

    Opal::StringUtf8 shader_contents = ReadEntireTextFile(full_path);
    if (shader_contents.IsEmpty())
    {
        RNDR_LOG_ERROR("Failed to read shader file {}!", full_path.GetData());
        return {};
    }

    static constexpr u8 k_bom[] = {0xEF, 0xBB, 0xBF};
    if (shader_contents.GetSize() > sizeof(k_bom) && memcmp(shader_contents.GetData(), k_bom, sizeof(k_bom)) == 0)
    {
        shader_contents.Erase(0, 3);
    }

    while (true)
    {
        u64 const result = Opal::Find(shader_contents, "#include");
        if (result == Opal::StringUtf8::k_npos)
        {
            break;
        }

        const u64 include_start = Opal::Find(shader_contents, "#include");
        const u64 include_end = Opal::Find(shader_contents, "\n", include_start);
        const u64 include_length = include_end - include_start;
        const Opal::StringUtf8 include_line = Opal::GetSubString(shader_contents, include_start, include_length).GetValue();
        const u64 quote_start = Opal::Find(include_line, '\"');
        const u64 quote_end = Opal::ReverseFind(include_line, '\"');
        const u64 quote_length = quote_end - quote_start;
        const u64 bracket_start = Opal::Find(include_line, '<');
        const u64 bracket_end = Opal::ReverseFind(include_line, '>');
        const u64 bracket_length = bracket_end - bracket_start;
        Opal::StringUtf8 include_path;
        if (quote_start != Opal::StringUtf8::k_npos)
        {
            include_path = Opal::GetSubString(include_line, quote_start + 1, quote_length - 1).GetValue();
        }
        else if (bracket_start != Opal::StringUtf8::k_npos)
        {
            include_path = Opal::GetSubString(include_line, bracket_start + 1, bracket_length - 1).GetValue();
        }
        else
        {
            RNDR_LOG_ERROR("Invalid include statement {}", include_line.GetData());
            return {};
        }
        auto parent_path_result = Opal::Paths::GetParentPath(full_path);
        RNDR_ASSERT(parent_path_result.HasValue(), "Shader parent directory path is empty!");
        const Opal::StringUtf8 parent_path = std::move(parent_path_result.GetValue());
        const Opal::StringUtf8 include_contents = ReadShader(parent_path, include_path);
        shader_contents.Erase(include_start, include_length);
        shader_contents.Insert(include_start, include_contents);
    }

    return shader_contents;
}

void Rndr::File::PrintShader(const Opal::StringUtf8& shader_contents)
{
    int line_number = 1;
    Opal::StringUtf8 line_buffer;
    for (const auto& c : shader_contents)
    {
        if (c == '\n')
        {
            RNDR_LOG_INFO("{}: {}", line_number, line_buffer.GetData());
            line_buffer.Erase();
            line_number++;
        }
        else if (c == '\r')
        {
            continue;
        }
        else
        {
            line_buffer += c;
        }
    }
}

bool Rndr::File::SaveImage(const Bitmap& bitmap, const Opal::StringUtf8& file_path)
{
    Opal::StringLocale file_path_locale;
    file_path_locale.Resize(300);
    const Opal::ErrorCode err = Opal::Transcode(file_path, file_path_locale);
    if (err != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Failed to transcode file path!");
        return false;
    }
    int status = 0;
    const PixelFormat pixel_format = bitmap.GetPixelFormat();
    if (IsLowPrecisionFormat(pixel_format))
    {
        status = stbi_write_png(file_path_locale.GetData(), bitmap.GetWidth(), bitmap.GetHeight(), bitmap.GetComponentCount(),
                                bitmap.GetData(), static_cast<int>(bitmap.GetRowSize()));
    }
    else if (IsHighPrecisionFormat(pixel_format))
    {
        const f32* data = reinterpret_cast<const f32*>(bitmap.GetData());
        status = stbi_write_hdr(file_path_locale.GetData(), bitmap.GetWidth(), bitmap.GetHeight(), bitmap.GetComponentCount(), data);
    }
    else
    {
        RNDR_LOG_ERROR("Unsupported pixel format for saving!");
    }
    return status == 1;
}

#if RNDR_FORGE && defined(RNDR_KTX)
namespace
{
Rndr::PixelFormat VkFormatToPixelFormat(ktx_uint32_t vk_format)
{
    return static_cast<Rndr::PixelFormat>(vk_format);
}
}  // namespace
#endif

Opal::Expected<Rndr::Bitmap, Rndr::ErrorCode> Rndr::File::LoadImage(const Opal::StringUtf8& file_path, bool flip_vertically,
                                                                    bool generate_mips)
{
    using ResultType = Opal::Expected<Rndr::Bitmap, Rndr::ErrorCode>;
    if (!Opal::Exists(file_path))
    {
        RNDR_LOG_ERROR("Image file does not exist: {}", *file_path);
        return ResultType(ErrorCode::FileNotFound);
    }

    // Determine file extension.
    const Opal::StringUtf8 extension = Opal::Paths::GetExtension(file_path).GetValue();

#if RNDR_FORGE && defined(RNDR_KTX)
    if (extension == ".ktx" || extension == ".ktx2")
    {
        ktxTexture* ktx_texture = nullptr;
        const KTX_error_code result = ktxTexture_CreateFromNamedFile(*file_path, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx_texture);
        if (result != KTX_SUCCESS || ktx_texture == nullptr)
        {
            RNDR_LOG_ERROR("Failed to create ktx texture from file: {}", *file_path);
            return ResultType(ErrorCode::CorruptData);
        }

        const ktx_uint32_t vk_format = ktxTexture_GetVkFormat(ktx_texture);
        const PixelFormat pixel_format = VkFormatToPixelFormat(vk_format);
        const i32 width = static_cast<i32>(ktx_texture->baseWidth);
        const i32 height = static_cast<i32>(ktx_texture->baseHeight);
        const i32 depth = static_cast<i32>(ktx_texture->baseDepth);
        const i32 mip_count = static_cast<i32>(ktx_texture->numLevels);
        u8* data = ktxTexture_GetData(ktx_texture);
        const u64 data_size = ktxTexture_GetDataSize(ktx_texture);

        ResultType bitmap_result = Bitmap::Create(width, height, depth, pixel_format, mip_count, {data, data_size});
        ktxTexture_Destroy(ktx_texture);
        if (!bitmap_result.HasValue())
        {
            return bitmap_result;
        }

        Bitmap bitmap = std::move(bitmap_result).GetValue();
        if (generate_mips && mip_count <= 1)
        {
            bitmap.GenerateMips();
        }

        return ResultType(std::move(bitmap));
    }
#endif

    stbi_set_flip_vertically_on_load(flip_vertically);

    int width = 0;
    int height = 0;
    int channels_in_file = 0;
    constexpr int k_desired_channels = 4;

    PixelFormat pixel_format = PixelFormat::Undefined;
    u8* pixel_data = nullptr;
    u64 data_size = 0;

    if (stbi_is_hdr(*file_path) > 0)
    {
        f32* data = stbi_loadf(*file_path, &width, &height, &channels_in_file, k_desired_channels);
        if (data == nullptr)
        {
            RNDR_LOG_ERROR("Failed to load HDR image {}: {}", *file_path, stbi_failure_reason());
            return ResultType(ErrorCode::CorruptData);
        }
        pixel_data = reinterpret_cast<u8*>(data);
        pixel_format = PixelFormat::R32G32B32A32_SFLOAT;
        data_size = static_cast<u64>(width) * height * k_desired_channels * sizeof(f32);
    }
    else if (stbi_is_16_bit(*file_path) > 0)
    {
        u16* data = reinterpret_cast<u16*>(stbi_load_16(*file_path, &width, &height, &channels_in_file, k_desired_channels));
        if (data == nullptr)
        {
            RNDR_LOG_ERROR("Failed to load 16-bit image {}: {}", *file_path, stbi_failure_reason());
            return ResultType(ErrorCode::CorruptData);
        }
        pixel_data = reinterpret_cast<u8*>(data);
        pixel_format = PixelFormat::R16G16B16A16_UNORM;
        data_size = static_cast<u64>(width) * height * k_desired_channels * sizeof(u16);
    }
    else
    {
        u8* data = stbi_load(*file_path, &width, &height, &channels_in_file, k_desired_channels);
        if (data == nullptr)
        {
            RNDR_LOG_ERROR("Failed to load image {}: {}", *file_path, stbi_failure_reason());
            return ResultType(ErrorCode::CorruptData);
        }
        pixel_data = data;
        pixel_format = PixelFormat::R8G8B8A8_SRGB;
        data_size = static_cast<u64>(width) * height * k_desired_channels * sizeof(u8);
    }

    ResultType bitmap_result = Bitmap::Create(width, height, 1, pixel_format, 1, {pixel_data, data_size});
    stbi_image_free(pixel_data);
    if (!bitmap_result.HasValue())
    {
        return bitmap_result;
    }

    Bitmap bitmap = std::move(bitmap_result).GetValue();
    if (generate_mips)
    {
        bitmap.GenerateMips();
    }

    return ResultType(std::move(bitmap));
}

