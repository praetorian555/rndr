#include <catch2/catch_test_macros.hpp>

#include "rndr/core/log.h"

#include "rndr/render/graphicscontext.h"
#include "rndr/render/image.h"

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
    }
    SECTION("Immutable Image")
    {
        const int Width = 100;
        const int Height = 400;
        rndr::ByteSpan InitData{};
        InitData.Size = Width * Height * 4;
        InitData.Data = new uint8_t[InitData.Size];
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::GPURead;
        rndr::Image* Image = GC.CreateImage(Width, Height, ImageProps, InitData);
        REQUIRE(Image != nullptr);
        delete[] InitData.Data;
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
    }
    SECTION("Use as render target and shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget | rndr::ImageBindFlags::ShaderResource;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
    }
    SECTION("Use dynamic image as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::GPUReadCPUWrite;
        ImageProps.CPUAccess = rndr::CPUAccess::Write;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
    }
    SECTION("Create staging image")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::FromGPUToCPU;
        ImageProps.ImageBindFlags = 0;
        ImageProps.CPUAccess = rndr::CPUAccess::Read;
        rndr::Image* Image = GC.CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image != nullptr);
    }

    rndr::StdAsyncLogger::Get()->ShutDown();
}
