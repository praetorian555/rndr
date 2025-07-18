#include <catch2/catch2.hpp>

#if RNDR_OPENGL
#include <glad/glad.h>
#endif

#include "opal/container/in-place-array.h"

#include "rndr/bitmap.hpp"
#include "rndr/file.hpp"
#include "rndr/render-api.hpp"

#include "rndr/application.hpp"
#include "rndr/window.h"

#if RNDR_OPENGL
#include "platform/opengl-helpers.hpp"
#include "rndr/platform/opengl-frame-buffer.hpp"
#endif

TEST_CASE("App", "[app]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);

    Rndr::GenericWindow* window = app->CreateGenericWindow();

    const Rndr::GraphicsContextDesc desc{.window_handle = window->GetNativeHandle()};
    Rndr::GraphicsContext graphics_context(desc);
    Rndr::SwapChainDesc const sc_desc;
    Rndr::SwapChain const swap_chain(graphics_context, sc_desc);
    while (!window->IsClosed())
    {
        app->ProcessSystemEvents();
        app->ProcessDeferredMessages(0.016f);
        graphics_context.ClearColor({1, 0, 0, 1});
        graphics_context.Present(swap_chain);
    }
    app->DestroyGenericWindow(window);
}

TEST_CASE("Graphics context", "[render-api][graphics-context]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);

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

    Rndr::Application::Destroy();
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
    REQUIRE(FromImageInfoToTarget(TextureType::Texture2D, false) == GL_TEXTURE_2D);
    REQUIRE(FromImageInfoToTarget(TextureType::Texture2D, true) == GL_TEXTURE_2D_MULTISAMPLE);
    REQUIRE(FromImageInfoToTarget(TextureType::Texture2DArray, false) == GL_TEXTURE_2D_ARRAY);
    REQUIRE(FromImageInfoToTarget(TextureType::Texture2DArray, true) == GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
    REQUIRE(FromImageInfoToTarget(TextureType::CubeMap, false) == GL_TEXTURE_CUBE_MAP);
    REQUIRE(FromImageInfoToTarget(TextureType::CubeMap, true) == GL_TEXTURE_CUBE_MAP);
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
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Create buffer with bad type")
    {
        const Rndr::BufferDesc desc{.type = static_cast<Rndr::BufferType>(10), .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc);
        REQUIRE(!ss_buffer.IsValid());
    }
    SECTION("Create buffer with bad usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = static_cast<Rndr::Usage>(10), .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc);
        REQUIRE(!ss_buffer.IsValid());
    }
    SECTION("Create buffer with zero size")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 0};
        const Rndr::Buffer ss_buffer(graphics_context, desc);
        REQUIRE(!ss_buffer.IsValid());
    }
    SECTION("Create buffer with negative offset")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 1024, .offset = -1};
        const Rndr::Buffer ss_buffer(graphics_context, desc);
        REQUIRE(!ss_buffer.IsValid());
    }
    SECTION("Create buffer with negative stride")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 1024, .stride = -1};
        const Rndr::Buffer ss_buffer(graphics_context, desc);
        REQUIRE(!ss_buffer.IsValid());
    }
    SECTION("Create buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc);
        REQUIRE(ss_buffer.IsValid());
    }
    SECTION("Create buffer with special constructor")
    {
        constexpr Rndr::i32 k_buffer_size = 1024;
        Opal::InPlaceArray<uint8_t, k_buffer_size> data;
        const Rndr::Buffer ss_buffer(graphics_context, Opal::AsBytes(data), Rndr::BufferType::ShaderStorage, Rndr::Usage::Default, 1024);
        REQUIRE(ss_buffer.IsValid());
    }

    graphics_context.Destroy();
    hidden_window.Destroy();
    Rndr::Application::Destroy();
}

TEST_CASE("Reading from GPU buffers", "[render-api][buffer]")
{
    constexpr Rndr::i32 k_buffer_size = 1024;
    Opal::InPlaceArray<uint8_t, k_buffer_size> data;
    for (Rndr::i32 i = 0; i < k_buffer_size; ++i)
    {
        data[i] = 0xAB;
    }

    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Reading from invalid buffer")
    {
        const Rndr::Buffer invalid_buffer;
        REQUIRE(!invalid_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(invalid_buffer, read_data);
        REQUIRE(result == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Reading from a buffer that doesn't have ReadBack usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data);
        REQUIRE(result == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Reading from a buffer with 0 size")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 0};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(!ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data);
        REQUIRE(result == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Reading from a buffer with negative offset")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024, .offset = -1};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(!ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data);
        REQUIRE(result == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Trying to read from buffer that doesn't have ReadBack usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data);
        REQUIRE(result == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Trying to read from buffer from a negative offset")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data, -1, 512);
        REQUIRE(result == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Trying to read from a buffer with offset that is out of bounds")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data, 1024, 512);
        REQUIRE(result == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Trying to read more bytes then the buffer has")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data, 0, 1025);
        REQUIRE(result == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Reading from a buffer that has data")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data);
        REQUIRE(result == Rndr::ErrorCode::Success);
        for (Rndr::i32 i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i]);
        }
    }
    SECTION("Read part of the data")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data, 0, 512);
        REQUIRE(result == Rndr::ErrorCode::Success);
        for (Rndr::i32 i = 0; i < 512; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i]);
        }
    }
    SECTION("Read chunk of data from the end")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer ss_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(ss_buffer.IsValid());

        Opal::InPlaceArray<uint8_t, k_buffer_size> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data{read_data_storage};
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(ss_buffer, read_data, 512, 512);
        REQUIRE(result == Rndr::ErrorCode::Success);
        for (Rndr::i32 i = 0; i < 512; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i + 512]);
        }
    }

    graphics_context.Destroy();
    hidden_window.Destroy();
    Rndr::Application::Destroy();
}

TEST_CASE("Update the GPU buffer contents", "[render-api][buffer]")
{
    constexpr Rndr::i32 k_buffer_size = 1024;
    Opal::InPlaceArray<uint8_t, k_buffer_size> data;
    for (Rndr::i32 i = 0; i < k_buffer_size; ++i)
    {
        data[i] = 0xAB;
    }

    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Update contents of an invalid buffer")
    {
        const Rndr::Buffer buffer;
        const Rndr::ErrorCode result = graphics_context.UpdateBuffer(buffer, Opal::AsBytes(data));
        REQUIRE(result == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Update contents of a buffer with default usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());
        const Rndr::ErrorCode result = graphics_context.UpdateBuffer(buffer, Opal::AsBytes(data));
        REQUIRE(result == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Update contents of a buffer with read back usage")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());
        const Rndr::ErrorCode result = graphics_context.UpdateBuffer(buffer, Opal::AsBytes(data));
        REQUIRE(result == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Update buffer with negative offset")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());
        const Rndr::ErrorCode result = graphics_context.UpdateBuffer(buffer, Opal::AsBytes(data), -1);
        REQUIRE(result == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Update buffer with offset equal to the size of the buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());
        const Rndr::ErrorCode result = graphics_context.UpdateBuffer(buffer, Opal::AsBytes(data), 1024);
        REQUIRE(result == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Update buffer with offset larger then the size of the buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());
        const Rndr::ErrorCode result = graphics_context.UpdateBuffer(buffer, Opal::AsBytes(data), 1025);
        REQUIRE(result == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Update buffer with no data")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());
        const Rndr::ErrorCode result = graphics_context.UpdateBuffer(buffer, {});
        REQUIRE(result == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Update buffer with data and offset that exceed the buffer size")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());
        const Rndr::ErrorCode result = graphics_context.UpdateBuffer(buffer, Opal::AsBytes(data), 512);
        REQUIRE(result == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Update buffer with dynamic usage and good offset and data")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc);
        REQUIRE(buffer.IsValid());
        const Rndr::ErrorCode result = graphics_context.UpdateBuffer(buffer, Opal::AsBytes(data));
        REQUIRE(result == Rndr::ErrorCode::Success);
    }

    graphics_context.Destroy();
    hidden_window.Destroy();
    Rndr::Application::Destroy();
}

TEST_CASE("Copy of buffers", "[render-api][buffer]")
{
    constexpr Rndr::i32 k_buffer_size_int = 256;
    Opal::InPlaceArray<Rndr::i32, k_buffer_size_int> data;
    for (Rndr::i32 i = 0; i < k_buffer_size_int; ++i)
    {
        data[i] = i;
    }

    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Copy from invalid buffer")
    {
        const Rndr::Buffer invalid_buffer;
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(buffer.IsValid());

        const Rndr::ErrorCode error_code = graphics_context.CopyBuffer(invalid_buffer, buffer);
        REQUIRE(error_code == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Copy to an invalid buffer")
    {
        const Rndr::Buffer invalid_buffer;
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(buffer.IsValid());

        const Rndr::ErrorCode error_code = graphics_context.CopyBuffer(buffer, invalid_buffer);
        REQUIRE(error_code == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Negative source offset")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer src_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        const Rndr::ErrorCode error_code = graphics_context.CopyBuffer(dst_buffer, src_buffer, 512, -1);
        REQUIRE(error_code == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Negative destination offset")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer src_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        const Rndr::ErrorCode error_code = graphics_context.CopyBuffer(dst_buffer, src_buffer, -1, 512);
        REQUIRE(error_code == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Out of bounds source offset")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 512};
        const Rndr::Buffer src_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        const Rndr::ErrorCode error_code = graphics_context.CopyBuffer(dst_buffer, src_buffer, 512, 512);
        REQUIRE(error_code == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Out of bounds destination offset")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer src_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 512};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        const Rndr::ErrorCode error_code = graphics_context.CopyBuffer(dst_buffer, src_buffer, 512, 512);
        REQUIRE(error_code == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Invalid size for the source buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 512};
        const Rndr::Buffer src_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        const Rndr::ErrorCode error_code = graphics_context.CopyBuffer(dst_buffer, src_buffer, 0, 0, 1024);
        REQUIRE(error_code == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Invalid size for the destination buffer")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 1024};
        const Rndr::Buffer src_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Default, .size = 512};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        const Rndr::ErrorCode error_code = graphics_context.CopyBuffer(dst_buffer, src_buffer, 0, 0, 1024);
        REQUIRE(error_code == Rndr::ErrorCode::OutOfBounds);
    }
    SECTION("Source and destination buffers are the same buffer and the ranges overlap")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(buffer.IsValid());

        const Rndr::ErrorCode error_code = graphics_context.CopyBuffer(buffer, buffer, 511, 0, 512);
        REQUIRE(error_code == Rndr::ErrorCode::InvalidArgument);
    }
    SECTION("Source and destination buffers are the same buffer and the ranges don't overlap")
    {
        const Rndr::BufferDesc desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::Dynamic, .size = 1024};
        const Rndr::Buffer buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(buffer.IsValid());

        Rndr::ErrorCode error_code = graphics_context.CopyBuffer(buffer, buffer, 512, 0, 512);
        REQUIRE(error_code == Rndr::ErrorCode::Success);

        const Rndr::BufferDesc read_desc{.type = Rndr::BufferType::Vertex, .usage = Rndr::Usage::ReadBack, .size = 1024};
        const Rndr::Buffer read_buffer(graphics_context, read_desc);
        REQUIRE(read_buffer.IsValid());
        Opal::InPlaceArray<Rndr::i32, k_buffer_size_int> read_data_storage = {0};
        Opal::ArrayView<Rndr::u8> read_data = Opal::AsWritableBytes(read_data_storage);
        error_code = graphics_context.CopyBuffer(read_buffer, buffer);
        REQUIRE(error_code == Rndr::ErrorCode::Success);
        error_code = graphics_context.ReadBuffer(read_buffer, read_data);
        REQUIRE(error_code == Rndr::ErrorCode::Success);
        for (Rndr::i32 i = 0; i < k_buffer_size_int / 2; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i]);
        }
        for (Rndr::i32 i = k_buffer_size_int / 2; i < k_buffer_size_int; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i - k_buffer_size_int / 2]);
        }
    }

    graphics_context.Destroy();
    hidden_window.Destroy();
    Rndr::Application::Destroy();
}

TEST_CASE("Creating a shader", "[render-api][shader]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Invalid shader")
    {
        const Rndr::ShaderDesc desc{.type = Rndr::ShaderType::Compute};
        const Rndr::Shader shader(graphics_context, desc);
        REQUIRE(!shader.IsValid());
    }
    SECTION("Default shader")
    {
        Rndr::Shader shader;
        REQUIRE(!shader.IsValid());
    }
    SECTION("Shader with bad shader code")
    {
        const Rndr::char8 shader_code[] =
            R"(
            #version 460 corehhe
            void main()
            {
                gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
            }
        )";
        const Rndr::ShaderDesc desc{.type = Rndr::ShaderType::Vertex, .source = shader_code};
        Rndr::Shader shader;
        Rndr::ErrorCode err = shader.Initialize(graphics_context, desc);
        REQUIRE(err == Rndr::ErrorCode::ShaderCompilationError);
        REQUIRE(!shader.IsValid());
    }
    SECTION("With defines")
    {
        const Rndr::char8 shader_code[] =
            R"(
            #version 460 core
            void main()
            {
                gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
            }
        )";
        const Rndr::ShaderDesc desc{.type = Rndr::ShaderType::Vertex, .source = shader_code, .defines = {"MY_DEFINE"}};
        Rndr::Shader shader;
        Rndr::ErrorCode err = shader.Initialize(graphics_context, desc);
        REQUIRE(err == Rndr::ErrorCode::Success);
        REQUIRE(shader.IsValid());
    }

    graphics_context.Destroy();
    hidden_window.Destroy();
    Rndr::Application::Destroy();
}

TEST_CASE("Running a compute shader", "[render-api][shader]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Use shader storage buffers")
    {
        constexpr Rndr::i32 k_buffer_size = 1024;
        Opal::InPlaceArray<Rndr::f32, k_buffer_size> data;
        for (Rndr::i32 i = 0; i < k_buffer_size; ++i)
        {
            data[i] = static_cast<Rndr::f32>(i);
        }

        const Rndr::BufferDesc desc{
            .type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = k_buffer_size * sizeof(Rndr::f32)};
        const Rndr::Buffer src_buffer(graphics_context, desc, Opal::AsBytes(data));
        REQUIRE(src_buffer.IsValid());

        const Rndr::BufferDesc desc2{
            .type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::ReadBack, .size = k_buffer_size * sizeof(Rndr::f32)};
        const Rndr::Buffer dst_buffer(graphics_context, desc2);
        REQUIRE(dst_buffer.IsValid());

        const Rndr::char8 compute_shader_code[] =
            R"(
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

        graphics_context.BindPipeline(compute_pipeline);
        graphics_context.BindBuffer(src_buffer, 0);
        graphics_context.BindBuffer(dst_buffer, 1);
        graphics_context.DispatchCompute(k_buffer_size / 64, 1, 1);

        Opal::DynamicArray<Rndr::f32> read_data_storage(k_buffer_size);
        Opal::ArrayView<Rndr::u8> read_data = Opal::AsWritableBytes(read_data_storage);
        const Rndr::ErrorCode result = graphics_context.ReadBuffer(dst_buffer, read_data);
        REQUIRE(result == Rndr::ErrorCode::Success);
        for (Rndr::i32 i = 0; i < k_buffer_size; ++i)
        {
            REQUIRE(read_data_storage[i] == data[i] * 2.0f);
        }
    }

    SECTION("Using images")
    {
        constexpr Rndr::i32 k_image_width = 512;
        constexpr Rndr::i32 k_image_height = 512;
        Opal::DynamicArray<Rndr::f32> data(k_image_width * k_image_height);
        for (Rndr::i32 i = 0; i < k_image_width * k_image_height; ++i)
        {
            data[i] = static_cast<Rndr::f32>(i);
        }

        const Rndr::Texture src_image(graphics_context,
                                      Rndr::TextureDesc{.width = k_image_width,
                                                        .height = k_image_height,
                                                        .type = Rndr::TextureType::Texture2D,
                                                        .pixel_format = Rndr::PixelFormat::R32_FLOAT},
                                      {}, Opal::AsBytes(data));
        const Rndr::Texture dst_image(graphics_context,
                                      Rndr::TextureDesc{.width = k_image_width,
                                                        .height = k_image_height,
                                                        .type = Rndr::TextureType::Texture2D,
                                                        .pixel_format = Rndr::PixelFormat::R32_FLOAT},
                                      {});

        const Rndr::char8 compute_shader_code[] =
            R"(
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

        graphics_context.BindPipeline(compute_pipeline);
        graphics_context.BindTextureForCompute(src_image, 0, 0, Rndr::TextureAccess::Read);
        graphics_context.BindTextureForCompute(dst_image, 1, 0, Rndr::TextureAccess::Write);
        graphics_context.DispatchCompute(k_image_width, k_image_height, 1);

        Rndr::Bitmap dst_bitmap;
        const bool result = graphics_context.Read(dst_image, dst_bitmap, 0);
        REQUIRE(result);
        for (Rndr::i32 i = 0; i < k_image_height; ++i)
        {
            for (Rndr::i32 j = 0; j < k_image_width; j++)
            {
                REQUIRE(dst_bitmap.GetPixel(j, i).x == data[j + i * k_image_width] * 2.0f);
            }
        }
    }

    graphics_context.Destroy();
    hidden_window.Destroy();
    Rndr::Application::Destroy();
}

TEST_CASE("Creating a texture", "[render-api][texture]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("Texture 2D")
    {
        SECTION("Bad width")
        {
            const Rndr::TextureDesc desc{.width = 0, .height = 512};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(!texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::InvalidArgument);
        }
        SECTION("Bad height")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 0};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(!texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::InvalidArgument);
        }
        SECTION("Regular with no data")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 512};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Regular with data")
        {
            Opal::DynamicArray<Rndr::u8> data(512 * 512 * 4, 5);
            const Rndr::TextureDesc desc{.width = 512, .height = 512};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc, {}, Opal::AsBytes(data));
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("With mip maps")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .use_mips = true};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("With mip maps and data")
        {
            Opal::DynamicArray<Rndr::u8> data(512 * 512 * 4, 5);
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .use_mips = true};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc, {}, Opal::AsBytes(data));
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Multi-sample")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .sample_count = 4};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Multi-sample with data")
        {
            Opal::DynamicArray<Rndr::u8> data(512 * 512 * 4, 5);
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .sample_count = 4};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc, {}, Opal::AsBytes(data));
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Mips and multi-sample")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .use_mips = true, .sample_count = 4};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Mips and multi-sample with data")
        {
            Opal::DynamicArray<Rndr::u8> data(512 * 512 * 4, 5);
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .use_mips = true, .sample_count = 4};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc, {}, Opal::AsBytes(data));
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
    }
    SECTION("Texture 2D Array")
    {
        SECTION("Bad width")
        {
            const Rndr::TextureDesc desc{.width = 0, .height = 512, .array_size = 6};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(!texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::InvalidArgument);
        }
        SECTION("Bad height")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 0, .array_size = 6};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(!texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::InvalidArgument);
        }
        SECTION("Bad array size")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .array_size = 0, .type = Rndr::TextureType::Texture2DArray};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(!texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::InvalidArgument);
        }
        SECTION("Regular with no data")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::Texture2DArray};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Regular with data")
        {
            Opal::DynamicArray<Rndr::u8> data(512 * 512 * 4 * 6, 5);
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::Texture2DArray};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc, {}, Opal::AsBytes(data));
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("With mip maps")
        {
            const Rndr::TextureDesc desc{
                .width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::Texture2DArray, .use_mips = true};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("With mip maps and data")
        {
            Opal::DynamicArray<Rndr::u8> data(512 * 512 * 4 * 6, 5);
            const Rndr::TextureDesc desc{
                .width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::Texture2DArray, .use_mips = true};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc, {}, Opal::AsBytes(data));
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Multi-sample")
        {
            const Rndr::TextureDesc desc{
                .width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::Texture2DArray, .sample_count = 4};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Multi-sample with data")
        {
            Opal::DynamicArray<Rndr::u8> data(512 * 512 * 4 * 6, 5);
            const Rndr::TextureDesc desc{
                .width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::Texture2DArray, .sample_count = 4};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc, {}, Opal::AsBytes(data));
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Mips and multi-sample")
        {
            const Rndr::TextureDesc desc{.width = 512,
                                         .height = 512,
                                         .array_size = 6,
                                         .type = Rndr::TextureType::Texture2DArray,
                                         .use_mips = true,
                                         .sample_count = 4};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
    }
    SECTION("Creating cube map")
    {
        SECTION("Bad width")
        {
            const Rndr::TextureDesc desc{.width = 0, .height = 512, .array_size = 6, .type = Rndr::TextureType::CubeMap};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(!texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::InvalidArgument);
        }
        SECTION("Bad height")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 0, .array_size = 6, .type = Rndr::TextureType::CubeMap};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(!texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::InvalidArgument);
        }
        SECTION("Regular with no data")
        {
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::CubeMap};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Regular with data")
        {
            Opal::DynamicArray<Rndr::u8> data(512 * 512 * 4 * 6, 5);
            const Rndr::TextureDesc desc{.width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::CubeMap};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc, {}, Opal::AsBytes(data));
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("With mips")
        {
            const Rndr::TextureDesc desc{
                .width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::CubeMap, .use_mips = true};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("With mips and data")
        {
            Opal::DynamicArray<Rndr::u8> data(512 * 512 * 4 * 6, 5);
            const Rndr::TextureDesc desc{
                .width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::CubeMap, .use_mips = true};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc, {}, Opal::AsBytes(data));
            REQUIRE(texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::Success);
        }
        SECTION("Multi-sample")
        {
            const Rndr::TextureDesc desc{
                .width = 512, .height = 512, .array_size = 6, .type = Rndr::TextureType::CubeMap, .sample_count = 4};
            Rndr::Texture texture;
            const Rndr::ErrorCode err = texture.Initialize(graphics_context, desc);
            REQUIRE(!texture.IsValid());
            REQUIRE(err == Rndr::ErrorCode::GraphicsAPIError);
        }
    }

    graphics_context.Destroy();
    hidden_window.Destroy();
    Rndr::Application::Destroy();
}

TEST_CASE("Creating a frame buffer", "[render-api][framebuffer]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    SECTION("with single color attachment")
    {
        const Rndr::TextureDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc}, .color_attachment_samplers = {{}}};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(frame_buffer.IsValid());
        REQUIRE(frame_buffer.GetColorAttachmentCount() == 1);
        REQUIRE(!frame_buffer.GetDepthStencilAttachment().IsValid());
    }
    SECTION("with depth stencil attachment")
    {
        const Rndr::TextureDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::TextureDesc depth_stencil_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::D24_UNORM_S8_UINT};
        const Rndr::FrameBufferDesc desc{
            .color_attachments = {color_attachment_desc},
            .color_attachment_samplers = {{}},
            .use_depth_stencil = true,
            .depth_stencil_attachment = depth_stencil_attachment_desc,
        };
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(frame_buffer.IsValid());
        REQUIRE(frame_buffer.GetColorAttachmentCount() == 1);
        REQUIRE(frame_buffer.GetDepthStencilAttachment().IsValid());
    }
    SECTION("with multiple color attachments")
    {
        const Rndr::TextureDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::TextureDesc color_attachment_desc2{
            .width = 512, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc, color_attachment_desc2},
                                         .color_attachment_samplers = {{}, {}}};
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
        const Rndr::TextureDesc color_attachment_desc{
            .width = 0, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc}, .color_attachment_samplers = {{}}};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(!frame_buffer.IsValid());
    }
    SECTION("with invalid width and height in depth stencil attachment")
    {
        const Rndr::TextureDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::TextureDesc depth_stencil_attachment_desc{
            .width = 0, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::D24_UNORM_S8_UINT};
        const Rndr::FrameBufferDesc desc{
            .color_attachments = {color_attachment_desc},
            .color_attachment_samplers = {{}},
            .use_depth_stencil = true,
            .depth_stencil_attachment = depth_stencil_attachment_desc,
        };
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(!frame_buffer.IsValid());
    }
    SECTION("with invalid image type in color attachment")
    {
        const Rndr::TextureDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::CubeMap, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc}, .color_attachment_samplers = {{}}};
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(!frame_buffer.IsValid());
    }
    SECTION("with invalid image type in depth stencil attachment")
    {
        const Rndr::TextureDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::TextureDesc depth_stencil_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::CubeMap, .pixel_format = Rndr::PixelFormat::D24_UNORM_S8_UINT};
        const Rndr::FrameBufferDesc desc{
            .color_attachments = {color_attachment_desc},
            .color_attachment_samplers = {{}},
            .use_depth_stencil = true,
            .depth_stencil_attachment = depth_stencil_attachment_desc,
        };
        const Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(!frame_buffer.IsValid());
    }
    SECTION("with move constructor")
    {
        const Rndr::TextureDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc}, .color_attachment_samplers = {{}}};
        Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(frame_buffer.IsValid());
        Rndr::FrameBuffer frame_buffer2(std::move(frame_buffer));
        REQUIRE(frame_buffer2.IsValid());
        REQUIRE(!frame_buffer.IsValid());
    }
    SECTION("with move assignment")
    {
        const Rndr::TextureDesc color_attachment_desc{
            .width = 512, .height = 512, .type = Rndr::TextureType::Texture2D, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UINT};
        const Rndr::FrameBufferDesc desc{.color_attachments = {color_attachment_desc}, .color_attachment_samplers = {{}}};
        Rndr::FrameBuffer frame_buffer(graphics_context, desc);
        REQUIRE(frame_buffer.IsValid());
        Rndr::FrameBuffer frame_buffer2;
        frame_buffer2 = std::move(frame_buffer);
        REQUIRE(frame_buffer2.IsValid());
        REQUIRE(!frame_buffer.IsValid());
    }

    graphics_context.Destroy();
    hidden_window.Destroy();
    Rndr::Application::Destroy();
}

TEST_CASE("Render full screen quad", "[render-api]")
{
    Rndr::Application* app = Rndr::Application::Create();
    REQUIRE(app != nullptr);
    Rndr::Window hidden_window({.start_visible = false});
    const Rndr::GraphicsContextDesc gc_desc{.window_handle = hidden_window.GetNativeWindowHandle()};
    Rndr::GraphicsContext graphics_context(gc_desc);

    const Opal::StringUtf8 vertex_shader_code = Rndr::File::ReadShader(RNDR_CORE_ASSETS_DIR, "full-screen-quad.vert");
    REQUIRE(!vertex_shader_code.IsEmpty());
    const Opal::StringUtf8 fragment_shader_code =
        R"(
        #version 460 core
        layout(location = 0) out vec4 fragColor;
        void main()
        {
            fragColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    )";

    SECTION("Using a screen space rectangle")
    {
        Rndr::Shader vertex_shader(graphics_context, Rndr::ShaderDesc{.type = Rndr::ShaderType::Vertex, .source = vertex_shader_code});
        REQUIRE(vertex_shader.IsValid());

        Rndr::Shader fragment_shader(graphics_context,
                                     Rndr::ShaderDesc{.type = Rndr::ShaderType::Fragment, .source = fragment_shader_code});
        REQUIRE(fragment_shader.IsValid());

        const Rndr::TextureDesc color_attachment_desc{.width = 128, .height = 128};
        const Rndr::FrameBuffer frame_buffer(
            graphics_context, Rndr::FrameBufferDesc{.color_attachments = {color_attachment_desc}, .color_attachment_samplers = {{}}});
        REQUIRE(frame_buffer.IsValid());

        const Rndr::PipelineDesc pipeline_desc{.vertex_shader = &vertex_shader, .pixel_shader = &fragment_shader};
        const Rndr::Pipeline pipeline(graphics_context, pipeline_desc);
        REQUIRE(pipeline.IsValid());

        graphics_context.BindFrameBuffer(frame_buffer);
        graphics_context.ClearColor(Rndr::Vector4f(0.0f, 0.0f, 0.0f, 0.0f));
        graphics_context.BindPipeline(pipeline);

        graphics_context.DrawVertices(Rndr::PrimitiveTopology::Triangle, 6);

        Rndr::Bitmap bitmap;
        graphics_context.Read(frame_buffer.GetColorAttachment(0), bitmap);

        for (Rndr::i32 i = 0; i < 128; ++i)
        {
            for (Rndr::i32 j = 0; j < 128; ++j)
            {
                REQUIRE(bitmap.GetPixel(j, i).r == 1.0f);
                REQUIRE(bitmap.GetPixel(j, i).g == 0);
                REQUIRE(bitmap.GetPixel(j, i).b == 0);
                REQUIRE(bitmap.GetPixel(j, i).a == 1.0f);
            }
        }
    }

    graphics_context.Destroy();
    hidden_window.Destroy();
    Rndr::Application::Destroy();
}
