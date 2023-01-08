#include "rndr/core/fileutils.h"

#include <filesystem>

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

#include "rndr/core/log.h"
#include "rndr/core/memory.h"

rndr::Array<uint8_t> rndr::file::ReadEntireFile(const std::string& FilePath)
{
    FILE* File = nullptr;
    fopen_s(&File, FilePath.c_str(), "rb");
    if (File == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s", FilePath.c_str());
        return {};
    }

    fseek(File, 0, SEEK_END);
    const int ContentsSize = ftell(File);
    fseek(File, 0, SEEK_SET);

    Array<uint8_t> Contents(ContentsSize);
    const size_t ReadBytes = fread(Contents.data(), 1, Contents.size(), File);
    if (ReadBytes != ContentsSize)
    {
        RNDR_LOG_WARNING("Failed to read all bytes from the file!");
    }

    fclose(File);

    return Contents;
}

rndr::CPUImage rndr::file::ReadEntireImage(const std::string& FilePath)
{
    int ChannelsInFile = 0;
    CPUImage Image;
    Image.Format = PixelFormat::R8G8B8A8_UNORM_SRGB;
    uint8_t* TmpData = stbi_load(FilePath.c_str(), &Image.Width, &Image.Height, &ChannelsInFile, 4);
    const int DataSize = 4 * Image.Width * Image.Height;
    Image.Data.resize(DataSize);
    memcpy(Image.Data.data(), TmpData, DataSize);
    stbi_image_free(TmpData);
    return Image;
}

bool rndr::file::SaveImage(const CPUImage& Image, const std::string& FilePath)
{
    const int Channels = rndr::GetPixelSize(Image.Format);
    const int Status = stbi_write_png(FilePath.c_str(), Image.Width, Image.Height, Channels,
                                      Image.Data.data(), Image.Width * Channels);

    return Status == 1;
}
