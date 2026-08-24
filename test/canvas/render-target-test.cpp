#include <catch2/catch2.hpp>

#include "canvas-test-common.hpp"

#include "opal/container/scope-ptr.h"
#include "opal/exceptions.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/render-target.hpp"
#include "rndr/exception.hpp"
#include "rndr/generic-window.hpp"

namespace
{

struct RenderTargetTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    RenderTargetTestFixture() : context(CanvasTest::CreateTestContext(app, window)) {}
};

}  // namespace

TEST_CASE("Canvas RenderTargetDesc builder", "[canvas][render-target]")
{
    SECTION("AddColor with dimensions and default format")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(128, 64);

        REQUIRE(desc.color_attachments.GetSize() == 1);
        REQUIRE(desc.color_attachments[0].width == 128);
        REQUIRE(desc.color_attachments[0].height == 64);
        REQUIRE(desc.color_attachments[0].format == Rndr::Canvas::Format::RGBA8);
    }

    SECTION("AddColor with explicit format")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(32, 32, Rndr::Canvas::Format::RGBA16F);

        REQUIRE(desc.color_attachments[0].format == Rndr::Canvas::Format::RGBA16F);
    }

    SECTION("AddColor with TextureDesc")
    {
        Rndr::Canvas::TextureDesc tex;
        tex.width = 16;
        tex.height = 16;
        tex.min_filter = Rndr::Canvas::TextureFilter::Nearest;

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(tex);

        REQUIRE(desc.color_attachments.GetSize() == 1);
        REQUIRE(desc.color_attachments[0].min_filter == Rndr::Canvas::TextureFilter::Nearest);
    }

    SECTION("SetDepthStencil")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.SetDepthStencil(64, 64);

        REQUIRE(desc.use_depth_stencil);
        REQUIRE(desc.depth_stencil_attachment.width == 64);
        REQUIRE(desc.depth_stencil_attachment.height == 64);
        REQUIRE(desc.depth_stencil_attachment.format == Rndr::Canvas::Format::D24S8);
    }

    SECTION("SetDepthStencil with explicit format")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.SetDepthStencil(32, 32, Rndr::Canvas::Format::D32F);

        REQUIRE(desc.depth_stencil_attachment.format == Rndr::Canvas::Format::D32F);
    }

    SECTION("Attachment description Clone copies inherited and external fields")
    {
        Rndr::Canvas::RenderTargetAttachmentDesc attachment;
        attachment.width = 128;
        attachment.height = 64;
        attachment.format = Rndr::Canvas::Format::RGBA16F;
        attachment.mip_level = 2;
        attachment.layer = 3;

        Rndr::Canvas::RenderTargetAttachmentDesc clone = attachment.Clone();
        REQUIRE(clone.width == 128);
        REQUIRE(clone.height == 64);
        REQUIRE(clone.format == Rndr::Canvas::Format::RGBA16F);
        REQUIRE(clone.mip_level == 2);
        REQUIRE(clone.layer == 3);
        REQUIRE_FALSE(clone.texture.IsValid());
    }

    SECTION("Chaining")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64, Rndr::Canvas::Format::RGBA8).AddColor(64, 64, Rndr::Canvas::Format::R8).SetDepthStencil(64, 64);

        REQUIRE(desc.color_attachments.GetSize() == 2);
        REQUIRE(desc.color_attachments[0].format == Rndr::Canvas::Format::RGBA8);
        REQUIRE(desc.color_attachments[1].format == Rndr::Canvas::Format::R8);
        REQUIRE(desc.use_depth_stencil);
    }
}

TEST_CASE("Canvas RenderTarget", "[canvas][render-target]")
{
    RenderTargetTestFixture f;

    SECTION("Default constructed render target is invalid")
    {
        Rndr::Canvas::RenderTarget rt;
        REQUIRE_FALSE(rt.IsValid());
    }

    SECTION("Create with single color attachment")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(128, 128);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetWidth() == 128);
        REQUIRE(rt.GetHeight() == 128);
        REQUIRE(rt.GetColorAttachmentCount() == 1);
        REQUIRE(rt.GetColorAttachment(0).IsValid());
        REQUIRE(rt.GetNativeHandle() != 0);
    }

    SECTION("Create with debug name")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64);

        Rndr::Canvas::RenderTarget rt(desc, "TestRT");
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetName() == "TestRT");
    }

    SECTION("No color and no depth/stencil attachment throws")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(desc));
    }

    SECTION("Too many color attachments throws")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        for (Rndr::i32 i = 0; i < Rndr::Canvas::k_max_color_attachments + 1; ++i)
        {
            desc.AddColor(32, 32);
        }
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(desc));
    }

    SECTION("Multiple color attachments")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64).AddColor(64, 64);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetColorAttachmentCount() == 2);
        REQUIRE(rt.GetColorAttachment(0).IsValid());
        REQUIRE(rt.GetColorAttachment(1).IsValid());
    }

    SECTION("With depth/stencil attachment")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64).SetDepthStencil(64, 64);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetDepthStencilAttachment().IsValid());
    }

    SECTION("With depth-only attachment")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64).SetDepthStencil(64, 64, Rndr::Canvas::Format::D32F);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
    }

    SECTION("Destroy makes render target invalid")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(32, 32);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
        rt.Destroy();
        REQUIRE_FALSE(rt.IsValid());
    }

    SECTION("Move constructor")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64);

        Rndr::Canvas::RenderTarget rt(desc, "MoveRT");
        REQUIRE(rt.IsValid());

        Rndr::Canvas::RenderTarget moved(std::move(rt));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetWidth() == 64);
        REQUIRE(moved.GetHeight() == 64);
        REQUIRE_FALSE(rt.IsValid());
    }

    SECTION("Move assignment")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(32, 32);

        Rndr::Canvas::RenderTarget rt(desc);
        Rndr::Canvas::RenderTarget other;

        other = std::move(rt);
        REQUIRE(other.IsValid());
        REQUIRE(other.GetWidth() == 32);
        REQUIRE_FALSE(rt.IsValid());
    }

    SECTION("Different pixel formats")
    {
        const Rndr::Canvas::Format formats[] = {
            Rndr::Canvas::Format::RGBA8, Rndr::Canvas::Format::RGBA16F, Rndr::Canvas::Format::RGBA32F,
            Rndr::Canvas::Format::R8,    Rndr::Canvas::Format::RG8,
        };

        for (auto fmt : formats)
        {
            Rndr::Canvas::RenderTargetDesc desc;
            desc.AddColor(16, 16, fmt);

            Rndr::Canvas::RenderTarget rt(desc);
            REQUIRE(rt.IsValid());
            REQUIRE(rt.GetColorAttachment(0).GetDesc().format == fmt);
        }
    }

    SECTION("Color attachment is usable as texture")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());

        const Rndr::Canvas::Texture& color = rt.GetColorAttachment(0);
        REQUIRE(color.IsValid());
        REQUIRE(color.GetNativeHandle() != 0);
        REQUIRE(color.GetDesc().width == 64);
        REQUIRE(color.GetDesc().height == 64);
    }

    SECTION("Clone")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64);

        Rndr::Canvas::RenderTarget rt(desc, "CloneSrc");
        REQUIRE(rt.IsValid());

        Rndr::Canvas::RenderTarget clone = rt.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetWidth() == 64);
        REQUIRE(clone.GetHeight() == 64);
        REQUIRE(clone.GetColorAttachmentCount() == 1);
        // Original still valid.
        REQUIRE(rt.IsValid());
        // Different native handles.
        REQUIRE(clone.GetNativeHandle() != rt.GetNativeHandle());
        REQUIRE(clone.GetColorAttachment(0).GetNativeHandle() != rt.GetColorAttachment(0).GetNativeHandle());
    }

    SECTION("Clone with depth/stencil")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(32, 32).SetDepthStencil(32, 32);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());

        Rndr::Canvas::RenderTarget clone = rt.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetDepthStencilAttachment().IsValid());
    }

    SECTION("Clone of invalid render target returns invalid")
    {
        Rndr::Canvas::RenderTarget rt;
        Rndr::Canvas::RenderTarget clone = rt.Clone();
        REQUIRE_FALSE(clone.IsValid());
    }
}

TEST_CASE("Canvas RenderTarget with external textures", "[canvas][render-target]")
{
    RenderTargetTestFixture f;

    SECTION("External color texture")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 64;
        tex_desc.height = 64;
        Rndr::Canvas::Texture color(tex_desc);
        REQUIRE(color.IsValid());

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(color);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetWidth() == 64);
        REQUIRE(rt.GetHeight() == 64);
        REQUIRE(rt.GetColorAttachmentCount() == 1);
        REQUIRE(rt.IsColorAttachmentExternal(0));
        REQUIRE(rt.GetColorAttachment(0).GetNativeHandle() == color.GetNativeHandle());
    }

    SECTION("External depth/stencil texture")
    {
        const Rndr::Canvas::Format depth_formats[] = {Rndr::Canvas::Format::D24S8, Rndr::Canvas::Format::D32F};

        for (auto fmt : depth_formats)
        {
            Rndr::Canvas::TextureDesc depth_desc;
            depth_desc.width = 64;
            depth_desc.height = 64;
            depth_desc.format = fmt;
            Rndr::Canvas::Texture depth(depth_desc);
            REQUIRE(depth.IsValid());

            Rndr::Canvas::RenderTargetDesc desc;
            desc.AddColor(64, 64).SetDepthStencil(depth);

            Rndr::Canvas::RenderTarget rt(desc);
            REQUIRE(rt.IsValid());
            REQUIRE(rt.IsDepthStencilExternal());
            REQUIRE_FALSE(rt.IsColorAttachmentExternal(0));
            REQUIRE(rt.GetDepthStencilAttachment().GetNativeHandle() == depth.GetNativeHandle());
        }
    }

    SECTION("External color and external depth")
    {
        Rndr::Canvas::TextureDesc color_desc;
        color_desc.width = 32;
        color_desc.height = 32;
        Rndr::Canvas::Texture color(color_desc);

        Rndr::Canvas::TextureDesc depth_desc;
        depth_desc.width = 32;
        depth_desc.height = 32;
        depth_desc.format = Rndr::Canvas::Format::D24S8;
        Rndr::Canvas::Texture depth(depth_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(color).SetDepthStencil(depth);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
        REQUIRE(rt.IsColorAttachmentExternal(0));
        REQUIRE(rt.IsDepthStencilExternal());
    }

    SECTION("Destroying the render target leaves the external texture alive")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        Rndr::Canvas::Texture color(tex_desc);
        const Rndr::u32 color_handle = color.GetNativeHandle();

        {
            Rndr::Canvas::RenderTargetDesc desc;
            desc.AddColor(color);
            Rndr::Canvas::RenderTarget rt(desc);
            REQUIRE(rt.IsValid());
            rt.Destroy();
            REQUIRE_FALSE(rt.IsValid());
        }

        REQUIRE(color.IsValid());
        REQUIRE(color.GetNativeHandle() == color_handle);
        // Still usable: reattach it to a new render target.
        Rndr::Canvas::RenderTargetDesc desc2;
        desc2.AddColor(color);
        Rndr::Canvas::RenderTarget rt2(desc2);
        REQUIRE(rt2.IsValid());
    }

    SECTION("Moved-from render target leaves the external texture alive")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        Rndr::Canvas::Texture color(tex_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(color);

        Rndr::Canvas::RenderTarget rt(desc);
        Rndr::Canvas::RenderTarget moved(std::move(rt));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.IsColorAttachmentExternal(0));
        REQUIRE(color.IsValid());
    }

    SECTION("Depth only render target")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.SetDepthStencil(128, 64, Rndr::Canvas::Format::D32F);

        Rndr::Canvas::RenderTarget rt(desc, "ShadowMap");
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetColorAttachmentCount() == 0);
        REQUIRE(rt.GetWidth() == 128);
        REQUIRE(rt.GetHeight() == 64);
        REQUIRE(rt.GetDepthStencilAttachment().IsValid());
    }

    SECTION("Depth only render target with external texture")
    {
        Rndr::Canvas::TextureDesc depth_desc;
        depth_desc.width = 64;
        depth_desc.height = 64;
        depth_desc.format = Rndr::Canvas::Format::D32F;
        Rndr::Canvas::Texture depth(depth_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.SetDepthStencil(depth);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetColorAttachmentCount() == 0);
        REQUIRE(rt.IsDepthStencilExternal());
    }

    SECTION("External cube map face")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        tex_desc.type = Rndr::Canvas::TextureType::CubeMap;
        Rndr::Canvas::Texture cube(tex_desc);
        REQUIRE(cube.IsValid());

        for (Rndr::i32 face = 0; face < 6; ++face)
        {
            Rndr::Canvas::RenderTargetDesc desc;
            desc.AddColor(cube, 0, face);

            Rndr::Canvas::RenderTarget rt(desc);
            REQUIRE(rt.IsValid());
            REQUIRE(rt.GetWidth() == 32);
        }
    }

    SECTION("External array layer")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        tex_desc.array_size = 4;
        tex_desc.type = Rndr::Canvas::TextureType::Texture2DArray;
        Rndr::Canvas::Texture array_tex(tex_desc);
        REQUIRE(array_tex.IsValid());

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(array_tex, 0, 3);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
    }

    SECTION("External mip level reports the mip dimensions")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 64;
        tex_desc.height = 64;
        tex_desc.use_mips = true;
        Rndr::Canvas::Texture color(tex_desc);
        REQUIRE(color.GetMipLevelCount() > 2);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(color, 2);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetWidth() == 16);
        REQUIRE(rt.GetHeight() == 16);
    }

    SECTION("Invalid external texture throws")
    {
        Rndr::Canvas::Texture empty;
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(empty);
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(desc));
    }

    SECTION("Out of bounds mip level throws")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        Rndr::Canvas::Texture color(tex_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(color, 4);
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(desc));
    }

    SECTION("Out of bounds layer throws")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        tex_desc.array_size = 2;
        tex_desc.type = Rndr::Canvas::TextureType::Texture2DArray;
        Rndr::Canvas::Texture array_tex(tex_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(array_tex, 0, 2);
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(desc));
    }

    SECTION("Layer on a Texture2D throws")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        Rndr::Canvas::Texture color(tex_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(color, 0, 0);
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(desc));
    }

    SECTION("Depth format texture in a color slot throws")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        tex_desc.format = Rndr::Canvas::Format::D32F;
        Rndr::Canvas::Texture depth(tex_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(depth);
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(desc));
    }

    SECTION("Color format texture in the depth slot throws")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        Rndr::Canvas::Texture color(tex_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(32, 32).SetDepthStencil(color);
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(desc));
    }

    SECTION("Mismatched attachment sizes throw")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        Rndr::Canvas::Texture color(tex_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64).AddColor(color);
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(desc));
    }

    SECTION("Clone keeps borrowing the external texture")
    {
        Rndr::Canvas::TextureDesc tex_desc;
        tex_desc.width = 32;
        tex_desc.height = 32;
        Rndr::Canvas::Texture color(tex_desc);

        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(color).AddColor(32, 32);

        Rndr::Canvas::RenderTarget rt(desc);
        REQUIRE(rt.IsValid());

        Rndr::Canvas::RenderTarget clone = rt.Clone();
        REQUIRE(clone.IsValid());
        REQUIRE(clone.GetNativeHandle() != rt.GetNativeHandle());
        // External attachment is shared, not copied.
        REQUIRE(clone.IsColorAttachmentExternal(0));
        REQUIRE(clone.GetColorAttachment(0).GetNativeHandle() == color.GetNativeHandle());
        // Owned attachment still gets a fresh texture.
        REQUIRE_FALSE(clone.IsColorAttachmentExternal(1));
        REQUIRE(clone.GetColorAttachment(1).GetNativeHandle() != rt.GetColorAttachment(1).GetNativeHandle());

        // Destroying the clone must not touch the borrowed texture.
        clone.Destroy();
        REQUIRE(color.IsValid());
        REQUIRE(rt.IsValid());
    }
}
