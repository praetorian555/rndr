#include <filesystem>

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

#include "rndr/core/file.h"

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
    RNDR_ASSERT(read_elements == element_count);
    return read_elements == element_count;
}

bool Rndr::FileHandler::Write(const void* buffer, u64 element_size, u64 element_count)
{
    const u64 written_elements = fwrite(buffer, element_size, element_count, m_file_handle);
    RNDR_ASSERT(written_elements == element_count);
    return written_elements == element_count;
}

Opal::Array<Rndr::u8> Rndr::File::ReadEntireFile(const String& file_path)
{
    FILE* file = nullptr;
    fopen_s(&file, file_path.c_str(), "rb");
    if (file == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s", file_path.c_str());
        return {};
    }

    fseek(file, 0, SEEK_END);
    const int contents_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    Opal::Array<u8> contents(contents_size);
    const u64 read_bytes = fread(contents.GetData(), 1, contents.GetSize(), file);
    if (read_bytes != contents_size)
    {
        RNDR_LOG_WARNING("Failed to read all bytes from the file!");
    }

    fclose(file);

    return contents;
}

Rndr::String Rndr::File::ReadEntireTextFile(const Rndr::String& file_path)
{
    Opal::Array<u8> contents = ReadEntireFile(file_path);
    return {contents.begin(), contents.end()};
}

Rndr::String Rndr::File::ReadShader(const Rndr::String& ref_path, const Rndr::String& shader_path)
{
    const std::filesystem::path full_shader_path = std::filesystem::path(ref_path) / shader_path;
    if (!std::filesystem::exists(full_shader_path))
    {
        RNDR_LOG_ERROR("Shader file %s does not exist!", full_shader_path.string().c_str());
        return {};
    }

    String shader_contents = ReadEntireTextFile(full_shader_path.string());
    if (shader_contents.empty())
    {
        RNDR_LOG_ERROR("Failed to read shader file %s!", full_shader_path.string().c_str());
        return {};
    }

    static constexpr uint8_t k_bom[] = {0xEF, 0xBB, 0xBF};
    if (shader_contents.size() > sizeof(k_bom) && memcmp(shader_contents.data(), k_bom, sizeof(k_bom)) == 0)
    {
        shader_contents.erase(0, sizeof(k_bom));
    }

    while (shader_contents.find("#include") != String::npos)
    {
        const u64 include_start = shader_contents.find("#include");
        const u64 include_end = shader_contents.find_first_of('\n', include_start);
        const u64 include_length = include_end - include_start;
        const String include_line = shader_contents.substr(include_start, include_length);
        const u64 quote_start = include_line.find_first_of('\"');
        const u64 quote_end = include_line.find_last_of('\"');
        const u64 quote_length = quote_end - quote_start;
        const u64 bracket_start = include_line.find_first_of('<');
        const u64 bracket_end = include_line.find_last_of('>');
        const u64 bracket_length = bracket_end - bracket_start;
        String include_path;
        if (quote_start != String::npos)
        {
            include_path = include_line.substr(quote_start + 1, quote_length - 1);
        }
        else if (bracket_start != String::npos)
        {
            include_path = include_line.substr(bracket_start + 1, bracket_length - 1);
        }
        else
        {
            RNDR_LOG_ERROR("Invalid include statement %s", include_line.c_str());
            return {};
        }
        const String include_contents = ReadShader(full_shader_path.parent_path().string(), include_path);
        shader_contents.replace(include_start, include_length, include_contents);
    }

    return shader_contents;
}

void Rndr::File::PrintShader(const Rndr::String& shader_contents)
{
    int line_number = 1;
    String line_buffer;
    for (const auto& c : shader_contents)
    {
        if (c == '\n')
        {
            RNDR_LOG_INFO("%d: %s", line_number, line_buffer.c_str());
            line_buffer.clear();
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

Rndr::Bitmap Rndr::File::ReadEntireImage(const String& file_path, PixelFormat desired_format, bool flip_vertically)
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
    if (Rndr::IsComponentLowPrecision(desired_format))
    {
        tmp_data = stbi_load(file_path.c_str(), &width, &height, &channels_in_file, desired_channel_count);
        if (tmp_data == nullptr)
        {
            RNDR_LOG_ERROR("Failed to load image %s", file_path.c_str());
            return invalid_bitmap;
        }
    }
    else
    {
        float* tmp_data_float = stbi_loadf(file_path.c_str(), &width, &height, &channels_in_file, desired_channel_count);
        if (tmp_data_float == nullptr)
        {
            RNDR_LOG_ERROR("Failed to load image %s", file_path.c_str());
            return invalid_bitmap;
        }
        tmp_data = reinterpret_cast<uint8_t*>(tmp_data_float);
    }
    const u64 pixel_size = FromPixelFormatToPixelSize(desired_format);
    Bitmap bitmap{width, height, 1, desired_format, {tmp_data, width * height * pixel_size}};
    stbi_image_free(tmp_data);
    return bitmap;
}

bool Rndr::File::SaveImage(const Bitmap& bitmap, const String& file_path)
{
    int status = 0;
    const PixelFormat pixel_format = bitmap.GetPixelFormat();
    if (IsComponentLowPrecision(pixel_format))
    {
        status = stbi_write_png(file_path.c_str(), bitmap.GetWidth(), bitmap.GetHeight(), bitmap.GetComponentCount(), bitmap.GetData(),
                                static_cast<int>(bitmap.GetRowSize()));
    }
    else if (IsComponentHighPrecision(pixel_format))
    {
        const float* data = reinterpret_cast<const float*>(bitmap.GetData());
        status = stbi_write_hdr(file_path.c_str(), bitmap.GetWidth(), bitmap.GetHeight(), bitmap.GetComponentCount(), data);
    }
    else
    {
        RNDR_LOG_ERROR("Unsupported pixel format!");
    }
    return status == 1;
}
