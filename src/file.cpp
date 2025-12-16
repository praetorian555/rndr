#include "rndr/file.hpp"

#include "../build/opengl-msvc-opt-debug/_deps/opal-src/include/opal/file-system.h"
#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

#include "opal/paths.h"

#include "rndr/log.hpp"

Rndr::FileHandler::FileHandler(const char* file_path, const char* mode)
{
    fopen_s(&m_file_handle, file_path, mode);
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

bool Rndr::FileHandler::Read(void* buffer, u64 element_size, u64 element_count)
{
    const u64 read_elements = fread(buffer, element_size, element_count, m_file_handle);
    RNDR_ASSERT(read_elements == element_count, "Expected to read element_count elements!");
    return read_elements == element_count;
}

bool Rndr::FileHandler::Write(const void* buffer, u64 element_size, u64 element_count)
{
    const u64 written_elements = fwrite(buffer, element_size, element_count, m_file_handle);
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
    FILE* file = nullptr;
    fopen_s(&file, file_path_locale.GetData(), "rb");
    if (file == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s", file_path.GetData());
        return {};
    }

    fseek(file, 0, SEEK_END);
    const int contents_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    Opal::DynamicArray<u8> contents(contents_size);
    const u64 read_bytes = fread(contents.GetData(), 1, contents.GetSize(), file);
    if (read_bytes != contents_size)
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
    const Opal::StringUtf8 full_path = Opal::Paths::Combine(ref_path, shader_path);
    if (!Opal::Exists(full_path))
    {
        RNDR_LOG_ERROR("Shader file %s does not exist!", full_path.GetData());
        return {};
    }

    Opal::StringUtf8 shader_contents = ReadEntireTextFile(full_path);
    if (shader_contents.IsEmpty())
    {
        RNDR_LOG_ERROR("Failed to read shader file %s!", full_path.GetData());
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
            RNDR_LOG_ERROR("Invalid include statement %s", include_line.GetData());
            return {};
        }
        auto parent_path_result = Opal::Paths::GetParentPath(full_path);
        RNDR_ASSERT(parent_path_result.HasValue(), "Shader parent directory path is empty!");
        const Opal::StringUtf8 parent_path = parent_path_result.GetValue();
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
            RNDR_LOG_INFO("%d: %s", line_number, line_buffer.GetData());
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

Rndr::Bitmap Rndr::File::ReadEntireImage(const Opal::StringUtf8& file_path, PixelFormat desired_format, bool flip_vertically)
{
    Bitmap invalid_bitmap = {-1, -1, -1, PixelFormat::R8G8B8A8_UNORM_SRGB, {}};
    if (!Bitmap::IsPixelFormatSupported(desired_format))
    {
        RNDR_LOG_ERROR("Desired pixel format is not supported!");
        return invalid_bitmap;
    }
    const int desired_channel_count = Rndr::FromPixelFormatToComponentCount(desired_format);

    int channels_in_file = 0;
    int width = 0;
    int height = 0;
    uint8_t* tmp_data = nullptr;
    stbi_set_flip_vertically_on_load(static_cast<int>(flip_vertically));
    Opal::StringLocale file_path_locale;
    file_path_locale.Resize(300);
    const Opal::ErrorCode err = Opal::Transcode(file_path, file_path_locale);
    if (err != Opal::ErrorCode::Success)
    {
        RNDR_LOG_ERROR("Failed to transcode file path!");
        return invalid_bitmap;
    }
    if (Rndr::IsComponentLowPrecision(desired_format))
    {
        tmp_data = stbi_load(file_path_locale.GetData(), &width, &height, &channels_in_file, desired_channel_count);
        if (tmp_data == nullptr)
        {
            RNDR_LOG_ERROR("Failed to load image %s", file_path_locale.GetData());
            return invalid_bitmap;
        }
    }
    else
    {
        f32* tmp_data_float = stbi_loadf(file_path_locale.GetData(), &width, &height, &channels_in_file, desired_channel_count);
        if (tmp_data_float == nullptr)
        {
            RNDR_LOG_ERROR("Failed to load image %s", file_path_locale.GetData());
            return invalid_bitmap;
        }
        tmp_data = reinterpret_cast<u8*>(tmp_data_float);
    }
    const u64 pixel_size = FromPixelFormatToPixelSize(desired_format);
    Bitmap bitmap{width, height, 1, desired_format, {tmp_data, width * height * pixel_size}};
    stbi_image_free(tmp_data);
    return bitmap;
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
    if (IsComponentLowPrecision(pixel_format))
    {
        status = stbi_write_png(file_path_locale.GetData(), bitmap.GetWidth(), bitmap.GetHeight(), bitmap.GetComponentCount(),
                                bitmap.GetData(), static_cast<int>(bitmap.GetRowSize()));
    }
    else if (IsComponentHighPrecision(pixel_format))
    {
        const f32* data = reinterpret_cast<const f32*>(bitmap.GetData());
        status = stbi_write_hdr(file_path_locale.GetData(), bitmap.GetWidth(), bitmap.GetHeight(), bitmap.GetComponentCount(), data);
    }
    else
    {
        RNDR_LOG_ERROR("Unsupported pixel format!");
    }
    return status == 1;
}
