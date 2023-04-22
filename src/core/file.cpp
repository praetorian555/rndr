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

Rndr::CPUImage Rndr::File::ReadEntireImage(const String& file_path)
{
    int channels_in_file = 0;
    CPUImage image;
    image.pixel_format = PixelFormat::R8G8B8A8_UNORM_SRGB;
    uint8_t* tmp_data =
        stbi_load(file_path.c_str(), &image.width, &image.height, &channels_in_file, 4);
    const int data_size = 4 * image.width * image.height;
    image.data.resize(data_size);
    memcpy(image.data.data(), tmp_data, data_size);
    stbi_image_free(tmp_data);
    return image;
}

bool Rndr::File::SaveImage(const CPUImage& image, const String& file_path)
{
    const int channels = Rndr::GetPixelSize(image.pixel_format);
    const int status = stbi_write_png(file_path.c_str(),
                                      image.width,
                                      image.height,
                                      channels,
                                      image.data.data(),
                                      image.width * channels);

    return status == 1;
}
