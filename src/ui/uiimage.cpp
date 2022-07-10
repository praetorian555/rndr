#include "rndr/ui/uiimage.h"

#include "rndr/core/fileutils.h"
#include "rndr/core/log.h"
#include "rndr/core/pipeline.h"

#include "rndr/ui/uisystem.h"

namespace rndr
{
namespace ui
{

struct ImageInfo
{
    RenderId RenderId;
    int Width;
    int Height;
    PixelFormat Format;
    math::Point2 BottomLeftTexCoords;
    math::Point2 TopRightTexCoords;
};

RenderId AllocateRenderId();
void FreeRenderId(RenderId Id);
void UpdateRenderResource(RenderId Id, ByteSpan Contents, int Width, int Height);

bool InitImage();
void ShutDownImage();

static ImageInfo* GetImageInfo(ImageId Id);

extern UIProperties g_UIProps;

static Span<ImageInfo*> g_Images;

}  // namespace ui
}  // namespace rndr

bool rndr::ui::InitImage()
{
    assert(g_UIProps.MaxImageCount > 0);
    g_Images.Size = g_UIProps.MaxImageCount;
    g_Images.Data = new ImageInfo*[g_Images.Size];
    for (int i = 0; i < g_Images.Size; i++)
    {
        g_Images[i] = nullptr;
    }
    return true;
}

void rndr::ui::ShutDownImage()
{
    delete[] g_Images.Data;
}

rndr::ui::ImageId rndr::ui::AddImage(const char* ImagePath)
{
    for (int i = 0; i < g_Images.Size; i++)
    {
        if (!g_Images[i])
        {
            CPUImage Image = ReadEntireImage(ImagePath);
            if (!Image.Data)
            {
                RNDR_LOG_ERROR("Failed to read image from file %s", ImagePath);
                return kInvalidImageId;
            }
            g_Images[i] = new ImageInfo();
            g_Images[i]->RenderId = AllocateRenderId();
            g_Images[i]->Width = Image.Width;
            g_Images[i]->Height = Image.Height;
            g_Images[i]->Format = Image.Format;
            g_Images[i]->BottomLeftTexCoords = math::Point2{0, 0};
            g_Images[i]->TopRightTexCoords.X = Image.Width / (float)g_UIProps.MaxImageSideSize;
            g_Images[i]->TopRightTexCoords.Y = Image.Height / (float)g_UIProps.MaxImageSideSize;

            UpdateRenderResource(g_Images[i]->RenderId, Image.Data, Image.Width, Image.Height);

            FreeImage(Image);
            return i;
        }
    }
    RNDR_LOG_ERROR("No more slots for images!");
    return kInvalidImageId;
}

void rndr::ui::RemoveImage(ImageId Id)
{
    if (Id < 0 || Id >= g_Images.Size)
    {
        return;
    }
    FreeRenderId(g_Images[Id]->RenderId);
    delete g_Images[Id];
    g_Images[Id] = nullptr;
}

math::Vector2 rndr::ui::GetImageSize(ImageId Id)
{
    ImageInfo* Image = GetImageInfo(Id);
    if (!Image)
    {
        return math::Vector2{};
    }
    return math::Vector2{(float)Image->Width, (float)Image->Height};
}

void rndr::ui::GetImageTexCoords(ImageId Id, math::Point2* BottomLeft, math::Point2* TopRight)
{
    ImageInfo* Image = GetImageInfo(Id);
    if (!Image)
    {
        return;
    }
    if (BottomLeft)
    {
        *BottomLeft = Image->BottomLeftTexCoords;
    }
    if (TopRight)
    {
        *TopRight = Image->TopRightTexCoords;
    }
}

rndr::ui::RenderId rndr::ui::GetImageRenderId(ImageId Id)
{
    ImageInfo* Image = GetImageInfo(Id);
    if (!Image)
    {
        return kInvalidRenderId;
    }
    return Image->RenderId;
}

rndr::ui::ImageInfo* rndr::ui::GetImageInfo(ImageId Id)
{
    if (Id < 0 || Id >= g_Images.Size)
    {
        return nullptr;
    }
    return g_Images[Id];
}
