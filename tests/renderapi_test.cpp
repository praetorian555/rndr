#include <catch2/catch_test_macros.hpp>

#include "math/math.h"

#include "rndr/core/log.h"

#include "rndr/render/buffer.h"
#include "rndr/render/framebuffer.h"
#include "rndr/render/graphicscontext.h"
#include "rndr/render/image.h"
#include "rndr/render/sampler.h"

TEST_CASE("GraphicsContext", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    rndr::GraphicsContextProperties Props;
    Props.bDisableGPUTimeout = false;
    Props.bEnableDebugLayer = true;
    Props.bFailWarning = true;
    Props.bMakeThreadSafe = true;

    SECTION("Default")
    {
        rndr::GraphicsContext GC;
        bool Result = GC.Init(Props);
        REQUIRE(Result == true);
    }
    SECTION("Disable GPU Timeout")
    {
        rndr::GraphicsContext GC;
        Props.bDisableGPUTimeout = true;
        bool Result = GC.Init(Props);
        REQUIRE(Result == true);
    }
    SECTION("No Debug Layer")
    {
        rndr::GraphicsContext GC;
        Props.bEnableDebugLayer = false;
        bool Result = GC.Init(Props);
        REQUIRE(Result == true);
    }
    SECTION("Not Thread Safe")
    {
        rndr::GraphicsContext GC;
        Props.bMakeThreadSafe = false;
        bool Result = GC.Init(Props);
        REQUIRE(Result == true);
    }

    rndr::StdAsyncLogger::Get()->ShutDown();
}

TEST_CASE("Image", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    rndr::GraphicsContext GC;
    {
        rndr::GraphicsContextProperties Props;
        Props.bDisableGPUTimeout = false;
        Props.bEnableDebugLayer = true;
        Props.bFailWarning = true;
        Props.bMakeThreadSafe = true;
        REQUIRE(GC.Init(Props) == true);
    }

    SECTION("Default Props with Invalid Width or Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = GC.CreateImage(0, 0, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image == nullptr);
    }
    SECTION("Default Props with Valid Width and Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use as render target and shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget | rndr::ImageBindFlags::ShaderResource;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use dynamic image as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Dynamic;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Create readback image")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Multisampling Valid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Multisampling Invalid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 3;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image == nullptr);
    }
    SECTION("Multisampling Valid Render Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Multisampling Valid Depth Stencil Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        delete Image;
    }

    rndr::StdAsyncLogger::Get()->ShutDown();
}

TEST_CASE("ImageArray", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    rndr::GraphicsContext GC;
    {
        rndr::GraphicsContextProperties Props;
        Props.bDisableGPUTimeout = false;
        Props.bEnableDebugLayer = true;
        Props.bFailWarning = true;
        Props.bMakeThreadSafe = true;
        REQUIRE(GC.Init(Props) == true);
    }

    const rndr::Span<rndr::ByteSpan> EmptyData;
    const int Width = 400;
    const int Height = 100;
    const int ArraySize = 8;
    SECTION("Default Props with Invalid Width or Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = GC.CreateImageArray(0, 0, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Default Props with Invalid ArraySize")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, 1, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Default Props with Valid Width and Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use dynamic image array as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Dynamic;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Create readback image array")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Multisampling Valid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Multisampling Invalid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 3;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Multisampling Valid Render Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Multisampling Valid Depth Stencil Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }

    rndr::StdAsyncLogger::Get()->ShutDown();
}

TEST_CASE("CubeMap", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    rndr::GraphicsContext GC;
    {
        rndr::GraphicsContextProperties Props;
        Props.bDisableGPUTimeout = false;
        Props.bEnableDebugLayer = true;
        Props.bFailWarning = true;
        Props.bMakeThreadSafe = true;
        REQUIRE(GC.Init(Props) == true);
    }

    const rndr::Span<rndr::ByteSpan> EmptyData;
    const int Width = 400;
    const int Height = 400;
    const int ArraySize = 6;
    SECTION("Default Props with Invalid Width or Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = GC.CreateCubeMap(0, 0, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Default Props")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }
    SECTION("Use dynamic cube map as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Dynamic;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Create readback cube map")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        delete Image;
    }

    rndr::StdAsyncLogger::Get()->ShutDown();
}

TEST_CASE("ImageUpdate", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    const rndr::Span<rndr::ByteSpan> EmptyDataArray;
    const rndr::ByteSpan EmptyData;
    const int Width = 400;
    const int Height = 100;
    const int ArraySize = 8;
    const math::Point2 Start;
    const math::Point2 BadStart{200, 500};
    const math::Vector2 Size{50, 50};
    rndr::ByteSpan UpdateData;
    UpdateData.Size = 50 * 50 * 4;
    UpdateData.Data = new uint8_t[UpdateData.Size];
    rndr::GraphicsContext GC;
    rndr::GraphicsContextProperties Props;
    Props.bDisableGPUTimeout = false;
    Props.bEnableDebugLayer = true;
    Props.bFailWarning = true;
    Props.bMakeThreadSafe = true;
    REQUIRE(GC.Init(Props) == true);

    SECTION("Default Usage")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyDataArray);
        REQUIRE(Image != nullptr);

        SECTION("Update")
        {
            const bool Result = Image->Update(&GC, 0, Start, Size, UpdateData);
            REQUIRE(Result == true);
        }
        SECTION("Invalid Update")
        {
            bool Result;
            Result = Image->Update(&GC, ArraySize, Start, Size, UpdateData);
            REQUIRE(Result == false);
            Result = Image->Update(&GC, -1, Start, Size, UpdateData);
            REQUIRE(Result == false);
            Result = Image->Update(&GC, 3, BadStart, Size, UpdateData);
            REQUIRE(Result == false);
            Result = Image->Update(&GC, 3, Start, Size, EmptyData);
            REQUIRE(Result == false);
        }

        delete Image;
    }
    SECTION("Dynamic Usage")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyDataArray);
        REQUIRE(Image != nullptr);

        SECTION("Update")
        {
            const bool Result = Image->Update(&GC, 0, Start, Size, UpdateData);
            REQUIRE(Result == true);
        }

        delete Image;
    }

    delete[] UpdateData.Data;
    rndr::StdAsyncLogger::Get()->ShutDown();
}

TEST_CASE("ImageRead", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    const rndr::Span<rndr::ByteSpan> EmptyDataArray;
    const rndr::ByteSpan EmptyData;
    const int Width = 400;
    const int Height = 100;
    const int ArraySize = 8;
    const math::Point2 Start;
    const math::Point2 BadStart{200, 500};
    const math::Vector2 Size{50, 50};
    rndr::ByteSpan InitData;
    InitData.Size = Width * Height * 4;
    InitData.Data = new uint8_t[InitData.Size];
    const uint32_t PixelPattern = 0xABABABAB;
    for (int i = 0; i < InitData.Size / 4; i++)
    {
        uint32_t* PixelData = reinterpret_cast<uint32_t*>(InitData.Data);
        *PixelData = PixelPattern;
    }
    rndr::GraphicsContext GC;
    rndr::GraphicsContextProperties Props;
    Props.bDisableGPUTimeout = false;
    Props.bEnableDebugLayer = true;
    Props.bFailWarning = true;
    Props.bMakeThreadSafe = true;
    REQUIRE(GC.Init(Props) == true);

    SECTION("From GPU to CPU Usage")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::Image* Image = GC.CreateImage(Width, Height, ImageProps, InitData);
        REQUIRE(Image != nullptr);

        rndr::ByteSpan ReadContents;
        ReadContents.Size = 50 * 50 * 4;
        ReadContents.Data = new uint8_t[ReadContents.Size];
        const bool ReadStatus = Image->Read(&GC, 0, Start, Size, ReadContents);
        REQUIRE(ReadStatus == true);
        for (int i = 0; i < ReadContents.Size / 4; i++)
        {
            uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.Data);
            REQUIRE(*PixelData == PixelPattern);
        }

        delete Image;
        delete[] ReadContents.Data;
    }

    delete[] InitData.Data;
    rndr::StdAsyncLogger::Get()->ShutDown();
}

TEST_CASE("ImageCopy", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    const rndr::ByteSpan EmptyData;
    const int Width = 400;
    const int Height = 100;
    const int ArraySize = 8;
    const math::Point2 Start;
    const math::Point2 BadStart{200, 500};
    const math::Vector2 Size{50, 50};
    rndr::ByteSpan InitData;
    InitData.Size = Width * Height * 4;
    InitData.Data = new uint8_t[InitData.Size];
    const uint32_t PixelPattern = 0xABABABAB;
    for (int i = 0; i < InitData.Size / 4; i++)
    {
        uint32_t* PixelData = reinterpret_cast<uint32_t*>(InitData.Data);
        *PixelData = PixelPattern;
    }
    rndr::GraphicsContext GC;
    rndr::GraphicsContextProperties Props;
    Props.bDisableGPUTimeout = false;
    Props.bEnableDebugLayer = true;
    Props.bFailWarning = true;
    Props.bMakeThreadSafe = true;
    REQUIRE(GC.Init(Props) == true);

    SECTION("Copy and Read Back")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* RenderingImage = GC.CreateImage(Width, Height, ImageProps, InitData);
        REQUIRE(RenderingImage != nullptr);

        ImageProps.ImageBindFlags = 0;
        ImageProps.Usage = rndr::Usage::Readback;
        rndr::Image* DstImage = GC.CreateImage(Width, Height, ImageProps, EmptyData);
        REQUIRE(DstImage != nullptr);

        const bool CopyStatus = rndr::Image::Copy(&GC, RenderingImage, DstImage);
        REQUIRE(CopyStatus == true);

        rndr::ByteSpan ReadContents;
        ReadContents.Size = 50 * 50 * 4;
        ReadContents.Data = new uint8_t[ReadContents.Size];
        const bool ReadStatus = DstImage->Read(&GC, 0, Start, Size, ReadContents);
        REQUIRE(ReadStatus == true);
        for (int i = 0; i < ReadContents.Size / 4; i++)
        {
            uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.Data);
            REQUIRE(*PixelData == PixelPattern);
        }

        delete RenderingImage;
        delete DstImage;
        delete[] ReadContents.Data;
    }

    delete[] InitData.Data;
    rndr::StdAsyncLogger::Get()->ShutDown();
}

TEST_CASE("FrameBuffer", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    rndr::GraphicsContext GC;
    rndr::GraphicsContextProperties Props;
    Props.bDisableGPUTimeout = false;
    Props.bEnableDebugLayer = true;
    Props.bFailWarning = true;
    Props.bMakeThreadSafe = true;
    REQUIRE(GC.Init(Props) == true);

    SECTION("Default")
    {
        rndr::FrameBufferProperties Props;
        rndr::FrameBuffer* FB = GC.CreateFrameBuffer(100, 400, Props);
        REQUIRE(FB != nullptr);
        delete FB;
    }
    SECTION("All On")
    {
        rndr::FrameBufferProperties Props;
        Props.bUseDepthStencil = true;
        Props.ColorBufferCount = rndr::GraphicsConstants::MaxFrameBufferColorBuffers;
        rndr::FrameBuffer* FB = GC.CreateFrameBuffer(100, 400, Props);
        REQUIRE(FB != nullptr);
        delete FB;
    }
    SECTION("Bad Size")
    {
        rndr::FrameBufferProperties Props;
        rndr::FrameBuffer* FB = GC.CreateFrameBuffer(0, 400, Props);
        REQUIRE(FB == nullptr);
    }
    SECTION("Resize")
    {
        rndr::FrameBufferProperties Props;
        rndr::FrameBuffer* FB = GC.CreateFrameBuffer(100, 400, Props);
        REQUIRE(FB != nullptr);

        bool ResizeStatus = FB->Resize(&GC, 500, 500);
        REQUIRE(ResizeStatus == true);

        bool ResizeStatus2 = FB->Resize(&GC, 200, 0);
        REQUIRE(ResizeStatus2 == false);

        delete FB;
    }

    rndr::StdAsyncLogger::Get()->ShutDown();
}

TEST_CASE("Buffer", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    rndr::GraphicsContext GC;
    REQUIRE(GC.Init() == true);

    rndr::ByteSpan EmptyData;
    const int Size = 32;
    const int Stride = 16;
    rndr::ByteSpan InitData;
    InitData.Size = Size;
    InitData.Data = new uint8_t[Size];
    memset(InitData.Data, 0xAF, Size);

    SECTION("Default No Init Data")
    {
        rndr::BufferProperties Props;
        rndr::Buffer* Buff = GC.CreateBuffer(Props, EmptyData);
        REQUIRE(Buff == nullptr);
    }
    SECTION("Default No Init Data Valid Size")
    {
        rndr::BufferProperties Props;
        Props.Size = Size;
        rndr::Buffer* Buff = GC.CreateBuffer(Props, EmptyData);
        REQUIRE(Buff == nullptr);
    }
    SECTION("Default No Init Data All Valid")
    {
        rndr::BufferProperties Props;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::Buffer* Buff = GC.CreateBuffer(Props, EmptyData);
        REQUIRE(Buff != nullptr);
        delete Buff;
    }
    SECTION("With Init Data")
    {
        rndr::BufferProperties Props;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::Buffer* Buff = GC.CreateBuffer(Props, InitData);
        REQUIRE(Buff != nullptr);
        delete Buff;
    }
    SECTION("Dynamic")
    {
        rndr::BufferProperties Props;
        Props.Usage = rndr::Usage::Dynamic;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::Buffer* Buff = GC.CreateBuffer(Props, InitData);
        REQUIRE(Buff != nullptr);

        SECTION("Update")
        {
            uint8_t UpdateData[16] = {};
            const bool Status = Buff->Update(&GC, rndr::ByteSpan{UpdateData, 16}, 16);
            REQUIRE(Status == true);
        }

        delete Buff;
    }
    SECTION("Readback")
    {
        rndr::BufferProperties Props;
        Props.Usage = rndr::Usage::Readback;
        Props.Type = rndr::BufferType::Readback;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::Buffer* Buff = GC.CreateBuffer(Props, InitData);
        REQUIRE(Buff != nullptr);

        SECTION("Read")
        {
            uint8_t ReadData[16] = {};
            const bool Status = Buff->Read(&GC, rndr::ByteSpan{ReadData, 16}, 16);
            REQUIRE(Status == true);
            for (int i = 0; i < 16; i++)
            {
                REQUIRE(ReadData[i] == 0xAF);
            }
        }

        delete Buff;
    }

    delete InitData.Data;
    rndr::StdAsyncLogger::Get()->ShutDown();
}

TEST_CASE("Sampler", "RenderAPI")
{
    rndr::StdAsyncLogger::Get()->Init();

    rndr::GraphicsContext GC;
    REQUIRE(GC.Init() == true);

    SECTION("Default")
    {
        rndr::SamplerProperties Props;
        rndr::Sampler* S = GC.CreateSampler(Props);
        REQUIRE(S != nullptr);
        delete S;
    }

    rndr::StdAsyncLogger::Get()->ShutDown();
}
