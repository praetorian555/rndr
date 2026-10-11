#include <chrono>
#include <thread>

#include "opal/assert.h"
#include "opal/container/dynamic-array.h"

#include "rndr/application.hpp"
#include "rndr/forge/buffer.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/frame-context.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/pipeline.hpp"
#include "rndr/forge/shader.hpp"
#include "rndr/forge/swap-chain.hpp"
#include "rndr/forge/synchronization.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/forge/transfer.hpp"
#include "rndr/generic-window.hpp"
#include "rndr/input-system.hpp"
#include "rndr/log.hpp"
#include "rndr/types.hpp"

// One still video frame, drawn to the window through a sampler YCbCr conversion, in each of the three layouts decoders
// hand out: NV12, P010 and I420. Space shows the next one, and the command line can name the first (nv12, p010, i420).
// The frame is made here rather than read from a file: colour bars over a grey ramp, converted to BT.709 narrow range
// on the CPU, which is what the conversion undoes on the GPU. All three should look the same.

using u8 = Rndr::u8;
using u32 = Rndr::u32;
using f32 = Rndr::f32;

namespace
{

template <typename T>
T Require(Opal::Expected<T, Rndr::ErrorCode>&& result)
{
    if (!result.HasValue())
    {
        Opal::HandleContractViolation("A Forge call failed. The log above says which and why.");
    }
    return std::move(result).GetValue();
}

void RequireOk(Rndr::ErrorCode status)
{
    if (status != Rndr::ErrorCode::Success)
    {
        Opal::HandleContractViolation("A Forge call failed. The log above says which and why.");
    }
}

constexpr u32 k_frame_width = 1280;
constexpr u32 k_frame_height = 720;

constexpr const char* k_shader_source = R"(
[[vk::binding(0, 0)]] Sampler2D frame;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// One triangle that covers the whole window. SV_VulkanVertexID is gl_VertexIndex as it is; SV_VertexID would subtract
// the base vertex, which needs the draw parameters feature.
[shader("vertex")]
VertexOutput main_vertex(uint vertex_id : SV_VulkanVertexID)
{
    const float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    VertexOutput output;
    output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.uv = uv;
    return output;
}

[shader("fragment")]
float4 main_fragment(VertexOutput input) : SV_Target
{
    return float4(frame.Sample(input.uv).rgb, 1.0);
}
)";

struct Rgb
{
    f32 r;
    f32 g;
    f32 b;
};

/** The colour at a pixel of the test card: eight bars over the top two thirds, a grey ramp under them. */
Rgb TestCard(u32 x, u32 y)
{
    constexpr Rgb k_bars[] = {{1, 1, 1}, {1, 1, 0}, {0, 1, 1}, {0, 1, 0}, {1, 0, 1}, {1, 0, 0}, {0, 0, 1}, {0, 0, 0}};
    if (y < k_frame_height * 2 / 3)
    {
        return k_bars[x * 8 / k_frame_width];
    }
    const f32 grey = static_cast<f32>(x) / static_cast<f32>(k_frame_width - 1);
    return {grey, grey, grey};
}

/** A test card in one of the formats, and the copy of each of its planes out of the bytes. */
struct TestCardFrame
{
    Opal::DynamicArray<u8> bytes;
    Opal::DynamicArray<Rndr::Forge::BufferTextureCopyRegion> planes;
};

/**
 * The test card in BT.709 narrow range. NV12 is the Y plane then Cb,Cr pairs for each 2x2 block; P010 the same in 16-bit
 * words with the 10-bit value in the top bits; I420 the Y plane, then a plane of Cb, then a plane of Cr.
 */
TestCardFrame MakeTestCard(Rndr::PixelFormat format)
{
    constexpr f32 k_kr = 0.2126f;
    constexpr f32 k_kb = 0.0722f;
    const bool ten_bit = format == Rndr::PixelFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
    const bool three_planes = format == Rndr::PixelFormat::G8_B8_R8_3PLANE_420_UNORM;
    const u32 sample_size = ten_bit ? 2 : 1;
    // Narrow range: 16 to 235 for luma and 16 to 240 for chroma in 8 bits, four times that in 10.
    const f32 scale = ten_bit ? 4.0f : 1.0f;
    const u32 luma_bytes = k_frame_width * k_frame_height * sample_size;
    const u32 chroma_bytes = k_frame_width * k_frame_height / 4 * sample_size;

    TestCardFrame frame;
    frame.bytes = Opal::DynamicArray<u8>(luma_bytes + 2 * chroma_bytes);
    const auto put = [&frame, ten_bit](u32 offset, f32 code)
    {
        const u32 value = static_cast<u32>(code + 0.5f);
        if (!ten_bit)
        {
            frame.bytes[offset] = static_cast<u8>(value);
            return;
        }
        const u32 word = value << 6;
        frame.bytes[offset] = static_cast<u8>(word & 0xFF);
        frame.bytes[offset + 1] = static_cast<u8>(word >> 8);
    };
    const auto luma = [](const Rgb& c) { return k_kr * c.r + (1.0f - k_kr - k_kb) * c.g + k_kb * c.b; };

    for (u32 y = 0; y < k_frame_height; ++y)
    {
        for (u32 x = 0; x < k_frame_width; ++x)
        {
            put((y * k_frame_width + x) * sample_size, scale * (16.0f + 219.0f * luma(TestCard(x, y))));
        }
    }
    for (u32 y = 0; y < k_frame_height / 2; ++y)
    {
        for (u32 x = 0; x < k_frame_width / 2; ++x)
        {
            const Rgb c = TestCard(x * 2, y * 2);
            const f32 y_value = luma(c);
            const f32 cb = scale * (128.0f + 224.0f * (c.b - y_value) / (2.0f * (1.0f - k_kb)));
            const f32 cr = scale * (128.0f + 224.0f * (c.r - y_value) / (2.0f * (1.0f - k_kr)));
            const u32 index = y * (k_frame_width / 2) + x;
            if (three_planes)
            {
                put(luma_bytes + index, cb);
                put(luma_bytes + chroma_bytes + index, cr);
            }
            else
            {
                put(luma_bytes + index * 2 * sample_size, cb);
                put(luma_bytes + (index * 2 + 1) * sample_size, cr);
            }
        }
    }
    frame.planes.PushBack({.buffer_offset = 0, .texture_subresource = {.aspect_mask = Rndr::Forge::ImageAspectBits::Plane0}});
    frame.planes.PushBack({.buffer_offset = luma_bytes, .texture_subresource = {.aspect_mask = Rndr::Forge::ImageAspectBits::Plane1}});
    if (three_planes)
    {
        frame.planes.PushBack(
            {.buffer_offset = luma_bytes + chroma_bytes, .texture_subresource = {.aspect_mask = Rndr::Forge::ImageAspectBits::Plane2}});
    }
    return frame;
}

/**
 * The conversion gives R'G'B' as the video carries it, already gamma encoded, so it goes to a UNORM swap chain as it is:
 * an sRGB one would encode it a second time and wash it out.
 */
Rndr::Forge::SwapChainDesc MakeSwapChainDesc(const Rndr::Forge::Device& device, const Rndr::Forge::Surface& surface)
{
    const Rndr::Forge::SwapChainSupportDetails support = Require(surface.GetSwapChainSupportDetails(device.GetPhysicalDevice()));
    for (const Rndr::PixelFormat candidate : {Rndr::PixelFormat::B8G8R8A8_UNORM, Rndr::PixelFormat::R8G8B8A8_UNORM})
    {
        for (const VkSurfaceFormatKHR& offered : support.formats)
        {
            if (offered.format == Rndr::ToVkFormat(candidate) && offered.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return {.use_depth = false, .pixel_format = candidate};
            }
        }
    }
    Opal::HandleContractViolation("This surface offers neither B8G8R8A8_UNORM nor R8G8B8A8_UNORM.");
    return {};
}

struct FormatInfo
{
    Rndr::PixelFormat format;
    const char* key;
    const char* title;
};

constexpr FormatInfo k_formats[] = {
    {Rndr::PixelFormat::G8_B8R8_2PLANE_420_UNORM, "nv12", "Forge - video-frame - NV12, 8-bit, two planes - Space for the next format"},
    {Rndr::PixelFormat::G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, "p010",
     "Forge - video-frame - P010, 10-bit, two planes - Space for the next format"},
    {Rndr::PixelFormat::G8_B8_R8_3PLANE_420_UNORM, "i420", "Forge - video-frame - I420, 8-bit, three planes - Space for the next format"},
};
constexpr u32 k_format_count = sizeof(k_formats) / sizeof(k_formats[0]);

/**
 * Everything one format is drawn with. The conversion is baked into the sampler, the sampler into the layout and the
 * layout into the pipeline, so each format has its own of all four. Filled in place and never moved, since the view
 * and the layout refer to the texture and the sampler beside them.
 */
struct ShownFrame
{
    bool is_supported = false;
    Rndr::Forge::Texture texture;
    Rndr::Forge::SamplerYcbcrConversion conversion;
    Rndr::Forge::Sampler sampler;
    Rndr::Forge::TextureView view;
    Rndr::Forge::DescriptorSetLayout layout;
    Rndr::Forge::DescriptorSet set;
    Rndr::Forge::Pipeline pipeline;
};

void BuildFrame(ShownFrame& out, Rndr::Forge::Device& device, Rndr::Forge::DeviceQueue& queue, const Rndr::Forge::DescriptorPool& pool,
                const Rndr::Forge::Shader& vertex_shader, const Rndr::Forge::Shader& fragment_shader, Rndr::PixelFormat target_format,
                Rndr::PixelFormat format)
{
    // The frame, filled one plane at a time from one staging buffer holding them all.
    out.texture = Require(Rndr::Forge::Texture::Create(
        device, {.format = format,
                 .width = k_frame_width,
                 .height = k_frame_height,
                 .usage = Rndr::Forge::TextureUsageBits::Sampled | Rndr::Forge::TextureUsageBits::TransferDestination}));
    const TestCardFrame frame = MakeTestCard(format);
    const Rndr::Forge::Buffer staging = Require(Rndr::Forge::Buffer::Create(
        device, {.size = frame.bytes.GetSize(), .usage = Rndr::Forge::BufferUsageBits::TransferSource},
        {frame.bytes.GetData(), frame.bytes.GetSize()}));
    RequireOk(Rndr::Forge::ImmediateSubmit(
        device, queue,
        [&](Rndr::Forge::CommandBuffer& command_buffer)
        {
            RequireOk(command_buffer.CmdTextureBarrier(Rndr::Forge::TextureBarrier::ToTransferDestination(out.texture)));
            RequireOk(command_buffer.CmdCopyBufferToTexture(staging, out.texture, frame.planes));
            RequireOk(command_buffer.CmdTextureBarrier(
                Rndr::Forge::TextureBarrier::ToShaderRead(out.texture, Rndr::Forge::PipelineStageBits::FragmentShader)));
        }));

    // How the frame is read: the stream's matrix, range and siting. The sampler and the view carry the same conversion,
    // and the sampler goes into the layout as an immutable one, the only way Vulkan takes it.
    const Rndr::Forge::YcbcrFormatSupport support = device.GetPhysicalDevice().GetYcbcrFormatSupport(format);
    const Rndr::ImageFilter chroma_filter = support.linear_chroma_filter ? Rndr::ImageFilter::Linear : Rndr::ImageFilter::Nearest;
    out.conversion = Require(Rndr::Forge::SamplerYcbcrConversion::Create(device, {.format = format, .chroma_filter = chroma_filter}));
    out.sampler = Require(Rndr::Forge::Sampler::Create(device,
                                                       {.min_filter = chroma_filter,
                                                        .mag_filter = chroma_filter,
                                                        .address_mode_u = Rndr::ImageAddressMode::Clamp,
                                                        .address_mode_v = Rndr::ImageAddressMode::Clamp,
                                                        .address_mode_w = Rndr::ImageAddressMode::Clamp},
                                                       out.conversion));
    out.view = Require(Rndr::Forge::TextureView::Create(device, out.texture, out.conversion));

    Rndr::Forge::DescriptorSetLayoutDesc layout_desc;
    const Opal::Ref<const Rndr::Forge::Sampler> immutable(out.sampler);
    RequireOk(layout_desc.AddBinding(0, Rndr::Forge::DescriptorType::CombinedImageSampler, 1, Rndr::ShaderTypeBits::Fragment, {&immutable, 1}));
    out.layout = Require(Rndr::Forge::DescriptorSetLayout::Create(device, layout_desc));
    out.set = Require(Rndr::Forge::DescriptorSet::Create(pool, out.layout));
    RequireOk(out.set.Update(0, out.view, out.sampler));

    const Rndr::Forge::GraphicsPipelineDesc pipeline_desc{.vertex_shader = vertex_shader,
                                                          .fragment_shader = fragment_shader,
                                                          .descriptor_set_layouts = {out.layout},
                                                          .rasterizer = {.cull_mode = Rndr::Face::None},
                                                          .color_blend_attachments = {Rndr::Forge::ColorBlendDesc{}},
                                                          .color_attachment_formats = {target_format}};
    out.pipeline = Require(Rndr::Forge::Pipeline::Create(device, pipeline_desc));
    out.is_supported = true;
}

}  // namespace

int main(int argc, char** argv)
{
    u32 shown = 0;
    for (u32 i = 0; argc > 1 && i < k_format_count; ++i)
    {
        if (Opal::StringViewUtf8(argv[1]) == Opal::StringViewUtf8(k_formats[i].key))
        {
            shown = i;
        }
    }

    auto rndr_app = Require(Rndr::Application::Create({.enable_input_system = true}));
    auto window = Require(rndr_app->CreateGenericWindow({.width = static_cast<int>(k_frame_width), .height = static_cast<int>(k_frame_height)}));

    Rndr::Forge::GraphicsContext graphics_context = Require(Rndr::Forge::GraphicsContext::Create({.collect_debug_messages = true}));
    Rndr::Forge::Surface surface = Require(Rndr::Forge::Surface::Create(graphics_context, *window));
    const Rndr::Forge::DeviceDesc device_desc{.features = {.sampler_ycbcr_conversion = true}, .surface = surface};
    auto physical_devices = Require(graphics_context.EnumeratePhysicalDevices());
    Rndr::Forge::Device device = Require(Rndr::Forge::Device::Create(
        Require(Rndr::Forge::SelectPhysicalDevice(physical_devices, device_desc)), graphics_context, device_desc));
    Rndr::Forge::DeviceQueue& graphics_queue = Require(device.GetQueue(Rndr::Forge::QueueFamily::Graphics));
    Rndr::Forge::DeviceQueue& present_queue = Require(device.GetQueue(Rndr::Forge::QueueFamily::Present));
    Rndr::Forge::SwapChain swap_chain = Require(Rndr::Forge::SwapChain::Create(device, surface, MakeSwapChainDesc(device, surface)));
    Rndr::Forge::FrameContext frame_context =
        Require(Rndr::Forge::FrameContext::Create(device, swap_chain, graphics_queue, present_queue, {.frames_in_flight = 2}));

    const Rndr::Forge::Shader vertex_shader =
        Require(Rndr::Forge::Shader::FromSourceInMemory(device, k_shader_source, {.entry_point = "main_vertex"}));
    const Rndr::Forge::Shader fragment_shader =
        Require(Rndr::Forge::Shader::FromSourceInMemory(device, k_shader_source, {.entry_point = "main_fragment"}));

    // One set per format, each binding as many descriptors as the device says its format takes.
    u32 descriptor_count = 0;
    for (const FormatInfo& info : k_formats)
    {
        const u32 count = device.GetPhysicalDevice().GetYcbcrFormatSupport(info.format).combined_image_sampler_descriptor_count;
        descriptor_count += count > 0 ? count : 1;
    }
    Rndr::Forge::DescriptorPoolDesc pool_desc;
    RequireOk(pool_desc.Add(Rndr::Forge::DescriptorType::CombinedImageSampler, descriptor_count));
    pool_desc.max_sets = k_format_count;
    const Rndr::Forge::DescriptorPool pool = Require(Rndr::Forge::DescriptorPool::Create(device, pool_desc));

    ShownFrame frames[k_format_count];
    for (u32 i = 0; i < k_format_count; ++i)
    {
        const Rndr::Forge::YcbcrFormatSupport support = device.GetPhysicalDevice().GetYcbcrFormatSupport(k_formats[i].format);
        if (!support.sampled || !support.transfer_destination)
        {
            RNDR_LOG_WARNING("This device cannot sample and fill {}, so it is left out", k_formats[i].key);
            continue;
        }
        BuildFrame(frames[i], device, graphics_queue, pool, vertex_shader, fragment_shader, swap_chain.GetDesc().pixel_format,
                   k_formats[i].format);
    }
    const auto next_supported = [&frames](u32 from)
    {
        for (u32 step = 0; step < k_format_count; ++step)
        {
            const u32 candidate = (from + step) % k_format_count;
            if (frames[candidate].is_supported)
            {
                return candidate;
            }
        }
        Opal::HandleContractViolation("This device cannot sample any of the three formats.");
        return from;
    };
    shown = next_supported(shown);
    window->SetTitle(Opal::StringUtf8(k_formats[shown].title));

    Rndr::InputContext& input = rndr_app->GetInputSystemChecked().GetCurrentContext();
    input.AddAction("Next format")
        .GetValue()
        .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed)
        .OnButton(
            [&](Rndr::Trigger, bool)
            {
                shown = next_supported(shown + 1);
                window->SetTitle(Opal::StringUtf8(k_formats[shown].title));
            });
    input.AddAction("Exit").GetValue().Bind(Rndr::Key::Escape, Rndr::Trigger::Pressed).OnButton([&window](Rndr::Trigger, bool) {
        window->RequestClose();
    });

    while (!window->IsClosed())
    {
        rndr_app->ProcessSystemEvents(0);
        rndr_app->GetInputSystemChecked().ProcessSystemEvents(1.0f / 60.0f);
        if (Require(frame_context.BeginFrame()) != Rndr::Forge::SwapChainStatus::Success)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        const ShownFrame& frame = frames[shown];
        const Rndr::Vector2i render_size = frame_context.GetRenderSize();
        auto& command_buffer = Require(frame_context.GetCommandBuffer());
        RequireOk(command_buffer.CmdTextureBarrier(Require(Rndr::Forge::TextureBarrier::ToColorAttachment(Require(frame_context.GetColorTexture())))));
        const Rndr::Forge::RenderingDesc rendering_desc{
            .render_area_extent = render_size,
            .color_attachments = {Rndr::Forge::RenderingAttachmentDesc{.texture = Require(frame_context.GetColorTexture()),
                                                                       .load_operation = Rndr::Forge::AttachmentLoadOperation::DontCare,
                                                                       .store_operation = Rndr::Forge::AttachmentStoreOperation::Store}}};
        RequireOk(command_buffer.CmdBeginRendering(rendering_desc));
        RequireOk(command_buffer.CmdSetViewport(Rndr::Vector2f::Zero(), {static_cast<f32>(render_size.x), static_cast<f32>(render_size.y)}));
        RequireOk(command_buffer.CmdSetScissor(Rndr::Vector2i::Zero(), render_size));
        RequireOk(command_buffer.CmdBindPipeline(frame.pipeline));
        RequireOk(command_buffer.CmdBindDescriptorSet(frame.pipeline, frame.set));
        RequireOk(command_buffer.CmdDraw(3));
        RequireOk(command_buffer.CmdEndRendering());
        (void)Require(frame_context.EndFrame());
    }
    (void)device.WaitForAll();
    return 0;
}
