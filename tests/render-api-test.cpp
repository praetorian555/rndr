#include <catch2/catch_test_macros.hpp>

#include "rndr/rndr.h"

TEST_CASE("Graphics context", "[render-api][graphics-context]")
{
    Rndr::Init();

    SECTION("Default")
    {
        const Rndr::Window hidden_window({.start_visible = false});
        const Rndr::GraphicsContextDesc desc{.window_handle = hidden_window.GetNativeWindowHandle()};
        const Rndr::GraphicsContext graphics_context(desc);
        REQUIRE(graphics_context.IsValid());
        REQUIRE(graphics_context.GetDesc().enable_debug_layer == true);
        REQUIRE(graphics_context.GetDesc().window_handle == hidden_window.GetNativeWindowHandle());
    }
    SECTION("No Debug Layer")
    {
        const Rndr::Window hidden_window({.start_visible = false});
        const Rndr::GraphicsContextDesc desc{.enable_debug_layer = false, .window_handle = hidden_window.GetNativeWindowHandle()};
        const Rndr::GraphicsContext graphics_context(desc);
        REQUIRE(graphics_context.IsValid());
        REQUIRE(graphics_context.GetDesc().enable_debug_layer == false);
        REQUIRE(graphics_context.GetDesc().window_handle == hidden_window.GetNativeWindowHandle());
    }
    SECTION("No window handle")
    {
        const Rndr::GraphicsContext graphics_context({});
#if RNDR_OPENGL
        REQUIRE(!graphics_context.IsValid());
#endif
        REQUIRE(graphics_context.GetDesc().enable_debug_layer == true);
        REQUIRE(graphics_context.GetDesc().window_handle == nullptr);
    }

    Rndr::Destroy();
}

// TEST_CASE("CommandList", "RenderAPI")
//{
//     const std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//
//     SECTION("Default")
//     {
//         rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//         REQUIRE(Ctx.IsValid());
//         const rndr::ScopePtr<rndr::CommandList> CL = Ctx->CreateCommandList();
//         REQUIRE(CL.IsValid());
//     }
//
//     SECTION("Single-threaded")
//     {
//         rndr::ScopePtr<rndr::GraphicsContext> Ctx =
//             RndrCtx->CreateGraphicsContext({.IsResourceCreationThreadSafe = false});
//         REQUIRE(Ctx.IsValid());
//         const rndr::ScopePtr<rndr::CommandList> CL = Ctx->CreateCommandList();
//         REQUIRE(!CL.IsValid());
//     }
//     // TODO(Marko): Move to a different test files, this one should be only about creation
//     SECTION("Submitting Commands")
//     {
//         rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//         REQUIRE(Ctx.IsValid());
//         rndr::ScopePtr<rndr::CommandList> CL = Ctx->CreateCommandList();
//         REQUIRE(CL.IsValid());
//
//         constexpr int kImageWidth = 800;
//         constexpr int kImageHeight = 600;
//         rndr::ImageProperties Props;
//         Props.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
//         rndr::ScopePtr<rndr::Image> Im =
//             Ctx->CreateImage(kImageWidth, kImageHeight, Props, rndr::ByteSpan{});
//         REQUIRE(Im.IsValid());
//
//         CL->ClearColor(Im.Get(), math::Vector4{1, 1, 1, 1});
//
//         CL->Finish(Ctx.Get());
//         REQUIRE(CL->IsFinished());
//
//         const bool Status = Ctx->SubmitCommandList(CL.Get());
//         REQUIRE(Status == true);
//     }
// }
//
// TEST_CASE("Image", "RenderAPI")
//{
//     const std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     constexpr int kSampleCount = 8;
//
//     SECTION("Default Props with Invalid Width or Height")
//     {
//         const rndr::ImageProperties ImageProps;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(0, 0, ImageProps, rndr::ByteSpan{});
//         REQUIRE(!Image.IsValid());
//     }
//     SECTION("Default Props with Valid Width and Height")
//     {
//         const rndr::ImageProperties ImageProps;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Generate Mips")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.UseMips = true;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use as render target")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use as depth stencil texture")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.PixelFormat = rndr::PixelFormat::D24_UNORM_S8_UINT;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use as render target and shader resource")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.ImageBindFlags =
//             rndr::ImageBindFlags::RenderTarget | rndr::ImageBindFlags::ShaderResource;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use dynamic image as shader resource")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.Usage = rndr::Usage::Dynamic;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Create readback image")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.Usage = rndr::Usage::Readback;
//         ImageProps.ImageBindFlags = 0;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Multisampling Valid")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.SampleCount = kSampleCount;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Multisampling Invalid")
//     {
//         rndr::ImageProperties ImageProps;
//         constexpr int kInvalidSampleCount = 3;
//         ImageProps.SampleCount = kInvalidSampleCount;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(!Image.IsValid());
//     }
//     SECTION("Multisampling Valid Render Target")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.SampleCount = kSampleCount;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Multisampling Valid Depth Stencil Target")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.SampleCount = kSampleCount;
//         ImageProps.PixelFormat = rndr::PixelFormat::D24_UNORM_S8_UINT;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Unordered Access")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::UnorderedAccess;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImage(100, 400, ImageProps, rndr::ByteSpan{});
//         REQUIRE(Image.IsValid());
//     }
// }
//
// TEST_CASE("ImageArray", "RenderAPI")
//{
//     const std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     const rndr::Span<rndr::ByteSpan> EmptyData;
//     constexpr int kWidth = 400;
//     constexpr int kHeight = 100;
//     constexpr int kArraySize = 8;
//     constexpr int kSampleCount = 8;
//     SECTION("Default Props with Invalid Width or Height")
//     {
//         const rndr::ImageProperties ImageProps;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(0, 0, kArraySize, ImageProps, EmptyData);
//         REQUIRE(!Image.IsValid());
//     }
//     SECTION("Default Props with Invalid ArraySize")
//     {
//         const rndr::ImageProperties ImageProps;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, 1, ImageProps, EmptyData);
//         REQUIRE(!Image.IsValid());
//     }
//     SECTION("Default Props with Valid Width and Height")
//     {
//         const rndr::ImageProperties ImageProps;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Generate Mips")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.UseMips = true;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use as render target")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use as depth stencil texture")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.PixelFormat = rndr::PixelFormat::D24_UNORM_S8_UINT;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use dynamic image array as shader resource")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.Usage = rndr::Usage::Dynamic;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(!Image.IsValid());
//     }
//     SECTION("Create readback image array")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.Usage = rndr::Usage::Readback;
//         ImageProps.ImageBindFlags = 0;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Multisampling Valid")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.SampleCount = kSampleCount;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Multisampling Invalid")
//     {
//         rndr::ImageProperties ImageProps;
//         constexpr int kInvalidSampleCount = 3;
//         ImageProps.SampleCount = kInvalidSampleCount;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(!Image.IsValid());
//     }
//     SECTION("Multisampling Valid Render Target")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.SampleCount = kSampleCount;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Multisampling Valid Depth Stencil Target")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.SampleCount = kSampleCount;
//         ImageProps.PixelFormat = rndr::PixelFormat::D24_UNORM_S8_UINT;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Unordered Access")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::UnorderedAccess;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(kWidth, kHeight, kArraySize, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
// }
//
// TEST_CASE("CubeMap", "RenderAPI")
//{
//     const std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     const rndr::Span<rndr::ByteSpan> EmptyData;
//     constexpr int kWidth = 400;
//     constexpr int kHeight = 400;
//     SECTION("Default Props with Invalid Width or Height")
//     {
//         const rndr::ImageProperties ImageProps;
//         const rndr::ScopePtr<rndr::Image> Image = Ctx->CreateCubeMap(0, 0, ImageProps, EmptyData);
//         REQUIRE(!Image.IsValid());
//     }
//     SECTION("Default Props")
//     {
//         const rndr::ImageProperties ImageProps;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateCubeMap(kWidth, kHeight, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Generate Mips")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.UseMips = true;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateCubeMap(kWidth, kHeight, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use as render target")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::RenderTarget;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateCubeMap(kWidth, kHeight, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use as depth stencil texture")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.PixelFormat = rndr::PixelFormat::D24_UNORM_S8_UINT;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::DepthStencil;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateCubeMap(kWidth, kHeight, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Use dynamic cube map as shader resource")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.Usage = rndr::Usage::Dynamic;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateCubeMap(kWidth, kHeight, ImageProps, EmptyData);
//         REQUIRE(!Image.IsValid());
//     }
//     SECTION("Create readback cube map")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.Usage = rndr::Usage::Readback;
//         ImageProps.ImageBindFlags = 0;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateCubeMap(kWidth, kHeight, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
//     SECTION("Unordered Access")
//     {
//         rndr::ImageProperties ImageProps;
//         ImageProps.PixelFormat = rndr::PixelFormat::R32_TYPELESS;
//         ImageProps.ImageBindFlags = rndr::ImageBindFlags::UnorderedAccess;
//         const rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateCubeMap(kWidth, kHeight, ImageProps, EmptyData);
//         REQUIRE(Image.IsValid());
//     }
// }
//
//// TODO(Marko): Move to separate test file.
// TEST_CASE("ImageRead", "RenderAPI")
//{
//     const std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     constexpr int kWidth = 10;
//     constexpr int kHeight = 10;
//     constexpr int kTotalByteSize = kWidth * kHeight * 4;
//     rndr::ByteArray InitData(kTotalByteSize);
//     constexpr uint32_t kFirstPixelPattern = 0xABABABAB;
//     constexpr uint32_t kSecondPixelPattern = 0xCDCDCDCD;
//     // Fill the first half with the first pattern
//     for (size_t i = 0; i < InitData.size() / 8; i++)
//     {
//         uint32_t* PixelData = reinterpret_cast<uint32_t*>(InitData.data());
//         PixelData[i] = kFirstPixelPattern;
//     }
//     // Fill the second half with the second pattern
//     for (size_t i = InitData.size() / 8; i < InitData.size() / 4; i++)
//     {
//         uint32_t* PixelData = reinterpret_cast<uint32_t*>(InitData.data());
//         PixelData[i] = kSecondPixelPattern;
//     }
//
//     rndr::ImageProperties ImageProps;
//     ImageProps.Usage = rndr::Usage::Readback;
//     ImageProps.ImageBindFlags = 0;
//     rndr::ScopePtr<rndr::Image> Image =
//         Ctx->CreateImage(kWidth, kHeight, ImageProps, rndr::ByteSpan(InitData));
//     REQUIRE(Image.IsValid());
//
//     SECTION("Read Full")
//     {
//         rndr::ByteArray ReadContents(kTotalByteSize);
//         const math::Point2& ReadStart{0, 0};
//         const math::Vector2& ReadSize{static_cast<float>(kWidth), static_cast<float>(kHeight)};
//         const bool ReadStatus =
//             Image->Read(Ctx.Get(), 0, ReadStart, ReadSize, rndr::ByteSpan(ReadContents));
//         REQUIRE(ReadStatus == true);
//         for (size_t i = 0; i < ReadContents.size() / 8; i++)
//         {
//             uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.data());
//             REQUIRE(PixelData[i] == kFirstPixelPattern);
//         }
//         for (size_t i = InitData.size() / 8; i < InitData.size() / 4; i++)
//         {
//             uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.data());
//             REQUIRE(PixelData[i] == kSecondPixelPattern);
//         }
//     }
//     SECTION("Read Partial First Half")
//     {
//         rndr::ByteArray ReadContents(kTotalByteSize / 2);
//         const math::Point2& ReadStart{0, 0};
//         const math::Vector2& ReadSize{static_cast<float>(kWidth), static_cast<float>(kHeight / 2)};
//         const bool ReadStatus =
//             Image->Read(Ctx.Get(), 0, ReadStart, ReadSize, rndr::ByteSpan(ReadContents));
//         REQUIRE(ReadStatus == true);
//         for (size_t i = 0; i < ReadContents.size() / 4; i++)
//         {
//             uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.data());
//             REQUIRE(PixelData[i] == kFirstPixelPattern);
//         }
//     }
//     SECTION("Read Partial Second Half")
//     {
//         rndr::ByteArray ReadContents(kTotalByteSize / 2);
//         const math::Point2& ReadStart{0, kHeight / 2};
//         const math::Vector2& ReadSize{static_cast<float>(kWidth), static_cast<float>(kHeight / 2)};
//         const bool ReadStatus =
//             Image->Read(Ctx.Get(), 0, ReadStart, ReadSize, rndr::ByteSpan(ReadContents));
//         REQUIRE(ReadStatus == true);
//         for (size_t i = 0; i < ReadContents.size() / 4; i++)
//         {
//             uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.data());
//             REQUIRE(PixelData[i] == kSecondPixelPattern);
//         }
//     }
//
//     // TODO(Marko): Write test to read from image array.
// }
//
//// TODO(Marko): Move this to a separate test file
// TEST_CASE("ImageUpdate", "RenderAPI")
//{
//     const std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     const rndr::Span<rndr::ByteSpan> EmptyDataArray;
//     const rndr::ByteSpan EmptyData;
//     const int Width = 400;
//     const int Height = 100;
//     const int ArraySize = 8;
//     const math::Point2 Start;
//     const math::Point2 BadStart{200, 500};
//     const math::Vector2 Size{50, 50};
//     rndr::ByteSpan UpdateData;
//     UpdateData.Size = 50 * 50 * 4;
//     UpdateData.Data = new uint8_t[UpdateData.Size];
//
//     SECTION("Default Usage")
//     {
//         rndr::ImageProperties ImageProps;
//         rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyDataArray);
//         REQUIRE(Image.IsValid());
//
//         SECTION("Update")
//         {
//             // TODO(Marko): Verify the bytes on the CPU side.
//             const bool Result = Image->Update(Ctx.Get(), 0, Start, Size, UpdateData);
//             REQUIRE(Result == true);
//         }
//         // TODO(Marko): Move this to separate thing, doesn't really care if its Default or Dynamic
//         // usage.
//         SECTION("Invalid Update")
//         {
//             bool Result;
//             Result = Image->Update(Ctx.Get(), ArraySize, Start, Size, UpdateData);
//             REQUIRE(Result == false);
//             Result = Image->Update(Ctx.Get(), -1, Start, Size, UpdateData);
//             REQUIRE(Result == false);
//             Result = Image->Update(Ctx.Get(), 3, BadStart, Size, UpdateData);
//             REQUIRE(Result == false);
//             Result = Image->Update(Ctx.Get(), 3, Start, Size, EmptyData);
//             REQUIRE(Result == false);
//         }
//     }
//     SECTION("Dynamic Usage")
//     {
//         rndr::ImageProperties ImageProps;
//         rndr::ScopePtr<rndr::Image> Image =
//             Ctx->CreateImageArray(Width, Height, ArraySize, ImageProps, EmptyDataArray);
//         REQUIRE(Image.IsValid());
//
//         SECTION("Update")
//         {
//             // TODO(Marko): Verify the bytes on the CPU side.
//             const bool Result = Image->Update(Ctx.Get(), 0, Start, Size, UpdateData);
//             REQUIRE(Result == true);
//         }
//
//         // TODO(Marko): Write a test that does a partial update from zero start.
//         // TODO(Marko): Write a test that does a partial update from random, non-zero start.
//     }
//
//     delete[] UpdateData.Data;
// }
//
//// TODO(Marko): Move this to separate test file.
// TEST_CASE("ImageCopy", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     const rndr::ByteSpan EmptyData;
//     const int Width = 400;
//     const int Height = 100;
//     const math::Point2 Start;
//     const math::Point2 BadStart{200, 500};
//     const math::Vector2 Size{50, 50};
//     rndr::ByteSpan InitData;
//     InitData.Size = Width * Height * 4;
//     InitData.Data = new uint8_t[InitData.Size];
//     const uint32_t PixelPattern = 0xABABABAB;
//     for (int i = 0; i < InitData.Size / 4; i++)
//     {
//         uint32_t* PixelData = reinterpret_cast<uint32_t*>(InitData.Data);
//         *PixelData = PixelPattern;
//     }
//
//     SECTION("Copy and Read Back")
//     {
//         rndr::ImageProperties ImageProps;
//         rndr::ScopePtr<rndr::Image> RenderingImage =
//             Ctx->CreateImage(Width, Height, ImageProps, InitData);
//         REQUIRE(RenderingImage.IsValid());
//
//         ImageProps.ImageBindFlags = 0;
//         ImageProps.Usage = rndr::Usage::Readback;
//         rndr::ScopePtr<rndr::Image> DstImage =
//             Ctx->CreateImage(Width, Height, ImageProps, EmptyData);
//         REQUIRE(DstImage.IsValid());
//
//         const bool CopyStatus = rndr::Image::Copy(Ctx.Get(), RenderingImage.Get(), DstImage.Get());
//         REQUIRE(CopyStatus == true);
//
//         rndr::ByteSpan ReadContents;
//         ReadContents.Size = 50 * 50 * 4;
//         ReadContents.Data = new uint8_t[ReadContents.Size];
//         const bool ReadStatus = DstImage->Read(Ctx.Get(), 0, Start, Size, ReadContents);
//         REQUIRE(ReadStatus == true);
//         for (int i = 0; i < ReadContents.Size / 4; i++)
//         {
//             uint32_t* PixelData = reinterpret_cast<uint32_t*>(ReadContents.Data);
//             REQUIRE(*PixelData == PixelPattern);
//         }
//
//         delete[] ReadContents.Data;
//     }
//
//     // TODO(Marko): Write a test for full copy.
//     // TODO(Marko): Write a test for partial copy from non-zero start.
//
//     delete[] InitData.Data;
// }
//
// TEST_CASE("FrameBuffer", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     SECTION("Default")
//     {
//         rndr::FrameBufferProperties Props;
//         rndr::ScopePtr<rndr::FrameBuffer> FB = Ctx->CreateFrameBuffer(100, 400, Props);
//         REQUIRE(FB.IsValid());
//     }
//     SECTION("All On")
//     {
//         rndr::FrameBufferProperties Props;
//         Props.UseDepthStencil = true;
//         Props.ColorBufferCount = rndr::GraphicsConstants::kMaxFrameBufferColorBuffers;
//         rndr::ScopePtr<rndr::FrameBuffer> FB = Ctx->CreateFrameBuffer(100, 400, Props);
//         REQUIRE(FB.IsValid());
//     }
//     SECTION("Bad Size")
//     {
//         rndr::FrameBufferProperties Props;
//         rndr::ScopePtr<rndr::FrameBuffer> FB = Ctx->CreateFrameBuffer(0, 400, Props);
//         REQUIRE(!FB.IsValid());
//     }
//     SECTION("Resize")
//     {
//         rndr::FrameBufferProperties Props;
//         rndr::ScopePtr<rndr::FrameBuffer> FB = Ctx->CreateFrameBuffer(100, 400, Props);
//         REQUIRE(FB.IsValid());
//
//         // TODO(Marko): Move resize tests to other file.
//         bool ResizeStatus = FB->Resize(Ctx.Get(), 500, 500);
//         REQUIRE(ResizeStatus == true);
//
//         bool ResizeStatus2 = FB->Resize(Ctx.Get(), 200, 0);
//         REQUIRE(ResizeStatus2 == false);
//     }
// }
//
//// TODO(Marko): Move non-creation tests to other file.
//// TODO(Marko): Implement proper tests for read, copy and update same as for the images.
// TEST_CASE("Buffer", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     rndr::ByteSpan EmptyData;
//     const int Size = 32;
//     const int Stride = 16;
//     rndr::ByteSpan InitData;
//     InitData.Size = Size;
//     InitData.Data = new uint8_t[Size];
//     memset(InitData.Data, 0xAF, Size);
//
//     SECTION("Default No Init Data")
//     {
//         rndr::BufferProperties Props;
//         rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, EmptyData);
//         REQUIRE(!Buff.IsValid());
//     }
//     SECTION("Default No Init Data Valid Size")
//     {
//         rndr::BufferProperties Props;
//         Props.Size = Size;
//         rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, EmptyData);
//         REQUIRE(!Buff.IsValid());
//     }
//     SECTION("Default No Init Data All Valid")
//     {
//         rndr::BufferProperties Props;
//         Props.Size = Size;
//         Props.Stride = Stride;
//         rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, EmptyData);
//         REQUIRE(Buff.IsValid());
//     }
//     SECTION("With Init Data")
//     {
//         rndr::BufferProperties Props;
//         Props.Size = Size;
//         Props.Stride = Stride;
//         rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, InitData);
//         REQUIRE(Buff.IsValid());
//     }
//     SECTION("Dynamic")
//     {
//         rndr::BufferProperties Props;
//         Props.Usage = rndr::Usage::Dynamic;
//         Props.Size = Size;
//         Props.Stride = Stride;
//         rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, InitData);
//         REQUIRE(Buff.IsValid());
//
//         SECTION("Update")
//         {
//             uint8_t UpdateData[16] = {};
//             const bool Status = Buff->Update(Ctx.Get(), rndr::ByteSpan{UpdateData, 16}, 16);
//             REQUIRE(Status == true);
//         }
//     }
//     SECTION("Readback")
//     {
//         rndr::BufferProperties Props;
//         Props.Usage = rndr::Usage::Readback;
//         Props.Type = rndr::BufferType::Readback;
//         Props.Size = Size;
//         Props.Stride = Stride;
//         rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, InitData);
//         REQUIRE(Buff.IsValid());
//
//         SECTION("Read")
//         {
//             uint8_t ReadData[16] = {};
//             const bool Status = Buff->Read(Ctx.Get(), rndr::ByteSpan{ReadData, 16}, 16);
//             REQUIRE(Status == true);
//             for (int i = 0; i < 16; i++)
//             {
//                 REQUIRE(ReadData[i] == 0xAF);
//             }
//         }
//     }
//     SECTION("Unordered Access")
//     {
//         rndr::BufferProperties Props;
//         SECTION("Default Usage")
//         {
//             Props.Type = rndr::BufferType::UnorderedAccess;
//             Props.Size = Size;
//             Props.Stride = Stride;
//             rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, InitData);
//             REQUIRE(Buff.IsValid());
//         }
//         SECTION("Dynamic Usage")
//         {
//             Props.Usage = rndr::Usage::Dynamic;
//             Props.Type = rndr::BufferType::UnorderedAccess;
//             Props.Size = Size;
//             Props.Stride = Stride;
//             rndr::ScopePtr<rndr::Buffer> Buff = Ctx->CreateBuffer(Props, InitData);
//             REQUIRE(!Buff.IsValid());
//         }
//     }
//
//     delete InitData.Data;
// }

// TEST_CASE("Sampler", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     SECTION("Default")
//     {
//         rndr::SamplerProperties Props;
//         rndr::ScopePtr<rndr::Sampler> S = Ctx->CreateSampler(Props);
//         REQUIRE(S.IsValid());
//     }
// }
//
// TEST_CASE("Shader", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     SECTION("Default")
//     {
//         std::string ShaderContents =
//             "struct InVertex{};\nstruct OutVertex{float4 Position : SV_POSITION;};\nOutVertex "
//             "main(InVertex In)\n{\nOutVertex "
//             "Out;\nOut.Position = float4(1, 1, 1, 1);\nreturn Out;\n}\n";
//         rndr::ByteSpan Data((uint8_t*)ShaderContents.data(), ShaderContents.size());
//
//         rndr::ShaderProperties Props;
//         Props.Type = rndr::ShaderType::Vertex;
//         Props.EntryPoint = "main";
//         rndr::ScopePtr<rndr::Shader> S = Ctx->CreateShader(Data, Props);
//         REQUIRE(S.IsValid());
//     }
// }
//
// TEST_CASE("InputLayout", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     SECTION("Default")
//     {
//         std::string ShaderContents =
//             "struct InVertex{ float4 Position : POSITION; float3 Normal : NORMAL; };\nstruct "
//             "OutVertex{float4 Position : "
//             "SV_POSITION;};\nOutVertex main(InVertex In)\n{\nOutVertex "
//             "Out;\nOut.Position = float4(1, 1, 1, 1);\nreturn Out;\n}\n";
//         rndr::ByteSpan Data((uint8_t*)ShaderContents.data(), ShaderContents.size());
//
//         rndr::ShaderProperties Props;
//         Props.Type = rndr::ShaderType::Vertex;
//         Props.EntryPoint = "main";
//         rndr::ScopePtr<rndr::Shader> S = Ctx->CreateShader(Data, Props);
//         REQUIRE(S.IsValid());
//
//         rndr::InputLayoutProperties LayoutProps[2];
//         LayoutProps[0].SemanticName = "POSITION";
//         LayoutProps[0].SemanticIndex = 0;
//         LayoutProps[0].InputSlot = 0;
//         LayoutProps[0].OffsetInVertex = 0;
//         LayoutProps[0].Repetition = rndr::DataRepetition::PerVertex;
//         LayoutProps[0].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
//         LayoutProps[1].SemanticName = "NORMAL";
//         LayoutProps[1].SemanticIndex = 0;
//         LayoutProps[1].InputSlot = 0;
//         LayoutProps[1].OffsetInVertex = rndr::kAppendAlignedElement;
//         LayoutProps[1].Repetition = rndr::DataRepetition::PerVertex;
//         LayoutProps[1].Format = rndr::PixelFormat::R32G32B32_FLOAT;
//         rndr::Span<rndr::InputLayoutProperties> PackedProps(LayoutProps, 2);
//
//         rndr::ScopePtr<rndr::InputLayout> Layout = Ctx->CreateInputLayout(PackedProps, S.Get());
//         REQUIRE(Layout.IsValid());
//     }
// }
//
// TEST_CASE("InputLayoutBuilder", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     SECTION("Default")
//     {
//         std::string ShaderContents =
//             "struct InVertex{ float4 Position : POSITION; float3 Normal : NORMAL; };\nstruct "
//             "OutVertex{float4 Position : "
//             "SV_POSITION;};\nOutVertex main(InVertex In)\n{\nOutVertex "
//             "Out;\nOut.Position = float4(1, 1, 1, 1);\nreturn Out;\n}\n";
//         rndr::ByteSpan Data((uint8_t*)ShaderContents.data(), ShaderContents.size());
//
//         rndr::ShaderProperties Props;
//         Props.Type = rndr::ShaderType::Vertex;
//         Props.EntryPoint = "main";
//         rndr::ScopePtr<rndr::Shader> S = Ctx->CreateShader(Data, Props);
//         REQUIRE(S.IsValid());
//
//         rndr::InputLayoutBuilder Builder;
//         rndr::Span<rndr::InputLayoutProperties> LayoutProps =
//             Builder.AddBuffer(0, rndr::DataRepetition::PerVertex, 0)
//                 .AppendElement(0, "POSITION", rndr::PixelFormat::R32G32B32A32_FLOAT)
//                 .AppendElement(0, "NORMAL", rndr::PixelFormat::R32G32B32_FLOAT)
//                 .Build();
//
//         REQUIRE(LayoutProps.Size == 2);
//         REQUIRE(LayoutProps[0].SemanticName == "POSITION");
//         REQUIRE(LayoutProps[0].SemanticIndex == 0);
//         REQUIRE(LayoutProps[0].InputSlot == 0);
//         REQUIRE(LayoutProps[0].OffsetInVertex == 0);
//         REQUIRE(LayoutProps[0].Repetition == rndr::DataRepetition::PerVertex);
//         REQUIRE(LayoutProps[0].Format == rndr::PixelFormat::R32G32B32A32_FLOAT);
//         REQUIRE(LayoutProps[1].SemanticName == "NORMAL");
//         REQUIRE(LayoutProps[1].SemanticIndex == 0);
//         REQUIRE(LayoutProps[1].InputSlot == 0);
//         REQUIRE(LayoutProps[1].OffsetInVertex == rndr::kAppendAlignedElement);
//         REQUIRE(LayoutProps[1].Repetition == rndr::DataRepetition::PerVertex);
//         REQUIRE(LayoutProps[1].Format == rndr::PixelFormat::R32G32B32_FLOAT);
//
//         rndr::ScopePtr<rndr::InputLayout> Layout = Ctx->CreateInputLayout(LayoutProps, S.Get());
//         REQUIRE(Layout.IsValid());
//     }
//     SECTION("Complex")
//     {
//         rndr::InputLayoutBuilder Builder;
//         rndr::Span<rndr::InputLayoutProperties> LayoutProps =
//             Builder.AddBuffer(0, rndr::DataRepetition::PerInstance, 1)
//                 .AppendElement(0, "POSITION", rndr::PixelFormat::R32G32_FLOAT)
//                 .AppendElement(0, "POSITION", rndr::PixelFormat::R32G32_FLOAT)
//                 .AppendElement(0, "TEXCOORD", rndr::PixelFormat::R32G32_FLOAT)
//                 .AppendElement(0, "TEXCOORD", rndr::PixelFormat::R32G32_FLOAT)
//                 .AppendElement(0, "COLOR", rndr::PixelFormat::R32G32B32A32_FLOAT)
//                 .AppendElement(0, "BLENDINDICES", rndr::PixelFormat::R32_FLOAT)
//                 .AppendElement(0, "BLENDINDICES", rndr::PixelFormat::R32_FLOAT)
//                 .AppendElement(0, "BLENDINDICES", rndr::PixelFormat::R32_FLOAT)
//                 .AppendElement(0, "BLENDINDICES", rndr::PixelFormat::R32_FLOAT)
//                 .Build();
//
//         REQUIRE(LayoutProps.Size == 9);
//         REQUIRE(LayoutProps[0].SemanticName == "POSITION");
//         REQUIRE(LayoutProps[0].SemanticIndex == 0);
//         REQUIRE(LayoutProps[0].InputSlot == 0);
//         REQUIRE(LayoutProps[0].Format == rndr::PixelFormat::R32G32_FLOAT);
//         REQUIRE(LayoutProps[0].OffsetInVertex == 0);
//         REQUIRE(LayoutProps[0].Repetition == rndr::DataRepetition::PerInstance);
//         REQUIRE(LayoutProps[0].InstanceStepRate == 1);
//         REQUIRE(LayoutProps[1].SemanticName == "POSITION");
//         REQUIRE(LayoutProps[1].SemanticIndex == 1);
//         REQUIRE(LayoutProps[1].InputSlot == 0);
//         REQUIRE(LayoutProps[1].Format == rndr::PixelFormat::R32G32_FLOAT);
//         REQUIRE(LayoutProps[1].OffsetInVertex == rndr::kAppendAlignedElement);
//         REQUIRE(LayoutProps[1].Repetition == rndr::DataRepetition::PerInstance);
//         REQUIRE(LayoutProps[1].InstanceStepRate == 1);
//         REQUIRE(LayoutProps[2].SemanticName == "TEXCOORD");
//         REQUIRE(LayoutProps[2].SemanticIndex == 0);
//         REQUIRE(LayoutProps[2].InputSlot == 0);
//         REQUIRE(LayoutProps[2].Format == rndr::PixelFormat::R32G32_FLOAT);
//         REQUIRE(LayoutProps[2].OffsetInVertex == rndr::kAppendAlignedElement);
//         REQUIRE(LayoutProps[2].Repetition == rndr::DataRepetition::PerInstance);
//         REQUIRE(LayoutProps[2].InstanceStepRate == 1);
//         REQUIRE(LayoutProps[3].SemanticName == "TEXCOORD");
//         REQUIRE(LayoutProps[3].SemanticIndex == 1);
//         REQUIRE(LayoutProps[3].InputSlot == 0);
//         REQUIRE(LayoutProps[3].Format == rndr::PixelFormat::R32G32_FLOAT);
//         REQUIRE(LayoutProps[3].OffsetInVertex == rndr::kAppendAlignedElement);
//         REQUIRE(LayoutProps[3].Repetition == rndr::DataRepetition::PerInstance);
//         REQUIRE(LayoutProps[3].InstanceStepRate == 1);
//         REQUIRE(LayoutProps[4].SemanticName == "COLOR");
//         REQUIRE(LayoutProps[4].SemanticIndex == 0);
//         REQUIRE(LayoutProps[4].InputSlot == 0);
//         REQUIRE(LayoutProps[4].Format == rndr::PixelFormat::R32G32B32A32_FLOAT);
//         REQUIRE(LayoutProps[4].OffsetInVertex == rndr::kAppendAlignedElement);
//         REQUIRE(LayoutProps[4].Repetition == rndr::DataRepetition::PerInstance);
//         REQUIRE(LayoutProps[4].InstanceStepRate == 1);
//         REQUIRE(LayoutProps[5].SemanticName == "BLENDINDICES");
//         REQUIRE(LayoutProps[5].SemanticIndex == 0);
//         REQUIRE(LayoutProps[5].InputSlot == 0);
//         REQUIRE(LayoutProps[5].Format == rndr::PixelFormat::R32_FLOAT);
//         REQUIRE(LayoutProps[5].OffsetInVertex == rndr::kAppendAlignedElement);
//         REQUIRE(LayoutProps[5].Repetition == rndr::DataRepetition::PerInstance);
//         REQUIRE(LayoutProps[5].InstanceStepRate == 1);
//         REQUIRE(LayoutProps[6].SemanticName == "BLENDINDICES");
//         REQUIRE(LayoutProps[6].SemanticIndex == 1);
//         REQUIRE(LayoutProps[6].InputSlot == 0);
//         REQUIRE(LayoutProps[6].Format == rndr::PixelFormat::R32_FLOAT);
//         REQUIRE(LayoutProps[6].OffsetInVertex == rndr::kAppendAlignedElement);
//         REQUIRE(LayoutProps[6].Repetition == rndr::DataRepetition::PerInstance);
//         REQUIRE(LayoutProps[6].InstanceStepRate == 1);
//         REQUIRE(LayoutProps[7].SemanticName == "BLENDINDICES");
//         REQUIRE(LayoutProps[7].SemanticIndex == 2);
//         REQUIRE(LayoutProps[7].InputSlot == 0);
//         REQUIRE(LayoutProps[7].Format == rndr::PixelFormat::R32_FLOAT);
//         REQUIRE(LayoutProps[7].OffsetInVertex == rndr::kAppendAlignedElement);
//         REQUIRE(LayoutProps[7].Repetition == rndr::DataRepetition::PerInstance);
//         REQUIRE(LayoutProps[7].InstanceStepRate == 1);
//         REQUIRE(LayoutProps[8].SemanticName == "BLENDINDICES");
//         REQUIRE(LayoutProps[8].SemanticIndex == 3);
//         REQUIRE(LayoutProps[8].InputSlot == 0);
//         REQUIRE(LayoutProps[8].Format == rndr::PixelFormat::R32_FLOAT);
//         REQUIRE(LayoutProps[8].OffsetInVertex == rndr::kAppendAlignedElement);
//         REQUIRE(LayoutProps[8].Repetition == rndr::DataRepetition::PerInstance);
//         REQUIRE(LayoutProps[8].InstanceStepRate == 1);
//     }
// }
//
// TEST_CASE("RasterizerState", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     SECTION("Default")
//     {
//         rndr::RasterizerProperties Props;
//         rndr::ScopePtr<rndr::RasterizerState> S = Ctx->CreateRasterizerState(Props);
//         REQUIRE(S.IsValid());
//     }
// }
//
// TEST_CASE("DepthStencilState", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     SECTION("Default")
//     {
//         rndr::DepthStencilProperties Props;
//         rndr::ScopePtr<rndr::DepthStencilState> S = Ctx->CreateDepthStencilState(Props);
//         REQUIRE(S.IsValid());
//     }
// }
//
// TEST_CASE("BlendState", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     REQUIRE(Ctx.IsValid());
//
//     SECTION("Default")
//     {
//         rndr::BlendProperties Props;
//         rndr::ScopePtr<rndr::BlendState> S = Ctx->CreateBlendState(Props);
//         REQUIRE(S.IsValid());
//     }
// }
//
// TEST_CASE("SwapChain", "RenderAPI")
//{
//     std::unique_ptr<rndr::RndrContext> RndrCtx = std::make_unique<rndr::RndrContext>();
//     rndr::ScopePtr<rndr::GraphicsContext> Ctx = RndrCtx->CreateGraphicsContext();
//     rndr::ScopePtr<rndr::Window> Win = RndrCtx->CreateWin(800, 600);
//     REQUIRE(Ctx.IsValid());
//
//     rndr::NativeWindowHandle NativeWinHandle = Win->GetNativeWindowHandle();
//     rndr::SwapChainProperties SwapProps;
//     rndr::ScopePtr<rndr::SwapChain> S =
//         Ctx->CreateSwapChain(NativeWinHandle, Win->GetWidth(), Win->GetHeight(), SwapProps);
//     REQUIRE(S.IsValid());
//
//     Ctx->ClearColor(S->frame_buffer->color_buffers[0].Get(), math::Vector4{1, 1, 1, 1});
//     Ctx->Present(S.Get(), true);
//     Ctx->ClearColor(S->frame_buffer->color_buffers[0].Get(), math::Vector4{1, 1, 0.5, 1});
//     Ctx->Present(S.Get(), true);
// }
