#include <catch2/catch2.hpp>

#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/container/scope-ptr.h"
#include "opal/exceptions.h"

#include "rndr/application.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/frame-context.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/swap-chain.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/forge/transfer.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/types.hpp"

#include "forge-test-common.hpp"

/**
 * Windowed tests for Forge: Surface, SwapChain and FrameContext, the three types the frame loop is made of
 * and the three the headless file cannot reach. They need a window system as well as a device, so they carry
 * their own tag and skip on a machine that has neither.
 *
 * What a frame put on the screen is checked the way the headless file checks a pass: the frame copies the
 * swap chain texture it rendered into onto a texture of this file's own, inside the same command buffer, and
 * that copy is what is read back and compared. Copying inside the frame rather than reading the swap chain
 * texture after the present is what makes it a synchronized read of the bytes that were presented instead of
 * a race against the presentation engine.
 */

namespace
{

using namespace Rndr;

/** Big enough that a wrong extent is obvious, small enough that reading every frame back stays cheap. */
constexpr i32 k_window_width = 128;
constexpr i32 k_window_height = 96;

/** The format the swap chain is asked for. Its bytes come back as blue, green, red, alpha in that order. */
constexpr PixelFormat k_swap_chain_format = PixelFormat::B8G8R8A8_SRGB;

/**
 * An application, an offscreen window, a surface over it and a device that can present to it. Declared in
 * the order they have to be built, which is the reverse of the order they have to be released - the device
 * before the surface, the surface before the instance, and the window before the application that owns it.
 */
struct ForgeWindowFixture
{
    Opal::ScopePtr<Application> app;
    Opal::Ref<GenericWindow> window;
    Forge::GraphicsContext context;
    Forge::Surface surface;
    Forge::Device device;

    explicit ForgeWindowFixture(i32 width = k_window_width, i32 height = k_window_height)
        // Never shown, and undecorated on purpose. A suite that pops windows up steals focus from whoever is
        // using the machine, and a hidden window has a client area and a surface all the same. Without a
        // title bar or a sizing frame the client area is the whole window, so what is asked for here and in
        // Reshape is what the surface reports - and, unlike a caption window, it can be sized to nothing,
        // which is the state the recovery case needs.
        : app(Application::Create({})),
          window(app->CreateGenericWindow({.width = width,
                                           .height = height,
                                           .name = "Forge window test",
                                           .resizable = false,
                                           .has_title_bar = false,
                                           .has_border = false,
                                           .show_in_taskbar = false,
                                           .start_visible = false}))
    {
        Opal::Expected<Forge::GraphicsContext, ErrorCode> context_result = Forge::GraphicsContext::Create(ForgeTest::TestContextDesc());
        if (!context_result.HasValue())
        {
            status = context_result.GetError();
            return;
        }
        context = std::move(context_result.GetValue());

        Opal::Expected<Forge::Surface, ErrorCode> surface_result = Forge::Surface::Create(context, *window);
        if (!surface_result.HasValue())
        {
            status = surface_result.GetError();
            return;
        }
        surface = std::move(surface_result.GetValue());
        // The graphics and the present family only: asking for the optional ones would make every case here
        // skip on a device whose single family does everything, the way the headless file explains.
        const Forge::DeviceDesc device_desc{.surface = surface, .use_async_compute_queue = false, .use_dedicated_transfer_queue = false};
        Opal::Expected<Opal::DynamicArray<Forge::PhysicalDevice>, ErrorCode> physical_devices = context.EnumeratePhysicalDevices();
        if (!physical_devices.HasValue())
        {
            status = physical_devices.GetError();
            return;
        }
        Opal::Expected<Forge::PhysicalDevice, ErrorCode> chosen = Forge::SelectPhysicalDevice(physical_devices.GetValue(), device_desc);
        if (!chosen.HasValue())
        {
            status = chosen.GetError();
            return;
        }
        Opal::Expected<Forge::Device, ErrorCode> device_result = Forge::Device::Create(std::move(chosen.GetValue()), context, device_desc);
        if (!device_result.HasValue())
        {
            status = device_result.GetError();
            return;
        }
        device = std::move(device_result.GetValue());
    }

    /** What the machine said when this fixture asked for what it wanted. The probe below reads it. */
    ErrorCode status = ErrorCode::Success;

    Forge::DeviceQueue& GetGraphicsQueue() { return ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics)); }
    Forge::DeviceQueue& GetPresentQueue() { return ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Present)); }

    [[nodiscard]] Opal::StringUtf8 GetValidationErrors() const { return ForgeTest::CollectValidationErrors(context); }
    [[nodiscard]] u32 GetValidationErrorCount() const { return ForgeTest::CountValidationErrors(context); }

    /** The client area, which is what the surface reports its extent from and what the swap chain matches. */
    [[nodiscard]] Vector2i GetClientSize() const { return window->GetSize(); }

    [[nodiscard]] Forge::SwapChainSupportDetails GetSupportDetails() const
    {
        return ForgeTest::Unwrap(surface.GetSwapChainSupportDetails(device.GetPhysicalDevice()));
    }

    /**
     * Resize the whole window and let the window system catch up. What the client area ends up as is read
     * back off the window rather than assumed, since the frame around it is counted in what is passed here.
     */
    void ResizeWindow(i32 width, i32 height)
    {
        window->Reshape(0, 0, width, height);
        PumpEvents();
    }

    void PumpEvents()
    {
        for (i32 i = 0; i < 4; ++i)
        {
            app->ProcessSystemEvents();
        }
    }

    void DestroyDevice() { device.Destroy(); }
};

/**
 * Whether this machine can run any of this: a window system, and a Vulkan device that can present to a
 * window over it. Probed once, so a machine with neither skips every case here instead of failing them.
 */
bool IsForgeWindowAvailable()
{
    static const bool available = []
    {
        // Rndr::Application and the window still report by throwing, so both ways of failing are caught here.
        try
        {
            const ForgeWindowFixture probe;
            return probe.status == ErrorCode::Success;
        }
        catch (const Opal::Exception&)
        {
            return false;
        }
    }();
    return available;
}

/** Whether the surface offers the format and the colour space every case here builds its swap chain with. */
bool SupportsSwapChainFormat(const ForgeWindowFixture& fixture)
{
    const Forge::SwapChainSupportDetails details = fixture.GetSupportDetails();
    for (const VkSurfaceFormatKHR& format : details.formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return true;
        }
    }
    return false;
}

/** Whether this surface's textures can be copied from, which is what SwapChainDesc::allow_readback needs. */
bool SupportsReadback(const ForgeWindowFixture& fixture)
{
    const Forge::SwapChainSupportDetails details = fixture.GetSupportDetails();
    return (details.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
}

/**
 * The colour a frame clears to, each channel zero or one. Those are the only values a UNORM or an sRGB
 * format converts exactly, so the comparison is against what was asked for rather than against how the
 * driver rounds.
 */
Vector4f GetFrameColor(i32 frame)
{
    static const Vector4f k_palette[] = {{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f},
                                         {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}};
    constexpr i32 k_palette_size = 6;
    return k_palette[frame % k_palette_size];
}

/** How many pixels are not the colour the frame cleared to. Zero is what every case here wants. */
i32 CountWrongPixels(Opal::ArrayView<const u8> pixels, const Vector4f& color)
{
    auto to_byte = [](f32 channel) { return static_cast<u8>(channel >= 0.5f ? 255 : 0); };
    // B8G8R8A8, so the channels come back in the order the format names rather than the order they are written.
    const u8 expected_b = to_byte(color.b);
    const u8 expected_g = to_byte(color.g);
    const u8 expected_r = to_byte(color.r);
    const u8 expected_a = to_byte(color.a);
    i32 wrong = 0;
    for (i32 i = 0; i + 3 < pixels.GetSize(); i += 4)
    {
        const bool matches =
            pixels[i] == expected_b && pixels[i + 1] == expected_g && pixels[i + 2] == expected_r && pixels[i + 3] == expected_a;
        wrong += matches ? 0 : 1;
    }
    return wrong;
}

/** A texture of this file's own, the frame's copy target, so that what was presented can be read back. */
Forge::Texture MakeReadbackTexture(const Forge::Device& device, const VkExtent2D& extent)
{
    return ForgeTest::Unwrap(
        Forge::Texture::Create(device, {.format = k_swap_chain_format,
                                        .width = extent.width,
                                        .height = extent.height,
                                        .usage = Forge::TextureUsageBits::TransferSource | Forge::TextureUsageBits::TransferDestination}));
}

/**
 * Record one frame into an already-begun command buffer: clear the swap chain texture to a colour, then copy
 * what came out onto the readback texture.
 *
 * @param readback Where the frame's result is copied, leaving the swap chain texture in TransferSource for
 *        the transition to Present to pick up. Null for a case that only wants the frame to run.
 * @param depth_texture The depth attachment of the pass, or null for a swap chain that has no depth.
 */
void RecordClearFrame(Forge::CommandBuffer& command_buffer, Forge::Texture& color_texture, Forge::Texture* readback,
                      const VkExtent2D& extent, const Vector4f& color, Forge::Texture* depth_texture)
{
    Opal::DynamicArray<Forge::TextureBarrier> barriers;
    barriers.PushBack(ForgeTest::Unwrap(Forge::TextureBarrier::ToColorAttachment(color_texture)));
    if (depth_texture != nullptr)
    {
        barriers.PushBack(ForgeTest::Unwrap(Forge::TextureBarrier::ToDepthStencilAttachment(*depth_texture)));
    }
    REQUIRE(command_buffer.CmdTextureBarriers(barriers) == ErrorCode::Success);

    Forge::RenderingDesc rendering_desc{
        .render_area_extent = {static_cast<i32>(extent.width), static_cast<i32>(extent.height)},
        .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color_texture,
                                                             .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                             .store_operation = Forge::AttachmentStoreOperation::Store,
                                                             .clear_value = color}}};
    if (depth_texture != nullptr)
    {
        rendering_desc.depth_attachment =
            Forge::RenderingAttachmentDesc{.texture = *depth_texture,
                                           .load_operation = Forge::AttachmentLoadOperation::Clear,
                                           .store_operation = Forge::AttachmentStoreOperation::DontCare,
                                           .clear_value = Forge::DepthStencilClearValue{.depth = 1.0f, .stencil = 0}};
    }
    REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);

    if (readback != nullptr)
    {
        Opal::InPlaceArray<Forge::TextureBarrier, 2> copy_barriers{
            ForgeTest::Unwrap(Forge::TextureBarrier::ToTransferSource(color_texture)),
            ForgeTest::Unwrap(Forge::TextureBarrier::ToTransferDestination(*readback))};
        REQUIRE(command_buffer.CmdTextureBarriers(copy_barriers) == ErrorCode::Success);
        const Forge::TextureCopyRegion region;
        REQUIRE(command_buffer.CmdCopyTexture(color_texture, *readback, {&region, 1}) == ErrorCode::Success);
    }
}

/** Submit one frame's command buffer against the two swap chain semaphores and wait for it to finish. */
void SubmitAndWait(ForgeWindowFixture& fixture, Forge::CommandBuffer& command_buffer, const Forge::Semaphore& texture_ready,
                   const Forge::Semaphore& render_finished)
{
    const Opal::Ref<const Forge::CommandBuffer> submitted(command_buffer);
    const Forge::SemaphoreSubmit wait{.semaphore = texture_ready, .stages = Forge::PipelineStageBits::ColorAttachmentOutput};
    const Forge::SemaphoreSubmit signal{.semaphore = render_finished, .stages = Forge::PipelineStageBits::AllCommands};
    const Forge::Fence frame_done = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
    REQUIRE(
        fixture.GetGraphicsQueue().Submit(
            {.command_buffers = {&submitted, 1}, .wait_semaphores = {&wait, 1}, .signal_semaphores = {&signal, 1}, .fence = frame_done}) ==
        ErrorCode::Success);
    REQUIRE(frame_done.Wait() == ErrorCode::Success);
}

}  // namespace

TEST_CASE("Forge surface reports what a swap chain can be built from", "[forge-window]")
{
    if (!IsForgeWindowAvailable())
    {
        SKIP("No window system with a Vulkan device that can present to it on this machine.");
    }
    ForgeWindowFixture fixture;

    SECTION("A surface carries the window it was made over and answers about the device")
    {
        REQUIRE(fixture.surface.IsValid());
        REQUIRE(fixture.surface.GetNativeSurface() != VK_NULL_HANDLE);
        REQUIRE(&fixture.surface.GetWindow() == &fixture.window.Get());

        // The one call 3.21 left to this file: a device selected for this surface has to be able to present
        // to it, and the family index is what says so.
        const Opal::Optional<u32> present_family = fixture.device.GetPhysicalDevice().GetPresentQueueFamilyIndex(fixture.surface);
        REQUIRE(present_family.HasValue());
        REQUIRE(fixture.GetPresentQueue().GetQueueFamilyIndex() == present_family.GetValue());
    }
    SECTION("The support details name at least one format and the present mode every surface has")
    {
        const Forge::SwapChainSupportDetails details = fixture.GetSupportDetails();
        REQUIRE_FALSE(details.formats.IsEmpty());
        REQUIRE_FALSE(details.present_modes.IsEmpty());
        REQUIRE(details.capabilities.minImageCount >= 1);

        // Fifo is the one mode the specification makes every implementation support, so a list without it is
        // a list that was not filled in.
        bool has_fifo = false;
        for (const VkPresentModeKHR mode : details.present_modes)
        {
            has_fifo = has_fifo || mode == VK_PRESENT_MODE_FIFO_KHR;
        }
        REQUIRE(has_fifo);

        // The extent the surface reports is the client area of the window, which is what the swap chain
        // matches. A surface that dictates its own size says so through currentExtent instead.
        constexpr u32 k_extent_driven_by_window = 0xFFFFFFFF;
        if (details.capabilities.currentExtent.width != k_extent_driven_by_window)
        {
            const Vector2i client_size = fixture.GetClientSize();
            REQUIRE(static_cast<i32>(details.capabilities.currentExtent.width) == client_size.x);
            REQUIRE(static_cast<i32>(details.capabilities.currentExtent.height) == client_size.y);
        }
    }
    SECTION("A moved surface leaves the source empty")
    {
        Forge::Surface moved(std::move(fixture.surface));
        REQUIRE(moved.IsValid());
        REQUIRE_FALSE(fixture.surface.IsValid());
        // Back where the fixture keeps it, so that teardown releases it before the instance goes.
        fixture.surface = std::move(moved);
        REQUIRE(fixture.surface.IsValid());
    }
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

TEST_CASE("Forge swap chain matches the window it was built over", "[forge-window]")
{
    if (!IsForgeWindowAvailable())
    {
        SKIP("No window system with a Vulkan device that can present to it on this machine.");
    }
    ForgeWindowFixture fixture;
    if (!SupportsSwapChainFormat(fixture))
    {
        SKIP("This surface does not offer B8G8R8A8_SRGB with the sRGB non-linear colour space.");
    }

    SECTION("The textures come out at the size of the client area, with a depth texture beside them")
    {
        const Forge::SwapChain swap_chain =
            ForgeTest::Unwrap(Forge::SwapChain::Create(fixture.device, fixture.surface, {.pixel_format = k_swap_chain_format}));
        REQUIRE(swap_chain.IsValid());

        const Vector2i client_size = fixture.GetClientSize();
        REQUIRE(static_cast<i32>(swap_chain.GetExtent().width) == client_size.x);
        REQUIRE(static_cast<i32>(swap_chain.GetExtent().height) == client_size.y);

        REQUIRE(swap_chain.GetColorTextureCount() >= 1);
        for (u32 i = 0; i < swap_chain.GetColorTextureCount(); ++i)
        {
            const Forge::Texture& texture = swap_chain.GetColorTexture(i);
            REQUIRE(texture.IsValid());
            REQUIRE(texture.GetNativeImageView() != VK_NULL_HANDLE);
            REQUIRE(texture.GetDesc().width == swap_chain.GetExtent().width);
            REQUIRE(texture.GetDesc().height == swap_chain.GetExtent().height);
        }

        REQUIRE(swap_chain.HasDepth());
        REQUIRE(swap_chain.GetDepthTexture().IsValid());
        REQUIRE(swap_chain.GetDepthTexture().GetDesc().width == swap_chain.GetExtent().width);
    }
    SECTION("A swap chain asked for no depth has none")
    {
        // 4.5 from the other side: the sample reads HasDepth to decide whether to name a depth attachment,
        // and nothing checked that a swap chain without one answers false rather than handing out a texture.
        const Forge::SwapChain swap_chain = ForgeTest::Unwrap(
            Forge::SwapChain::Create(fixture.device, fixture.surface, {.use_depth = false, .pixel_format = k_swap_chain_format}));
        REQUIRE(swap_chain.IsValid());
        REQUIRE_FALSE(swap_chain.HasDepth());
        REQUIRE_FALSE(swap_chain.GetDepthTexture().IsValid());
    }
    SECTION("Nothing is acquired until AcquireTexture says so")
    {
        Forge::SwapChain swap_chain =
            ForgeTest::Unwrap(Forge::SwapChain::Create(fixture.device, fixture.surface, {.pixel_format = k_swap_chain_format}));
        REQUIRE_FALSE(swap_chain.HasAcquiredTexture());
        REQUIRE(swap_chain.GetCurrentTextureIndex() == Forge::k_invalid_texture_index);
        REQUIRE_FALSE(swap_chain.GetCurrentColorTexture().HasValue());

        const Forge::Semaphore render_finished = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        REQUIRE_FALSE(swap_chain.Present(fixture.GetPresentQueue(), render_finished).HasValue());

        // A timeline cannot take part in presentation either way round, and both calls say so rather than
        // handing the driver a semaphore it will reject.
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        REQUIRE_FALSE(swap_chain.AcquireTexture(timeline).HasValue());
        REQUIRE_FALSE(swap_chain.Present(fixture.GetPresentQueue(), timeline).HasValue());
    }
    SECTION("A moved swap chain leaves the source empty")
    {
        Forge::SwapChain swap_chain =
            ForgeTest::Unwrap(Forge::SwapChain::Create(fixture.device, fixture.surface, {.pixel_format = k_swap_chain_format}));
        const u32 texture_count = swap_chain.GetColorTextureCount();
        Forge::SwapChain moved(std::move(swap_chain));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.GetColorTextureCount() == texture_count);
        REQUIRE_FALSE(swap_chain.IsValid());
    }
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

TEST_CASE("Forge swap chain presents the frames that were rendered into it", "[forge-window]")
{
    if (!IsForgeWindowAvailable())
    {
        SKIP("No window system with a Vulkan device that can present to it on this machine.");
    }
    ForgeWindowFixture fixture;
    if (!SupportsSwapChainFormat(fixture))
    {
        SKIP("This surface does not offer B8G8R8A8_SRGB with the sRGB non-linear colour space.");
    }
    if (!SupportsReadback(fixture))
    {
        SKIP("This surface's textures cannot be copied from, so what was presented cannot be read back.");
    }

    // The acquire, render, submit, present cycle written out, which is what FrameContext does for the caller
    // and what the case below drives through it instead.
    Forge::SwapChain swap_chain = ForgeTest::Unwrap(
        Forge::SwapChain::Create(fixture.device, fixture.surface, {.pixel_format = k_swap_chain_format, .allow_readback = true}));
    Forge::Texture readback = MakeReadbackTexture(fixture.device, swap_chain.GetExtent());
    Opal::DynamicArray<u8> pixels(static_cast<i32>(swap_chain.GetExtent().width * swap_chain.GetExtent().height * 4));

    // Two rounds of every texture, so that a texture is presented, acquired again and rendered into again -
    // the case one pass over them cannot see.
    const i32 frame_count = static_cast<i32>(swap_chain.GetColorTextureCount()) * 2;
    for (i32 frame = 0; frame < frame_count; ++frame)
    {
        const Forge::Semaphore texture_ready = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        const Forge::Semaphore render_finished = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        const Forge::AcquiredTexture acquired = ForgeTest::Unwrap(swap_chain.AcquireTexture(texture_ready));
        REQUIRE(acquired.status == Forge::SwapChainStatus::Success);
        REQUIRE(acquired.texture_index < swap_chain.GetColorTextureCount());
        REQUIRE(swap_chain.HasAcquiredTexture());
        REQUIRE(swap_chain.GetCurrentTextureIndex() == acquired.texture_index);

        const Vector4f color = GetFrameColor(frame);
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetGraphicsQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        RecordClearFrame(command_buffer, ForgeTest::Unwrap(swap_chain.GetCurrentColorTexture()), &readback, swap_chain.GetExtent(), color,
                         &swap_chain.GetDepthTexture());
        REQUIRE(command_buffer.CmdTextureBarrier(
                    Forge::TextureBarrier::ToPresent(ForgeTest::Unwrap(swap_chain.GetCurrentColorTexture()))) == ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
        SubmitAndWait(fixture, command_buffer, texture_ready, render_finished);

        REQUIRE(ForgeTest::Unwrap(swap_chain.Present(fixture.GetPresentQueue(), render_finished)) == Forge::SwapChainStatus::Success);
        REQUIRE_FALSE(swap_chain.HasAcquiredTexture());

        // The copy was part of the frame that was just waited for, so what it holds is the bytes the present
        // handed to the presentation engine.
        INFO("frame " << frame << " rendered into texture " << acquired.texture_index);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetGraphicsQueue(), readback, pixels, 0, Forge::ImageLayout::Undefined) ==
                ErrorCode::Success);
        REQUIRE(CountWrongPixels(pixels, color) == 0);
    }

    REQUIRE(fixture.device.WaitForAll() == ErrorCode::Success);
    readback.Destroy();
    swap_chain.Destroy();
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

TEST_CASE("Forge frame context runs frames past the end of its timeline", "[forge-window]")
{
    if (!IsForgeWindowAvailable())
    {
        SKIP("No window system with a Vulkan device that can present to it on this machine.");
    }
    ForgeWindowFixture fixture;
    if (!SupportsSwapChainFormat(fixture))
    {
        SKIP("This surface does not offer B8G8R8A8_SRGB with the sRGB non-linear colour space.");
    }
    if (!SupportsReadback(fixture))
    {
        SKIP("This surface's textures cannot be copied from, so what was presented cannot be read back.");
    }

    // One makes every frame wait for the one before it, two lets the host run ahead: the two the desc
    // documents, and the two whose slot arithmetic differs.
    const i32 frames_in_flight = GENERATE(1, 2);
    // A frame with no depth attachment is 4.5 seen through the frame loop rather than through the swap chain.
    const bool use_depth = GENERATE(true, false);
    INFO("frames_in_flight " << frames_in_flight << ", use_depth " << use_depth);

    Forge::SwapChain swap_chain = ForgeTest::Unwrap(Forge::SwapChain::Create(
        fixture.device, fixture.surface, {.use_depth = use_depth, .pixel_format = k_swap_chain_format, .allow_readback = true}));
    REQUIRE(swap_chain.HasDepth() == use_depth);

    Forge::FrameContext frame_context = ForgeTest::Unwrap(Forge::FrameContext::Create(
        fixture.device, swap_chain, fixture.GetGraphicsQueue(), fixture.GetPresentQueue(), {.frames_in_flight = frames_in_flight}));
    REQUIRE(frame_context.IsValid());
    REQUIRE(frame_context.GetDesc().frames_in_flight == frames_in_flight);

    Forge::Texture readback = MakeReadbackTexture(fixture.device, swap_chain.GetExtent());
    Opal::DynamicArray<u8> pixels(static_cast<i32>(swap_chain.GetExtent().width * swap_chain.GetExtent().height * 4));

    // Three times round the slots and one more, so the timeline is reused more than once rather than merely
    // reaching its end - arithmetic that only breaks on the second wrap has somewhere to break.
    const i32 frame_count = frames_in_flight * 3 + 1;
    i32 submitted = 0;
    Vector4f last_color{};
    for (i32 frame = 0; frame < frame_count; ++frame)
    {
        if (ForgeTest::Unwrap(frame_context.BeginFrame()) == Forge::SwapChainStatus::OutOfDate)
        {
            // Nothing was recorded and nothing was submitted, so the slot did not move on.
            continue;
        }
        REQUIRE(frame_context.GetFrameIndex() == static_cast<u32>(submitted % frames_in_flight));
        REQUIRE(frame_context.GetTextureIndex() < swap_chain.GetColorTextureCount());
        REQUIRE(frame_context.GetRenderSize().x == static_cast<i32>(swap_chain.GetExtent().width));
        REQUIRE(frame_context.GetRenderSize().y == static_cast<i32>(swap_chain.GetExtent().height));

        last_color = GetFrameColor(frame);
        RecordClearFrame(ForgeTest::Unwrap(frame_context.GetCommandBuffer()), ForgeTest::Unwrap(frame_context.GetColorTexture()), &readback,
                         swap_chain.GetExtent(), last_color, use_depth ? &swap_chain.GetDepthTexture() : nullptr);
        ForgeTest::Unwrap(frame_context.EndFrame());
        ++submitted;
    }
    REQUIRE(submitted == frame_count);

    // The readback holds the last frame's copy, which is what the last present put on the screen.
    REQUIRE(fixture.device.WaitForAll() == ErrorCode::Success);
    REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetGraphicsQueue(), readback, pixels, 0, Forge::ImageLayout::Undefined) ==
            ErrorCode::Success);
    REQUIRE(CountWrongPixels(pixels, last_color) == 0);

    readback.Destroy();
    frame_context.Destroy();
    swap_chain.Destroy();
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

TEST_CASE("Forge swap chain follows the window across a resize", "[forge-window]")
{
    if (!IsForgeWindowAvailable())
    {
        SKIP("No window system with a Vulkan device that can present to it on this machine.");
    }
    ForgeWindowFixture fixture;
    if (!SupportsSwapChainFormat(fixture))
    {
        SKIP("This surface does not offer B8G8R8A8_SRGB with the sRGB non-linear colour space.");
    }

    Forge::SwapChain swap_chain =
        ForgeTest::Unwrap(Forge::SwapChain::Create(fixture.device, fixture.surface, {.pixel_format = k_swap_chain_format}));
    const VkExtent2D first_extent = swap_chain.GetExtent();
    REQUIRE(first_extent.width > 0);

    fixture.ResizeWindow(k_window_width * 2, k_window_height * 2);
    const Vector2i resized_client = fixture.GetClientSize();
    REQUIRE(resized_client.x != static_cast<i32>(first_extent.width));

    REQUIRE(swap_chain.Recreate() == ErrorCode::Success);
    REQUIRE(swap_chain.IsValid());
    REQUIRE(static_cast<i32>(swap_chain.GetExtent().width) == resized_client.x);
    REQUIRE(static_cast<i32>(swap_chain.GetExtent().height) == resized_client.y);

    // The textures are rebuilt rather than kept, which the extent alone would not catch: an extent that
    // moved with textures that did not is a pass rendering into the old size.
    REQUIRE(swap_chain.GetColorTextureCount() >= 1);
    for (u32 i = 0; i < swap_chain.GetColorTextureCount(); ++i)
    {
        REQUIRE(swap_chain.GetColorTexture(i).GetDesc().width == swap_chain.GetExtent().width);
        REQUIRE(swap_chain.GetColorTexture(i).GetDesc().height == swap_chain.GetExtent().height);
    }
    REQUIRE(swap_chain.GetDepthTexture().GetDesc().width == swap_chain.GetExtent().width);

    // A frame after the resize renders at the new size and presents, which is what says the rebuilt textures
    // and their views are usable rather than merely present. In its own scope, so that the semaphores and
    // the command buffer are gone before the teardown check releases the device they came from.
    {
        const Forge::Semaphore texture_ready = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        const Forge::Semaphore render_finished = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        const Forge::AcquiredTexture acquired = ForgeTest::Unwrap(swap_chain.AcquireTexture(texture_ready));
        REQUIRE(acquired.status == Forge::SwapChainStatus::Success);
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetGraphicsQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        RecordClearFrame(command_buffer, ForgeTest::Unwrap(swap_chain.GetCurrentColorTexture()), nullptr, swap_chain.GetExtent(),
                         GetFrameColor(0), &swap_chain.GetDepthTexture());
        REQUIRE(command_buffer.CmdTextureBarrier(
                    Forge::TextureBarrier::ToPresent(ForgeTest::Unwrap(swap_chain.GetCurrentColorTexture()))) == ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
        SubmitAndWait(fixture, command_buffer, texture_ready, render_finished);
        ForgeTest::Unwrap(swap_chain.Present(fixture.GetPresentQueue(), render_finished));
        REQUIRE(fixture.device.WaitForAll() == ErrorCode::Success);
    }

    REQUIRE(fixture.device.WaitForAll() == ErrorCode::Success);
    swap_chain.Destroy();
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

TEST_CASE("Forge swap chain recovers from a window with no client area", "[forge-window]")
{
    if (!IsForgeWindowAvailable())
    {
        SKIP("No window system with a Vulkan device that can present to it on this machine.");
    }
    ForgeWindowFixture fixture;
    if (!SupportsSwapChainFormat(fixture))
    {
        SKIP("This surface does not offer B8G8R8A8_SRGB with the sRGB non-linear colour space.");
    }

    // 1.1, which nothing checked. A minimized window is a window with no client area as far as the surface
    // is concerned, and sizing one to nothing is the same thing without stealing focus to do it.
    SECTION("A swap chain over an empty client area is left empty and recovers on the next acquire")
    {
        Forge::SwapChain swap_chain =
            ForgeTest::Unwrap(Forge::SwapChain::Create(fixture.device, fixture.surface, {.pixel_format = k_swap_chain_format}));
        REQUIRE(swap_chain.IsValid());

        fixture.ResizeWindow(0, 0);
        REQUIRE(fixture.GetClientSize().x == 0);
        REQUIRE(swap_chain.Recreate() == ErrorCode::Success);
        REQUIRE_FALSE(swap_chain.IsValid());
        REQUIRE(swap_chain.GetExtent().width == 0);
        REQUIRE(swap_chain.GetExtent().height == 0);
        REQUIRE(swap_chain.GetColorTextureCount() == 0);

        // Acquiring while there is nothing to render into tries again and says the frame has to be skipped,
        // rather than throwing or handing out an index into textures that do not exist.
        const Forge::Semaphore texture_ready = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        const Forge::AcquiredTexture while_empty = ForgeTest::Unwrap(swap_chain.AcquireTexture(texture_ready));
        REQUIRE(while_empty.status == Forge::SwapChainStatus::OutOfDate);
        REQUIRE(while_empty.texture_index == Forge::k_invalid_texture_index);
        REQUIRE_FALSE(swap_chain.HasAcquiredTexture());

        fixture.ResizeWindow(k_window_width, k_window_height);
        REQUIRE(fixture.GetClientSize().x > 0);

        // The first acquire after the window is back rebuilds the swap chain and still skips its frame; the
        // one after it is the frame that runs.
        const Forge::AcquiredTexture rebuilding = ForgeTest::Unwrap(swap_chain.AcquireTexture(texture_ready));
        REQUIRE(rebuilding.status == Forge::SwapChainStatus::OutOfDate);
        REQUIRE(swap_chain.IsValid());
        REQUIRE(static_cast<i32>(swap_chain.GetExtent().width) == fixture.GetClientSize().x);

        const Forge::AcquiredTexture recovered = ForgeTest::Unwrap(swap_chain.AcquireTexture(texture_ready));
        REQUIRE(recovered.status == Forge::SwapChainStatus::Success);
        REQUIRE(recovered.texture_index < swap_chain.GetColorTextureCount());

        REQUIRE(fixture.device.WaitForAll() == ErrorCode::Success);
        swap_chain.Destroy();
    }
    SECTION("A frame context over an empty client area skips its frames and picks up again")
    {
        Forge::SwapChain swap_chain =
            ForgeTest::Unwrap(Forge::SwapChain::Create(fixture.device, fixture.surface, {.pixel_format = k_swap_chain_format}));
        Forge::FrameContext frame_context = ForgeTest::Unwrap(Forge::FrameContext::Create(
            fixture.device, swap_chain, fixture.GetGraphicsQueue(), fixture.GetPresentQueue(), {.frames_in_flight = 2}));

        // One frame first, so the timeline has something behind it when the frames start being skipped.
        REQUIRE(ForgeTest::Unwrap(frame_context.BeginFrame()) == Forge::SwapChainStatus::Success);
        RecordClearFrame(ForgeTest::Unwrap(frame_context.GetCommandBuffer()), ForgeTest::Unwrap(frame_context.GetColorTexture()), nullptr,
                         swap_chain.GetExtent(), GetFrameColor(0), &swap_chain.GetDepthTexture());
        ForgeTest::Unwrap(frame_context.EndFrame());

        // A window that lost its client area is something the application learns from the window system
        // rather than from the driver. Whether an acquire or a present volunteers OutOfDate for one is up to
        // the implementation - the specification lets it stay silent, and the software driver CI runs on
        // does - so a test cannot demand it of either. Recreate is the call that reads the surface, finds
        // nothing to present to and releases the swap chain, and that is the state the rest of this case is
        // about.
        fixture.ResizeWindow(0, 0);
        REQUIRE(swap_chain.Recreate() == ErrorCode::Success);
        REQUIRE_FALSE(swap_chain.IsValid());

        // From here every frame is skipped, and the slot a skipped frame would have used is still the next
        // one: nothing was submitted, so the counter did not move.
        const u32 frame_index_while_empty = frame_context.GetFrameIndex();
        for (i32 i = 0; i < 4; ++i)
        {
            REQUIRE(ForgeTest::Unwrap(frame_context.BeginFrame()) == Forge::SwapChainStatus::OutOfDate);
            REQUIRE(frame_context.GetFrameIndex() == frame_index_while_empty);
        }

        fixture.ResizeWindow(k_window_width, k_window_height);
        // The first acquire after the window is back rebuilds the swap chain and still skips its frame; the
        // one after it is the frame that runs.
        REQUIRE(ForgeTest::Unwrap(frame_context.BeginFrame()) == Forge::SwapChainStatus::OutOfDate);
        REQUIRE(swap_chain.IsValid());
        REQUIRE(ForgeTest::Unwrap(frame_context.BeginFrame()) == Forge::SwapChainStatus::Success);
        RecordClearFrame(ForgeTest::Unwrap(frame_context.GetCommandBuffer()), ForgeTest::Unwrap(frame_context.GetColorTexture()), nullptr,
                         swap_chain.GetExtent(), GetFrameColor(1), &swap_chain.GetDepthTexture());
        REQUIRE(ForgeTest::Unwrap(frame_context.EndFrame()) == Forge::SwapChainStatus::Success);

        REQUIRE(fixture.device.WaitForAll() == ErrorCode::Success);
        frame_context.Destroy();
        swap_chain.Destroy();
    }
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}
