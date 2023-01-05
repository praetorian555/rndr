#include "rndr/core/fileutils.h"

#include <filesystem>

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

#include "rndr/core/log.h"
#include "rndr/core/memory.h"

rndr::ByteSpan rndr::file::ReadEntireFile(const std::string& FilePath)
{
    FILE* File = nullptr;
    fopen_s(&File, FilePath.c_str(), "rb");
    if (File == nullptr)
    {
        RNDR_LOG_ERROR("Failed to open file %s", FilePath.c_str());
        return ByteSpan{};
    }

    fseek(File, 0, SEEK_END);
    const int ContentsSize = ftell(File);
    fseek(File, 0, SEEK_SET);

    ByteSpan Contents;
    Contents.Size = ContentsSize;
    Contents.Data = RNDR_NEW_ARRAY(uint8_t, ContentsSize, "");
    const size_t ReadBytes = fread(Contents.Data, 1, Contents.Size, File);
    assert(ReadBytes == ContentsSize);

    fclose(File);

    return Contents;
}

rndr::CPUImage rndr::file::ReadEntireImage(const std::string& FilePath)
{
    int ChannelsInFile = 0;
    CPUImage Image;
    Image.Format = PixelFormat::R8G8B8A8_UNORM_SRGB;
    Image.Data.Data = stbi_load(FilePath.c_str(), &Image.Width, &Image.Height, &ChannelsInFile, 4);
    Image.Data.Size = 4 * Image.Width * Image.Height;

    return Image;
}

void rndr::file::FreeImage(const CPUImage& Image)
{
    stbi_image_free(Image.Data.Data);
}

bool rndr::file::SaveImage(const CPUImage& Image, const std::string& FilePath)
{
    const int Channels = rndr::GetPixelSize(Image.Format);
    const int Status = stbi_write_png(FilePath.c_str(), Image.Width, Image.Height, Channels,
                                      Image.Data.Data, Image.Width * Channels);

    return Status == 1;
}
