#include <filesystem>

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

#include "rndr/core/file.h"

Rndr::ByteArray Rndr::File::ReadEntireFile(const String& file_path)
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

    ByteArray contents(contents_size);
    const size_t read_bytes = fread(contents.data(), 1, contents.size(), file);
    if (read_bytes != contents_size)
    {
        RNDR_LOG_WARNING("Failed to read all bytes from the file!");
    }

    fclose(file);

    return contents;
}

Rndr::Bitmap Rndr::File::ReadEntireImage(const String& file_path, PixelFormat desired_format)
{
    Bitmap invalid_bitmap = {-1, -1, PixelFormat::R8G8B8A8_UNORM_SRGB, {}};
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
    if (Rndr::IsComponentLowPrecision(desired_format))
    {
        tmp_data =
            stbi_load(file_path.c_str(), &width, &height, &channels_in_file, desired_channel_count);
        if (tmp_data == nullptr)
        {
            RNDR_LOG_ERROR("Failed to load image %s", file_path.c_str());
            return invalid_bitmap;
        }
    }
    else
    {
        float* tmp_data_float = stbi_loadf(file_path.c_str(),
                                           &width,
                                           &height,
                                           &channels_in_file,
                                           desired_channel_count);
        if (tmp_data_float == nullptr)
        {
            RNDR_LOG_ERROR("Failed to load image %s", file_path.c_str());
            return invalid_bitmap;
        }
        tmp_data = reinterpret_cast<uint8_t*>(tmp_data_float);
    }
    Bitmap bitmap{width, height, desired_format, tmp_data};
    stbi_image_free(tmp_data);
    return bitmap;
}

Rndr::String Rndr::File::ReadEntireTextFile(const Rndr::String& file_path)
{
    ByteArray contents = ReadEntireFile(file_path);
    return {contents.begin(), contents.end()};
}

bool Rndr::File::SaveImage(const Bitmap& bitmap, const String& file_path)
{
    const int status = stbi_write_png(file_path.c_str(),
                                      bitmap.GetWidth(),
                                      bitmap.GetHeight(),
                                      bitmap.GetComponentCount(),
                                      bitmap.GetData(),
                                      bitmap.GetRowSize());
    return status == 1;
}
