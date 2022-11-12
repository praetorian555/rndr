#include <catch2/catch_test_macros.hpp>

#include "math/math.h"

#include "rndr/core/log.h"
#include "rndr/core/memory.h"
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
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();

    rndr::GraphicsContextProperties Props;
    Props.bDisableGPUTimeout = false;
    Props.bEnableDebugLayer = true;
    Props.bFailWarning = true;
    Props.bMakeThreadSafe = true;

    SECTION("Default")
    {
        rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext(Props);
        REQUIRE(Ctx.IsValid());
    }
    SECTION("Disable GPU Timeout")
    {
        Props.bDisableGPUTimeout = true;
        rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext(Props);
        REQUIRE(Ctx.IsValid());
    }
    SECTION("No Debug Layer")
    {
        Props.bEnableDebugLayer = false;
        rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext(Props);
        REQUIRE(Ctx.IsValid());
    }
    SECTION("Not Thread Safe")
    {
        Props.bMakeThreadSafe = false;
        rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext(Props);
        REQUIRE(Ctx.IsValid());
    }
}

TEST_CASE("Image", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

    SECTION("Default Props with Invalid Width or Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(0, 0, ImageProps, rndr::ByteSpan{});
        REQUIRE(!Image.IsValid());
    }
    SECTION("Default Props with Valid Width and Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
    SECTION("Use as render target and shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget | rndr::ImageBindFlags::ShaderResource;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
    SECTION("Use dynamic image as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Dynamic;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
    SECTION("Create readback image")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
    SECTION("Multisampling Valid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
    SECTION("Multisampling Invalid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 3;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(!Image.Get());
    }
    SECTION("Multisampling Valid Render Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
    SECTION("Multisampling Valid Depth Stencil Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
        REQUIRE(Image.IsValid());
    }
}

TEST_CASE("ImageArray", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

    const rndr::Span<rndr::ByteSpan> EmptyData;
    const int Width = 400;
    const int Height = 100;
    const int ArraySize = 8;
    SECTION("Default Props with Invalid Width or Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(0, 0, ArraySize, ImageProps, EmptyData);
        REQUIRE(!Image.IsValid());
    }
    SECTION("Default Props with Invalid ArraySize")
    {
        rndr::ImageProperties ImageProps;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, 1, ImageProps, EmptyData);
        REQUIRE(!Image.IsValid());
    }
    SECTION("Default Props with Valid Width and Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Use dynamic image array as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Dynamic;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(!Image.IsValid());
    }
    SECTION("Create readback image array")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Multisampling Valid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Multisampling Invalid")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 3;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(!Image.IsValid());
    }
    SECTION("Multisampling Valid Render Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Multisampling Valid Depth Stencil Target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.SampleCount = 8;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
}

TEST_CASE("CubeMap", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

    const rndr::Span<rndr::ByteSpan> EmptyData;
    const int Width = 400;
    const int Height = 400;
    const int ArraySize = 6;
    SECTION("Default Props with Invalid Width or Height")
    {
        rndr::ImageProperties ImageProps;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateCubeMap(0, 0, ImageProps, EmptyData);
        REQUIRE(!Image.IsValid());
    }
    SECTION("Default Props")
    {
        rndr::ImageProperties ImageProps;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Generate Mips")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.bUseMips = true;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Use as render target")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Use as depth stencil texture")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.PixelFormat = rndr::PixelFormat::DEPTH24_STENCIL8;
        ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
    SECTION("Use dynamic cube map as shader resource")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Dynamic;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(!Image.IsValid());
    }
    SECTION("Create readback cube map")
    {
        rndr::ImageProperties ImageProps;
        ImageProps.Usage = rndr::Usage::Readback;
        ImageProps.ImageBindFlags = 0;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateCubeMap(Width, Height, ImageProps, EmptyData);
        REQUIRE(Image.IsValid());
    }
}

TEST_CASE("ImageUpdate", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

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
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyDataArray);
        REQUIRE(Image.IsValid());

        SECTION("Update")
        {
            const bool Result = Image->Update(Ctx.Get(), 0, Start, Size, UpdateData);
            REQUIRE(Result == true);
        }
        SECTION("Invalid Update")
        {
            bool Result;
            Result = Image->Update(Ctx.Get(), ArraySize, Start, Size, UpdateData);
            REQUIRE(Result == false);
            Result = Image->Update(Ctx.Get(), -1, Start, Size, UpdateData);
            REQUIRE(Result == false);
            Result = Image->Update(Ctx.Get(), 3, BadStart, Size, UpdateData);
            REQUIRE(Result == false);
            Result = Image->Update(Ctx.Get(), 3, Start, Size, EmptyData);
            REQUIRE(Result == false);
        }
    }
    SECTION("Dynamic Usage")
    {
        rndr::ImageProperties ImageProps;
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyDataArray);
        REQUIRE(Image.IsValid());

        SECTION("Update")
        {
            const bool Result = Image->Update(Ctx.Get(), 0, Start, Size, UpdateData);
            REQUIRE(Result == true);
        }
    }

    delete[] UpdateData.Data;
}

TEST_CASE("ImageRead", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

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
        rndr::ScopePtr<rndr::Image> Image = Ctx->CreateImage(Width, Height, ImageProps, InitData);
        REQUIRE(Image.IsValid());

        rndr::ByteSpan ReadContents;
        ReadContents.Size = 50 * 50 * 4;
        ReadContents.Data = new uint8_t[ReadContents.Size];
        const bool ReadStatus = Image->Read(Ctx.Get(), 0, Start, Size, ReadContents);
        REQUIRE(ReadStatus == true);
        for (int i = 0; i < ReadContents.Size / 4; i++)
        {
            uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.Data);
            REQUIRE(*PixelData == PixelPattern);
        }

        delete[] ReadContents.Data;
    }

    delete[] InitData.Data;
}

TEST_CASE("ImageCopy", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

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
        rndr::ScopePtr<rndr::Image> RenderingImage = Ctx->CreateImage(Width, Height, ImageProps, InitData);
        REQUIRE(RenderingImage.IsValid());

        ImageProps.ImageBindFlags = 0;
        ImageProps.Usage = rndr::Usage::Readback;
        rndr::ScopePtr<rndr::Image> DstImage = Ctx->CreateImage(Width, Height, ImageProps, EmptyData);
        REQUIRE(DstImage.IsValid());

        const bool CopyStatus = rndr::Image::Copy(Ctx.Get(), RenderingImage.Get(), DstImage.Get());
        REQUIRE(CopyStatus == true);

        rndr::ByteSpan ReadContents;
        ReadContents.Size = 50 * 50 * 4;
        ReadContents.Data = new uint8_t[ReadContents.Size];
        const bool ReadStatus = DstImage->Read(Ctx.Get(), 0, Start, Size, ReadContents);
        REQUIRE(ReadStatus == true);
        for (int i = 0; i < ReadContents.Size / 4; i++)
        {
            uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.Data);
            REQUIRE(*PixelData == PixelPattern);
        }

        delete[] ReadContents.Data;
    }

    delete[] InitData.Data;
}

TEST_CASE("FrameBuffer", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

    SECTION("Default")
    {
        rndr::FrameBufferProperties Props;
        rndr::ScopePtr<rndr::FrameBuffer> FB = Ctx->CreateFrameBuffer(100, 400, Props);
        REQUIRE(FB.IsValid());
    }
    SECTION("All On")
    {
        rndr::FrameBufferProperties Props;
        Props.bUseDepthStencil = true;
        Props.ColorBufferCount = rndr::GraphicsConstants::MaxFrameBufferColorBuffers;
        rndr::ScopePtr<rndr::FrameBuffer> FB = Ctx->CreateFrameBuffer(100, 400, Props);
        REQUIRE(FB.IsValid());
    }
    SECTION("Bad Size")
    {
        rndr::FrameBufferProperties Props;
        rndr::ScopePtr<rndr::FrameBuffer> FB = Ctx->CreateFrameBuffer(0, 400, Props);
        REQUIRE(!FB.IsValid());
    }
    SECTION("Resize")
    {
        rndr::FrameBufferProperties Props;
        rndr::ScopePtr<rndr::FrameBuffer> FB = Ctx->CreateFrameBuffer(100, 400, Props);
        REQUIRE(FB.IsValid());

        bool ResizeStatus = FB->Resize(Ctx.Get(), 500, 500);
        REQUIRE(ResizeStatus == true);

        bool ResizeStatus2 = FB->Resize(Ctx.Get(), 200, 0);
        REQUIRE(ResizeStatus2 == false);
    }
}

TEST_CASE("Buffer", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

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
        rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, EmptyData);
        REQUIRE(!Buff.IsValid());
    }
    SECTION("Default No Init Data Valid Size")
    {
        rndr::BufferProperties Props;
        Props.Size = Size;
        rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, EmptyData);
        REQUIRE(!Buff.IsValid());
    }
    SECTION("Default No Init Data All Valid")
    {
        rndr::BufferProperties Props;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, EmptyData);
        REQUIRE(Buff.IsValid());
    }
    SECTION("With Init Data")
    {
        rndr::BufferProperties Props;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, InitData);
        REQUIRE(Buff.IsValid());
    }
    SECTION("Dynamic")
    {
        rndr::BufferProperties Props;
        Props.Usage = rndr::Usage::Dynamic;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, InitData);
        REQUIRE(Buff.IsValid());

        SECTION("Update")
        {
            uint8_t UpdateData[16] = {};
            const bool Status = Buff->Update(Ctx.Get(), rndr::ByteSpan{UpdateData, 16}, 16);
            REQUIRE(Status == true);
        }
    }
    SECTION("Readback")
    {
        rndr::BufferProperties Props;
        Props.Usage = rndr::Usage::Readback;
        Props.Type = rndr::BufferType::Readback;
        Props.Size = Size;
        Props.Stride = Stride;
        rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, InitData);
        REQUIRE(Buff.IsValid());

        SECTION("Read")
        {
            uint8_t ReadData[16] = {};
            const bool Status = Buff->Read(Ctx.Get(), rndr::ByteSpan{ReadData, 16}, 16);
            REQUIRE(Status == true);
            for (int i = 0; i < 16; i++)
            {
                REQUIRE(ReadData[i] == 0xAF);
            }
        }
    }

    delete InitData.Data;
}

TEST_CASE("Sampler", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

    SECTION("Default")
    {
        rndr::SamplerProperties Props;
        rndr::ScopePtr<rndr::Sampler> S = Ctx->CreateSampler(Props);
        REQUIRE(S.IsValid());
    }
}

TEST_CASE("Shader", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

    SECTION("Default")
    {
        std::string ShaderContents =
            "struct InVertex{};\nstruct OutVertex{float4 Position : SV_POSITION;};\nOutVertex main(InVertex In)\n{\nOutVertex "
            "Out;\nOut.Position = float4(1, 1, 1, 1);\nreturn Out;\n}\n";
        rndr::ByteSpan Data((uint8_t*)ShaderContents.data(), ShaderContents.size());

        rndr::ShaderProperties Props;
        Props.Type = rndr::ShaderType::Vertex;
        Props.EntryPoint = "main";
        rndr::ScopePtr<rndr::Shader> S = Ctx->CreateShader(Data, Props);
        REQUIRE(S.IsValid());
    }
}

TEST_CASE("InputLayout", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

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
        rndr::ScopePtr<rndr::Shader> S = Ctx->CreateShader(Data, Props);
        REQUIRE(S.IsValid());

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

        rndr::ScopePtr<rndr::InputLayout> Layout = Ctx->CreateInputLayout(PackedProps, S.Get());
        REQUIRE(Layout.IsValid());
    }
}

TEST_CASE("InputLayoutBuilder", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

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
        rndr::ScopePtr<rndr::Shader> S = Ctx->CreateShader(Data, Props);
        REQUIRE(S.IsValid());

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

        rndr::ScopePtr<rndr::InputLayout> Layout = Ctx->CreateInputLayout(LayoutProps, S.Get());
        REQUIRE(Layout.IsValid());
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
}

TEST_CASE("RasterizerState", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::GraphicsContext* Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx != nullptr);

    SECTION("Default")
    {
        rndr::RasterizerProperties Props;
        rndr::RasterizerState* S = Ctx->CreateRasterizerState(Props);
        REQUIRE(S != nullptr);
        RNDR_DELETE(rndr::RasterizerState, S);
    }

    RNDR_DELETE(rndr::GraphicsContext, Ctx);
}

TEST_CASE("DepthStencilState", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

    SECTION("Default")
    {
        rndr::DepthStencilProperties Props;
        rndr::ScopePtr<rndr::DepthStencilState> S = Ctx->CreateDepthStencilState(Props);
        REQUIRE(S.IsValid());
    }
}

TEST_CASE("BlendState", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    REQUIRE(Ctx.IsValid());

    SECTION("Default")
    {
        rndr::BlendProperties Props;
        rndr::ScopePtr<rndr::BlendState> S = Ctx->CreateBlendState(Props);
        REQUIRE(S.IsValid());
    }
}

TEST_CASE("SwapChain", "RenderAPI")
{
    std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
    rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
    rndr::ScopePtr<rndr::Window> Win = RndrCtx->CreateWin(800, 600);
    REQUIRE(Ctx.IsValid());

    void* NativeWinHandle = (void*)Win->GetNativeWindowHandle();
    rndr::SwapChainProperties SwapProps;
    rndr::ScopePtr<rndr::SwapChain> S = Ctx->CreateSwapChain(NativeWinHandle, Win->GetWidth(), Win->GetHeight(), SwapProps);
    REQUIRE(S.IsValid());

    Ctx->ClearColor(S->FrameBuffer->ColorBuffers[0], math::Vector4{1, 1, 1, 1});
    Ctx->Present(S.Get(), true);
    Ctx->ClearColor(S->FrameBuffer->ColorBuffers[0], math::Vector4{1, 1, 0.5, 1});
    Ctx->Present(S.Get(), true);
}
