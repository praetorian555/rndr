#include <catch2/catch_test_macros.hpp>

#include "math/math.h"

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
    }
    SECTION("Immutable Image Array")
    {
        rndr::ByteSpan Entry{};
        Entry.Size = Width * Height * 4;
        Entry.Data = new uint8_t[Entry.Size];
        rndr::Span<rndr::ByteSpan> InitData;
        InitData.Size = ArraySize;
        InitData.Data = new rndr::ByteSpan[InitData.Size];
        for (int i = 0; i < ArraySize; i++)
        {
            InitData[i] = Entry;
        }
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::GPURead;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, InitData);
        REQUIRE(Image != nullptr);
        delete[] Entry.Data;
        delete[] InitData.Data;
    }
    SECTION("Immutable Image Array With Invalid Init Data")
    {
        rndr::ByteSpan Entry{};
        Entry.Size = Width * Height * 4;
        Entry.Data = new uint8_t[Entry.Size];
        rndr::Span<rndr::ByteSpan> InitData;
        InitData.Size = ArraySize - 3;
        InitData.Data = new rndr::ByteSpan[InitData.Size];
        for (int i = 0; i < ArraySize - 3; i++)
        {
            InitData[i] = Entry;
        }
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::GPURead;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, InitData);
        REQUIRE(Image == nullptr);
        delete[] Entry.Data;
        delete[] InitData.Data;
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
    }
    SECTION("Use dynamic image array as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::GPUReadCPUWrite;
        ImageProps.CPUAccess = rndr::CPUAccess::Write;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Create staging image array")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::FromGPUToCPU;
        ImageProps.ImageBindFlags = 0;
        ImageProps.CPUAccess = rndr::CPUAccess::Read;
        rndr::Image* Image = GC.CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
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
    }
    SECTION("Immutable Cube Map")
    {
        rndr::ByteSpan Entry{};
        Entry.Size = Width * Height * 4;
        Entry.Data = new uint8_t[Entry.Size];
        rndr::Span<rndr::ByteSpan> InitData;
        InitData.Size = ArraySize;
        InitData.Data = new rndr::ByteSpan[InitData.Size];
        for (int i = 0; i < ArraySize; i++)
        {
            InitData[i] = Entry;
        }
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::GPURead;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, InitData);
        REQUIRE(Image != nullptr);
        delete[] Entry.Data;
        delete[] InitData.Data;
    }
    SECTION("Immutable Cube Map With Invalid Init Data")
    {
        rndr::ByteSpan Entry{};
        Entry.Size = Width * Height * 4;
        Entry.Data = new uint8_t[Entry.Size];
        rndr::Span<rndr::ByteSpan> InitData;
        InitData.Size = ArraySize - 3;
        InitData.Data = new rndr::ByteSpan[InitData.Size];
        for (int i = 0; i < ArraySize - 3; i++)
        {
            InitData[i] = Entry;
        }
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::GPURead;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, InitData);
        REQUIRE(Image == nullptr);
        delete[] Entry.Data;
        delete[] InitData.Data;
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
    }
    SECTION("Use dynamic cube map as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::GPUReadCPUWrite;
        ImageProps.CPUAccess = rndr::CPUAccess::Write;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image == nullptr);
    }
    SECTION("Create staging cube map")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::FromGPUToCPU;
        ImageProps.ImageBindFlags = 0;
        ImageProps.CPUAccess = rndr::CPUAccess::Read;
        rndr::Image* Image = GC.CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image != nullptr);
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
    }

    delete[] UpdateData.Data;
    rndr::StdAsyncLogger::Get()->ShutDown();
}
