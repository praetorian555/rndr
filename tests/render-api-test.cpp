#include <catch2/catch_test_macros.hpp>

#if RNDR_OPENGL
#include <glad/glad.h>
#endif

#include "rndr/rndr.h"

#if RNDR_OPENGL
#include "render-api/opengl-helpers.h"
#endif

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

#if RNDR_OPENGL

TEST_CASE("Conversion from Rndr::ShaderType to OpenGL shader type")
{
    using namespace Rndr;
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::Vertex) == GL_VERTEX_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::Fragment) == GL_FRAGMENT_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::Geometry) == GL_GEOMETRY_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::Compute) == GL_COMPUTE_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::TessellationControl) == GL_TESS_CONTROL_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::TessellationEvaluation) == GL_TESS_EVALUATION_SHADER);
}

TEST_CASE("Conversion from Rndr::Usage to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromUsageToOpenGL(Usage::Default) == GL_MAP_WRITE_BIT);
    REQUIRE(FromUsageToOpenGL(Usage::Dynamic) == GL_DYNAMIC_STORAGE_BIT);
    REQUIRE(FromUsageToOpenGL(Usage::ReadBack) == GL_MAP_READ_BIT);
}

TEST_CASE("Conversion from Rndr::Comparator to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromComparatorToOpenGL(Comparator::Never) == GL_NEVER);
    REQUIRE(FromComparatorToOpenGL(Comparator::Less) == GL_LESS);
    REQUIRE(FromComparatorToOpenGL(Comparator::Equal) == GL_EQUAL);
    REQUIRE(FromComparatorToOpenGL(Comparator::LessEqual) == GL_LEQUAL);
    REQUIRE(FromComparatorToOpenGL(Comparator::Greater) == GL_GREATER);
    REQUIRE(FromComparatorToOpenGL(Comparator::NotEqual) == GL_NOTEQUAL);
    REQUIRE(FromComparatorToOpenGL(Comparator::GreaterEqual) == GL_GEQUAL);
    REQUIRE(FromComparatorToOpenGL(Comparator::Always) == GL_ALWAYS);
}

TEST_CASE("Conversion from Rndr::StencilOperation to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromStencilOpToOpenGL(StencilOperation::Keep) == GL_KEEP);
    REQUIRE(FromStencilOpToOpenGL(StencilOperation::Zero) == GL_ZERO);
    REQUIRE(FromStencilOpToOpenGL(StencilOperation::Replace) == GL_REPLACE);
    REQUIRE(FromStencilOpToOpenGL(StencilOperation::Increment) == GL_INCR);
    REQUIRE(FromStencilOpToOpenGL(StencilOperation::Decrement) == GL_DECR);
    REQUIRE(FromStencilOpToOpenGL(StencilOperation::Invert) == GL_INVERT);
    REQUIRE(FromStencilOpToOpenGL(StencilOperation::IncrementWrap) == GL_INCR_WRAP);
    REQUIRE(FromStencilOpToOpenGL(StencilOperation::DecrementWrap) == GL_DECR_WRAP);
}

TEST_CASE("Conversion from Rndr::BufferType to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromBufferTypeToOpenGL(BufferType::Vertex) == GL_ARRAY_BUFFER);
    REQUIRE(FromBufferTypeToOpenGL(BufferType::Index) == GL_ELEMENT_ARRAY_BUFFER);
    REQUIRE(FromBufferTypeToOpenGL(BufferType::Constant) == GL_UNIFORM_BUFFER);
    REQUIRE(FromBufferTypeToOpenGL(BufferType::ShaderStorage) == GL_SHADER_STORAGE_BUFFER);
}

TEST_CASE("Conversion from Rndr::BlendFactor to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::Zero) == GL_ZERO);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::One) == GL_ONE);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::SrcColor) == GL_SRC_COLOR);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::InvSrcColor) == GL_ONE_MINUS_SRC_COLOR);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::DstColor) == GL_DST_COLOR);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::InvDstColor) == GL_ONE_MINUS_DST_COLOR);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::SrcAlpha) == GL_SRC_ALPHA);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::InvSrcAlpha) == GL_ONE_MINUS_SRC_ALPHA);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::DstAlpha) == GL_DST_ALPHA);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::InvDstAlpha) == GL_ONE_MINUS_DST_ALPHA);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::ConstColor) == GL_CONSTANT_COLOR);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::InvConstColor) == GL_ONE_MINUS_CONSTANT_COLOR);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::ConstAlpha) == GL_CONSTANT_ALPHA);
    REQUIRE(FromBlendFactorToOpenGL(BlendFactor::InvConstAlpha) == GL_ONE_MINUS_CONSTANT_ALPHA);
}

TEST_CASE("Conversion from Rndr::BlendOperation to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::Add) == GL_FUNC_ADD);
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::Subtract) == GL_FUNC_SUBTRACT);
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::ReverseSubtract) == GL_FUNC_REVERSE_SUBTRACT);
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::Min) == GL_MIN);
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::Max) == GL_MAX);
}

TEST_CASE("Conversion from image info to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromImageInfoToTarget(ImageType::Image2D, false) == GL_TEXTURE_2D);
    REQUIRE(FromImageInfoToTarget(ImageType::Image2D, true) == GL_TEXTURE_2D_MULTISAMPLE);
    REQUIRE(FromImageInfoToTarget(ImageType::Image2DArray, false) == GL_TEXTURE_2D_ARRAY);
    REQUIRE(FromImageInfoToTarget(ImageType::Image2DArray, true) == GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
    REQUIRE(FromImageInfoToTarget(ImageType::CubeMap, false) == GL_TEXTURE_CUBE_MAP);
    REQUIRE(FromImageInfoToTarget(ImageType::CubeMap, true) == GL_TEXTURE_CUBE_MAP);
}

TEST_CASE("Conversion from Rndr::ImageFilter to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromImageFilterToOpenGL(ImageFilter::Nearest) == GL_NEAREST);
    REQUIRE(FromImageFilterToOpenGL(ImageFilter::Linear) == GL_LINEAR);
    REQUIRE(FromMinAndMipFiltersToOpenGL(ImageFilter::Nearest, ImageFilter::Nearest) == GL_NEAREST_MIPMAP_NEAREST);
    REQUIRE(FromMinAndMipFiltersToOpenGL(ImageFilter::Linear, ImageFilter::Nearest) == GL_LINEAR_MIPMAP_NEAREST);
    REQUIRE(FromMinAndMipFiltersToOpenGL(ImageFilter::Nearest, ImageFilter::Linear) == GL_NEAREST_MIPMAP_LINEAR);
    REQUIRE(FromMinAndMipFiltersToOpenGL(ImageFilter::Linear, ImageFilter::Linear) == GL_LINEAR_MIPMAP_LINEAR);
}

TEST_CASE("Conversion from Rndr::ImageAddressMode to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromImageAddressModeToOpenGL(ImageAddressMode::Repeat) == GL_REPEAT);
    REQUIRE(FromImageAddressModeToOpenGL(ImageAddressMode::MirrorRepeat) == GL_MIRRORED_REPEAT);
    REQUIRE(FromImageAddressModeToOpenGL(ImageAddressMode::Clamp) == GL_CLAMP_TO_EDGE);
    REQUIRE(FromImageAddressModeToOpenGL(ImageAddressMode::Border) == GL_CLAMP_TO_BORDER);
}

TEST_CASE("Conversion from Rndr::PixelFormat to the internal format")
{
    using namespace Rndr;
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8_UNORM) == GL_R8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8_UNORM_SRGB) == GL_R8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8_SNORM) == GL_R8_SNORM);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8_UINT) == GL_R8UI);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8_SINT) == GL_R8I);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8_UNORM) == GL_RG8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8_UNORM_SRGB) == GL_RG8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8_SNORM) == GL_RG8_SNORM);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8_UINT) == GL_RG8UI);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8_SINT) == GL_RG8I);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8_UNORM) == GL_RGB8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8_UNORM_SRGB) == GL_SRGB8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8_SNORM) == GL_RGB8_SNORM);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8_UINT) == GL_RGB8UI);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8_SINT) == GL_RGB8I);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8A8_UNORM) == GL_RGBA8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8A8_UNORM_SRGB) == GL_SRGB8_ALPHA8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8A8_SNORM) == GL_RGBA8_SNORM);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8A8_UINT) == GL_RGBA8UI);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R8G8B8A8_SINT) == GL_RGBA8I);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::B8G8R8A8_UNORM) == GL_RGBA8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::B8G8R8A8_UNORM_SRGB) == GL_SRGB8_ALPHA8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::D24_UNORM_S8_UINT) == GL_DEPTH24_STENCIL8);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R16_TYPELESS) == GL_R16F);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32_TYPELESS) == GL_R32F);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32_FLOAT) == GL_R32F);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32_UINT) == GL_R32UI);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32_SINT) == GL_R32I);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32G32_FLOAT) == GL_RG32F);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32G32_UINT) == GL_RG32UI);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32G32_SINT) == GL_RG32I);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32G32B32_FLOAT) == GL_RGB32F);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32G32B32_UINT) == GL_RGB32UI);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32G32B32_SINT) == GL_RGB32I);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32G32B32A32_FLOAT) == GL_RGBA32F);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32G32B32A32_UINT) == GL_RGBA32UI);
    REQUIRE(FromPixelFormatToInternalFormat(PixelFormat::R32G32B32A32_SINT) == GL_RGBA32I);
}

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL pixel format")
{
    using namespace Rndr;
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8_UNORM) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8_UNORM_SRGB) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8_SNORM) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8_UINT) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8_SINT) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8_UNORM) == GL_RG);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8_UNORM_SRGB) == GL_RG);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8_SNORM) == GL_RG);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8_UINT) == GL_RG);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8_SINT) == GL_RG);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8_UNORM) == GL_RGB);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8_UNORM_SRGB) == GL_RGB);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8_SNORM) == GL_RGB);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8_UINT) == GL_RGB);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8_SINT) == GL_RGB);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8A8_UNORM) == GL_RGBA);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8A8_UNORM_SRGB) == GL_RGBA);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8A8_SNORM) == GL_RGBA);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8A8_UINT) == GL_RGBA);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R8G8B8A8_SINT) == GL_RGBA);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::B8G8R8A8_UNORM) == GL_BGRA);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::B8G8R8A8_UNORM_SRGB) == GL_BGRA);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::D24_UNORM_S8_UINT) == GL_DEPTH_STENCIL);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R16_TYPELESS) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32_TYPELESS) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32_FLOAT) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32_UINT) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32_SINT) == GL_RED);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32G32_FLOAT) == GL_RG);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32G32_UINT) == GL_RG);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32G32_SINT) == GL_RG);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32G32B32_FLOAT) == GL_RGB);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32G32B32_UINT) == GL_RGB);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32G32B32_SINT) == GL_RGB);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32G32B32A32_FLOAT) == GL_RGBA);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32G32B32A32_UINT) == GL_RGBA);
    REQUIRE(FromPixelFormatToFormat(PixelFormat::R32G32B32A32_SINT) == GL_RGBA);
}

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL component count")
{
    using namespace Rndr;
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8_UNORM) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8_UNORM_SRGB) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8_SNORM) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8_UINT) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8_SINT) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8_UNORM) == 2);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8_UNORM_SRGB) == 2);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8_SNORM) == 2);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8_UINT) == 2);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8_SINT) == 2);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8_UNORM) == 3);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8_UNORM_SRGB) == 3);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8_SNORM) == 3);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8_UINT) == 3);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8_SINT) == 3);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8A8_UNORM) == 4);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8A8_UNORM_SRGB) == 4);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8A8_SNORM) == 4);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8A8_UINT) == 4);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R8G8B8A8_SINT) == 4);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::B8G8R8A8_UNORM) == 4);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::B8G8R8A8_UNORM_SRGB) == 4);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::D24_UNORM_S8_UINT) == 2);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R16_TYPELESS) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32_TYPELESS) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32_FLOAT) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32_UINT) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32_SINT) == 1);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32G32_FLOAT) == 2);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32G32_UINT) == 2);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32G32_SINT) == 2);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32G32B32_FLOAT) == 3);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32G32B32_UINT) == 3);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32G32B32_SINT) == 3);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32G32B32A32_FLOAT) == 4);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32G32B32A32_UINT) == 4);
    REQUIRE(FromPixelFormatToComponentCount(PixelFormat::R32G32B32A32_SINT) == 4);
}

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL pixel size")
{
    using namespace Rndr;
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8_UNORM) == 1);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8_UNORM_SRGB) == 1);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8_SNORM) == 1);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8_UINT) == 1);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8_SINT) == 1);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8_UNORM) == 2);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8_UNORM_SRGB) == 2);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8_SNORM) == 2);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8_UINT) == 2);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8_SINT) == 2);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8_UNORM) == 3);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8_UNORM_SRGB) == 3);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8_SNORM) == 3);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8_UINT) == 3);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8_SINT) == 3);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8A8_UNORM) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8A8_UNORM_SRGB) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8A8_SNORM) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8A8_UINT) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R8G8B8A8_SINT) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::B8G8R8A8_UNORM) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::B8G8R8A8_UNORM_SRGB) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::D24_UNORM_S8_UINT) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R16_TYPELESS) == 2);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32_TYPELESS) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32_FLOAT) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32_UINT) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32_SINT) == 4);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32G32_FLOAT) == 8);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32G32_UINT) == 8);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32G32_SINT) == 8);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32G32B32_FLOAT) == 12);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32G32B32_UINT) == 12);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32G32B32_SINT) == 12);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32G32B32A32_FLOAT) == 16);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32G32B32A32_UINT) == 16);
    REQUIRE(FromPixelFormatToPixelSize(PixelFormat::R32G32B32A32_SINT) == 16);
}

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL component data type")
{
    using namespace Rndr;
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8_UNORM) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8_UNORM_SRGB) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8_SNORM) == GL_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8_UINT) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8_SINT) == GL_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8_UNORM) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8_UNORM_SRGB) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8_SNORM) == GL_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8_UINT) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8_SINT) == GL_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8_UNORM) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8_UNORM_SRGB) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8_SNORM) == GL_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8_UINT) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8_SINT) == GL_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8A8_UNORM) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8A8_UNORM_SRGB) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8A8_SNORM) == GL_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8A8_UINT) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R8G8B8A8_SINT) == GL_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::B8G8R8A8_UNORM) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::B8G8R8A8_UNORM_SRGB) == GL_UNSIGNED_BYTE);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R16_TYPELESS) == GL_HALF_FLOAT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32_TYPELESS) == GL_FLOAT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32_FLOAT) == GL_FLOAT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32_UINT) == GL_UNSIGNED_INT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32_SINT) == GL_INT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32G32_FLOAT) == GL_FLOAT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32G32_UINT) == GL_UNSIGNED_INT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32G32_SINT) == GL_INT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32G32B32_FLOAT) == GL_FLOAT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32G32B32_UINT) == GL_UNSIGNED_INT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32G32B32_SINT) == GL_INT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32G32B32A32_FLOAT) == GL_FLOAT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32G32B32A32_UINT) == GL_UNSIGNED_INT);
    REQUIRE(FromPixelFormatToDataType(PixelFormat::R32G32B32A32_SINT) == GL_INT);
}

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL should normalize data")
{
    using namespace Rndr;
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8_UNORM) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8_UNORM_SRGB) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8_SNORM) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8_UINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8_SINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8_UNORM) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8_UNORM_SRGB) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8_SNORM) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8_UINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8_SINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8_UNORM) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8_UNORM_SRGB) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8_SNORM) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8_UINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8_SINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8A8_UNORM) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8A8_UNORM_SRGB) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8A8_SNORM) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8A8_UINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R8G8B8A8_SINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::B8G8R8A8_UNORM) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::B8G8R8A8_UNORM_SRGB) == GL_TRUE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R16_TYPELESS) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32_TYPELESS) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32_FLOAT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32_UINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32_SINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32G32_FLOAT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32G32_UINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32G32_SINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32G32B32_FLOAT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32G32B32_UINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32G32B32_SINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32G32B32A32_FLOAT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32G32B32A32_UINT) == GL_FALSE);
    REQUIRE(FromPixelFormatToShouldNormalizeData(PixelFormat::R32G32B32A32_SINT) == GL_FALSE);
}

TEST_CASE("Conversion from Rndr::PrimitiveTopology to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::Point) == GL_POINTS);
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::Line) == GL_LINES);
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::LineStrip) == GL_LINE_STRIP);
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::Triangle) == GL_TRIANGLES);
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::TriangleStrip) == GL_TRIANGLE_STRIP);
}

TEST_CASE("Conversion from index size to OpenGL enum")
{
    using namespace Rndr;
    REQUIRE(FromIndexSizeToOpenGL(1) == GL_UNSIGNED_BYTE);
    REQUIRE(FromIndexSizeToOpenGL(2) == GL_UNSIGNED_SHORT);
    REQUIRE(FromIndexSizeToOpenGL(4) == GL_UNSIGNED_INT);
}

TEST_CASE("Is pixel format low precision")
{
    using namespace Rndr;
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8_UNORM) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8_UNORM_SRGB) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8_SNORM) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8_UINT) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8_SINT) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8_UNORM) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8_UNORM_SRGB) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8_SNORM) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8_UINT) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8_SINT) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8_UNORM) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8_UNORM_SRGB) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8_SNORM) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8_UINT) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8_SINT) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8A8_UNORM) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8A8_UNORM_SRGB) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8A8_SNORM) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8A8_UINT) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R8G8B8A8_SINT) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::B8G8R8A8_UNORM) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::B8G8R8A8_UNORM_SRGB) == true);
    REQUIRE(IsComponentLowPrecision(PixelFormat::D24_UNORM_S8_UINT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R16_TYPELESS) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32_TYPELESS) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32_FLOAT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32_UINT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32_SINT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32G32_FLOAT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32G32_UINT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32G32_SINT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32G32B32_FLOAT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32G32B32_UINT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32G32B32_SINT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32G32B32A32_FLOAT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32G32B32A32_UINT) == false);
    REQUIRE(IsComponentLowPrecision(PixelFormat::R32G32B32A32_SINT) == false);
}

TEST_CASE("Is pixel format high precision")
{
    using namespace Rndr;
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8_UNORM) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8_UNORM_SRGB) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8_SNORM) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8_UINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8_SINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8_UNORM) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8_UNORM_SRGB) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8_SNORM) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8_UINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8_SINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8_UNORM) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8_UNORM_SRGB) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8_SNORM) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8_UINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8_SINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8A8_UNORM) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8A8_UNORM_SRGB) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8A8_SNORM) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8A8_UINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R8G8B8A8_SINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::B8G8R8A8_UNORM) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::B8G8R8A8_UNORM_SRGB) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::D24_UNORM_S8_UINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R16_TYPELESS) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32_TYPELESS) == true);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32_FLOAT) == true);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32_UINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32_SINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32G32_FLOAT) == true);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32G32_UINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32G32_SINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32G32B32_FLOAT) == true);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32G32B32_UINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32G32B32_SINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32G32B32A32_FLOAT) == true);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32G32B32A32_UINT) == false);
    REQUIRE(IsComponentHighPrecision(PixelFormat::R32G32B32A32_SINT) == false);
}

#endif

TEST_CASE("Creating different types of buffers")
{
    Rndr::Init();
    const Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    const Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Create vertex buffer use for writing and rarely")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer vertex_buffer(graphics_context, desc);
        REQUIRE(vertex_buffer.IsValid());
    }
    SECTION("Create vertex buffer used for writing often")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer vertex_buffer(graphics_context, desc);
        REQUIRE(vertex_buffer.IsValid());
    }
    SECTION("Create vertex buffer used for reading")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer vertex_buffer(graphics_context, desc);
        REQUIRE(vertex_buffer.IsValid());
    }
    SECTION("Create index buffer used for rare writes only")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Index, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer index_buffer(graphics_context, desc);
        REQUIRE(index_buffer.IsValid());
    }
    SECTION("Create index buffer used for writing often")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Index, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer index_buffer(graphics_context, desc);
        REQUIRE(index_buffer.IsValid());
    }
    SECTION("Create index buffer used for reading")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Index, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer index_buffer(graphics_context, desc);
        REQUIRE(index_buffer.IsValid());
    }
    SECTION("Create constant buffer used for rare writes only")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer constant_buffer(graphics_context, desc);
        REQUIRE(constant_buffer.IsValid());
    }
    SECTION("Create constant buffer used for writing often")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer constant_buffer(graphics_context, desc);
        REQUIRE(constant_buffer.IsValid());
    }
    SECTION("Create constant buffer used for reading")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer constant_buffer(graphics_context, desc);
        REQUIRE(constant_buffer.IsValid());
    }
    SECTION("Create shader storage buffer used for rare writes only")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc);
        REQUIRE(ss_buffer.IsValid());
    }
    SECTION("Create shader storage buffer used for writing often")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc);
        REQUIRE(ss_buffer.IsValid());
    }
    SECTION("Create shader storage buffer used for reading")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc);
        REQUIRE(ss_buffer.IsValid());
    }
    Rndr::Destroy();
}

TEST_CASE("Reading from GPU buffers")
{
    constexpr int32_t k_buffer_size = 1024;
    const Rndr::StackArray<uint8_t, k_buffer_size> data = {0xAB};

    Rndr::Init();
    const Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    const Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Reading from write only vertex buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer vertex_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(vertex_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(vertex_buffer, read_data);
        REQUIRE(!result);
    }
    SECTION("Reading from dynamic vertex buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer vertex_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(vertex_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(vertex_buffer, read_data);
        REQUIRE(!result);
    }
    SECTION("Reading from read back vertex buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer vertex_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(vertex_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(vertex_buffer, read_data);
        REQUIRE(result);
        for (int i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i]);
        }
    }

    SECTION("Reading from write only index buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Index, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer index_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(index_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(index_buffer, read_data);
        REQUIRE(!result);
    }
    SECTION("Reading from dynamic index buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Index, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer index_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(index_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(index_buffer, read_data);
        REQUIRE(!result);
    }
    SECTION("Reading from read back index buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Index, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer index_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(index_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(index_buffer, read_data);
        REQUIRE(result);
        for (int i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i]);
        }
    }

    SECTION("Reading from write only constant buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer const_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(const_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(const_buffer, read_data);
        REQUIRE(!result);
    }
    SECTION("Reading from dynamic constant buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer const_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(const_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(const_buffer, read_data);
        REQUIRE(!result);
    }
    SECTION("Reading from read back constant buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer const_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(const_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(const_buffer, read_data);
        REQUIRE(result);
        for (int i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i]);
        }
    }

    SECTION("Reading from write only shader store buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(ss_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(ss_buffer, read_data);
        REQUIRE(!result);
    }
    SECTION("Reading from dynamic shader store buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(ss_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(ss_buffer, read_data);
        REQUIRE(!result);
    }
    SECTION("Reading from read back shader store buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(ss_buffer.IsValid());

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        const bool result = graphics_context.Read(ss_buffer, read_data);
        REQUIRE(result);
        for (int i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i]);
        }
    }

    SECTION("Read from a buffer with invalid offset or size")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(ss_buffer.IsValid());

        SECTION("Invalid offset")
        {
            Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
            Rndr::ByteSpan read_data{read_data_storage};
            REQUIRE(!graphics_context.Read(ss_buffer, read_data, -1, 512));
            REQUIRE(!graphics_context.Read(ss_buffer, read_data, 1024, 512));
            REQUIRE(!graphics_context.Read(ss_buffer, read_data, 1050, 512));
        }
        SECTION("Invalid size")
        {
            Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
            Rndr::ByteSpan read_data{read_data_storage};
            REQUIRE(!graphics_context.Read(ss_buffer, read_data, 0, -1));
            REQUIRE(!graphics_context.Read(ss_buffer, read_data, 0, 1025));
            REQUIRE(graphics_context.Read(ss_buffer, read_data, 0, 1024));
        }
        SECTION("Invalid combo of offset and size")
        {
            Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
            Rndr::ByteSpan read_data{read_data_storage};
            REQUIRE(graphics_context.Read(ss_buffer, read_data, 0, 1024));
            REQUIRE(!graphics_context.Read(ss_buffer, read_data, 0, 1025));
            REQUIRE(!graphics_context.Read(ss_buffer, read_data, 512, 1025));
            REQUIRE(graphics_context.Read(ss_buffer, read_data, 1023, 1));
        }
    }

    Rndr::Destroy();
}
