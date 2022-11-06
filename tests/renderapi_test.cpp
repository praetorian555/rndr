#include <catch2/catch_test_macros.hpp>

#include "math/math.h"

#include "rndr/core/log.h"
#include "rndr/core/rndrcontext.h"
#include "rndr/core/window.h"

#include "rndr/render/buffer.h"
#include "rndr/render/framebuffer.h"
#include "rndr/render/graphicscontext.h"
#include "rndr/render/image.h"
#include "rndr/render/pipeline.h"
#include "rndr/render/sampler.h"
#include "rndr/render/shader.h"
#include "rndr/render/swapchain.h"

TEST_CASE("GraphicsContext", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());

    rndr::GraphicsContextProperties Props;
    Props.bDisableGPUTimeout = false;
    Props.bEnableDebugLayer = true;
    Props.bFailWarning = true;
    Props.bMakeThreadSafe = true;

    SECTION("Default")
    {
        rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext(Props);
        REQUIRE(Ctx != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
    }
    SECTION("Disable GPU Timeout")
    {
        Props.bDisableGPUTimeout = true;
        rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext(Props);
        REQUIRE(Ctx != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
    }
    SECTION("No Debug Layer")
    {
        Props.bEnableDebugLayer = false;
        rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext(Props);
        REQUIRE(Ctx != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
    }
    SECTION("Not Thread Safe")
    {
        Props.bMakeThreadSafe = false;
        rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext(Props);
        REQUIRE(Ctx != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
    }
}

TEST_CASE("Image", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default Props with Invalid Width or Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = Ctx->CreateImage(0, 0, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image == nullptr);
    }
    SECTION("Default Props with Valid Width and Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use as render target and shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget | rndr::ImageBindFlags::ShaderResource;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use dynamic image as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Dynamic;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Create readback image")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Multisampling Valid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Multisampling Invalid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 3;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image == nullptr);
    }
    SECTION("Multisampling Valid Render Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Multisampling Valid Depth Stencil Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("ImageArray", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    const rndr::Span<rndr::ByteSpan> EmptyData;
    const int Width = 400;
    const int Height = 100;
    const int ArraySize = 8;
    SECTION("Default Props with Invalid Width or Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = Ctx->CreateImageArray(0, 0, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Default Props with Invalid ArraySize")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, 1, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Default Props with Valid Width and Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use dynamic image array as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Dynamic;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Create readback image array")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Multisampling Valid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Multisampling Invalid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 3;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Multisampling Valid Render Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Multisampling Valid Depth Stencil Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("CubeMap", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    const rndr::Span<rndr::ByteSpan> EmptyData;
    const int Width = 400;
    const int Height = 400;
    const int ArraySize = 6;
    SECTION("Default Props with Invalid Width or Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = Ctx->CreateCubeMap(0, 0, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Default Props")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::Image* Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Use dynamic cube map as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Dynamic;
        rndr::Image* Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Create readback cube map")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::Image* Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("ImageUpdate", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

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

    SECTION("Default Usage")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyDataArray);
        REQUIRE(Image != nullptr);

        SECTION("Update")
        {
            const bool Result = Image->Update(Ctx, 0, Start, Size, UpdateData);
            REQUIRE(Result == true);
        }
        SECTION("Invalid Update")
        {
            bool Result;
            Result = Image->Update(Ctx, ArraySize, Start, Size, UpdateData);
            REQUIRE(Result == false);
            Result = Image->Update(Ctx, -1, Start, Size, UpdateData);
            REQUIRE(Result == false);
            Result = Image->Update(Ctx, 3, BadStart, Size, UpdateData);
            REQUIRE(Result == false);
            Result = Image->Update(Ctx, 3, Start, Size, EmptyData);
            REQUIRE(Result == false);
        }

        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }
    SECTION("Dynamic Usage")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyDataArray);
        REQUIRE(Image != nullptr);

        SECTION("Update")
        {
            const bool Result = Image->Update(Ctx, 0, Start, Size, UpdateData);
            REQUIRE(Result == true);
        }

        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
    }

    delete[] UpdateData.Data;
    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("ImageRead", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

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

    SECTION("From GPU to CPU Usage")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::Image* Image = Ctx->CreateImage(Width, Height, ImageProps, InitData);
        REQUIRE(Image != nullptr);

        rndr::ByteSpan ReadContents;
        ReadContents.Size = 50 * 50 * 4;
        ReadContents.Data = new uint8_t[ReadContents.Size];
        const bool ReadStatus = Image->Read(Ctx, 0, Start, Size, ReadContents);
        REQUIRE(ReadStatus == true);
        for (int i = 0; i < ReadContents.Size / 4; i++)
        {
            uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.Data);
            REQUIRE(*PixelData == PixelPattern);
        }

        RNDR_DELETE(RndrCtx.get(), rndr::Image, Image);
        delete[] ReadContents.Data;
    }

    delete[] InitData.Data;
    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("ImageCopy", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

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

    SECTION("Copy and Read Back")
    {
        rndr::ImageProperties ImageProps;
        rndr::Image* RenderingImage = Ctx->CreateImage(Width, Height, ImageProps, InitData);
        REQUIRE(RenderingImage != nullptr);

        ImageProps.ImageBindFlags = 0;
        ImageProps.Usage = rndr::Usage::Readback;
        rndr::Image* DstImage = Ctx->CreateImage(Width, Height, ImageProps, EmptyData);
        REQUIRE(DstImage != nullptr);

        const bool CopyStatus = rndr::Image::Copy(Ctx, RenderingImage, DstImage);
        REQUIRE(CopyStatus == true);

        rndr::ByteSpan ReadContents;
        ReadContents.Size = 50 * 50 * 4;
        ReadContents.Data = new uint8_t[ReadContents.Size];
        const bool ReadStatus = DstImage->Read(Ctx, 0, Start, Size, ReadContents);
        REQUIRE(ReadStatus == true);
        for (int i = 0; i < ReadContents.Size / 4; i++)
        {
            uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.Data);
            REQUIRE(*PixelData == PixelPattern);
        }

        RNDR_DELETE(RndrCtx.get(), rndr::Image, RenderingImage);
        RNDR_DELETE(RndrCtx.get(), rndr::Image, DstImage);
        delete[] ReadContents.Data;
    }

    delete[] InitData.Data;
    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("FrameBuffer", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default")
    {
        rndr::FrameBufferProperties Props;
        rndr::FrameBuffer* FB = Ctx->CreateFrameBuffer(100, 400, Props);
        REQUIRE(FB != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::FrameBuffer, FB);
    }
    SECTION("All On")
    {
        rndr::FrameBufferProperties Props;
        Props.bUseDepthStencil = true;
        Props.ColorBufferCount = rndr::GraphicsConstants::MaxFrameBufferColorBuffers;
        rndr::FrameBuffer* FB = Ctx->CreateFrameBuffer(100, 400, Props);
        REQUIRE(FB != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::FrameBuffer, FB);
    }
    SECTION("Bad Size")
    {
        rndr::FrameBufferProperties Props;
        rndr::FrameBuffer* FB = Ctx->CreateFrameBuffer(0, 400, Props);
        REQUIRE(FB == nullptr);
    }
    SECTION("Resize")
    {
        rndr::FrameBufferProperties Props;
        rndr::FrameBuffer* FB = Ctx->CreateFrameBuffer(100, 400, Props);
        REQUIRE(FB != nullptr);

        bool ResizeStatus = FB->Resize(Ctx, 500, 500);
        REQUIRE(ResizeStatus == true);

        bool ResizeStatus2 = FB->Resize(Ctx, 200, 0);
        REQUIRE(ResizeStatus2 == false);

        RNDR_DELETE(RndrCtx.get(), rndr::FrameBuffer, FB);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("Buffer", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

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
        rndr::Buffer* Buff = Ctx->CreateBuffer(Props, EmptyData);
        REQUIRE(Buff == nullptr);
    }
    SECTION("Default No Init Data Valid Size")
    {
        rndr::BufferProperties Props;
        Props.Size = Size;
        rndr::Buffer* Buff = Ctx->CreateBuffer(Props, EmptyData);
        REQUIRE(Buff == nullptr);
    }
    SECTION("Default No Init Data All Valid")
    {
        rndr::BufferProperties Props;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::Buffer* Buff = Ctx->CreateBuffer(Props, EmptyData);
        REQUIRE(Buff != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Buffer, Buff);
    }
    SECTION("With Init Data")
    {
        rndr::BufferProperties Props;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::Buffer* Buff = Ctx->CreateBuffer(Props, InitData);
        REQUIRE(Buff != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Buffer, Buff);
    }
    SECTION("Dynamic")
    {
        rndr::BufferProperties Props;
        Props.Usage = rndr::Usage::Dynamic;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::Buffer* Buff = Ctx->CreateBuffer(Props, InitData);
        REQUIRE(Buff != nullptr);

        SECTION("Update")
        {
            uint8_t UpdateData[16] = {};
            const bool Status = Buff->Update(Ctx, rndr::ByteSpan{UpdateData, 16}, 16);
            REQUIRE(Status == true);
        }

        RNDR_DELETE(RndrCtx.get(), rndr::Buffer, Buff);
    }
    SECTION("Readback")
    {
        rndr::BufferProperties Props;
        Props.Usage = rndr::Usage::Readback;
        Props.Type = rndr::BufferType::Readback;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::Buffer* Buff = Ctx->CreateBuffer(Props, InitData);
        REQUIRE(Buff != nullptr);

        SECTION("Read")
        {
            uint8_t ReadData[16] = {};
            const bool Status = Buff->Read(Ctx, rndr::ByteSpan{ReadData, 16}, 16);
            REQUIRE(Status == true);
            for (int i = 0; i < 16; i++)
            {
                REQUIRE(ReadData[i] == 0xAF);
            }
        }

        RNDR_DELETE(RndrCtx.get(), rndr::Buffer, Buff);
    }

    delete InitData.Data;
    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("Sampler", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default")
    {
        rndr::SamplerProperties Props;
        rndr::Sampler* S = Ctx->CreateSampler(Props);
        REQUIRE(S != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Sampler, S);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("Shader", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default")
    {
        std::string ShaderContents =
            "struct InVertex{};\nstruct OutVertex{float4 Position : SV_POSITION;};\nOutVertex main(InVertex In)\n{\nOutVertex "
            "Out;\nOut.Position = float4(1, 1, 1, 1);\nreturn Out;\n}\n";
        rndr::ByteSpan Data((uint8_t*)ShaderContents.data(), ShaderContents.size());

        rndr::ShaderProperties Props;
        Props.Type = rndr::ShaderType::Vertex;
        Props.EntryPoint = "main";
        rndr::Shader* S = Ctx->CreateShader(Data, Props);
        REQUIRE(S != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::Shader, S);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("InputLayout", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default")
    {
        std::string ShaderContents =
            "struct InVertex{ float4 Position : POSITION; float3 Normal : NORMAL; };\nstruct OutVertex{float4 Position : "
            "SV_POSITION;};\nOutVertex main(InVertex In)\n{\nOutVertex "
            "Out;\nOut.Position = float4(1, 1, 1, 1);\nreturn Out;\n}\n";
        rndr::ByteSpan Data((uint8_t*)ShaderContents.data(), ShaderContents.size());

        rndr::ShaderProperties Props;
        Props.Type = rndr::ShaderType::Vertex;
        Props.EntryPoint = "main";
        rndr::Shader* S = Ctx->CreateShader(Data, Props);
        REQUIRE(S != nullptr);

        rndr::InputLayoutProperties LayoutProps[2];
        LayoutProps[0].SemanticName = "POSITION";
        LayoutProps[0].SemanticIndex = 0;
        LayoutProps[0].InputSlot = 0;
        LayoutProps[0].OffsetInVertex = 0;
        LayoutProps[0].Repetition = rndr::DataRepetition::PerVertex;
        LayoutProps[0].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
        LayoutProps[1].SemanticName = "NORMAL";
        LayoutProps[1].SemanticIndex = 0;
        LayoutProps[1].InputSlot = 0;
        LayoutProps[1].OffsetInVertex = rndr::AppendAlignedElement;
        LayoutProps[1].Repetition = rndr::DataRepetition::PerVertex;
        LayoutProps[1].Format = rndr::PixelFormat::R32G32B32_FLOAT;
        rndr::Span<rndr::InputLayoutProperties> PackedProps(LayoutProps, 2);

        rndr::InputLayout* Layout = Ctx->CreateInputLayout(PackedProps, S);
        REQUIRE(Layout != nullptr);

        RNDR_DELETE(RndrCtx.get(), rndr::InputLayout, Layout);
        RNDR_DELETE(RndrCtx.get(), rndr::Shader, S);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("InputLayoutBuilder", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default")
    {
        std::string ShaderContents =
            "struct InVertex{ float4 Position : POSITION; float3 Normal : NORMAL; };\nstruct OutVertex{float4 Position : "
            "SV_POSITION;};\nOutVertex main(InVertex In)\n{\nOutVertex "
            "Out;\nOut.Position = float4(1, 1, 1, 1);\nreturn Out;\n}\n";
        rndr::ByteSpan Data((uint8_t*)ShaderContents.data(), ShaderContents.size());

        rndr::ShaderProperties Props;
        Props.Type = rndr::ShaderType::Vertex;
        Props.EntryPoint = "main";
        rndr::Shader* S = Ctx->CreateShader(Data, Props);
        REQUIRE(S != nullptr);

        rndr::InputLayoutBuilder Builder;
        rndr::Span<rndr::InputLayoutProperties> LayoutProps = Builder.AddBuffer(0, rndr::DataRepetition::PerVertex, 0)
                                                                  .AppendElement(0, "POSITION", rndr::PixelFormat::R32G32B32A32_FLOAT)
                                                                  .AppendElement(0, "NORMAL", rndr::PixelFormat::R32G32B32_FLOAT)
                                                                  .Build();

        REQUIRE(LayoutProps.Size == 2);
        REQUIRE(LayoutProps[0].SemanticName == "POSITION");
        REQUIRE(LayoutProps[0].SemanticIndex == 0);
        REQUIRE(LayoutProps[0].InputSlot == 0);
        REQUIRE(LayoutProps[0].OffsetInVertex == 0);
        REQUIRE(LayoutProps[0].Repetition == rndr::DataRepetition::PerVertex);
        REQUIRE(LayoutProps[0].Format == rndr::PixelFormat::R32G32B32A32_FLOAT);
        REQUIRE(LayoutProps[1].SemanticName == "NORMAL");
        REQUIRE(LayoutProps[1].SemanticIndex == 0);
        REQUIRE(LayoutProps[1].InputSlot == 0);
        REQUIRE(LayoutProps[1].OffsetInVertex == rndr::AppendAlignedElement);
        REQUIRE(LayoutProps[1].Repetition == rndr::DataRepetition::PerVertex);
        REQUIRE(LayoutProps[1].Format == rndr::PixelFormat::R32G32B32_FLOAT);

        rndr::InputLayout* Layout = Ctx->CreateInputLayout(LayoutProps, S);
        REQUIRE(Layout != nullptr);

        RNDR_DELETE(RndrCtx.get(), rndr::InputLayout, Layout);
        RNDR_DELETE(RndrCtx.get(), rndr::Shader, S);
    }
    SECTION("Complex")
    {
        rndr::InputLayoutBuilder Builder;
        rndr::Span<rndr::InputLayoutProperties> LayoutProps = Builder.AddBuffer(0, rndr::DataRepetition::PerInstance, 1)
                                                                  .AppendElement(0, "POSITION", rndr::PixelFormat::R32G32_FLOAT)
                                                                  .AppendElement(0, "POSITION", rndr::PixelFormat::R32G32_FLOAT)
                                                                  .AppendElement(0, "TEXCOORD", rndr::PixelFormat::R32G32_FLOAT)
                                                                  .AppendElement(0, "TEXCOORD", rndr::PixelFormat::R32G32_FLOAT)
                                                                  .AppendElement(0, "COLOR", rndr::PixelFormat::R32G32B32A32_FLOAT)
                                                                  .AppendElement(0, "BLENDINDICES", rndr::PixelFormat::R32_FLOAT)
                                                                  .AppendElement(0, "BLENDINDICES", rndr::PixelFormat::R32_FLOAT)
                                                                  .AppendElement(0, "BLENDINDICES", rndr::PixelFormat::R32_FLOAT)
                                                                  .AppendElement(0, "BLENDINDICES", rndr::PixelFormat::R32_FLOAT)
                                                                  .Build();

        REQUIRE(LayoutProps.Size == 9);
        REQUIRE(LayoutProps[0].SemanticName == "POSITION");
        REQUIRE(LayoutProps[0].SemanticIndex == 0);
        REQUIRE(LayoutProps[0].InputSlot == 0);
        REQUIRE(LayoutProps[0].Format == rndr::PixelFormat::R32G32_FLOAT);
        REQUIRE(LayoutProps[0].OffsetInVertex == 0);
        REQUIRE(LayoutProps[0].Repetition == rndr::DataRepetition::PerInstance);
        REQUIRE(LayoutProps[0].InstanceStepRate == 1);
        REQUIRE(LayoutProps[1].SemanticName == "POSITION");
        REQUIRE(LayoutProps[1].SemanticIndex == 1);
        REQUIRE(LayoutProps[1].InputSlot == 0);
        REQUIRE(LayoutProps[1].Format == rndr::PixelFormat::R32G32_FLOAT);
        REQUIRE(LayoutProps[1].OffsetInVertex == rndr::AppendAlignedElement);
        REQUIRE(LayoutProps[1].Repetition == rndr::DataRepetition::PerInstance);
        REQUIRE(LayoutProps[1].InstanceStepRate == 1);
        REQUIRE(LayoutProps[2].SemanticName == "TEXCOORD");
        REQUIRE(LayoutProps[2].SemanticIndex == 0);
        REQUIRE(LayoutProps[2].InputSlot == 0);
        REQUIRE(LayoutProps[2].Format == rndr::PixelFormat::R32G32_FLOAT);
        REQUIRE(LayoutProps[2].OffsetInVertex == rndr::AppendAlignedElement);
        REQUIRE(LayoutProps[2].Repetition == rndr::DataRepetition::PerInstance);
        REQUIRE(LayoutProps[2].InstanceStepRate == 1);
        REQUIRE(LayoutProps[3].SemanticName == "TEXCOORD");
        REQUIRE(LayoutProps[3].SemanticIndex == 1);
        REQUIRE(LayoutProps[3].InputSlot == 0);
        REQUIRE(LayoutProps[3].Format == rndr::PixelFormat::R32G32_FLOAT);
        REQUIRE(LayoutProps[3].OffsetInVertex == rndr::AppendAlignedElement);
        REQUIRE(LayoutProps[3].Repetition == rndr::DataRepetition::PerInstance);
        REQUIRE(LayoutProps[3].InstanceStepRate == 1);
        REQUIRE(LayoutProps[4].SemanticName == "COLOR");
        REQUIRE(LayoutProps[4].SemanticIndex == 0);
        REQUIRE(LayoutProps[4].InputSlot == 0);
        REQUIRE(LayoutProps[4].Format == rndr::PixelFormat::R32G32B32A32_FLOAT);
        REQUIRE(LayoutProps[4].OffsetInVertex == rndr::AppendAlignedElement);
        REQUIRE(LayoutProps[4].Repetition == rndr::DataRepetition::PerInstance);
        REQUIRE(LayoutProps[4].InstanceStepRate == 1);
        REQUIRE(LayoutProps[5].SemanticName == "BLENDINDICES");
        REQUIRE(LayoutProps[5].SemanticIndex == 0);
        REQUIRE(LayoutProps[5].InputSlot == 0);
        REQUIRE(LayoutProps[5].Format == rndr::PixelFormat::R32_FLOAT);
        REQUIRE(LayoutProps[5].OffsetInVertex == rndr::AppendAlignedElement);
        REQUIRE(LayoutProps[5].Repetition == rndr::DataRepetition::PerInstance);
        REQUIRE(LayoutProps[5].InstanceStepRate == 1);
        REQUIRE(LayoutProps[6].SemanticName == "BLENDINDICES");
        REQUIRE(LayoutProps[6].SemanticIndex == 1);
        REQUIRE(LayoutProps[6].InputSlot == 0);
        REQUIRE(LayoutProps[6].Format == rndr::PixelFormat::R32_FLOAT);
        REQUIRE(LayoutProps[6].OffsetInVertex == rndr::AppendAlignedElement);
        REQUIRE(LayoutProps[6].Repetition == rndr::DataRepetition::PerInstance);
        REQUIRE(LayoutProps[6].InstanceStepRate == 1);
        REQUIRE(LayoutProps[7].SemanticName == "BLENDINDICES");
        REQUIRE(LayoutProps[7].SemanticIndex == 2);
        REQUIRE(LayoutProps[7].InputSlot == 0);
        REQUIRE(LayoutProps[7].Format == rndr::PixelFormat::R32_FLOAT);
        REQUIRE(LayoutProps[7].OffsetInVertex == rndr::AppendAlignedElement);
        REQUIRE(LayoutProps[7].Repetition == rndr::DataRepetition::PerInstance);
        REQUIRE(LayoutProps[7].InstanceStepRate == 1);
        REQUIRE(LayoutProps[8].SemanticName == "BLENDINDICES");
        REQUIRE(LayoutProps[8].SemanticIndex == 3);
        REQUIRE(LayoutProps[8].InputSlot == 0);
        REQUIRE(LayoutProps[8].Format == rndr::PixelFormat::R32_FLOAT);
        REQUIRE(LayoutProps[8].OffsetInVertex == rndr::AppendAlignedElement);
        REQUIRE(LayoutProps[8].Repetition == rndr::DataRepetition::PerInstance);
        REQUIRE(LayoutProps[8].InstanceStepRate == 1);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("RasterizerState", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default")
    {
        rndr::RasterizerProperties Props;
        rndr::RasterizerState* S = Ctx->CreateRasterizerState(Props);
        REQUIRE(S != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::RasterizerState, S);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("DepthStencilState", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default")
    {
        rndr::DepthStencilProperties Props;
        rndr::DepthStencilState* S = Ctx->CreateDepthStencilState(Props);
        REQUIRE(S != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::DepthStencilState, S);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("BlendState", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default")
    {
        rndr::BlendProperties Props;
        rndr::BlendState* S = Ctx->CreateBlendState(Props);
        REQUIRE(S != nullptr);
        RNDR_DELETE(RndrCtx.get(), rndr::BlendState, S);
    }

    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}

TEST_CASE("SwapChain", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx(new rndr::RndrContext());
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    rndr::Window* Win = RndrCtx->CreateWin(800, 600);
    REQUIRE(Ctx != nullptr);

    void* NativeWinHandle = (void*)Win->GetNativeWindowHandle();
    rndr::SwapChainProperties SwapProps;
    rndr::SwapChain* S = Ctx->CreateSwapChain(NativeWinHandle, Win->GetWidth(), Win->GetHeight(), SwapProps);
    REQUIRE(S != nullptr);

    Ctx->ClearColor(S->FrameBuffer->ColorBuffers[0], math::Vector4{1, 1, 1, 1});
    Ctx->Present(S, true);
    Ctx->ClearColor(S->FrameBuffer->ColorBuffers[0], math::Vector4{1, 1, 0.5, 1});
    Ctx->Present(S, true);

    RNDR_DELETE(RndrCtx.get(), rndr::SwapChain, S);
    RNDR_DELETE(RndrCtx.get(), rndr::Window, Win);
    RNDR_DELETE(RndrCtx.get(), rndr::GraphicsContext, Ctx);
}
