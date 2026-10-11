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
#include "rndr/types.hpp"

// One still video frame, NV12 as a decoder hands it out, drawn to the window through a sampler YCbCr conversion. The
// frame is made here rather than read from a file: colour bars over a grey ramp, converted to BT.709 narrow range on
// the CPU, which is what the conversion undoes on the GPU.

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

/** The test card as NV12, BT.709 narrow range: the Y plane, then the Cb and Cr pairs of each 2x2 block. */
Opal::DynamicArray<u8> MakeNv12TestCard()
{
    constexpr f32 k_kr = 0.2126f;
    constexpr f32 k_kb = 0.0722f;
    const auto luma = [](const Rgb& c) { return k_kr * c.r + (1.0f - k_kr - k_kb) * c.g + k_kb * c.b; };
    const auto code = [](f32 value) { return static_cast<u8>(value + 0.5f); };

    Opal::DynamicArray<u8> bytes(k_frame_width * k_frame_height * 3 / 2);
    for (u32 y = 0; y < k_frame_height; ++y)
    {
        for (u32 x = 0; x < k_frame_width; ++x)
        {
            bytes[y * k_frame_width + x] = code(16.0f + 219.0f * luma(TestCard(x, y)));
        }
    }
    u8* chroma = bytes.GetData() + k_frame_width * k_frame_height;
    for (u32 y = 0; y < k_frame_height / 2; ++y)
    {
        for (u32 x = 0; x < k_frame_width / 2; ++x)
        {
            const Rgb c = TestCard(x * 2, y * 2);
            const f32 y_value = luma(c);
            chroma[(y * k_frame_width / 2 + x) * 2] = code(128.0f + 224.0f * (c.b - y_value) / (2.0f * (1.0f - k_kb)));
            chroma[(y * k_frame_width / 2 + x) * 2 + 1] = code(128.0f + 224.0f * (c.r - y_value) / (2.0f * (1.0f - k_kr)));
        }
    }
    return bytes;
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

}  // namespace

int main()
{
    constexpr Rndr::PixelFormat k_format = Rndr::PixelFormat::G8_B8R8_2PLANE_420_UNORM;

    auto rndr_app = Require(Rndr::Application::Create({}));
    auto window = Require(rndr_app->CreateGenericWindow({.width = static_cast<int>(k_frame_width), .height = static_cast<int>(k_frame_height)}));
    window->SetTitle(Opal::StringUtf8("Forge - video-frame - NV12 through a YCbCr conversion"));

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

    // The frame, filled one plane at a time from one staging buffer: Y at the start, the chroma pairs after it.
    Rndr::Forge::Texture frame = Require(Rndr::Forge::Texture::Create(
        device, {.format = k_format,
                 .width = k_frame_width,
                 .height = k_frame_height,
                 .usage = Rndr::Forge::TextureUsageBits::Sampled | Rndr::Forge::TextureUsageBits::TransferDestination}));
    const Opal::DynamicArray<u8> bytes = MakeNv12TestCard();
    const Rndr::Forge::Buffer staging = Require(Rndr::Forge::Buffer::Create(
        device, {.size = bytes.GetSize(), .usage = Rndr::Forge::BufferUsageBits::TransferSource}, {bytes.GetData(), bytes.GetSize()}));
    const Rndr::Forge::BufferTextureCopyRegion planes[] = {
        {.buffer_offset = 0, .texture_subresource = {.aspect_mask = Rndr::Forge::ImageAspectBits::Plane0}},
        {.buffer_offset = k_frame_width * k_frame_height, .texture_subresource = {.aspect_mask = Rndr::Forge::ImageAspectBits::Plane1}}};
    RequireOk(Rndr::Forge::ImmediateSubmit(
        device, graphics_queue,
        [&](Rndr::Forge::CommandBuffer& command_buffer)
        {
            RequireOk(command_buffer.CmdTextureBarrier(Rndr::Forge::TextureBarrier::ToTransferDestination(frame)));
            RequireOk(command_buffer.CmdCopyBufferToTexture(staging, frame, {planes, 2}));
            RequireOk(command_buffer.CmdTextureBarrier(
                Rndr::Forge::TextureBarrier::ToShaderRead(frame, Rndr::Forge::PipelineStageBits::FragmentShader)));
        }));

    // How the frame is read: the stream's matrix, range and siting. The sampler and the view carry the same conversion,
    // and the sampler goes into the layout as an immutable one, the only way Vulkan takes it.
    const Rndr::Forge::YcbcrFormatSupport support = device.GetPhysicalDevice().GetYcbcrFormatSupport(k_format);
    const Rndr::ImageFilter chroma_filter = support.linear_chroma_filter ? Rndr::ImageFilter::Linear : Rndr::ImageFilter::Nearest;
    const Rndr::Forge::SamplerYcbcrConversion conversion =
        Require(Rndr::Forge::SamplerYcbcrConversion::Create(device, {.format = k_format, .chroma_filter = chroma_filter}));
    const Rndr::Forge::Sampler sampler = Require(Rndr::Forge::Sampler::Create(device,
                                                                              {.min_filter = chroma_filter,
                                                                               .mag_filter = chroma_filter,
                                                                               .address_mode_u = Rndr::ImageAddressMode::Clamp,
                                                                               .address_mode_v = Rndr::ImageAddressMode::Clamp,
                                                                               .address_mode_w = Rndr::ImageAddressMode::Clamp},
                                                                              conversion));
    const Rndr::Forge::TextureView view = Require(Rndr::Forge::TextureView::Create(device, frame, conversion));

    Rndr::Forge::DescriptorPoolDesc pool_desc;
    RequireOk(pool_desc.Add(Rndr::Forge::DescriptorType::CombinedImageSampler, support.combined_image_sampler_descriptor_count));
    pool_desc.max_sets = 1;
    const Rndr::Forge::DescriptorPool pool = Require(Rndr::Forge::DescriptorPool::Create(device, pool_desc));
    Rndr::Forge::DescriptorSetLayoutDesc layout_desc;
    const Opal::Ref<const Rndr::Forge::Sampler> immutable(sampler);
    RequireOk(layout_desc.AddBinding(0, Rndr::Forge::DescriptorType::CombinedImageSampler, 1, Rndr::ShaderTypeBits::Fragment, {&immutable, 1}));
    const Rndr::Forge::DescriptorSetLayout layout = Require(Rndr::Forge::DescriptorSetLayout::Create(device, layout_desc));
    Rndr::Forge::DescriptorSet set = Require(Rndr::Forge::DescriptorSet::Create(pool, layout));
    RequireOk(set.Update(0, view, sampler));

    const Rndr::Forge::Shader vertex_shader =
        Require(Rndr::Forge::Shader::FromSourceInMemory(device, k_shader_source, {.entry_point = "main_vertex"}));
    const Rndr::Forge::Shader fragment_shader =
        Require(Rndr::Forge::Shader::FromSourceInMemory(device, k_shader_source, {.entry_point = "main_fragment"}));
    const Rndr::Forge::GraphicsPipelineDesc pipeline_desc{.vertex_shader = vertex_shader,
                                                          .fragment_shader = fragment_shader,
                                                          .descriptor_set_layouts = {layout},
                                                          .rasterizer = {.cull_mode = Rndr::Face::None},
                                                          .color_blend_attachments = {Rndr::Forge::ColorBlendDesc{}},
                                                          .color_attachment_formats = {swap_chain.GetDesc().pixel_format}};
    const Rndr::Forge::Pipeline pipeline = Require(Rndr::Forge::Pipeline::Create(device, pipeline_desc));

    while (!window->IsClosed())
    {
        rndr_app->ProcessSystemEvents(0);
        if (Require(frame_context.BeginFrame()) != Rndr::Forge::SwapChainStatus::Success)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
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
        RequireOk(command_buffer.CmdBindPipeline(pipeline));
        RequireOk(command_buffer.CmdBindDescriptorSet(pipeline, set));
        RequireOk(command_buffer.CmdDraw(3));
        RequireOk(command_buffer.CmdEndRendering());
        (void)Require(frame_context.EndFrame());
    }
    (void)device.WaitForAll();
    return 0;
}
