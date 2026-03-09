#include <catch2/catch2.hpp>

#include "opal/container/scope-ptr.h"
#include "opal/exceptions.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/render-target.hpp"
#include "rndr/exception.hpp"
#include "rndr/generic-window.hpp"

namespace
{

Rndr::Canvas::Context CreateTestContext(Opal::ScopePtr<Rndr::Application>& app, Opal::Ref<Rndr::GenericWindow>& window)
{
    app = Rndr::Application::Create();
    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    window = app->CreateGenericWindow(window_desc);
    return Rndr::Canvas::Context::Init(window->GetNativeHandle());
}

struct RenderTargetTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    RenderTargetTestFixture() : context(CreateTestContext(app, window)) {}
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

    SECTION("Chaining")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64, Rndr::Canvas::Format::RGBA8)
            .AddColor(64, 64, Rndr::Canvas::Format::R8)
            .SetDepthStencil(64, 64);

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

        Rndr::Canvas::RenderTarget rt(f.context, desc);
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

        Rndr::Canvas::RenderTarget rt(f.context, desc, "TestRT");
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetName() == "TestRT");
    }

    SECTION("Empty color attachments throws")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(f.context, desc));
    }

    SECTION("Too many color attachments throws")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        for (Rndr::i32 i = 0; i < Rndr::Canvas::k_max_color_attachments + 1; ++i)
        {
            desc.AddColor(32, 32);
        }
        REQUIRE_THROWS(Rndr::Canvas::RenderTarget(f.context, desc));
    }

    SECTION("Multiple color attachments")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64).AddColor(64, 64);

        Rndr::Canvas::RenderTarget rt(f.context, desc);
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetColorAttachmentCount() == 2);
        REQUIRE(rt.GetColorAttachment(0).IsValid());
        REQUIRE(rt.GetColorAttachment(1).IsValid());
    }

    SECTION("With depth/stencil attachment")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64).SetDepthStencil(64, 64);

        Rndr::Canvas::RenderTarget rt(f.context, desc);
        REQUIRE(rt.IsValid());
        REQUIRE(rt.GetDepthStencilAttachment().IsValid());
    }

    SECTION("With depth-only attachment")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64).SetDepthStencil(64, 64, Rndr::Canvas::Format::D32F);

        Rndr::Canvas::RenderTarget rt(f.context, desc);
        REQUIRE(rt.IsValid());
    }

    SECTION("Destroy makes render target invalid")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(32, 32);

        Rndr::Canvas::RenderTarget rt(f.context, desc);
        REQUIRE(rt.IsValid());
        rt.Destroy();
        REQUIRE_FALSE(rt.IsValid());
    }

    SECTION("Move constructor")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64);

        Rndr::Canvas::RenderTarget rt(f.context, desc, "MoveRT");
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

        Rndr::Canvas::RenderTarget rt(f.context, desc);
        Rndr::Canvas::RenderTarget other;

        other = std::move(rt);
        REQUIRE(other.IsValid());
        REQUIRE(other.GetWidth() == 32);
        REQUIRE_FALSE(rt.IsValid());
    }

    SECTION("Different pixel formats")
    {
        const Rndr::Canvas::Format formats[] = {
            Rndr::Canvas::Format::RGBA8,
            Rndr::Canvas::Format::RGBA16F,
            Rndr::Canvas::Format::RGBA32F,
            Rndr::Canvas::Format::R8,
            Rndr::Canvas::Format::RG8,
        };

        for (auto fmt : formats)
        {
            Rndr::Canvas::RenderTargetDesc desc;
            desc.AddColor(16, 16, fmt);

            Rndr::Canvas::RenderTarget rt(f.context, desc);
            REQUIRE(rt.IsValid());
            REQUIRE(rt.GetColorAttachment(0).GetDesc().format == fmt);
        }
    }

    SECTION("Color attachment is usable as texture")
    {
        Rndr::Canvas::RenderTargetDesc desc;
        desc.AddColor(64, 64);

        Rndr::Canvas::RenderTarget rt(f.context, desc);
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

        Rndr::Canvas::RenderTarget rt(f.context, desc, "CloneSrc");
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

        Rndr::Canvas::RenderTarget rt(f.context, desc);
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
