#include <catch2/catch_test_macros.hpp>

#if RNDR_OPENGL
#include <glad/glad.h>
#endif

#include "rndr/rndr.h"

#if RNDR_OPENGL
#include "core/platform/opengl-helpers.h"
#include "rndr/core/platform/opengl-frame-buffer.h"
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

TEST_CASE("Conversion from Rndr::ShaderType to OpenGL shader type", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::Vertex) == GL_VERTEX_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::Fragment) == GL_FRAGMENT_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::Geometry) == GL_GEOMETRY_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::Compute) == GL_COMPUTE_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::TessellationControl) == GL_TESS_CONTROL_SHADER);
    REQUIRE(FromShaderTypeToOpenGL(ShaderType::TessellationEvaluation) == GL_TESS_EVALUATION_SHADER);
}

TEST_CASE("Conversion from Rndr::Usage to OpenGL enum", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromUsageToOpenGL(Usage::Default) == GL_MAP_WRITE_BIT);
    REQUIRE(FromUsageToOpenGL(Usage::Dynamic) == GL_DYNAMIC_STORAGE_BIT);
    REQUIRE(FromUsageToOpenGL(Usage::ReadBack) == GL_MAP_READ_BIT);
}

TEST_CASE("Conversion from Rndr::Comparator to OpenGL enum", "[misc][opengl]")
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

TEST_CASE("Conversion from Rndr::StencilOperation to OpenGL enum", "[misc][opengl]")
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

TEST_CASE("Conversion from Rndr::BufferType to OpenGL enum", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromBufferTypeToOpenGL(BufferType::Vertex) == GL_ARRAY_BUFFER);
    REQUIRE(FromBufferTypeToOpenGL(BufferType::Index) == GL_ELEMENT_ARRAY_BUFFER);
    REQUIRE(FromBufferTypeToOpenGL(BufferType::Constant) == GL_UNIFORM_BUFFER);
    REQUIRE(FromBufferTypeToOpenGL(BufferType::ShaderStorage) == GL_SHADER_STORAGE_BUFFER);
}

TEST_CASE("Conversion from Rndr::BlendFactor to OpenGL enum", "[misc][opengl]")
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

TEST_CASE("Conversion from Rndr::BlendOperation to OpenGL enum", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::Add) == GL_FUNC_ADD);
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::Subtract) == GL_FUNC_SUBTRACT);
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::ReverseSubtract) == GL_FUNC_REVERSE_SUBTRACT);
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::Min) == GL_MIN);
    REQUIRE(FromBlendOperationToOpenGL(BlendOperation::Max) == GL_MAX);
}

TEST_CASE("Conversion from image info to OpenGL enum", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromImageInfoToTarget(ImageType::Image2D, false) == GL_TEXTURE_2D);
    REQUIRE(FromImageInfoToTarget(ImageType::Image2D, true) == GL_TEXTURE_2D_MULTISAMPLE);
    REQUIRE(FromImageInfoToTarget(ImageType::Image2DArray, false) == GL_TEXTURE_2D_ARRAY);
    REQUIRE(FromImageInfoToTarget(ImageType::Image2DArray, true) == GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
    REQUIRE(FromImageInfoToTarget(ImageType::CubeMap, false) == GL_TEXTURE_CUBE_MAP);
    REQUIRE(FromImageInfoToTarget(ImageType::CubeMap, true) == GL_TEXTURE_CUBE_MAP);
}

TEST_CASE("Conversion from Rndr::ImageFilter to OpenGL enum", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromImageFilterToOpenGL(ImageFilter::Nearest) == GL_NEAREST);
    REQUIRE(FromImageFilterToOpenGL(ImageFilter::Linear) == GL_LINEAR);
    REQUIRE(FromMinAndMipFiltersToOpenGL(ImageFilter::Nearest, ImageFilter::Nearest) == GL_NEAREST_MIPMAP_NEAREST);
    REQUIRE(FromMinAndMipFiltersToOpenGL(ImageFilter::Linear, ImageFilter::Nearest) == GL_LINEAR_MIPMAP_NEAREST);
    REQUIRE(FromMinAndMipFiltersToOpenGL(ImageFilter::Nearest, ImageFilter::Linear) == GL_NEAREST_MIPMAP_LINEAR);
    REQUIRE(FromMinAndMipFiltersToOpenGL(ImageFilter::Linear, ImageFilter::Linear) == GL_LINEAR_MIPMAP_LINEAR);
}

TEST_CASE("Conversion from Rndr::ImageAddressMode to OpenGL enum", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromImageAddressModeToOpenGL(ImageAddressMode::Repeat) == GL_REPEAT);
    REQUIRE(FromImageAddressModeToOpenGL(ImageAddressMode::MirrorRepeat) == GL_MIRRORED_REPEAT);
    REQUIRE(FromImageAddressModeToOpenGL(ImageAddressMode::Clamp) == GL_CLAMP_TO_EDGE);
    REQUIRE(FromImageAddressModeToOpenGL(ImageAddressMode::Border) == GL_CLAMP_TO_BORDER);
}

TEST_CASE("Conversion from Rndr::PixelFormat to the internal format", "[misc][opengl]")
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

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL pixel format", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8_UNORM) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8_UNORM_SRGB) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8_SNORM) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8_UINT) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8_SINT) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8_UNORM) == GL_RG);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8_UNORM_SRGB) == GL_RG);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8_SNORM) == GL_RG);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8_UINT) == GL_RG);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8_SINT) == GL_RG);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8_UNORM) == GL_RGB);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8_UNORM_SRGB) == GL_RGB);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8_SNORM) == GL_RGB);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8_UINT) == GL_RGB);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8_SINT) == GL_RGB);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8A8_UNORM) == GL_RGBA);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8A8_UNORM_SRGB) == GL_RGBA);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8A8_SNORM) == GL_RGBA);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8A8_UINT) == GL_RGBA);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R8G8B8A8_SINT) == GL_RGBA);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::B8G8R8A8_UNORM) == GL_BGRA);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::B8G8R8A8_UNORM_SRGB) == GL_BGRA);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::D24_UNORM_S8_UINT) == GL_DEPTH_STENCIL);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R16_TYPELESS) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32_TYPELESS) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32_FLOAT) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32_UINT) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32_SINT) == GL_RED);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32G32_FLOAT) == GL_RG);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32G32_UINT) == GL_RG);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32G32_SINT) == GL_RG);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32G32B32_FLOAT) == GL_RGB);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32G32B32_UINT) == GL_RGB);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32G32B32_SINT) == GL_RGB);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32G32B32A32_FLOAT) == GL_RGBA);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32G32B32A32_UINT) == GL_RGBA);
    REQUIRE(FromPixelFormatToExternalFormat(PixelFormat::R32G32B32A32_SINT) == GL_RGBA);
}

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL component count", "[misc]")
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

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL pixel size", "[misc]")
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

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL component data type", "[misc][opengl]")
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

TEST_CASE("Conversion of Rndr::PixelFormat to OpenGL should normalize data", "[misc][opengl]")
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

TEST_CASE("Conversion from Rndr::PrimitiveTopology to OpenGL enum", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::Point) == GL_POINTS);
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::Line) == GL_LINES);
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::LineStrip) == GL_LINE_STRIP);
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::Triangle) == GL_TRIANGLES);
    REQUIRE(FromPrimitiveTopologyToOpenGL(PrimitiveTopology::TriangleStrip) == GL_TRIANGLE_STRIP);
}

TEST_CASE("Conversion from index size to OpenGL enum", "[misc][opengl]")
{
    using namespace Rndr;
    REQUIRE(FromIndexSizeToOpenGL(1) == GL_UNSIGNED_BYTE);
    REQUIRE(FromIndexSizeToOpenGL(2) == GL_UNSIGNED_SHORT);
    REQUIRE(FromIndexSizeToOpenGL(4) == GL_UNSIGNED_INT);
}

TEST_CASE("Is pixel format low precision", "[misc]")
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

TEST_CASE("Is pixel format high precision", "[misc]")
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

TEST_CASE("Creating different types of buffers", "[render-api][buffer]")
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

TEST_CASE("Reading from GPU buffers", "[render-api][buffer]")
{
    constexpr int32_t k_buffer_size = 1024;
    Rndr::StackArray<uint8_t, k_buffer_size> data;
    for (int i = 0; i < k_buffer_size; ++i)
    {
        data[i] = 0xAB;
    }

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

TEST_CASE("Update the GPU buffer contents", "[render-api][buffer]")
{
    constexpr int32_t k_buffer_size = 1024;
    Rndr::StackArray<uint8_t, k_buffer_size> data;
    for (int i = 0; i < k_buffer_size; ++i)
    {
        data[i] = 0xAB;
    }

    Rndr::Init();
    const Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Update contents of the vertex buffer with default usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(!result);
    }
    SECTION("Update contents of the vertex buffer with dynamic usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(result);
    }
    SECTION("Update contents of the vertex buffer with read back usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(!result);
    }

    SECTION("Update contents of the index buffer with default usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Index, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(!result);
    }
    SECTION("Update contents of the index buffer with dynamic usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Index, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(result);
    }
    SECTION("Update contents of the index buffer with read back usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Index, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(!result);
    }

    SECTION("Update contents of the constant buffer with default usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(!result);
    }
    SECTION("Update contents of the constant buffer with dynamic usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(result);
    }
    SECTION("Update contents of the constant buffer with read back usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(!result);
    }

    SECTION("Update contents of the shader storage buffer with default usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(!result);
    }
    SECTION("Update contents of the shader storage buffer with dynamic usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(result);
    }
    SECTION("Update contents of the shader storage buffer with read back usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());

        const bool result = graphics_context.Update(buffer, Rndr::ConstByteSpan(data));
        REQUIRE(!result);
    }

    Rndr::Destroy();
}

TEST_CASE("Copy of buffers", "[render-api][buffer]")
{
    constexpr int32_t k_buffer_size = 1024;
    Rndr::StackArray<uint8_t, k_buffer_size> data;
    for (int i = 0; i < k_buffer_size; ++i)
    {
        data[i] = 0xAB;
    }

    Rndr::Init();
    const Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Copy vertex buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer src_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        bool result = false;
        result = graphics_context.Copy(dst_buffer, src_buffer);
        REQUIRE(result);

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        result = graphics_context.Read(dst_buffer, read_data);
        REQUIRE(result);

        for (int i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data[i] == data[i]);
        }
    }
    SECTION("Copy shader storage buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer src_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        bool result = false;
        result = graphics_context.Copy(dst_buffer, src_buffer);
        REQUIRE(result);

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        result = graphics_context.Read(dst_buffer, read_data);
        REQUIRE(result);

        for (int i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data[i] == data[i]);
        }
    }

    SECTION("Copy between different types of buffers")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer src_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        bool result = false;
        result = graphics_context.Copy(dst_buffer, src_buffer);
        REQUIRE(result);

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        result = graphics_context.Read(dst_buffer, read_data);
        REQUIRE(result);

        for (int i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data[i] == data[i]);
        }
    }

    SECTION("Copy with bad parameters")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer src_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        bool result = false;
        result = graphics_context.Copy(dst_buffer, src_buffer);
        REQUIRE(result);

        SECTION("Copy with invalid source buffer")
        {
            const Rndr::Buffer invalid_src_buffer;
            result = graphics_context.Copy(dst_buffer, invalid_src_buffer);
            REQUIRE(!result);
        }
        SECTION("Copy with invalid destination buffer")
        {
            const Rndr::Buffer invalid_dst_buffer;
            result = graphics_context.Copy(invalid_dst_buffer, src_buffer);
            REQUIRE(!result);
        }
        SECTION("Copy with bad offset")
        {
            result = graphics_context.Copy(dst_buffer, src_buffer, -1, 512);
            REQUIRE(!result);
            result = graphics_context.Copy(dst_buffer, src_buffer, 1024, 512);
            REQUIRE(!result);
            result = graphics_context.Copy(dst_buffer, src_buffer, 1050, 512);
            REQUIRE(!result);
            result = graphics_context.Copy(dst_buffer, src_buffer, 512, -1);
            REQUIRE(!result);
            result = graphics_context.Copy(dst_buffer, src_buffer, 512, 1024);
            REQUIRE(!result);
            result = graphics_context.Copy(dst_buffer, src_buffer, 512, 1050);
            REQUIRE(!result);
        }
        SECTION("Copy with bad size")
        {
            result = graphics_context.Copy(dst_buffer, src_buffer, 0, 0 - 1);
            REQUIRE(!result);
            result = graphics_context.Copy(dst_buffer, src_buffer, 0, 0, 1025);
            REQUIRE(!result);
        }
    }

    SECTION("Copy partial array")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer src_buffer(graphics_context, desc, Rndr::ConstByteSpan(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        bool result = false;
        result = graphics_context.Copy(dst_buffer, src_buffer, 0, k_buffer_size / 2);
        REQUIRE(result);

        Rndr::StackArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Rndr::ByteSpan read_data{read_data_storage};
        result = graphics_context.Read(dst_buffer, read_data);
        REQUIRE(result);

        for (int i = 0; i < k_buffer_size / 2; ++i)
        {
            REQUIRE(read_data[i] == data[i]);
        }
        for (int i = k_buffer_size / 2; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data[i] == 0);
        }
    }

    Rndr::Destroy();
}

TEST_CASE("Running a compute shader", "[render-api][shader]")
{
    Rndr::Init();
    const Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Use shader storage buffers")
    {
        constexpr int32_t k_buffer_size = 1024;
        Rndr::StackArray<float, k_buffer_size> data;
        for (int i = 0; i < k_buffer_size; ++i)
        {
            data[i] = static_cast<float>(i);
        }

        const Rndr::BufferDesc desc{
            .type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = k_buffer_size * sizeof(float)};
        const Rndr::Buffer src_buffer(graphics_context, desc, Rndr::ToByteSpan(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{
            .type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = k_buffer_size * sizeof(float)};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        const char* compute_shader_code = R"(
            #version 460 core
            layout(std430, binding = 0) buffer Data
            {
                float in_data[];
            };
            layout(std430, binding = 1) buffer OutData
            {
                float out_data[];
            };
            layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
            void main()
            {
                const uint index = gl_GlobalInvocationID.x;
                out_data[index] = in_data[index] * 2.0f;
            }
        )";

        Rndr::Shader compute_shader(graphics_context, Rndr::ShaderDesc{.type = Rndr::ShaderType::Compute, .source = compute_shader_code});
        REQUIRE(compute_shader.IsValid());
        const Rndr::Pipeline compute_pipeline(graphics_context, Rndr::PipelineDesc{.compute_shader = &compute_shader});
        REQUIRE(compute_pipeline.IsValid());

        graphics_context.Bind(compute_pipeline);
        graphics_context.Bind(src_buffer, 0);
        graphics_context.Bind(dst_buffer, 1);
        graphics_context.DispatchCompute(k_buffer_size / 64, 1, 1);

        Rndr::Array<float> read_data_storage(k_buffer_size);
        Rndr::ByteSpan read_data = Rndr::ToByteSpan(read_data_storage);
        const bool result = graphics_context.Read(dst_buffer, read_data);
        REQUIRE(result);
        for (int i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i] * 2.0f);
        }
    }

    SECTION("Using images")
    {
        constexpr int32_t k_image_width = 512;
        constexpr int32_t k_image_height = 512;
        Rndr::Array<float> data(k_image_width * k_image_height);
        for (int i = 0; i < k_image_width * k_image_height; ++i)
        {
            data[i] = static_cast<float>(i);
        }

        const Rndr::Image src_image(graphics_context,
                                    Rndr::ImageDesc{.width = k_image_width,
                                                    .height = k_image_height,
                                                    .type = Rndr::ImageType::Image2D,
                                                    .pixel_format = Rndr::PixelFormat::R32_FLOAT},
                                    Rndr::ToByteSpan(data));
        const Rndr::Image dst_image(graphics_context, Rndr::ImageDesc{.width = k_image_width,
                                                                      .height = k_image_height,
                                                                      .type = Rndr::ImageType::Image2D,
                                                                      .pixel_format = Rndr::PixelFormat::R32_FLOAT});

        const char* compute_shader_code = R"(
            #version 460 core
            layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
            layout(r32f, binding = 0) uniform image2D in_data;
            layout(r32f, binding = 1) uniform image2D out_data;
            void main()
            {
                ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
                vec4 res = imageLoad(in_data, texelCoord);
                res.x *= 2.0f;
                imageStore(out_data, texelCoord, res);
            }
        )";

        Rndr::Shader compute_shader(graphics_context, Rndr::ShaderDesc{.type = Rndr::ShaderType::Compute, .source = compute_shader_code});
        REQUIRE(compute_shader.IsValid());
        const Rndr::Pipeline compute_pipeline(graphics_context, Rndr::PipelineDesc{.compute_shader = &compute_shader});
        REQUIRE(compute_pipeline.IsValid());

        graphics_context.Bind(compute_pipeline);
        graphics_context.BindImageForCompute(src_image, 0, 0, Rndr::ImageAccess::Read);
        graphics_context.BindImageForCompute(dst_image, 1, 0, Rndr::ImageAccess::Write);
        graphics_context.DispatchCompute(k_image_width, k_image_height, 1);

        Rndr::Bitmap dst_bitmap;
        const bool result = graphics_context.Read(dst_image, dst_bitmap, 0);
        REQUIRE(result);
        for (int i = 0; i < k_image_height; ++i)
        {
            for (int j = 0; j < k_image_width; j++)
            {
                REQUIRE(dst_bitmap.GetPixel(j, i).x == data[j + i * k_image_width] * 2.0f);
            }
        }
    }

    Rndr::Destroy();
}

TEST_CASE("Creating a image", "[render-api][image]")
{
    Rndr::Init();
    const Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    const Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Creating a 2D image")
    {
        const Rndr::ImageDesc desc{.width = 512, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::Image image(graphics_context, desc);
        REQUIRE(image.IsValid());
    }
    // TODO: Enable once support for Image2DArray is added
//    SECTION("Creating a 2D array image")
//    {
//        const Rndr::ImageDesc desc{.width = 512, .height = 512, .array_size = 3, .type = Rndr::ImageType::Image2DArray, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
//        const Rndr::Image image(graphics_context, desc);
//        REQUIRE(image.IsValid());
//    }
    SECTION("Creating cube map")
    {
        const Rndr::ImageDesc desc{.width = 512, .height = 512, .array_size = 6, .type = Rndr::ImageType::CubeMap, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UNORM};
        const Rndr::Image image(graphics_context, desc);
        REQUIRE(image.IsValid());
    }
    SECTION("Creating image from a bitmap")
    {
        Rndr::Bitmap bitmap(512, 512, 1, Rndr::PixelFormat::R8G8B8A8_UNORM);
        const Rndr::Image image(graphics_context, bitmap, false, Rndr::SamplerDesc{});
        REQUIRE(image.IsValid());
    }
    SECTION("Creating image from a bitmap with mipmaps")
    {
        Rndr::Bitmap bitmap(512, 512, 1, Rndr::PixelFormat::R8G8B8A8_UNORM);
        const Rndr::Image image(graphics_context, bitmap, true, Rndr::SamplerDesc{});
        REQUIRE(image.IsValid());
    }
    SECTION("Creating image with invalid width or height")
    {
        const Rndr::ImageDesc desc{.width = 0, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::Image image(graphics_context, desc);
        REQUIRE(!image.IsValid());
    }
    // TODO: Enable once support for Image2DArray is added
//    SECTION("Creating image with invalid array size")
//    {
//        const Rndr::ImageDesc desc{.width = 512, .height = 512, .array_size = 0, .type = Rndr::ImageType::Image2DArray, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
//        const Rndr::Image image(graphics_context, desc);
//        REQUIRE(!image.IsValid());
//    }

    Rndr::Destroy();
}

TEST_CASE("Creating a frame buffer", "[render-api][framebuffer]")
{
    Rndr::Init();
    const Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    const Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("with single color attachment")
    {
        const Rndr::ImageDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc}};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(frame_buffer.IsValid());
        REQUIRE(frame_buffer.GetColorAttachmentCount() == 1);
        REQUIRE(!frame_buffer.GetDepthStencilAttachment().IsValid());
    }
    SECTION("with depth stencil attachment")
    {
        const Rndr::ImageDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::ImageDesc depth_stencil_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::D24_UNORM_S8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc},
                                         .depth_stencil_attachment = depth_stencil_attachment_desc,
                                         .use_depth_stencil = true};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(frame_buffer.IsValid());
        REQUIRE(frame_buffer.GetColorAttachmentCount() == 1);
        REQUIRE(frame_buffer.GetDepthStencilAttachment().IsValid());
    }
    SECTION("with multiple color attachments")
    {
        const Rndr::ImageDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::ImageDesc color_attachment_desc2{
            .width = 512, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc, color_attachment_desc2}};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(frame_buffer.IsValid());
        REQUIRE(frame_buffer.GetColorAttachmentCount() == 2);
        REQUIRE(!frame_buffer.GetDepthStencilAttachment().IsValid());
    }
    SECTION("with no attachments")
    {
        const Rndr::FrameBufferDesc desc;
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(!frame_buffer.IsValid());
    }
    SECTION("with invalid width and height in color attachment")
    {
        const Rndr::ImageDesc color_attachment_desc{
            .width = 0, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc}};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(!frame_buffer.IsValid());
    }
    SECTION("with invalid width and height in depth stencil attachment")
    {
        const Rndr::ImageDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::ImageDesc depth_stencil_attachment_desc{
            .width = 0, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::D24_UNORM_S8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc},
                                         .depth_stencil_attachment = depth_stencil_attachment_desc,
                                         .use_depth_stencil = true};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(!frame_buffer.IsValid());
    }
    SECTION("with invalid image type in color attachment")
    {
        const Rndr::ImageDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::ImageType::CubeMap, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc}};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(!frame_buffer.IsValid());
    }
    SECTION("with invalid image type in depth stencil attachment")
    {
        const Rndr::ImageDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::ImageType::Image2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::ImageDesc depth_stencil_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::ImageType::CubeMap, .pixel_format = Rndr::PixelFormat::D24_UNORM_S8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc},
                                         .depth_stencil_attachment = depth_stencil_attachment_desc,
                                         .use_depth_stencil = true};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(!frame_buffer.IsValid());
    }

    Rndr::Destroy();
}
