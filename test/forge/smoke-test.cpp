#include <cstdlib>
#include <cstring>

#include <catch2/catch2.hpp>

#include "opal/file-system.h"
#include "opal/paths.h"

#include "rndr/core/shader-cache.hpp"
#include "rndr/core/shader-compiler.hpp"

#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/exceptions.h"

#include "rndr/bitmap.hpp"
#include "rndr/forge/buffer.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/debug.hpp"
#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/mesh.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/pipeline.hpp"
#include "rndr/forge/query.hpp"
#include "rndr/forge/shader.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/forge/transfer.hpp"
#include "rndr/trace.hpp"
#include "rndr/types.hpp"

#include "forge-test-common.hpp"

/**
 * Headless tests for Forge: no window, no surface, no swap chain, so they run anywhere a Vulkan device
 * exists. Every one of them ends in a readback and compares against a value computed on the CPU, since the
 * absence of a validation message says nothing about whether the device did the right thing.
 */

namespace
{

using namespace Rndr;

/**
 * One shader cache for the whole binary. Catch2 re-runs a TEST_CASE body once per SECTION, so a case with
 * nine sections compiles its shaders nine times; the in-memory tier turns all but the first into a lookup.
 * The directory turns the first one of a later run into a lookup as well.
 *
 * A static here rather than in the library, which has no globals of its own - this is the application, and
 * a cache that died with the fixture would be no cache at all.
 */
Rndr::ShaderCache& GetShaderCache()
{
    static Rndr::ShaderCache cache{ForgeTest::GetTestDataPath("shader-cache")};
    return cache;
}

/**
 * Which of the optional queue families a fixture's device asks for. Both off by default: Device reports when
 * a family it was asked for is not there, so a fixture that asked for them unconditionally would make every
 * test in this file skip on a machine whose one family does everything - and the message would say there was
 * no Vulkan device.
 */
struct ForgeQueues
{
    bool async_compute = false;
    bool dedicated_transfer = false;
};

/**
 * A device desc that asks for nothing this file does not need. DeviceDesc turns the async compute and the
 * dedicated transfer queue on by default, and Device reports when a family it was asked for is not there - so
 * a case that built a device without saying otherwise would fail outright on a machine whose one family does
 * everything, which is a legal Vulkan device and what a software driver offers. ForgeFixture takes a
 * ForgeQueues; every device built outside it goes through here.
 */
Forge::DeviceDesc MakeHeadlessDeviceDesc(const Forge::DeviceFeatures& features = {})
{
    return {.features = features, .use_async_compute_queue = false, .use_dedicated_transfer_queue = false};
}

/** A Vulkan instance and a device with no surface. Everything below is built on one of these. */
struct ForgeFixture
{
    Forge::GraphicsContext context;
    Forge::Device device;

    /**
     * What the machine said when this fixture asked for what it wanted, so the probes below can tell a
     * machine that cannot do this from one that can. Every case using a fixture has already skipped on a
     * machine that reported anything here.
     *
     * @param features What the device is asked to turn on. The default is what every test but the ones about
     *        a specific feature wants; asking for one this device lacks leaves the fixture empty.
     * @param queues Optional queue families to create alongside the graphics one. Asking for one this device
     *        does not have leaves the fixture empty as well.
     */
    ErrorCode status = ErrorCode::Success;

    explicit ForgeFixture(const Forge::DeviceFeatures& features = {}, const ForgeQueues& queues = {},
                          Opal::ArrayView<const char* const> extensions = {})
    {
        Opal::Expected<Forge::GraphicsContext, ErrorCode> context_result = Forge::GraphicsContext::Create(ForgeTest::TestContextDesc());
        if (!context_result.HasValue())
        {
            status = context_result.GetError();
            return;
        }
        context = std::move(context_result.GetValue());

        Opal::Expected<Opal::DynamicArray<Forge::PhysicalDevice>, ErrorCode> physical_devices = context.EnumeratePhysicalDevices();
        if (!physical_devices.HasValue())
        {
            status = physical_devices.GetError();
            return;
        }
        Forge::DeviceDesc device_desc{.features = features,
                                      .use_async_compute_queue = queues.async_compute,
                                      .use_dedicated_transfer_queue = queues.dedicated_transfer};
        for (const char* extension : extensions)
        {
            device_desc.extensions.PushBack(extension);
        }
        Opal::Expected<Forge::Device, ErrorCode> device_result =
            Forge::Device::Create(std::move(physical_devices.GetValue()[0]), context, device_desc);
        if (!device_result.HasValue())
        {
            status = device_result.GetError();
            return;
        }
        device = std::move(device_result.GetValue());
    }

    Forge::DeviceQueue& GetQueue(Forge::QueueFamily queue_family = Forge::QueueFamily::Graphics)
    {
        return ForgeTest::Unwrap(device.GetQueue(queue_family));
    }

    [[nodiscard]] Opal::StringUtf8 GetValidationErrors() const { return ForgeTest::CollectValidationErrors(context); }

    [[nodiscard]] u32 GetValidationErrorCount() const { return ForgeTest::CountValidationErrors(context); }

    /**
     * Release the device, which is where the layer names anything that outlived it. A leak has no other
     * witness: the object is a live Vulkan handle rather than a heap allocation, so the sanitizer never sees
     * it, and vkDestroyDevice runs after the last assertion of a case unless something asks for it early.
     * The context outlives the device, so the message is still collected when it arrives.
     */
    void DestroyDevice() { device.Destroy(); }
};

/**
 * Whether a device asking for these features and queues can be created on this machine. The one question
 * every probe in this file asks - a feature the device lacks, a queue family it has not got, an extension a
 * feature pulls in. Forge reports rather than falling back when asked for what is not there, so a fixture
 * that could not be built is the answer, and a case skips on it the way the whole file skips on a machine
 * with no device.
 *
 * Built and thrown away on every call. Catch2 re-runs a case body per section, so a case that asks at the
 * top pays a device creation per section, which is what every case already pays for its fixture.
 */
bool CanCreateDevice(const Forge::DeviceFeatures& features = {}, const ForgeQueues& queues = {},
                    Opal::ArrayView<const char* const> extensions = {})
{
    const ForgeFixture probe(features, queues, extensions);
    return probe.status == ErrorCode::Success;
}

/**
 * Whether this machine has a Vulkan device at all, so a machine without one skips rather than fails.
 *
 * This rests on EnumeratePhysicalDevices reporting NoGraphicsDevice when it finds none. While it handed back
 * an empty list, the probe reached `physical_devices[0]` on it and read off the end of the array - so the
 * one machine this function exists for is the one machine it did not work on.
 */
bool IsForgeAvailable()
{
    static const bool available = CanCreateDevice();
    return available;
}

/**
 * Whether the first physical device is a software one. Lavapipe reports every descriptor indexing feature
 * and then hands each invocation element zero of a descriptor array indexed non-uniformly, so a case that
 * asserts on which element was read has nothing to say there. Everything else in this file runs on it, which
 * is what makes a software driver worth pointing CI at.
 */
bool IsSoftwareDevice()
{
    static const bool software = []
    {
        const ForgeFixture probe;
        return probe.status == ErrorCode::Success &&
               probe.device.GetPhysicalDevice().GetProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
    }();
    return software;
}

/** Writes its own thread index plus a constant into a buffer named by its address, so every value is checkable. */
constexpr const char* k_compute_source = R"(
[shader("compute")]
[numthreads(64, 1, 1)]
void main_compute(uint3 thread_id : SV_DispatchThreadID, uniform uint32_t *output)
{
    output[thread_id.x] = thread_id.x + 1000;
}
)";

Opal::DynamicArray<u8> MakeBytes(i32 size, u8 seed)
{
    Opal::DynamicArray<u8> bytes(size);
    for (i32 i = 0; i < size; ++i)
    {
        bytes[i] = static_cast<u8>((i * 7 + seed) & 0xFF);
    }
    return bytes;
}

i32 CountMismatches(Opal::ArrayView<const u8> expected, Opal::ArrayView<const u8> actual)
{
    if (expected.GetSize() != actual.GetSize())
    {
        return static_cast<i32>(expected.GetSize());
    }
    i32 mismatches = 0;
    for (i32 i = 0; i < expected.GetSize(); ++i)
    {
        mismatches += expected[i] == actual[i] ? 0 : 1;
    }
    return mismatches;
}

/** The compute pipeline k_compute_source needs, which pushes the address it writes through. */
Forge::Pipeline MakeAddressPipeline(const Forge::Device& device, const Forge::Shader& shader)
{
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
    return ForgeTest::Unwrap(Forge::Pipeline::Create(device, pipeline_desc));
}

/**
 * Bind a compute pipeline and one set at slot zero, dispatch, and wait for it - what every case that reads
 * its output through a descriptor set rather than a device address records.
 */
void DispatchWithSet(const Forge::Device& device, Forge::DeviceQueue& queue, const Forge::Pipeline& pipeline, const Forge::DescriptorSet& set,
                     u32 group_count = 1)
{
    REQUIRE(Forge::ImmediateSubmit(device, queue,
                                   [&](Forge::CommandBuffer& command_buffer)
                                   {
                                       REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdDispatch(group_count) == ErrorCode::Success);
                                   }) == ErrorCode::Success);
}

/**
 * A storage buffer of that many u32, readable by the host and addressable by a shader, wiped so nothing a
 * previous run left in it can pass for a dispatch that ran.
 */
Forge::Buffer MakeWipedOutput(const Forge::Device& device, i32 element_count)
{
    Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = element_count * sizeof(u32),
                                                                            .usage = Forge::BufferUsageBits::StorageBuffer,
                                                                            .host_access = Forge::HostAccess::Random,
                                                                            .use_device_address = true}));
    const Opal::DynamicArray<u8> zeros(element_count * sizeof(u32));
    REQUIRE(output.Update(zeros) == ErrorCode::Success);
    return output;
}

/** Every element k_compute_source wrote into the buffer, read back and compared against what the shader computes. */
void RequireComputeWrote(const Forge::Buffer& output, i32 element_count)
{
    Opal::DynamicArray<u32> values(element_count);
    REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
    for (i32 i = 0; i < element_count; ++i)
    {
        INFO("element " << i);
        REQUIRE(values[i] == static_cast<u32>(i) + 1000);
    }
}

/**
 * Uploads bytes into one mip level of a texture, every array layer of that level packed one after another.
 *
 * @param reader The stage that reads the texture afterwards, which is the barrier the upload closes with.
 *        Compute by default, since that is what most of the cases here dispatch; a draw that samples in its
 *        fragment stage has to say so, or the barrier names a stage the read never happens in and the layer
 *        has nothing to object to. PipelineStageBits::None leaves the texture in TransferDestination, which
 *        is where a texture nothing samples belongs - ShaderReadOnly is a layout an image without the Sampled
 *        usage cannot be in.
 */
void UploadMip(const Forge::Device& device, Forge::DeviceQueue& queue, Forge::Texture& texture, Opal::ArrayView<const u8> pixels,
               u32 mip_level, Forge::PipelineStageBits reader = Forge::PipelineStageBits::ComputeShader)
{
    const Forge::Buffer staging = ForgeTest::Unwrap(
        Forge::Buffer::Create(device, {.size = pixels.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, pixels));
    const Forge::BufferTextureCopyRegion region{
        .texture_subresource = {.mip_level = mip_level, .array_layer_count = texture.GetDesc().array_layer_count}};
    REQUIRE(Forge::ImmediateSubmit(device, queue,
                                   [&](Forge::CommandBuffer& command_buffer)
                                   {
                                       REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(texture)) ==
                                               ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdCopyBufferToTexture(staging, texture, {&region, 1}) == ErrorCode::Success);
                                       if (reader != Forge::PipelineStageBits::None)
                                       {
                                           REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToShaderRead(texture, reader)) ==
                                                   ErrorCode::Success);
                                       }
                                   }) == ErrorCode::Success);
}

/**
 * One buffer filled by two command buffers, a half each, which is what the submit cases drive through the
 * queue in whichever batches they are about: a batch that dropped either one shows as half the buffer
 * missing rather than as nothing at all. Both command buffers are recorded and ended here, so a case only
 * decides how they are submitted and then asks whether both halves arrived.
 */
struct SplitCopy
{
    static constexpr i32 k_size = 64;

    Opal::DynamicArray<u8> first_half = MakeBytes(k_size, 5);
    Opal::DynamicArray<u8> second_half = MakeBytes(k_size, 90);
    Forge::Buffer source_a;
    Forge::Buffer source_b;
    Forge::Buffer destination;
    Forge::CommandBuffer first;
    Forge::CommandBuffer second;
    /** The same two as the one-element batches a SubmitDesc takes. */
    Opal::Ref<const Forge::CommandBuffer> first_batch[1];
    Opal::Ref<const Forge::CommandBuffer> second_batch[1];

    explicit SplitCopy(ForgeFixture& fixture)
    {
        constexpr Forge::BufferUsageBits k_both_ways =
            Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination;
        source_a = ForgeTest::Unwrap(
            Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, first_half));
        source_b = ForgeTest::Unwrap(
            Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, second_half));
        destination = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size * 2, .usage = k_both_ways}));
        const Opal::DynamicArray<u8> zeros(k_size * 2);
        REQUIRE(destination.Update(zeros) == ErrorCode::Success);

        first = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        second = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        const Forge::BufferCopyRegion first_region{.source_offset = 0, .destination_offset = 0, .size = k_size};
        const Forge::BufferCopyRegion second_region{.source_offset = 0, .destination_offset = k_size, .size = k_size};
        REQUIRE(first.Begin() == ErrorCode::Success);
        REQUIRE(first.CmdCopyBuffer(source_a, destination, {&first_region, 1}) == ErrorCode::Success);
        REQUIRE(first.End() == ErrorCode::Success);
        REQUIRE(second.Begin() == ErrorCode::Success);
        REQUIRE(second.CmdCopyBuffer(source_b, destination, {&second_region, 1}) == ErrorCode::Success);
        REQUIRE(second.End() == ErrorCode::Success);
        first_batch[0] = Opal::Ref<const Forge::CommandBuffer>(first);
        second_batch[0] = Opal::Ref<const Forge::CommandBuffer>(second);
    }

    /** Both halves in the destination, which is what every way of submitting the two has to end up with. */
    void RequireWholeBufferCopied(ForgeFixture& fixture) const
    {
        Opal::DynamicArray<u8> read_back(k_size * 2);
        REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), destination, read_back) == ErrorCode::Success);
        for (i32 i = 0; i < k_size; ++i)
        {
            REQUIRE(read_back[i] == first_half[i]);
            REQUIRE(read_back[k_size + i] == second_half[i]);
        }
    }
};

/**
 * A square colour texture read back whole, tightly packed. The layout defaults to TransferSource, which is
 * where every one of these cases leaves its texture; a case that samples its own readback afterward hands
 * ShaderReadOnly instead.
 */
Opal::DynamicArray<u8> ReadColorPixels(ForgeFixture& fixture, Forge::Texture& texture, i32 side,
                                       Forge::ImageLayout layout = Forge::ImageLayout::TransferSource)
{
    Opal::DynamicArray<u8> pixels(side * side * 4);
    REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0, layout) == ErrorCode::Success);
    return pixels;
}

/** A colour target these cases render into and read straight back out of. */
Forge::Texture MakeColorTarget(const Forge::Device& device, i32 side, PixelFormat format = PixelFormat::R8G8B8A8_UNORM)
{
    return ForgeTest::Unwrap(Forge::Texture::Create(device, {.format = format,
                                   .width = static_cast<u32>(side),
                                   .height = static_cast<u32>(side),
                                   .usage = Forge::TextureUsageBits::ColorAttachment |
                                            Forge::TextureUsageBits::TransferSource}));
}

/** A depth target of that format that can be read back, for the cases that look at what the depth test left. */
Forge::Texture MakeDepthTarget(const Forge::Device& device, i32 side, PixelFormat format = PixelFormat::D32_SFLOAT)
{
    return ForgeTest::Unwrap(Forge::Texture::Create(device, {.format = format,
                                                             .width = static_cast<u32>(side),
                                                             .height = static_cast<u32>(side),
                                                             .usage = Forge::TextureUsageBits::DepthStencilAttachment |
                                                                      Forge::TextureUsageBits::TransferSource}));
}

/**
 * Clear the target, run one recorded draw over it and hand back the pixels, left in TransferSource. The
 * viewport and the scissor are set to the whole target first, so a case that wants something else says so by
 * setting it again inside the draw.
 *
 * @param clear_color What the target is cleared to. Opaque red by default, which is what IsCovered reads a
 *        texel no fragment reached as.
 */
template <typename Record>
Opal::DynamicArray<u8> RenderRaster(ForgeFixture& fixture, Forge::Texture& color, i32 side, Record&& record,
                                    const Vector4f& clear_color = Vector4f{1.0f, 0.0f, 0.0f, 1.0f})
{
    REQUIRE(Forge::ImmediateSubmit(
                fixture.device, fixture.GetQueue(),
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                    const Forge::RenderingDesc rendering_desc{
                        .render_area_extent = {side, side},
                        .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                             .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                             .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                             .clear_value = clear_color}}};
                    REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {static_cast<f32>(side), static_cast<f32>(side)}) ==
                            ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {side, side}) == ErrorCode::Success);
                    record(command_buffer);
                    REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                }) == ErrorCode::Success);
    return ReadColorPixels(fixture, color, side);
}

/**
 * The pipeline desc of a draw over a two float position at location zero and one colour attachment, culling
 * nothing - the shape of every fullscreen triangle and quad here that reads nothing else. A case changes what
 * it is about on the desc this hands back.
 */
Forge::GraphicsPipelineDesc MakeFullscreenPipelineDesc(const Forge::Shader& vertex_shader, const Forge::Shader& fragment_shader,
                                                       PixelFormat format)
{
    Forge::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.vertex_shader = vertex_shader;
    pipeline_desc.fragment_shader = fragment_shader;
    pipeline_desc.rasterizer.cull_mode = Face::None;
    pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
    REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
    pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
    pipeline_desc.color_attachment_formats.PushBack(format);
    return pipeline_desc;
}

/** A source and a destination texture whose formats differ in texel size, for RecordMismatchedFormatCopy. */
struct MismatchedFormatTextures
{
    Forge::Texture source;
    Forge::Texture destination;
};

MismatchedFormatTextures MakeMismatchedFormatTextures(const Forge::Device& device)
{
    constexpr Forge::TextureUsageBits k_transfer_usage =
        Forge::TextureUsageBits::TransferSource | Forge::TextureUsageBits::TransferDestination;
    return {ForgeTest::Unwrap(Forge::Texture::Create(
                device, {.format = PixelFormat::R8G8B8A8_UNORM, .width = 4, .height = 4, .usage = k_transfer_usage})),
            ForgeTest::Unwrap(Forge::Texture::Create(
                device, {.format = PixelFormat::R16G16B16A16_SFLOAT, .width = 4, .height = 4, .usage = k_transfer_usage}))};
}

/**
 * Records and ends one command buffer copying between two mismatched texel sizes, which breaks a rule the
 * guards do not check and the validation layer does - the one way in this file to provoke exactly one
 * validation error at record time. Never submitted: work the layer rejects while recording is undefined
 * behaviour once it runs.
 */
void RecordMismatchedFormatCopy(const Forge::Device& device, Forge::DeviceQueue& queue, Forge::Texture& source,
                                Forge::Texture& destination)
{
    const Forge::TextureCopyRegion region;
    Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(device, queue));
    REQUIRE(command_buffer.Begin() == ErrorCode::Success);
    REQUIRE(command_buffer.CmdTransition(source, Forge::ImageLayout::TransferSource) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdTransition(destination, Forge::ImageLayout::TransferDestination) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdCopyTexture(source, destination, {&region, 1}) == ErrorCode::Success);
    REQUIRE(command_buffer.End() == ErrorCode::Success);
}

}  // namespace

/**
 * The one case here that refuses to skip. Every other case steps aside on a machine with no Vulkan device,
 * which is what makes a run that found none look exactly like a run that passed. The same goes for a build
 * without RNDR_FORGE_VALIDATION, where the context collects no debug messages and every
 * REQUIRE_NO_VALIDATION_ERROR below asserts on nothing. A machine that is meant to have both says so through
 * RNDR_TEST_REQUIRE_VULKAN, and then this fails instead of the file going quiet.
 */
TEST_CASE("Forge maps a VkResult to the error code it reports as", "[forge]")
{
    CHECK(Forge::VkResultToErrorCode(VK_SUCCESS) == ErrorCode::Success);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_OUT_OF_HOST_MEMORY) == ErrorCode::OutOfMemory);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_OUT_OF_DEVICE_MEMORY) == ErrorCode::OutOfMemory);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_OUT_OF_POOL_MEMORY) == ErrorCode::OutOfResources);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_FRAGMENTED_POOL) == ErrorCode::OutOfResources);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_FRAGMENTATION) == ErrorCode::OutOfResources);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_TOO_MANY_OBJECTS) == ErrorCode::OutOfResources);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_DEVICE_LOST) == ErrorCode::DeviceLost);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_SURFACE_LOST_KHR) == ErrorCode::DeviceLost);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_LAYER_NOT_PRESENT) == ErrorCode::FeatureNotSupported);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_EXTENSION_NOT_PRESENT) == ErrorCode::FeatureNotSupported);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_FEATURE_NOT_PRESENT) == ErrorCode::FeatureNotSupported);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_FORMAT_NOT_SUPPORTED) == ErrorCode::FeatureNotSupported);
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_INCOMPATIBLE_DRIVER) == ErrorCode::FeatureNotSupported);
    // A failure this switch has no case for still has to read as a failure, not fall through to Success.
    CHECK(Forge::VkResultToErrorCode(VK_ERROR_UNKNOWN) == ErrorCode::GraphicsAPIError);
    // A non-error result this switch has no case for - a positive VK_INCOMPLETE, say - is not a failure.
    CHECK(Forge::VkResultToErrorCode(VK_INCOMPLETE) == ErrorCode::Success);
}

/**
 * The flag enums and the sentinels mirror the Vulkan values they stand for, which is what lets the source
 * translate them with a cast rather than a switch - docs/forge.md and types.hpp both say so. PixelFormat is
 * the same promise made by ToVkFormat and FromVkFormat, which are casts too, and FromVkFormat is how
 * reflection reports what format a vertex attribute is.
 *
 * Checked at compile time, one line per enumerator, so an enumerator added with the wrong value, or a value
 * Vulkan renumbered, stops the build where it is rather than handing the driver something else.
 */
/** A function rather than a cast in the macro: some of the Vulkan values already are u64, and GCC flags a cast to
 * the type a value has. */
constexpr u64 MirrorValue(auto value)
{
    return static_cast<u64>(value);
}
#define RNDR_FORGE_MIRRORS(forge_value, vulkan_value) \
    static_assert(MirrorValue(forge_value) == MirrorValue(vulkan_value), #forge_value " is not " #vulkan_value)

RNDR_FORGE_MIRRORS(Forge::BufferUsageBits::TransferSource, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
RNDR_FORGE_MIRRORS(Forge::BufferUsageBits::TransferDestination, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
RNDR_FORGE_MIRRORS(Forge::BufferUsageBits::ConstantBuffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
RNDR_FORGE_MIRRORS(Forge::BufferUsageBits::StorageBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
RNDR_FORGE_MIRRORS(Forge::BufferUsageBits::IndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
RNDR_FORGE_MIRRORS(Forge::BufferUsageBits::VertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
RNDR_FORGE_MIRRORS(Forge::BufferUsageBits::IndirectBuffer, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

RNDR_FORGE_MIRRORS(Forge::TextureUsageBits::TransferSource, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
RNDR_FORGE_MIRRORS(Forge::TextureUsageBits::TransferDestination, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
RNDR_FORGE_MIRRORS(Forge::TextureUsageBits::Sampled, VK_IMAGE_USAGE_SAMPLED_BIT);
RNDR_FORGE_MIRRORS(Forge::TextureUsageBits::Storage, VK_IMAGE_USAGE_STORAGE_BIT);
RNDR_FORGE_MIRRORS(Forge::TextureUsageBits::ColorAttachment, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
RNDR_FORGE_MIRRORS(Forge::TextureUsageBits::DepthStencilAttachment, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
RNDR_FORGE_MIRRORS(Forge::TextureUsageBits::TransientAttachment, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
RNDR_FORGE_MIRRORS(Forge::TextureUsageBits::InputAttachment, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

RNDR_FORGE_MIRRORS(Forge::ColorWriteMaskBits::Red, VK_COLOR_COMPONENT_R_BIT);
RNDR_FORGE_MIRRORS(Forge::ColorWriteMaskBits::Green, VK_COLOR_COMPONENT_G_BIT);
RNDR_FORGE_MIRRORS(Forge::ColorWriteMaskBits::Blue, VK_COLOR_COMPONENT_B_BIT);
RNDR_FORGE_MIRRORS(Forge::ColorWriteMaskBits::Alpha, VK_COLOR_COMPONENT_A_BIT);

RNDR_FORGE_MIRRORS(Forge::StencilFaceBits::Front, VK_STENCIL_FACE_FRONT_BIT);
RNDR_FORGE_MIRRORS(Forge::StencilFaceBits::Back, VK_STENCIL_FACE_BACK_BIT);
RNDR_FORGE_MIRRORS(Forge::StencilFaceBits::FrontAndBack, VK_STENCIL_FACE_FRONT_AND_BACK);

RNDR_FORGE_MIRRORS(Forge::ImageLayout::Undefined, VK_IMAGE_LAYOUT_UNDEFINED);
RNDR_FORGE_MIRRORS(Forge::ImageLayout::General, VK_IMAGE_LAYOUT_GENERAL);
RNDR_FORGE_MIRRORS(Forge::ImageLayout::ColorAttachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
RNDR_FORGE_MIRRORS(Forge::ImageLayout::DepthStencilAttachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
RNDR_FORGE_MIRRORS(Forge::ImageLayout::DepthStencilReadOnly, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
RNDR_FORGE_MIRRORS(Forge::ImageLayout::ShaderReadOnly, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
RNDR_FORGE_MIRRORS(Forge::ImageLayout::TransferSource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
RNDR_FORGE_MIRRORS(Forge::ImageLayout::TransferDestination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
RNDR_FORGE_MIRRORS(Forge::ImageLayout::Present, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

RNDR_FORGE_MIRRORS(Forge::ImageAspectBits::Color, VK_IMAGE_ASPECT_COLOR_BIT);
RNDR_FORGE_MIRRORS(Forge::ImageAspectBits::Depth, VK_IMAGE_ASPECT_DEPTH_BIT);
RNDR_FORGE_MIRRORS(Forge::ImageAspectBits::Stencil, VK_IMAGE_ASPECT_STENCIL_BIT);

RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::None, VK_PIPELINE_STAGE_2_NONE);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::PipelineStart, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::IndirectDraw, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::VertexInput, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::VertexShader, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::TessellationControlShader, VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::TessellationEvaluationShader, VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::GeometryShader, VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::FragmentShader, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::EarlyFragmentTests, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::LateFragmentTests, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::ColorAttachmentOutput, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::ComputeShader, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::Transfer, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::PipelineEnd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::Host, VK_PIPELINE_STAGE_2_HOST_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::AllGraphics, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::AllCommands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::TaskShader, VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::MeshShader, VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::Copy, VK_PIPELINE_STAGE_2_COPY_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::Resolve, VK_PIPELINE_STAGE_2_RESOLVE_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::Blit, VK_PIPELINE_STAGE_2_BLIT_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::Clear, VK_PIPELINE_STAGE_2_CLEAR_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::IndexInput, VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::VertexAttributeInput, VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageBits::PreRasterizationShaders, VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT);

RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::None, VK_ACCESS_2_NONE);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::IndirectCommandRead, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::IndexRead, VK_ACCESS_2_INDEX_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::VertexAttributeRead, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::ConstantBufferRead, VK_ACCESS_2_UNIFORM_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::InputAttachmentRead, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::ShaderRead, VK_ACCESS_2_SHADER_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::ShaderWrite, VK_ACCESS_2_SHADER_WRITE_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::ColorAttachmentRead, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::ColorAttachmentWrite, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::DepthStencilAttachmentRead, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::DepthStencilAttachmentWrite, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::TransferRead, VK_ACCESS_2_TRANSFER_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::TransferWrite, VK_ACCESS_2_TRANSFER_WRITE_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::HostRead, VK_ACCESS_2_HOST_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::HostWrite, VK_ACCESS_2_HOST_WRITE_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::Read, VK_ACCESS_2_MEMORY_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::Write, VK_ACCESS_2_MEMORY_WRITE_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::ShaderSampledRead, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::ShaderStorageRead, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
RNDR_FORGE_MIRRORS(Forge::PipelineStageAccessBits::ShaderStorageWrite, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

RNDR_FORGE_MIRRORS(Forge::DependencyFlagBits::ByRegion, VK_DEPENDENCY_BY_REGION_BIT);

RNDR_FORGE_MIRRORS(Forge::DescriptorBindingFlagBits::UpdateAfterBind, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
RNDR_FORGE_MIRRORS(Forge::DescriptorBindingFlagBits::UpdateUnusedWhilePending, VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT);
RNDR_FORGE_MIRRORS(Forge::DescriptorBindingFlagBits::PartiallyBound, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
RNDR_FORGE_MIRRORS(Forge::DescriptorBindingFlagBits::VariableDescriptorCount, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);

RNDR_FORGE_MIRRORS(Forge::k_whole_buffer, VK_WHOLE_SIZE);
RNDR_FORGE_MIRRORS(Forge::k_all_mip_levels, VK_REMAINING_MIP_LEVELS);
RNDR_FORGE_MIRRORS(Forge::k_all_array_layers, VK_REMAINING_ARRAY_LAYERS);
RNDR_FORGE_MIRRORS(Forge::k_ignored_queue_family, VK_QUEUE_FAMILY_IGNORED);

RNDR_FORGE_MIRRORS(PixelFormat::R4G4_UNORM_PACK8, VK_FORMAT_R4G4_UNORM_PACK8);
RNDR_FORGE_MIRRORS(PixelFormat::R8_UNORM, VK_FORMAT_R8_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R8_SNORM, VK_FORMAT_R8_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R8_USCALED, VK_FORMAT_R8_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R8_SSCALED, VK_FORMAT_R8_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R8_UINT, VK_FORMAT_R8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R8_SINT, VK_FORMAT_R8_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R8_SRGB, VK_FORMAT_R8_SRGB);
RNDR_FORGE_MIRRORS(PixelFormat::R4G4B4A4_UNORM_PACK16, VK_FORMAT_R4G4B4A4_UNORM_PACK16);
RNDR_FORGE_MIRRORS(PixelFormat::B4G4R4A4_UNORM_PACK16, VK_FORMAT_B4G4R4A4_UNORM_PACK16);
RNDR_FORGE_MIRRORS(PixelFormat::R5G6B5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16);
RNDR_FORGE_MIRRORS(PixelFormat::B5G6R5_UNORM_PACK16, VK_FORMAT_B5G6R5_UNORM_PACK16);
RNDR_FORGE_MIRRORS(PixelFormat::R5G5B5A1_UNORM_PACK16, VK_FORMAT_R5G5B5A1_UNORM_PACK16);
RNDR_FORGE_MIRRORS(PixelFormat::B5G5R5A1_UNORM_PACK16, VK_FORMAT_B5G5R5A1_UNORM_PACK16);
RNDR_FORGE_MIRRORS(PixelFormat::A1R5G5B5_UNORM_PACK16, VK_FORMAT_A1R5G5B5_UNORM_PACK16);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8_UNORM, VK_FORMAT_R8G8_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8_SNORM, VK_FORMAT_R8G8_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8_USCALED, VK_FORMAT_R8G8_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8_SSCALED, VK_FORMAT_R8G8_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8_UINT, VK_FORMAT_R8G8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8_SINT, VK_FORMAT_R8G8_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8_SRGB, VK_FORMAT_R8G8_SRGB);
RNDR_FORGE_MIRRORS(PixelFormat::R16_UNORM, VK_FORMAT_R16_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R16_SNORM, VK_FORMAT_R16_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R16_USCALED, VK_FORMAT_R16_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R16_SSCALED, VK_FORMAT_R16_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R16_UINT, VK_FORMAT_R16_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R16_SINT, VK_FORMAT_R16_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R16_SFLOAT, VK_FORMAT_R16_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8_UNORM, VK_FORMAT_R8G8B8_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8_SNORM, VK_FORMAT_R8G8B8_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8_USCALED, VK_FORMAT_R8G8B8_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8_SSCALED, VK_FORMAT_R8G8B8_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8_UINT, VK_FORMAT_R8G8B8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8_SINT, VK_FORMAT_R8G8B8_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8_SRGB, VK_FORMAT_R8G8B8_SRGB);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8_UNORM, VK_FORMAT_B8G8R8_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8_SNORM, VK_FORMAT_B8G8R8_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8_USCALED, VK_FORMAT_B8G8R8_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8_SSCALED, VK_FORMAT_B8G8R8_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8_UINT, VK_FORMAT_B8G8R8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8_SINT, VK_FORMAT_B8G8R8_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8_SRGB, VK_FORMAT_B8G8R8_SRGB);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8A8_SNORM, VK_FORMAT_R8G8B8A8_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8A8_USCALED, VK_FORMAT_R8G8B8A8_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8A8_SSCALED, VK_FORMAT_R8G8B8A8_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8A8_UINT, VK_FORMAT_R8G8B8A8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8A8_SINT, VK_FORMAT_R8G8B8A8_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8A8_SNORM, VK_FORMAT_B8G8R8A8_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8A8_USCALED, VK_FORMAT_B8G8R8A8_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8A8_SSCALED, VK_FORMAT_B8G8R8A8_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8A8_UINT, VK_FORMAT_B8G8R8A8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8A8_SINT, VK_FORMAT_B8G8R8A8_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB);
RNDR_FORGE_MIRRORS(PixelFormat::A8B8G8R8_UNORM_PACK32, VK_FORMAT_A8B8G8R8_UNORM_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A8B8G8R8_SNORM_PACK32, VK_FORMAT_A8B8G8R8_SNORM_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A8B8G8R8_USCALED_PACK32, VK_FORMAT_A8B8G8R8_USCALED_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A8B8G8R8_SSCALED_PACK32, VK_FORMAT_A8B8G8R8_SSCALED_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A8B8G8R8_UINT_PACK32, VK_FORMAT_A8B8G8R8_UINT_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A8B8G8R8_SINT_PACK32, VK_FORMAT_A8B8G8R8_SINT_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A8B8G8R8_SRGB_PACK32, VK_FORMAT_A8B8G8R8_SRGB_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2R10G10B10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2R10G10B10_SNORM_PACK32, VK_FORMAT_A2R10G10B10_SNORM_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2R10G10B10_USCALED_PACK32, VK_FORMAT_A2R10G10B10_USCALED_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2R10G10B10_SSCALED_PACK32, VK_FORMAT_A2R10G10B10_SSCALED_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2R10G10B10_UINT_PACK32, VK_FORMAT_A2R10G10B10_UINT_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2R10G10B10_SINT_PACK32, VK_FORMAT_A2R10G10B10_SINT_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2B10G10R10_SNORM_PACK32, VK_FORMAT_A2B10G10R10_SNORM_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2B10G10R10_USCALED_PACK32, VK_FORMAT_A2B10G10R10_USCALED_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2B10G10R10_SSCALED_PACK32, VK_FORMAT_A2B10G10R10_SSCALED_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2B10G10R10_UINT_PACK32, VK_FORMAT_A2B10G10R10_UINT_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::A2B10G10R10_SINT_PACK32, VK_FORMAT_A2B10G10R10_SINT_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16_UNORM, VK_FORMAT_R16G16_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16_SNORM, VK_FORMAT_R16G16_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16_USCALED, VK_FORMAT_R16G16_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16_SSCALED, VK_FORMAT_R16G16_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16_UINT, VK_FORMAT_R16G16_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16_SINT, VK_FORMAT_R16G16_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16_SFLOAT, VK_FORMAT_R16G16_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R32_UINT, VK_FORMAT_R32_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R32_SINT, VK_FORMAT_R32_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R32_SFLOAT, VK_FORMAT_R32_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16_UNORM, VK_FORMAT_R16G16B16_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16_SNORM, VK_FORMAT_R16G16B16_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16_USCALED, VK_FORMAT_R16G16B16_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16_SSCALED, VK_FORMAT_R16G16B16_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16_UINT, VK_FORMAT_R16G16B16_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16_SINT, VK_FORMAT_R16G16B16_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16A16_UNORM, VK_FORMAT_R16G16B16A16_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16A16_SNORM, VK_FORMAT_R16G16B16A16_SNORM);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16A16_USCALED, VK_FORMAT_R16G16B16A16_USCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16A16_SSCALED, VK_FORMAT_R16G16B16A16_SSCALED);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16A16_UINT, VK_FORMAT_R16G16B16A16_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R32G32_UINT, VK_FORMAT_R32G32_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R32G32_SINT, VK_FORMAT_R32G32_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R32G32_SFLOAT, VK_FORMAT_R32G32_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R64_UINT, VK_FORMAT_R64_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R64_SINT, VK_FORMAT_R64_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R64_SFLOAT, VK_FORMAT_R64_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R32G32B32_UINT, VK_FORMAT_R32G32B32_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R32G32B32_SINT, VK_FORMAT_R32G32B32_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R64G64_UINT, VK_FORMAT_R64G64_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R64G64_SINT, VK_FORMAT_R64G64_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R64G64_SFLOAT, VK_FORMAT_R64G64_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R64G64B64_UINT, VK_FORMAT_R64G64B64_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R64G64B64_SINT, VK_FORMAT_R64G64B64_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R64G64B64_SFLOAT, VK_FORMAT_R64G64B64_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::R64G64B64A64_UINT, VK_FORMAT_R64G64B64A64_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::R64G64B64A64_SINT, VK_FORMAT_R64G64B64A64_SINT);
RNDR_FORGE_MIRRORS(PixelFormat::R64G64B64A64_SFLOAT, VK_FORMAT_R64G64B64A64_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::E5B9G9R9_UFLOAT_PACK32, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::D16_UNORM, VK_FORMAT_D16_UNORM);
RNDR_FORGE_MIRRORS(PixelFormat::X8_D24_UNORM_PACK32, VK_FORMAT_X8_D24_UNORM_PACK32);
RNDR_FORGE_MIRRORS(PixelFormat::D32_SFLOAT, VK_FORMAT_D32_SFLOAT);
RNDR_FORGE_MIRRORS(PixelFormat::S8_UINT, VK_FORMAT_S8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT);
RNDR_FORGE_MIRRORS(PixelFormat::BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC1_RGB_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC1_RGB_SRGB_BLOCK, VK_FORMAT_BC1_RGB_SRGB_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC1_RGBA_SRGB_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC2_UNORM_BLOCK, VK_FORMAT_BC2_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC2_SRGB_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC3_UNORM_BLOCK, VK_FORMAT_BC3_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC3_SRGB_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC4_UNORM_BLOCK, VK_FORMAT_BC4_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC4_SNORM_BLOCK, VK_FORMAT_BC4_SNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC5_UNORM_BLOCK, VK_FORMAT_BC5_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC5_SNORM_BLOCK, VK_FORMAT_BC5_SNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC6H_UFLOAT_BLOCK, VK_FORMAT_BC6H_UFLOAT_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC6H_SFLOAT_BLOCK, VK_FORMAT_BC6H_SFLOAT_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC7_UNORM_BLOCK, VK_FORMAT_BC7_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::BC7_SRGB_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::ETC2_R8G8B8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::ETC2_R8G8B8_SRGB_BLOCK, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::ETC2_R8G8B8A1_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK, VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::ETC2_R8G8B8A8_UNORM_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK, VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::EAC_R11_UNORM_BLOCK, VK_FORMAT_EAC_R11_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::EAC_R11_SNORM_BLOCK, VK_FORMAT_EAC_R11_SNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::EAC_R11G11_UNORM_BLOCK, VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
RNDR_FORGE_MIRRORS(PixelFormat::EAC_R11G11_SNORM_BLOCK, VK_FORMAT_EAC_R11G11_SNORM_BLOCK);

#undef RNDR_FORGE_MIRRORS

/**
 * The pure functions of the public headers, none of which had a test: VkResultToString, IsDepthFormat,
 * IsStencilFormat, ResolveAspectMask and ImageLayoutToString. Every one of them is a switch over a closed
 * set with no device to reach, so what is checked is the table rather than anything Vulkan does with it.
 */
TEST_CASE("Forge pure functions of the public headers", "[forge]")
{
    SECTION("VkResultToString names the result rather than falling back to unknown")
    {
        CHECK(strcmp(Forge::VkResultToString(VK_SUCCESS), "VK_SUCCESS") == 0);
        CHECK(strstr(Forge::VkResultToString(VK_ERROR_DEVICE_LOST), "VK_ERROR_DEVICE_LOST") != nullptr);
        CHECK(strstr(Forge::VkResultToString(VK_ERROR_OUT_OF_DATE_KHR), "VK_ERROR_OUT_OF_DATE_KHR") != nullptr);
        // A value this switch has no case for reads as unknown rather than as whichever case happens first.
        CHECK(strcmp(Forge::VkResultToString(VK_ERROR_UNKNOWN), "Unknown VkResult.") == 0);
    }
    SECTION("IsDepthFormat and IsStencilFormat agree with the table each is a switch over")
    {
        CHECK(IsDepthFormat(PixelFormat::D32_SFLOAT));
        CHECK(IsDepthFormat(PixelFormat::D16_UNORM_S8_UINT));
        CHECK_FALSE(IsDepthFormat(PixelFormat::S8_UINT));
        CHECK_FALSE(IsDepthFormat(PixelFormat::R8G8B8A8_UNORM));

        CHECK(IsStencilFormat(PixelFormat::S8_UINT));
        CHECK(IsStencilFormat(PixelFormat::D24_UNORM_S8_UINT));
        CHECK_FALSE(IsStencilFormat(PixelFormat::D32_SFLOAT));
        CHECK_FALSE(IsStencilFormat(PixelFormat::R8G8B8A8_UNORM));

        // The combined formats carry both.
        CHECK(IsDepthFormat(PixelFormat::D32_SFLOAT_S8_UINT));
        CHECK(IsStencilFormat(PixelFormat::D32_SFLOAT_S8_UINT));
    }
    SECTION("ResolveAspectMask derives the aspect from the format only when none was named")
    {
        // An explicit mask passes through unchanged, whatever the format says.
        CHECK(Forge::ResolveAspectMask(Forge::ImageAspectBits::Color, PixelFormat::D32_SFLOAT) == Forge::ImageAspectBits::Color);

        // None derives it: colour for a format with neither, depth for a depth-only format, both for a
        // combined one, and stencil alone for the one format that carries it without a depth aspect.
        CHECK(Forge::ResolveAspectMask(Forge::ImageAspectBits::None, PixelFormat::R8G8B8A8_UNORM) == Forge::ImageAspectBits::Color);
        CHECK(Forge::ResolveAspectMask(Forge::ImageAspectBits::None, PixelFormat::D32_SFLOAT) == Forge::ImageAspectBits::Depth);
        CHECK(Forge::ResolveAspectMask(Forge::ImageAspectBits::None, PixelFormat::D24_UNORM_S8_UINT) ==
              (Forge::ImageAspectBits::Depth | Forge::ImageAspectBits::Stencil));
        CHECK(Forge::ResolveAspectMask(Forge::ImageAspectBits::None, PixelFormat::S8_UINT) == Forge::ImageAspectBits::Stencil);
    }
    SECTION("ImageLayoutToString names every layout rather than falling back to unknown")
    {
        CHECK(strcmp(Forge::ImageLayoutToString(Forge::ImageLayout::Undefined), "Undefined") == 0);
        CHECK(strcmp(Forge::ImageLayoutToString(Forge::ImageLayout::ColorAttachment), "ColorAttachment") == 0);
        CHECK(strcmp(Forge::ImageLayoutToString(Forge::ImageLayout::DepthStencilReadOnly), "DepthStencilReadOnly") == 0);
        CHECK(strcmp(Forge::ImageLayoutToString(Forge::ImageLayout::Present), "Present") == 0);
        // A value this switch has no case for reads as unknown rather than as whichever case happens first.
        CHECK(strcmp(Forge::ImageLayoutToString(static_cast<Forge::ImageLayout>(99)), "an unknown layout") == 0);
    }
}

TEST_CASE("Forge queue family indices report what was put in them", "[forge]")
{
    Forge::QueueFamilyIndices indices;
    REQUIRE(indices.GetValidQueueFamilies().IsEmpty());
    REQUIRE(indices.GetQueueFamilyIndex(Forge::QueueFamily::Graphics) == Forge::QueueFamilyIndices::k_invalid_index);
    // A value naming no family - EnumCount is not a family - has no index either.
    REQUIRE(indices.GetQueueFamilyIndex(Forge::QueueFamily::EnumCount) == Forge::QueueFamilyIndices::k_invalid_index);

    indices.graphics_family = 0;
    indices.present_family = 0;  // Shares the graphics family's index.
    indices.compute_family = 1;
    indices.transfer_family = 2;

    REQUIRE(indices.GetQueueFamilyIndex(Forge::QueueFamily::Graphics) == 0);
    REQUIRE(indices.GetQueueFamilyIndex(Forge::QueueFamily::Present) == 0);
    REQUIRE(indices.GetQueueFamilyIndex(Forge::QueueFamily::AsyncCompute) == 1);
    REQUIRE(indices.GetQueueFamilyIndex(Forge::QueueFamily::Transfer) == 2);
    REQUIRE(indices.GetQueueFamilyIndex(Forge::QueueFamily::Decode) == Forge::QueueFamilyIndices::k_invalid_index);

    const Opal::DynamicArray<u32> valid = indices.GetValidQueueFamilies();
    // Four families named, two of them (graphics and present) sharing an index, so three distinct entries.
    REQUIRE(valid.GetSize() == 3);
    REQUIRE(valid.Contains(0));
    REQUIRE(valid.Contains(1));
    REQUIRE(valid.Contains(2));
}

TEST_CASE("Forge has the device the environment says it has to have", "[forge]")
{
    if (!ForgeTest::IsEnvironmentFlagSet("RNDR_TEST_REQUIRE_VULKAN"))
    {
        SKIP("RNDR_TEST_REQUIRE_VULKAN is unset, so a machine with no Vulkan device is allowed here.");
    }
    REQUIRE(IsForgeAvailable());
#if !defined(RNDR_FORGE_VALIDATION)
    FAIL("Built without RNDR_FORGE_VALIDATION, so there is no layer behind any of the validation assertions.");
#endif
}

TEST_CASE("Forge context and device", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
    REQUIRE(context.IsValid());

    // Never empty: a machine with no device reports NoGraphicsDevice rather than handing back a list with
    // nothing in it, which is what lets every caller in this file index the first element without checking.
    // The machine that would have exercised that code is the one that skips this whole case, so what is
    // asserted here is the contract rather than the branch.
    Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
    REQUIRE_FALSE(physical_devices.IsEmpty());

    Forge::Device device = ForgeTest::Unwrap(Forge::Device::Create(std::move(physical_devices[0]), context, MakeHeadlessDeviceDesc()));
    REQUIRE(device.IsValid());
    REQUIRE(ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics)).IsValid());
    REQUIRE_NO_VALIDATION_ERROR_IN(context);
}

TEST_CASE("Forge context desc", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    SECTION("A required instance extension that is not supported is refused")
    {
        Forge::GraphicsContextDesc desc = ForgeTest::TestContextDesc();
        desc.required_instance_extensions.PushBack(Opal::StringUtf8("VK_EXT_this_extension_does_not_exist"));
        Opal::Expected<Forge::GraphicsContext, ErrorCode> result = Forge::GraphicsContext::Create(desc);
        REQUIRE_FALSE(result.HasValue());
        REQUIRE(result.GetError() == ErrorCode::FeatureNotSupported);
    }
    SECTION("max_stored_debug_messages caps the storage, not the count")
    {
        Forge::GraphicsContextDesc desc = ForgeTest::TestContextDesc();
        desc.max_stored_debug_messages = 1;
        Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(desc));
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
        Forge::Device device =
            ForgeTest::Unwrap(Forge::Device::Create(std::move(physical_devices[0]), context, MakeHeadlessDeviceDesc()));
        Forge::DeviceQueue& queue = ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics));

        // Copying between two formats of different texel size is what "Forge debug names reach the
        // validation layer" below uses to provoke one validation error at record time; recorded three times
        // over, it provokes three.
        MismatchedFormatTextures textures = MakeMismatchedFormatTextures(device);
        constexpr i32 k_error_count = 3;
        for (i32 i = 0; i < k_error_count; ++i)
        {
            RecordMismatchedFormatCopy(device, queue, textures.source, textures.destination);
        }

        REQUIRE(context.GetDebugMessages().GetSize() == 1);
        REQUIRE(ForgeTest::Unwrap(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation)) >=
               k_error_count);

        // The errors above were the point of this section, so they must not reach the assertion below.
        context.ClearDebugMessages();
        REQUIRE_NO_VALIDATION_ERROR_IN(context);
    }
    SECTION("GetDebugMessageCount refuses a severity that is none of the three")
    {
        const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
        const auto bad_severity = static_cast<Forge::DebugMessageSeverity>(99);
        Opal::Expected<u32, ErrorCode> result = context.GetDebugMessageCount(bad_severity);
        REQUIRE_FALSE(result.HasValue());
        REQUIRE(result.GetError() == ErrorCode::InvalidArgument);
        REQUIRE_NO_VALIDATION_ERROR_IN(context);
    }
    SECTION("GetDebugMessageCount counts Info messages that GetDebugMessages never stores")
    {
        // Info is one of the three real severities, so it is accepted rather than refused the way 99 is -
        // and docs/forge.md says only the count survives for it, which is what the second half checks: every
        // message this context has stored, if any, is a Warning or an Error and never an Info.
        const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
        // Accepted rather than refused - ForgeTest::Unwrap already fails the case if it were not.
        (void)ForgeTest::Unwrap(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Info));
        for (const Forge::DebugMessage& message : context.GetDebugMessages())
        {
            REQUIRE(message.severity != Forge::DebugMessageSeverity::Info);
        }
    }
}

TEST_CASE("Forge context outlives a second one", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    {
        // volk loads one set of function pointers for the whole process. This second context used to null
        // every one of them on the way out, so the fixture above died on its next Vulkan call.
        const ForgeFixture second;
        REQUIRE(second.context.IsValid());
        REQUIRE(second.device.IsValid());
    }
    // Two calls through what the second context used to unload: one instance level, one device level.
    const Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(fixture.context.EnumeratePhysicalDevices());
    REQUIRE_FALSE(physical_devices.IsEmpty());
    const Forge::Buffer buffer =
        ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = 32, .usage = Forge::BufferUsageBits::TransferDestination}));
    REQUIRE(buffer.IsValid());
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge buffer update and read", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 256;
    constexpr i32 k_offset = 64;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size - k_offset, 3);

    Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size,
                                          .usage = Forge::BufferUsageBits::TransferSource,
                                          .host_access = Forge::HostAccess::Random}));
    const Opal::DynamicArray<u8> zeros(k_size);
    REQUIRE(buffer.Update(zeros) == ErrorCode::Success);
    REQUIRE(buffer.Update(written, k_offset) == ErrorCode::Success);

    Opal::DynamicArray<u8> read_back(k_size - k_offset);
    REQUIRE(buffer.Read(read_back, k_offset) == ErrorCode::Success);
    REQUIRE(CountMismatches(written, read_back) == 0);

    // The bytes before the offset must be untouched, which is what makes this a test of the offset rather
    // than of the write.
    Opal::DynamicArray<u8> head(k_offset);
    REQUIRE(buffer.Read(head, 0) == ErrorCode::Success);
    for (i32 i = 0; i < k_offset; ++i)
    {
        REQUIRE(head[i] == 0);
    }

    SECTION("A write that does not fit is refused")
    {
        REQUIRE(buffer.Update(written, k_size - 1) == ErrorCode::OutOfBounds);
    }
    SECTION("A read of write-combined memory is refused")
    {
        const Forge::Buffer write_only =
            ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}));
        Opal::DynamicArray<u8> out(k_size);
        REQUIRE(write_only.Read(out) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge buffer copy and readback", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 256;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 29);

    const Forge::Buffer source = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written));
    const Forge::Buffer destination = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device,
        {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination}));
    const Opal::DynamicArray<u8> zeros(k_size);
    REQUIRE(destination.Update(zeros) == ErrorCode::Success);

    Opal::DynamicArray<u8> read_back(k_size);
    REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), destination, read_back) == ErrorCode::Success);
    REQUIRE(CountMismatches(zeros, read_back) == 0);

    REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(), [&](Forge::CommandBuffer& command_buffer)
                                   { REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success); }) ==
            ErrorCode::Success);
    REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), destination, read_back) == ErrorCode::Success);
    REQUIRE(CountMismatches(written, read_back) == 0);

    SECTION("A copy from a buffer without the transfer usage is refused")
    {
        const Forge::Buffer no_transfer =
            ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::ConstantBuffer}));
        // The submit itself is fine: what the recorder reported is the copy's own answer, and nothing was
        // recorded to run.
        ErrorCode copy_status = ErrorCode::Success;
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       { copy_status = command_buffer.CmdCopyBuffer(no_transfer, destination); }) == ErrorCode::Success);
        REQUIRE(copy_status == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * CmdFillBuffer and CmdUpdateBuffer. The buffer starts out holding a pattern and a size that is not a multiple
 * of four, and every case reads all of it back: what was written has to land where it was asked to and
 * nowhere else, which a range off by one word shows as a mismatch at either edge.
 */
TEST_CASE("Forge buffer fill and update from the command stream", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 62;
    const Opal::DynamicArray<u8> pattern = MakeBytes(k_size, 17);
    const Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination},
        pattern));
    Opal::DynamicArray<u8> expected(pattern.GetData(), pattern.GetSize());

    /** The pattern with the given bytes put over it from an offset, the way the device should have left it. */
    auto write_expected = [&](u64 offset, Opal::ArrayView<const u8> bytes)
    {
        for (u64 i = 0; i < bytes.GetSize(); ++i)
        {
            expected[offset + i] = bytes[i];
        }
    };
    auto require_contents = [&]()
    {
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), buffer, read_back) == ErrorCode::Success);
        for (i32 i = 0; i < k_size; ++i)
        {
            INFO("byte " << i << " expected " << +expected[i] << ", got " << +read_back[i]);
            REQUIRE(read_back[i] == expected[i]);
        }
    };

    SECTION("A fill writes its value over the range and leaves the bytes either side alone")
    {
        // Four distinct bytes, so a value written in the wrong byte order does not read back the same.
        constexpr u32 k_value = 0x04030201;
        constexpr u8 k_value_bytes[] = {0x01, 0x02, 0x03, 0x04};
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(), [&](Forge::CommandBuffer& command_buffer)
                                       { REQUIRE(command_buffer.CmdFillBuffer(buffer, k_value, 8, 16) == ErrorCode::Success); }) ==
                ErrorCode::Success);
        for (u64 offset = 8; offset < 24; offset += 4)
        {
            write_expected(offset, k_value_bytes);
        }
        require_contents();
    }
    SECTION("A fill to the end stops at the last whole word")
    {
        // 62 bytes from 52 leave ten: two words are filled and the two bytes after them are not.
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(), [&](Forge::CommandBuffer& command_buffer)
                                       { REQUIRE(command_buffer.CmdFillBuffer(buffer, 0, 52) == ErrorCode::Success); }) ==
                ErrorCode::Success);
        const u8 zeros[8] = {};
        write_expected(52, zeros);
        require_contents();
    }
    SECTION("An update lands its bytes at its offset, copied when it was recorded")
    {
        const u8 written[] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB};
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           // Gone before the command buffer is submitted, so what runs can only be
                                           // the copy the recording took.
                                           Opal::DynamicArray<u8> data(written, sizeof(written));
                                           REQUIRE(command_buffer.CmdUpdateBuffer(buffer, data, 20) == ErrorCode::Success);
                                           for (u64 i = 0; i < data.GetSize(); ++i)
                                           {
                                               data[i] = 0;
                                           }
                                       }) == ErrorCode::Success);
        write_expected(20, written);
        require_contents();
    }
    SECTION("A fill or an update the buffer cannot take is refused")
    {
        const Forge::Buffer no_transfer =
            ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = 64, .usage = Forge::BufferUsageBits::StorageBuffer}));
        const Opal::DynamicArray<u8> eight(8);
        const Opal::DynamicArray<u8> six(6);
        const Opal::DynamicArray<u8> too_large(65540);
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);

        REQUIRE(command_buffer.CmdFillBuffer(no_transfer, 0) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdFillBuffer(buffer, 0, 2, 4) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdFillBuffer(buffer, 0, 0, 6) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdFillBuffer(buffer, 0, 0, 0) == ErrorCode::OutOfBounds);
        REQUIRE(command_buffer.CmdFillBuffer(buffer, 0, 56, 8) == ErrorCode::OutOfBounds);
        REQUIRE(command_buffer.CmdFillBuffer(buffer, 0, 64, 4) == ErrorCode::OutOfBounds);
        // Two bytes past offset 60 hold no whole word, so filling to the end would fill nothing.
        REQUIRE(command_buffer.CmdFillBuffer(buffer, 0, 60) == ErrorCode::OutOfBounds);

        REQUIRE(command_buffer.CmdUpdateBuffer(no_transfer, eight) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdUpdateBuffer(buffer, eight, 2) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdUpdateBuffer(buffer, six) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdUpdateBuffer(buffer, {}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdUpdateBuffer(buffer, too_large) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdUpdateBuffer(buffer, eight, 56) == ErrorCode::OutOfBounds);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge buffer edges", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 256;

    SECTION("Create refuses initial data larger than the buffer")
    {
        // A range that does not fit, which docs/forge.md says is OutOfBounds rather than InvalidArgument.
        const Opal::DynamicArray<u8> too_much = MakeBytes(k_size + 1, 9);
        REQUIRE(Forge::Buffer::Create(fixture.device,
                                      {.size = k_size,
                                       .usage = Forge::BufferUsageBits::TransferSource,
                                       .host_access = Forge::HostAccess::Random},
                                      too_much)
                    .GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
    }
    SECTION("keep_memory_mapped = false still round-trips through Update and Read")
    {
        // Nothing stays mapped between calls, so this is the map-write-unmap and map-invalidate-read-unmap
        // path rather than the memcpy into memory already mapped that every other case here takes.
        const Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device,
            {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource, .host_access = Forge::HostAccess::Random, .keep_memory_mapped = false}));
        const Opal::DynamicArray<u8> written = MakeBytes(k_size, 41);
        REQUIRE(buffer.Update(written) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(buffer.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("A read that does not fit is OutOfBounds")
    {
        const Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource, .host_access = Forge::HostAccess::Random}));
        Opal::DynamicArray<u8> too_much(k_size + 1);
        REQUIRE(buffer.Read(too_much) == ErrorCode::OutOfBounds);
        Opal::DynamicArray<u8> at_the_edge(1);
        REQUIRE(buffer.Read(at_the_edge, k_size) == ErrorCode::OutOfBounds);
    }
    SECTION("UploadToBuffer and ReadBackBuffer honour their offset")
    {
        constexpr i32 k_offset = 64;
        const Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device, {.size = k_size,
                            .usage = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination,
                            .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(k_size);
        REQUIRE(buffer.Update(zeros) == ErrorCode::Success);

        const Opal::DynamicArray<u8> written = MakeBytes(k_size - k_offset, 7);
        REQUIRE(Forge::UploadToBuffer(fixture.device, fixture.GetQueue(), buffer, written, k_offset) == ErrorCode::Success);

        Opal::DynamicArray<u8> head(k_offset);
        REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), buffer, head, 0) == ErrorCode::Success);
        for (i32 i = 0; i < k_offset; ++i)
        {
            REQUIRE(head[i] == 0);
        }
        Opal::DynamicArray<u8> tail(k_size - k_offset);
        REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), buffer, tail, k_offset) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, tail) == 0);
    }
    SECTION("A CmdCopyBuffer region past the end of either buffer is refused")
    {
        const Forge::Buffer source =
            ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}));
        const Forge::Buffer destination = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device,
            {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination}));
        const Forge::BufferCopyRegion past_the_end{.source_offset = 0, .destination_offset = 0, .size = k_size + 1};
        ErrorCode copy_status = ErrorCode::Success;
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       { copy_status = command_buffer.CmdCopyBuffer(source, destination, {&past_the_end, 1}); }) ==
                ErrorCode::Success);
        REQUIRE(copy_status == ErrorCode::OutOfBounds);
    }
    SECTION("The default CmdCopyBuffer overload copies only as much as the smaller buffer has")
    {
        constexpr i32 k_small_size = 64;
        const Opal::DynamicArray<u8> written = MakeBytes(k_size, 53);
        const Forge::Buffer source = ForgeTest::Unwrap(
            Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written));
        Forge::Buffer destination = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device,
            {.size = k_small_size,
             .usage = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination,
             .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(k_small_size);
        REQUIRE(destination.Update(zeros) == ErrorCode::Success);

        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       { REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success); }) ==
                ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_small_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        const Opal::DynamicArray<u8> expected(written.GetData(), k_small_size);
        REQUIRE(CountMismatches(expected, read_back) == 0);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge texture upload, mip generation and readback", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    if (!fixture.device.GetPhysicalDevice().SupportsBlit(k_format, true) ||
        !fixture.device.GetPhysicalDevice().SupportsBlit(k_format, false))
    {
        SKIP("This device cannot blit R8G8B8A8_UNORM, so it cannot generate mips for one.");
    }
    constexpr i32 k_side = 8;
    constexpr u32 k_mip_count = 4;  // 8 -> 4 -> 2 -> 1
    // Every texel the same value: a box filter of a constant is that constant at every level, so the expected
    // result is exact whatever filtering the driver picked.
    constexpr u8 k_texel[4] = {200, 100, 50, 255};
    Opal::DynamicArray<u8> mip0(k_side * k_side * 4);
    for (i32 i = 0; i < mip0.GetSize(); ++i)
    {
        mip0[i] = k_texel[i % 4];
    }

    Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                            .width = k_side,
                                            .height = k_side,
                                            .mip_level_count = k_mip_count,
                                            .usage = Forge::TextureUsageBits::TransferSource |
                                                     Forge::TextureUsageBits::TransferDestination |
                                                     Forge::TextureUsageBits::Sampled}));
    const Forge::Buffer staging = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = mip0.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, mip0));
    const Forge::BufferTextureCopyRegion mip0_region;
    REQUIRE(Forge::ImmediateSubmit(
                fixture.device, fixture.GetQueue(),
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(texture)) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdCopyBufferToTexture(staging, texture, {&mip0_region, 1}) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdGenerateMips(texture, Forge::ImageLayout::TransferDestination) == ErrorCode::Success);
                }) == ErrorCode::Success);
    // Nothing above named a source layout: the mip chain reads each level off the texture as it goes, and
    // leaves every one of them in the layout it was told to finish in.
    REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferDestination);

    for (u32 level = 0; level < k_mip_count; ++level)
    {
        const i32 side = k_side >> level;
        Opal::DynamicArray<u8> level_pixels(side * side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, level_pixels, level,
                                       Forge::ImageLayout::TransferDestination) == ErrorCode::Success);
        INFO("mip level " << level);
        for (i32 i = 0; i < level_pixels.GetSize(); ++i)
        {
            REQUIRE(level_pixels[i] == k_texel[i % 4]);
        }
    }

    SECTION("Reading back into a view of the wrong size is refused")
    {
        Opal::DynamicArray<u8> too_small(4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, too_small, 0,
                                       Forge::ImageLayout::TransferDestination) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** A bitmap of that size whose every pixel differs from every other, so a copy that shifted shows up. */
Bitmap MakeGradientBitmap(i32 width, i32 height)
{
    Opal::DynamicArray<u8> pixels(width * height * 4);
    for (i32 pixel = 0; pixel < width * height; ++pixel)
    {
        pixels[pixel * 4 + 0] = static_cast<u8>(pixel * 16);
        pixels[pixel * 4 + 1] = static_cast<u8>(255 - pixel * 16);
        pixels[pixel * 4 + 2] = static_cast<u8>(pixel * 8);
        pixels[pixel * 4 + 3] = 255;
    }
    return ForgeTest::Unwrap(Bitmap::Create(width, height, 1, PixelFormat::R8G8B8A8_UNORM, 1, {pixels.GetData(), pixels.GetSize()}));
}

}  // namespace

TEST_CASE("Forge a bitmap uploaded into a texture", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;

    SECTION("The texture takes its extent, format and pixels from the bitmap")
    {
        // The desc says nothing about the size or the format on purpose: the bitmap is what those come from,
        // and a desc that repeated them would hide a path that read the wrong one.
        const Bitmap bitmap = MakeGradientBitmap(k_side, k_side);
        Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, fixture.GetQueue(), bitmap,
            {.usage = Forge::TextureUsageBits::Sampled | Forge::TextureUsageBits::TransferSource}));

        REQUIRE(texture.GetDesc().width == static_cast<u32>(k_side));
        REQUIRE(texture.GetDesc().height == static_cast<u32>(k_side));
        REQUIRE(texture.GetDesc().mip_level_count == 1);
        REQUIRE(texture.GetDesc().format == bitmap.GetPixelFormat());
        // The upload leaves it where a shader reads it, which is what the sample relies on and what the
        // usage above has to allow.
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::ShaderReadOnly);

        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, texture, k_side, Forge::ImageLayout::ShaderReadOnly);
        REQUIRE(CountMismatches({bitmap.GetData(), bitmap.GetTotalSize()}, {pixels.GetData(), pixels.GetSize()}) == 0);
    }
    SECTION("Generating the mips gives the texture the whole chain")
    {
        if (!fixture.device.GetPhysicalDevice().SupportsBlit(PixelFormat::R8G8B8A8_UNORM, true) ||
            !fixture.device.GetPhysicalDevice().SupportsBlit(PixelFormat::R8G8B8A8_UNORM, false))
        {
            SKIP("This device cannot blit R8G8B8A8_UNORM, so it cannot generate mips for one.");
        }
        // One colour everywhere, so every level below the first has to hold that colour whichever way the
        // driver filters it down.
        Opal::DynamicArray<u8> flat(k_side * k_side * 4);
        for (i32 pixel = 0; pixel < k_side * k_side; ++pixel)
        {
            flat[pixel * 4 + 0] = 10;
            flat[pixel * 4 + 1] = 200;
            flat[pixel * 4 + 2] = 30;
            flat[pixel * 4 + 3] = 255;
        }
        const Bitmap bitmap =
            ForgeTest::Unwrap(Bitmap::Create(k_side, k_side, 1, PixelFormat::R8G8B8A8_UNORM, 1, {flat.GetData(), flat.GetSize()}));

        Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, fixture.GetQueue(), bitmap, {.usage = Forge::TextureUsageBits::Sampled}, true));
        // Four texels across is three levels, and the bitmap carried one of them.
        REQUIRE(texture.GetDesc().mip_level_count == 3);

        for (u32 mip_level = 1; mip_level < 3; ++mip_level)
        {
            const i32 side = k_side >> mip_level;
            Opal::DynamicArray<u8> pixels(side * side * 4);
            REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, mip_level,
                                           Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
            for (i32 pixel = 0; pixel < side * side; ++pixel)
            {
                INFO("mip level " << mip_level << " pixel " << pixel);
                REQUIRE(static_cast<i32>(pixels[pixel * 4 + 0]) == 10);
                REQUIRE(static_cast<i32>(pixels[pixel * 4 + 1]) == 200);
                REQUIRE(static_cast<i32>(pixels[pixel * 4 + 2]) == 30);
            }
        }
    }
    SECTION("The copy command reads the mip levels out of the bitmap")
    {
        // The overload that takes a bitmap rather than a list of regions: where each level sits in the buffer
        // is the bitmap's to say. The two levels are given colours with no channel in common, so a level
        // copied from the wrong offset comes back as the other one.
        Bitmap bitmap = ForgeTest::Unwrap(Bitmap::Create(k_side, k_side, 1, PixelFormat::R8G8B8A8_UNORM, 2));
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                bitmap.SetPixel(x, y, 0, 0, Vector4f{1.0f, 0.0f, 0.0f, 1.0f});
            }
        }
        for (i32 y = 0; y < k_side / 2; ++y)
        {
            for (i32 x = 0; x < k_side / 2; ++x)
            {
                bitmap.SetPixel(x, y, 0, 1, Vector4f{0.0f, 1.0f, 0.0f, 1.0f});
            }
        }

        const Forge::Buffer staging = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device, {.size = bitmap.GetTotalSize(), .usage = Forge::BufferUsageBits::TransferSource},
            {bitmap.GetData(), bitmap.GetTotalSize()}));
        Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                                                                          {.format = PixelFormat::R8G8B8A8_UNORM,
                                                                           .width = k_side,
                                                                           .height = k_side,
                                                                           .mip_level_count = 2,
                                                                           .usage = Forge::TextureUsageBits::TransferDestination |
                                                                                    Forge::TextureUsageBits::TransferSource}));

        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           REQUIRE(command_buffer.CmdTextureBarrier(
                                                       Forge::TextureBarrier::ToTransferDestination(texture)) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdCopyBufferToTexture(staging, bitmap, texture) == ErrorCode::Success);
                                       }) == ErrorCode::Success);

        Opal::DynamicArray<u8> top(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, top, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        for (i32 pixel = 0; pixel < k_side * k_side; ++pixel)
        {
            INFO("top level pixel " << pixel);
            REQUIRE(static_cast<i32>(top[pixel * 4 + 0]) == 255);
            REQUIRE(static_cast<i32>(top[pixel * 4 + 1]) == 0);
        }

        Opal::DynamicArray<u8> bottom((k_side / 2) * (k_side / 2) * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, bottom, 1, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        for (i32 pixel = 0; pixel < (k_side / 2) * (k_side / 2); ++pixel)
        {
            INFO("second level pixel " << pixel);
            REQUIRE(static_cast<i32>(bottom[pixel * 4 + 0]) == 0);
            REQUIRE(static_cast<i32>(bottom[pixel * 4 + 1]) == 255);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge mip generation covers every array layer", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    if (!fixture.device.GetPhysicalDevice().SupportsBlit(k_format, true) ||
        !fixture.device.GetPhysicalDevice().SupportsBlit(k_format, false))
    {
        SKIP("This device cannot blit R8G8B8A8_UNORM, so it cannot generate mips for one.");
    }
    constexpr i32 k_side = 8;
    constexpr u32 k_mip_count = 4;  // 8 -> 4 -> 2 -> 1
    constexpr u32 k_layer_count = 2;
    // One constant colour per layer, so a box filter of either is that colour at every level, and a level of
    // the second layer that was never written cannot pass for one that was.
    constexpr u8 k_layer_texels[k_layer_count][4] = {{200, 100, 50, 255}, {17, 231, 88, 255}};

    constexpr i32 k_layer_bytes = k_side * k_side * 4;
    Opal::DynamicArray<u8> mip0(static_cast<i32>(k_layer_count) * k_layer_bytes);
    for (i32 i = 0; i < mip0.GetSize(); ++i)
    {
        mip0[i] = k_layer_texels[i / k_layer_bytes][i % 4];
    }

    Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                            .width = k_side,
                                            .height = k_side,
                                            .mip_level_count = k_mip_count,
                                            .array_layer_count = k_layer_count,
                                            .usage = Forge::TextureUsageBits::TransferSource |
                                                     Forge::TextureUsageBits::TransferDestination |
                                                     Forge::TextureUsageBits::Sampled,
                                            .view_type = Forge::TextureViewType::Texture2DArray}));
    const Forge::Buffer staging = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = mip0.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, mip0));
    // One region for both layers: the buffer holds them back to back, which is the order Vulkan copies them in.
    const Forge::BufferTextureCopyRegion mip0_region{.texture_subresource = {.array_layer_count = k_layer_count}};
    REQUIRE(Forge::ImmediateSubmit(
                fixture.device, fixture.GetQueue(),
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(texture)) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdCopyBufferToTexture(staging, texture, {&mip0_region, 1}) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdGenerateMips(texture, Forge::ImageLayout::TransferDestination) == ErrorCode::Success);
                }) == ErrorCode::Success);

    // Every level of every layer, not just of layer zero: a blit region names one array layer unless told
    // otherwise, so a mip chain built without saying so leaves every layer past the first holding whatever it
    // was created with, while the barriers around it still report the whole texture as filled and transitioned.
    for (u32 level = 0; level < k_mip_count; ++level)
    {
        const i32 side = k_side >> level;
        Opal::DynamicArray<u8> level_pixels(static_cast<i32>(k_layer_count) * side * side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, level_pixels, level,
                                       Forge::ImageLayout::TransferDestination) == ErrorCode::Success);
        for (i32 i = 0; i < level_pixels.GetSize(); ++i)
        {
            const i32 layer = i / (side * side * 4);
            INFO("mip level " << level << ", array layer " << layer << ", byte " << i);
            REQUIRE(level_pixels[i] == k_layer_texels[layer][i % 4]);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge device-only buffer", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 256;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 41);

    // HostAccess::None is the only kind that may land in memory the host cannot map, so it is the only kind
    // that has to go through the staging helpers at both ends.
    const Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device,
        {.size = k_size,
         .usage = Forge::BufferUsageBits::StorageBuffer | Forge::BufferUsageBits::TransferSource |
                  Forge::BufferUsageBits::TransferDestination,
         .host_access = Forge::HostAccess::None}));
    REQUIRE(buffer.IsValid());

    REQUIRE(Forge::UploadToBuffer(fixture.device, fixture.GetQueue(), buffer, written) == ErrorCode::Success);
    Opal::DynamicArray<u8> read_back(k_size);
    REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), buffer, read_back) == ErrorCode::Success);
    REQUIRE(CountMismatches(written, read_back) == 0);

    SECTION("Update and Read both refuse it")
    {
        Opal::DynamicArray<u8> out(k_size);
        REQUIRE(buffer.Update(written) == ErrorCode::InvalidArgument);
        REQUIRE(buffer.Read(out) == ErrorCode::InvalidArgument);
    }
    SECTION("Initial data is refused rather than leaking the allocation")
    {
        REQUIRE(Forge::Buffer::Create(fixture.device,
                                        {.size = k_size,
                                         .usage = Forge::BufferUsageBits::TransferDestination,
                                         .host_access = Forge::HostAccess::None},
                                        written).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}


TEST_CASE("Forge debug names reach the validation layer", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    if (!fixture.device.AreDebugUtilsEnabled())
    {
        SKIP("This build has no debug utils, so there is nothing to name and nothing to report.");
    }

    MismatchedFormatTextures textures = MakeMismatchedFormatTextures(fixture.device);
    Forge::SetDebugName(fixture.device, textures.source, "probe-source-texture");
    Forge::SetDebugName(fixture.device, textures.destination, "probe-destination-texture");

    // Copying between two formats of different texel size breaks a rule the guards do not check and the
    // validation layer does, which is what makes it a way to read back what the layer calls these two images.
    // The layer checks this while the command is recorded, so the command buffer is thrown away rather than
    // submitted: handing the driver work that breaks the specification is undefined behaviour, and it took
    // the next test down with it when this did.
    RecordMismatchedFormatCopy(fixture.device, fixture.GetQueue(), textures.source, textures.destination);

    const Opal::StringUtf8 errors = fixture.GetValidationErrors();
    INFO(*errors);
    REQUIRE(fixture.GetValidationErrorCount() > 0);
    REQUIRE(strstr(*errors, "probe-source-texture") != nullptr);
    REQUIRE(strstr(*errors, "probe-destination-texture") != nullptr);

    // The error above was the point of the test, so it must not be left for the next assertion to trip over.
    fixture.context.ClearDebugMessages();
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * Every headless SetDebugName overload but Texture and TimestampQueryPool, which were the only two called
 * anywhere in the suite. SwapChain and FrameContext need a window and are covered in window-test.cpp instead.
 *
 * SetName hands the handle and a VkObjectType off to vkSetDebugUtilsObjectNameEXT by hand per overload, so a
 * copy-pasted overload naming the wrong VkObjectType for its handle compiles fine and is silent without the
 * validation layer: it is the layer that knows a VK_OBJECT_TYPE_BUFFER handle is not a VkImage. So the check
 * here is not that the name reached the object - nothing hands that back out to ask - but that naming every
 * one of them raised nothing.
 */
TEST_CASE("Forge debug names on every headless object", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    if (!fixture.device.AreDebugUtilsEnabled())
    {
        SKIP("This build has no debug utils, so there is nothing to name and nothing to report.");
    }

    const Forge::Buffer buffer = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = 32, .usage = Forge::BufferUsageBits::TransferDestination}));
    Forge::SetDebugName(fixture.device, buffer, "probe-buffer");

    const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.max_anisotropy = 1.0f}));
    Forge::SetDebugName(fixture.device, sampler, "probe-sampler");

    const Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(
        fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM, .width = 4, .height = 4, .usage = Forge::TextureUsageBits::Sampled}));
    const Forge::TextureView view = ForgeTest::Unwrap(Forge::TextureView::Create(fixture.device, texture));
    Forge::SetDebugName(fixture.device, view, "probe-texture-view");

    const Forge::Shader shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
    Forge::SetDebugName(fixture.device, shader, "probe-shader");

    const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, shader);
    Forge::SetDebugName(fixture.device, pipeline, "probe-pipeline");

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1) == ErrorCode::Success);
    pool_desc.max_sets = 1;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
    Forge::SetDebugName(fixture.device, pool, "probe-descriptor-pool");

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));
    Forge::SetDebugName(fixture.device, layout, "probe-descriptor-set-layout");

    const Forge::DescriptorSet descriptor_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
    Forge::SetDebugName(fixture.device, descriptor_set, "probe-descriptor-set");

    const Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    Forge::SetDebugName(fixture.device, command_buffer, "probe-command-buffer");

    const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
    Forge::SetDebugName(fixture.device, fence, "probe-fence");

    const Forge::Semaphore semaphore = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
    Forge::SetDebugName(fixture.device, semaphore, "probe-semaphore");

    Forge::SetDebugName(fixture.device, fixture.GetQueue(), "probe-queue");

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge command buffer reset and repeated Begin", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Forge::Shader shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
    const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, shader);
    constexpr i32 k_element_count = 256;
    constexpr i32 k_group_size = 64;

    auto record_and_run = [&](Forge::CommandBuffer& command_buffer, const Forge::Buffer& output)
    {
        const VkDeviceAddress address = output.GetNativeDeviceAddress();
        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(address)) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
    };
    /** Submit one command buffer and wait for it, through a fence made and waited on here. */
    auto submit_and_wait = [&](Forge::CommandBuffer& command_buffer)
    {
        const Opal::Ref<const Forge::CommandBuffer> batch[1] = {Opal::Ref<const Forge::CommandBuffer>(command_buffer)};
        const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {batch, 1}, .fence = fence}) == ErrorCode::Success);
        REQUIRE(fence.Wait() == ErrorCode::Success);
    };

    SECTION("Reset lets a command buffer be recorded and submitted a second time")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));

        Forge::Buffer first_output = MakeWipedOutput(fixture.device, k_element_count);
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        record_and_run(command_buffer, first_output);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
        submit_and_wait(command_buffer);
        RequireComputeWrote(first_output, k_element_count);

        // Reset, not a fresh Create: the same native command buffer is recorded into again.
        REQUIRE(command_buffer.Reset() == ErrorCode::Success);
        Forge::Buffer second_output = MakeWipedOutput(fixture.device, k_element_count);
        REQUIRE(command_buffer.Begin(/*submit_one_time=*/false) == ErrorCode::Success);
        record_and_run(command_buffer, second_output);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
        submit_and_wait(command_buffer);
        RequireComputeWrote(second_output, k_element_count);
    }
    SECTION("A command buffer not marked submit_one_time can be submitted twice with no Reset between")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);
        REQUIRE(command_buffer.Begin(/*submit_one_time=*/false) == ErrorCode::Success);
        record_and_run(command_buffer, output);
        REQUIRE(command_buffer.End() == ErrorCode::Success);

        submit_and_wait(command_buffer);
        RequireComputeWrote(output, k_element_count);

        submit_and_wait(command_buffer);
        RequireComputeWrote(output, k_element_count);
    }
    SECTION("Reset does not roll back the layout bookkeeping a discarded barrier set")
    {
        Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                            .width = 4,
                            .height = 4,
                            .usage = Forge::TextureUsageBits::TransferSource | Forge::TextureUsageBits::TransferDestination}));
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::Undefined);

        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTransition(texture, Forge::ImageLayout::TransferDestination) == ErrorCode::Success);
        // The barrier is recorded, so the bookkeeping moves right away - this is record-time, not execution-time.
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferDestination);

        // Discarded, never submitted: the texture never actually left Undefined on the device.
        REQUIRE(command_buffer.Reset() == ErrorCode::Success);
        // docs/forge.md: Reset does not roll the bookkeeping back. It still says TransferDestination.
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferDestination);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge batched submit", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const SplitCopy copy(fixture);

    SECTION("Two command buffers in one batch, with a fence")
    {
        const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        const Opal::Ref<const Forge::CommandBuffer> batch[2] = {Opal::Ref<const Forge::CommandBuffer>(copy.first),
                                                                Opal::Ref<const Forge::CommandBuffer>(copy.second)};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {batch, 2}, .fence = fence}) == ErrorCode::Success);
        REQUIRE(fence.Wait() == ErrorCode::Success);
    }
    SECTION("The same batch without a fence, waited on through the queue")
    {
        const Opal::Ref<const Forge::CommandBuffer> batch[2] = {Opal::Ref<const Forge::CommandBuffer>(copy.first),
                                                                Opal::Ref<const Forge::CommandBuffer>(copy.second)};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {batch, 2}}) == ErrorCode::Success);
        REQUIRE(fixture.GetQueue().WaitIdle() == ErrorCode::Success);
    }
    SECTION("One batch per half, the second waiting on a semaphore the first signals")
    {
        const Forge::Semaphore semaphore = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        const Forge::SemaphoreSubmit signal{.semaphore = semaphore, .stages = Forge::PipelineStageBits::Transfer};
        const Forge::SemaphoreSubmit wait{.semaphore = semaphore, .stages = Forge::PipelineStageBits::Transfer};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.first_batch, 1}, .signal_semaphores = {&signal, 1}}) ==
                ErrorCode::Success);
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.second_batch, 1}, .wait_semaphores = {&wait, 1}, .fence = fence}) ==
                ErrorCode::Success);
        REQUIRE(fence.Wait() == ErrorCode::Success);
    }

    copy.RequireWholeBufferCopied(fixture);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge timeline semaphores", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    // The same split copy the batched submit case drives, submitted here against the value side of a timeline.
    const SplitCopy copy(fixture);

    SECTION("A fresh timeline starts at its initial value and the host can raise it")
    {
        constexpr u64 k_initial = 7;
        const Forge::Semaphore timeline = ForgeTest::Unwrap(
            Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = k_initial}));
        REQUIRE(timeline.IsTimeline());
        REQUIRE(timeline.GetType() == Forge::SemaphoreType::Timeline);
        REQUIRE(ForgeTest::Unwrap(timeline.GetValue()) == k_initial);
        REQUIRE(timeline.Signal(k_initial + 3) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(timeline.GetValue()) == k_initial + 3);
    }
    SECTION("A wait for a value already reached returns at once")
    {
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = 4}));
        REQUIRE(timeline.Wait(4) == ErrorCode::Success);
        REQUIRE(timeline.Wait(1) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(timeline.GetValue()) == 4);
    }
    SECTION("A wait that runs out of time answers false rather than failing")
    {
        // A millisecond, in the nanoseconds Vulkan counts timeouts in. Long enough that a machine under load
        // does not report a timeout for the value that was already reached, short enough not to stall the run.
        constexpr u64 k_short_timeout = 1000 * 1000;
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = 4}));
        REQUIRE(ForgeTest::Unwrap(timeline.TryWait(4, k_short_timeout)));
        // Nothing was submitted that could raise it, so the timeout is the only way out of these two.
        REQUIRE_FALSE(ForgeTest::Unwrap(timeline.TryWait(5, k_short_timeout)));
        const Forge::SemaphoreWait waits[1] = {{.semaphore = timeline, .value = 5}};
        REQUIRE_FALSE(ForgeTest::Unwrap(Forge::Semaphore::TryWaitForAll({waits, 1}, k_short_timeout)));
    }
    SECTION("One batch per half, the second waiting on the value the first signals")
    {
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        const Forge::SemaphoreSubmit signal{.semaphore = timeline, .stages = Forge::PipelineStageBits::Transfer, .value = 1};
        const Forge::SemaphoreSubmit wait{.semaphore = timeline, .stages = Forge::PipelineStageBits::Transfer, .value = 1};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.first_batch, 1}, .signal_semaphores = {&signal, 1}}) == ErrorCode::Success);
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.second_batch, 1}, .wait_semaphores = {&wait, 1}, .fence = fence}) ==
                ErrorCode::Success);
        REQUIRE(fence.Wait() == ErrorCode::Success);
        copy.RequireWholeBufferCopied(fixture);
    }
    SECTION("The host waits on a value the device signals, with no fence anywhere")
    {
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        const Forge::SemaphoreSubmit first_signal{.semaphore = timeline, .value = 1};
        const Forge::SemaphoreSubmit second_signal{.semaphore = timeline, .value = 2};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.first_batch, 1}, .signal_semaphores = {&first_signal, 1}}) ==
                ErrorCode::Success);
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.second_batch, 1}, .signal_semaphores = {&second_signal, 1}}) ==
                ErrorCode::Success);
        REQUIRE(timeline.Wait(2) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(timeline.GetValue()) == 2);
        copy.RequireWholeBufferCopied(fixture);
    }
    SECTION("WaitForAll over two timelines returns once both have been signalled")
    {
        const Forge::Semaphore first_timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        const Forge::Semaphore second_timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        const Forge::SemaphoreSubmit first_signal{.semaphore = first_timeline, .value = 1};
        const Forge::SemaphoreSubmit second_signal{.semaphore = second_timeline, .value = 1};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.first_batch, 1}, .signal_semaphores = {&first_signal, 1}}) ==
                ErrorCode::Success);
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.second_batch, 1}, .signal_semaphores = {&second_signal, 1}}) ==
                ErrorCode::Success);
        const Forge::SemaphoreWait waits[2] = {{.semaphore = first_timeline, .value = 1}, {.semaphore = second_timeline, .value = 1}};
        REQUIRE(Forge::Semaphore::WaitForAll({waits, 2}) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(first_timeline.GetValue()) == 1);
        REQUIRE(ForgeTest::Unwrap(second_timeline.GetValue()) == 1);
        copy.RequireWholeBufferCopied(fixture);
    }
    SECTION("The host side of a timeline is refused on a binary semaphore")
    {
        const Forge::Semaphore binary = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        REQUIRE_FALSE(binary.IsTimeline());
        REQUIRE(binary.Wait(1) == ErrorCode::InvalidArgument);
        REQUIRE(binary.Signal(1) == ErrorCode::InvalidArgument);
        REQUIRE_FALSE(binary.GetValue().HasValue());
    }
    SECTION("A value on a binary semaphore is refused, since Vulkan would ignore it")
    {
        const Forge::Semaphore binary = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        const Forge::SemaphoreSubmit signal{.semaphore = binary, .value = 1};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.first_batch, 1}, .signal_semaphores = {&signal, 1}}) ==
                ErrorCode::InvalidArgument);
    }
    SECTION("A timeline signalled with zero is refused, since no signal can reach it")
    {
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        const Forge::SemaphoreSubmit signal{.semaphore = timeline, .value = 0};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.first_batch, 1}, .signal_semaphores = {&signal, 1}}) ==
                ErrorCode::InvalidArgument);
        // A wait for zero is legal and trivially satisfied, so only the signal side is turned away.
        const Forge::SemaphoreSubmit wait{.semaphore = timeline, .value = 0};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {copy.first_batch, 1}, .wait_semaphores = {&wait, 1}}) == ErrorCode::Success);
        REQUIRE(fixture.GetQueue().WaitIdle() == ErrorCode::Success);
    }
    SECTION("A signal that does not raise the count is refused")
    {
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = 4}));
        REQUIRE(timeline.Signal(4) == ErrorCode::InvalidArgument);
        REQUIRE(timeline.Signal(3) == ErrorCode::InvalidArgument);
        REQUIRE(ForgeTest::Unwrap(timeline.GetValue()) == 4);
        REQUIRE(timeline.Signal(5) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(timeline.GetValue()) == 5);
    }
    SECTION("WaitForAll over two devices is refused rather than naming one of them")
    {
        // A second logical device on the same physical one. Not a second ForgeFixture: its context would
        // call volkFinalize on the way out and unload Vulkan from under this one.
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(fixture.context.EnumeratePhysicalDevices());
        const Forge::Device other =
            ForgeTest::Unwrap(Forge::Device::Create(std::move(physical_devices[0]), fixture.context, MakeHeadlessDeviceDesc()));
        const Forge::Semaphore here =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = 1}));
        const Forge::Semaphore there =
            ForgeTest::Unwrap(Forge::Semaphore::Create(other, {.type = Forge::SemaphoreType::Timeline, .initial_value = 1}));
        const Forge::SemaphoreWait waits[2] = {{.semaphore = here, .value = 1}, {.semaphore = there, .value = 1}};
        REQUIRE(Forge::Semaphore::WaitForAll({waits, 2}) == ErrorCode::InvalidArgument);
    }
    SECTION("An empty entry in WaitForAll is refused, either way it is empty")
    {
        const Forge::Semaphore empty;
        const Forge::SemaphoreWait empty_reference[1] = {{}};
        REQUIRE(Forge::Semaphore::WaitForAll({empty_reference, 1}) == ErrorCode::InvalidArgument);
        const Forge::SemaphoreWait empty_semaphore[1] = {{.semaphore = empty, .value = 1}};
        REQUIRE(Forge::Semaphore::WaitForAll({empty_semaphore, 1}) == ErrorCode::InvalidArgument);
    }

    REQUIRE(fixture.GetQueue().WaitIdle() == ErrorCode::Success);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge submit rejects an empty object", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Forge::CommandBuffer valid = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));

    SECTION("An empty command buffer is refused")
    {
        Forge::CommandBuffer empty;
        const Opal::Ref<const Forge::CommandBuffer> batch[1] = {Opal::Ref<const Forge::CommandBuffer>(empty)};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {batch, 1}}) == ErrorCode::InvalidArgument);
    }
    SECTION("An empty semaphore is refused")
    {
        Forge::Semaphore empty;
        const Forge::SemaphoreSubmit wait{.semaphore = empty};
        REQUIRE(fixture.GetQueue().Submit({.wait_semaphores = {&wait, 1}}) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge waiting on several fences at once", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 64;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 63);
    const Forge::Buffer source = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written));
    constexpr Forge::BufferUsageBits k_both_ways = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination;
    const Forge::Buffer first_destination =
        ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = k_both_ways}));
    const Forge::Buffer second_destination =
        ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = k_both_ways}));

    Opal::DynamicArray<Forge::Fence> fences;
    fences.EmplaceBack(ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false)));
    fences.EmplaceBack(ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false)));

    Forge::CommandBuffer first = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    Forge::CommandBuffer second = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    REQUIRE(first.Begin() == ErrorCode::Success);
    REQUIRE(first.CmdCopyBuffer(source, first_destination) == ErrorCode::Success);
    REQUIRE(first.End() == ErrorCode::Success);
    REQUIRE(second.Begin() == ErrorCode::Success);
    REQUIRE(second.CmdCopyBuffer(source, second_destination) == ErrorCode::Success);
    REQUIRE(second.End() == ErrorCode::Success);

    const Opal::Ref<const Forge::CommandBuffer> first_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(first)};
    const Opal::Ref<const Forge::CommandBuffer> second_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(second)};
    REQUIRE(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .fence = fences[0]}) == ErrorCode::Success);
    REQUIRE(fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .fence = fences[1]}) == ErrorCode::Success);
    REQUIRE(Forge::Fence::WaitForAll(fences) == ErrorCode::Success);

    Opal::DynamicArray<u8> read_back(k_size);
    REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), first_destination, read_back) == ErrorCode::Success);
    REQUIRE(CountMismatches(written, read_back) == 0);
    REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), second_destination, read_back) == ErrorCode::Success);
    REQUIRE(CountMismatches(written, read_back) == 0);

    SECTION("A fence that is not signalled in time answers false rather than failing")
    {
        constexpr u64 k_short_timeout = 1000 * 1000;
        // Both of the fences above have been waited on, so they are signalled and answer at once.
        REQUIRE(ForgeTest::Unwrap(fences[0].TryWait(k_short_timeout)));
        REQUIRE(ForgeTest::Unwrap(Forge::Fence::TryWaitForAll(fences, k_short_timeout)));
        // Nothing is submitted against this one, so it stays unsignalled and the timeout is the only way out.
        const Forge::Fence never_signalled = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        REQUIRE_FALSE(ForgeTest::Unwrap(never_signalled.TryWait(k_short_timeout)));
    }
    SECTION("Fences from two devices in one wait are refused")
    {
        // A second logical device on the same physical one, for the reason the timeline case above gives.
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(fixture.context.EnumeratePhysicalDevices());
        const Forge::Device other =
            ForgeTest::Unwrap(Forge::Device::Create(std::move(physical_devices[0]), fixture.context, MakeHeadlessDeviceDesc()));
        Opal::DynamicArray<Forge::Fence> across_devices;
        across_devices.EmplaceBack(ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, true)));
        across_devices.EmplaceBack(ForgeTest::Unwrap(Forge::Fence::Create(other, true)));
        REQUIRE(Forge::Fence::WaitForAll(across_devices) == ErrorCode::InvalidArgument);
    }
    SECTION("An empty fence in the list is refused")
    {
        Opal::DynamicArray<Forge::Fence> with_empty;
        with_empty.EmplaceBack(ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, true)));
        with_empty.EmplaceBack();
        REQUIRE(Forge::Fence::WaitForAll(with_empty) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge device features", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));

    // Builds a device on this machine's first physical device with the given features asked for. Hands back
    // what Device::Create reported, since one case below is about a feature this machine may not have.
    auto make_device = [&context](const Forge::DeviceFeatures& features)
    {
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
        return Forge::Device::Create(std::move(physical_devices[0]), context, MakeHeadlessDeviceDesc(features));
    };

    SECTION("The defaults are what the device reports back")
    {
        const Forge::Device device = ForgeTest::Unwrap(make_device({}));
        REQUIRE(device.GetFeatures().buffer_device_address);
        REQUIRE(device.GetFeatures().descriptor_indexing);
        REQUIRE(device.GetFeatures().sampler_anisotropy);
        REQUIRE_FALSE(device.GetFeatures().mesh_shader);
        REQUIRE_FALSE(device.GetFeatures().geometry_shader);
    }
    SECTION("Asking for mesh shaders succeeds exactly when this device has them")
    {
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
        const bool has_extension = physical_devices[0].IsExtensionSupported(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        INFO("VK_EXT_mesh_shader supported: " << has_extension);
        if (has_extension)
        {
            const Forge::Device device = ForgeTest::Unwrap(make_device({.mesh_shader = true, .task_shader = true}));
            REQUIRE(device.IsExtensionEnabled(VK_EXT_MESH_SHADER_EXTENSION_NAME));
        }
        else
        {
            REQUIRE(make_device({.mesh_shader = true}).GetErrorOr(ErrorCode::Success) == ErrorCode::FeatureNotSupported);
        }
    }
    SECTION("A buffer wanting a device address needs the feature")
    {
        const Forge::Device device = ForgeTest::Unwrap(make_device({.buffer_device_address = false}));
        REQUIRE(Forge::Buffer::Create(device, {.size = 64,
                                                 .usage = Forge::BufferUsageBits::StorageBuffer,
                                                 .use_device_address = true}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("An anisotropic sampler needs the feature")
    {
        const Forge::Device device = ForgeTest::Unwrap(make_device({.sampler_anisotropy = false}));
        REQUIRE(Forge::Sampler::Create(device, {.max_anisotropy = 8.0f}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        // One that does not ask for anisotropy is fine on the same device.
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(device, {.max_anisotropy = 1.0f}));
        REQUIRE(sampler.IsValid());
    }
    SECTION("More than one indirect command needs the feature")
    {
        Forge::Device device = ForgeTest::Unwrap(make_device({.multi_draw_indirect = false}));
        Forge::DeviceQueue& queue = ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics));
        const Forge::Buffer commands = ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = 2 * sizeof(Forge::DrawIndirectCommand),
                                              .usage = Forge::BufferUsageBits::IndirectBuffer}));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(device, queue));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdDrawIndirect(commands, 0, 2) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }

    REQUIRE_NO_VALIDATION_ERROR_IN(context);
}

TEST_CASE("Forge physical device selection", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
    Opal::DynamicArray<Forge::PhysicalDevice> devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
    REQUIRE_FALSE(devices.IsEmpty());

    SECTION("A headless desc is met by some device on this machine")
    {
        const Opal::Optional<u32> best = Forge::FindPhysicalDevice(devices, MakeHeadlessDeviceDesc());
        REQUIRE(best.HasValue());
        REQUIRE(best.GetValue() < static_cast<u32>(devices.GetSize()));
        // The one it picked has to actually work, which is the whole point of choosing rather than guessing.
        const Forge::Device device =
            ForgeTest::Unwrap(Forge::Device::Create(std::move(devices[static_cast<i32>(best.GetValue())]), context,
                                                    MakeHeadlessDeviceDesc()));
        REQUIRE(device.IsValid());
    }
    SECTION("A requirement nothing can meet leaves the answer empty")
    {
        // A device supporting an extension under this name would be a surprising machine indeed.
        const char* nonsense_extension = "VK_EXT_this_extension_does_not_exist";
        Forge::DeviceDesc desc = MakeHeadlessDeviceDesc();
        desc.extensions.PushBack(nonsense_extension);
        REQUIRE_FALSE(Forge::FindPhysicalDevice(devices, desc).HasValue());
    }
    SECTION("Selecting when nothing qualifies reports it, with the log naming the requirement")
    {
        Forge::DeviceDesc desc = MakeHeadlessDeviceDesc();
        desc.extensions.PushBack("VK_EXT_this_extension_does_not_exist");
        Opal::Expected<Forge::PhysicalDevice, ErrorCode> chosen = Forge::SelectPhysicalDevice(devices, desc);
        REQUIRE_FALSE(chosen.HasValue());
        REQUIRE(chosen.GetError() == ErrorCode::FeatureNotSupported);
    }
    SECTION("Selecting moves the chosen device out of the list")
    {
        Forge::PhysicalDevice chosen = ForgeTest::Unwrap(Forge::SelectPhysicalDevice(devices, MakeHeadlessDeviceDesc()));
        REQUIRE(chosen.IsValid());
        i32 valid_left = 0;
        for (const Forge::PhysicalDevice& device : devices)
        {
            valid_left += device.IsValid() ? 1 : 0;
        }
        REQUIRE(valid_left == static_cast<i32>(devices.GetSize()) - 1);
    }
    SECTION("A device that cannot present is not chosen for a desc that has to")
    {
        // No surface can be made without a window, so this only checks the other direction: a desc with no
        // surface must not reject a device for presentation it was never asked to do.
        Forge::DeviceDesc desc = MakeHeadlessDeviceDesc();
        REQUIRE(Forge::FindPhysicalDevice(devices, desc).HasValue());
    }

    REQUIRE_NO_VALIDATION_ERROR_IN(context);
}

TEST_CASE("Forge queue family lookup by flags, and creation from an empty physical device", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
    Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
    REQUIRE_FALSE(physical_devices.IsEmpty());
    const Forge::PhysicalDevice& physical_device = physical_devices[0];

    SECTION("A family matching queue_flags actually reports that bit")
    {
        const Opal::Optional<u32> graphics_family = physical_device.GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
        REQUIRE(graphics_family.HasValue());
        REQUIRE((physical_device.GetQueueFamilyProperties()[graphics_family.GetValue()].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0);
    }
    SECTION("not_queue_flags excludes a family that has the excluded bit")
    {
        const Opal::Optional<u32> async_compute_family =
            physical_device.GetQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT);
        if (!async_compute_family.HasValue())
        {
            SKIP("This device has no compute family separate from its graphics one.");
        }
        REQUIRE((physical_device.GetQueueFamilyProperties()[async_compute_family.GetValue()].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0);
    }
    SECTION("Asking for and excluding the same bit is unmet by any family")
    {
        REQUIRE_FALSE(physical_device.GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_GRAPHICS_BIT).HasValue());
    }
    SECTION("Device::Create refuses an empty physical device")
    {
        Forge::PhysicalDevice empty_physical_device;
        REQUIRE_FALSE(empty_physical_device.IsValid());
        Opal::Expected<Forge::Device, ErrorCode> result =
            Forge::Device::Create(std::move(empty_physical_device), context, MakeHeadlessDeviceDesc());
        REQUIRE_FALSE(result.HasValue());
        REQUIRE(result.GetError() == ErrorCode::InvalidArgument);
    }

    REQUIRE_NO_VALIDATION_ERROR_IN(context);
}

/** Writes into the second buffer of a bound array, so which descriptor was written is visible in the result. */
constexpr const char* k_bindless_source = R"(
[[vk::binding(0, 0)]] RWStructuredBuffer<uint> outputs[];

[shader("compute")]
[numthreads(64, 1, 1)]
void main_bindless(uint3 thread_id : SV_DispatchThreadID)
{
    outputs[1][thread_id.x] = thread_id.x + 2000;
}
)";

TEST_CASE("Forge bindless descriptor bindings", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_bindless_features{.partially_bound_descriptors = true,
                                                        .update_after_bind_descriptors = true,
                                                        .non_uniform_descriptor_indexing = true};
    if (!CanCreateDevice(k_bindless_features))
    {
        SKIP("This device does not support the descriptor indexing features bindless needs.");
    }
    ForgeFixture fixture(k_bindless_features);
    Forge::Device& device = fixture.device;
    Forge::DeviceQueue& queue = fixture.GetQueue();

    constexpr u32 k_max_descriptors = 4;
    constexpr u32 k_used_descriptors = 2;
    constexpr i32 k_element_count = 256;
    constexpr i32 k_group_size = 64;

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, k_max_descriptors) == ErrorCode::Success);
    pool_desc.max_sets = 1;
    pool_desc.use_update_after_bind = true;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, k_max_descriptors, ShaderTypeBits::Compute, {},
                           Forge::DescriptorBindingFlagBits::PartiallyBound | Forge::DescriptorBindingFlagBits::UpdateAfterBind |
                               Forge::DescriptorBindingFlagBits::VariableDescriptorCount) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(device, layout_desc));

    SECTION("A partially bound array is written and read where it was written")
    {
        // Two of the four descriptors, so the variable count is doing something, and only the second one is
        // ever written, so partially bound is doing something too.
        Forge::DescriptorSet descriptor_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout, k_used_descriptors));

        Forge::Buffer output = MakeWipedOutput(device, k_element_count);

        // Only descriptor 1 of the array is written. Descriptor 0 is left alone, which is what
        // PartiallyBound allows and what the shader stays away from.
        Opal::DynamicArray<Forge::DescriptorSetUpdateBinding> updates;
        updates.PushBack(Forge::DescriptorSetUpdateBinding{
            .descriptor_type = Forge::DescriptorType::StorageBuffer,
            .binding = 0,
            .array_element = 1,
            .resource_info = Forge::DescriptorSetUpdateBinding::BufferInfo{.buffer = output}});
        REQUIRE(descriptor_set.Update(updates) == ErrorCode::Success);

        const Forge::Shader shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(device, k_bindless_source, {.entry_point = "main_bindless", .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(device, pipeline_desc));

        DispatchWithSet(device, queue, pipeline, descriptor_set, k_element_count / k_group_size);

        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            REQUIRE(values[i] == static_cast<u32>(i) + 2000);
        }
    }
    SECTION("A variable count above the binding's descriptor count is refused")
    {
        REQUIRE(Forge::DescriptorSet::Create(pool, layout, k_max_descriptors + 1).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A variable count without a binding that allows it is refused")
    {
        Forge::DescriptorSetLayoutDesc plain_desc;
        REQUIRE(plain_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        const Forge::DescriptorSetLayout plain_layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(device, plain_desc));
        REQUIRE(Forge::DescriptorSet::Create(pool, plain_layout, 1).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A variable count on anything but the highest binding is refused")
    {
        Forge::DescriptorSetLayoutDesc bad_desc;
        REQUIRE(bad_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 4, ShaderTypeBits::Compute, {},
                            Forge::DescriptorBindingFlagBits::VariableDescriptorCount) == ErrorCode::Success);
        REQUIRE(bad_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        REQUIRE(Forge::DescriptorSetLayout::Create(device, bad_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("An update after bind layout needs a pool that expects one")
    {
        Forge::DescriptorPoolDesc plain_pool_desc;
        REQUIRE(plain_pool_desc.Add(Forge::DescriptorType::StorageBuffer, k_max_descriptors) == ErrorCode::Success);
        plain_pool_desc.use_update_after_bind = false;
        const Forge::DescriptorPool plain_pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(device, plain_pool_desc));
        REQUIRE(Forge::DescriptorSet::Create(plain_pool, layout, k_used_descriptors).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

constexpr const char* k_bindless_texture_source = R"(
[[vk::binding(0, 0)]] RWStructuredBuffer<uint> output;
[[vk::binding(1, 0)]] Sampler2D textures[];

[shader("compute")]
[numthreads(64, 1, 1)]
void main_bindless_textures(uint3 thread_id : SV_DispatchThreadID)
{
    // A different descriptor per invocation. NonUniformResourceIndex is what such an index is supposed to
    // carry; whether Slang decorates it is not something this test can see, so what is proven here is the
    // per-invocation indexing, not the feature behind it.
    // Descriptor zero is never touched: it is the one left unwritten, so PartiallyBound has to hold.
    uint index = 1 + (thread_id.x & 1);
    float4 texel = textures[NonUniformResourceIndex(index)].SampleLevel(float2(0.5, 0.5), 0.0);
    output[thread_id.x] = uint(texel.r * 255.0 + 0.5);
}
)";

TEST_CASE("Forge bindless texture array", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_bindless_features{.partially_bound_descriptors = true,
                                                        .update_after_bind_descriptors = true,
                                                        .non_uniform_descriptor_indexing = true};
    if (!CanCreateDevice(k_bindless_features))
    {
        SKIP("This device does not support the descriptor indexing features bindless needs.");
    }
    ForgeFixture fixture(k_bindless_features);
    Forge::Device& device = fixture.device;
    Forge::DeviceQueue& queue = fixture.GetQueue();

    constexpr u32 k_max_descriptors = 4;
    constexpr u32 k_used_descriptors = 3;
    constexpr i32 k_element_count = 256;
    constexpr i32 k_group_size = 64;
    // The red channel of each single texel texture, which is what comes back through the buffer.
    constexpr u32 k_red_at_one = 40;
    constexpr u32 k_red_at_two = 200;

    // A one by one texture in ShaderReadOnly, filled with one colour. Written through the tracked layout
    // rather than a spelled out one: created undefined, brought to TransferDestination, copied into, then
    // left where a shader reads it.
    auto make_texture = [&](u32 red)
    {
        Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                        .width = 1,
                                        .height = 1,
                                        .usage = Forge::TextureUsageBits::Sampled | Forge::TextureUsageBits::TransferDestination}));
        const u8 texel[4] = {static_cast<u8>(red), 0, 0, 255};
        const Forge::Buffer staging = ForgeTest::Unwrap(Forge::Buffer::Create(
            device, {.size = sizeof(texel), .usage = Forge::BufferUsageBits::TransferSource}, {texel, sizeof(texel)}));
        const Forge::BufferTextureCopyRegion region;
        REQUIRE(Forge::ImmediateSubmit(
                    device, queue,
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(texture)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdCopyBufferToTexture(staging, texture, {&region, 1}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
                    }) == ErrorCode::Success);
        return texture;
    };

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, k_max_descriptors) == ErrorCode::Success);
    pool_desc.max_sets = 1;
    pool_desc.use_update_after_bind = true;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(device, pool_desc));

    // The texture array is the highest binding, which is where a variable count is allowed to sit.
    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, k_max_descriptors, ShaderTypeBits::Compute, {},
                           Forge::DescriptorBindingFlagBits::PartiallyBound | Forge::DescriptorBindingFlagBits::UpdateAfterBind |
                               Forge::DescriptorBindingFlagBits::VariableDescriptorCount) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(device, layout_desc));

    SECTION("A shader samples the element of the array it indexes")
    {
        if (IsSoftwareDevice())
        {
            SKIP("A software driver reports the non-uniform indexing feature and then reads element zero anyway.");
        }
        // Three of the four, so the variable count is doing something.
        Forge::DescriptorSet descriptor_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout, k_used_descriptors));

        Forge::Buffer output = MakeWipedOutput(device, k_element_count);

        const Forge::Texture texture_one = make_texture(k_red_at_one);
        const Forge::Texture texture_two = make_texture(k_red_at_two);
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(device, {.max_anisotropy = 1.0f}));

        // Elements one and two, never element zero: a write past the first descriptor of a binding is the
        // part of this that a binding holding one descriptor could never have exercised.
        REQUIRE(descriptor_set.Update(0, output) == ErrorCode::Success);
        REQUIRE(descriptor_set.Update(1, texture_one, sampler, Forge::ImageLayout::ShaderReadOnly, 1) == ErrorCode::Success);
        REQUIRE(descriptor_set.Update(1, texture_two, sampler, Forge::ImageLayout::ShaderReadOnly, 2) == ErrorCode::Success);

        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            device, k_bindless_texture_source, {.entry_point = "main_bindless_textures", .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(device, pipeline_desc));

        DispatchWithSet(device, queue, pipeline, descriptor_set, k_element_count / k_group_size);

        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            // An even invocation reads element one and an odd one element two, so a shader that ignored the
            // index, or an update that landed on the wrong element, comes back uniform instead.
            const u32 expected = (i % 2) == 0 ? k_red_at_one : k_red_at_two;
            INFO("invocation " << i);
            REQUIRE(values[i] == expected);
        }
    }
    SECTION("The array Update overload writes a texture the same way the convenience one does")
    {
        // Everything else in this file writes a texture through Update(binding, texture, sampler, ...);
        // DescriptorSetUpdateBinding::TextureInfo through the array overload had no caller of its own,
        // though it is what that convenience overload builds underneath.
        if (IsSoftwareDevice())
        {
            SKIP("A software driver reports the non-uniform indexing feature and then reads element zero anyway.");
        }
        Forge::DescriptorSet descriptor_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout, k_used_descriptors));

        Forge::Buffer output = MakeWipedOutput(device, k_element_count);
        const Forge::Texture texture_one = make_texture(k_red_at_one);
        const Forge::Texture texture_two = make_texture(k_red_at_two);
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(device, {.max_anisotropy = 1.0f}));

        Opal::DynamicArray<Forge::DescriptorSetUpdateBinding> updates;
        updates.PushBack(Forge::DescriptorSetUpdateBinding{.descriptor_type = Forge::DescriptorType::StorageBuffer,
                                                            .binding = 0,
                                                            .resource_info = Forge::DescriptorSetUpdateBinding::BufferInfo{.buffer = output}});
        updates.PushBack(Forge::DescriptorSetUpdateBinding{
            .descriptor_type = Forge::DescriptorType::CombinedImageSampler,
            .binding = 1,
            .array_element = 1,
            .resource_info = Forge::DescriptorSetUpdateBinding::TextureInfo{
                .sampler = sampler, .texture = texture_one, .texture_layout = Forge::ImageLayout::ShaderReadOnly}});
        updates.PushBack(Forge::DescriptorSetUpdateBinding{
            .descriptor_type = Forge::DescriptorType::CombinedImageSampler,
            .binding = 1,
            .array_element = 2,
            .resource_info = Forge::DescriptorSetUpdateBinding::TextureInfo{
                .sampler = sampler, .texture = texture_two, .texture_layout = Forge::ImageLayout::ShaderReadOnly}});
        REQUIRE(descriptor_set.Update(updates) == ErrorCode::Success);

        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            device, k_bindless_texture_source, {.entry_point = "main_bindless_textures", .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(device, pipeline_desc));

        DispatchWithSet(device, queue, pipeline, descriptor_set, k_element_count / k_group_size);

        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            const u32 expected = (i % 2) == 0 ? k_red_at_one : k_red_at_two;
            INFO("invocation " << i);
            REQUIRE(values[i] == expected);
        }
    }
    SECTION("An array element past the end of the binding is refused")
    {
        // Three of four allocated, so elements zero through two exist and element three does not, even though
        // the layout declares four. Vulkan writes such an element without reporting anything.
        Forge::DescriptorSet descriptor_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout, k_used_descriptors));
        const Forge::Texture texture = make_texture(k_red_at_one);
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(device, {.max_anisotropy = 1.0f}));

        REQUIRE(descriptor_set.Update(1, texture, sampler, Forge::ImageLayout::ShaderReadOnly, k_used_descriptors - 1) ==
                ErrorCode::Success);
        REQUIRE(descriptor_set.Update(1, texture, sampler, Forge::ImageLayout::ShaderReadOnly, k_used_descriptors) == ErrorCode::OutOfBounds);
        REQUIRE(descriptor_set.Update(1, texture, sampler, Forge::ImageLayout::ShaderReadOnly, k_max_descriptors) == ErrorCode::OutOfBounds);
        // A binding of one descriptor has only element zero, variable count or not.
        Forge::Buffer output =
            ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = 4, .usage = Forge::BufferUsageBits::StorageBuffer}));
        REQUIRE(descriptor_set.Update(0, output, 0, Forge::k_whole_buffer, 1) == ErrorCode::OutOfBounds);
    }

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

constexpr const char* k_bindless_constant_source = R"(
struct Params
{
    uint value;
};

[[vk::binding(0, 0)]] RWStructuredBuffer<uint> output;
[[vk::binding(1, 0)]] ConstantBuffer<Params> params[];

[shader("compute")]
[numthreads(64, 1, 1)]
void main_bindless_constants(uint3 thread_id : SV_DispatchThreadID)
{
    uint index = thread_id.x & 1;
    output[thread_id.x] = params[NonUniformResourceIndex(index)].value;
}
)";

TEST_CASE("Forge bindless constant buffer array", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_bindless_features{.partially_bound_descriptors = true,
                                                        .update_after_bind_descriptors = true,
                                                        .non_uniform_descriptor_indexing = true};
    if (!CanCreateDevice(k_bindless_features))
    {
        SKIP("This device does not support the descriptor indexing features bindless needs.");
    }
    if (IsSoftwareDevice())
    {
        SKIP("A software driver reports the non-uniform indexing feature and then reads element zero anyway.");
    }
    ForgeFixture fixture(k_bindless_features);
    Forge::Device& device = fixture.device;
    Forge::DeviceQueue& queue = fixture.GetQueue();

    constexpr u32 k_max_descriptors = 2;
    constexpr i32 k_element_count = 256;
    constexpr i32 k_group_size = 64;
    constexpr u32 k_value_at_zero = 11;
    constexpr u32 k_value_at_one = 4242;

    // An array of constant buffers, written one element at a time and indexed per invocation. This is the
    // descriptor kind DeviceFeatures::non_uniform_descriptor_indexing used to leave out of its mapping.
    // It does not prove that bit is doing anything - the case passes with it off, so nothing in the SPIR-V
    // this shader compiles to demands it - it covers the array and the per-element writes into it.
    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::ConstantBuffer, k_max_descriptors) == ErrorCode::Success);
    pool_desc.max_sets = 1;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::ConstantBuffer, k_max_descriptors, ShaderTypeBits::Compute) ==
            ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(device, layout_desc));
    Forge::DescriptorSet descriptor_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));

    Forge::Buffer output = MakeWipedOutput(device, k_element_count);

    // One constant register each, which is the smallest a constant buffer is laid out in.
    auto make_params = [&](u32 value)
    {
        const u32 contents[4] = {value, 0, 0, 0};
        return ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = sizeof(contents), .usage = Forge::BufferUsageBits::ConstantBuffer},
                             {reinterpret_cast<const u8*>(contents), sizeof(contents)}));
    };
    const Forge::Buffer params_zero = make_params(k_value_at_zero);
    const Forge::Buffer params_one = make_params(k_value_at_one);

    REQUIRE(descriptor_set.Update(0, output) == ErrorCode::Success);
    REQUIRE(descriptor_set.Update(1, params_zero, 0, Forge::k_whole_buffer, 0) == ErrorCode::Success);
    REQUIRE(descriptor_set.Update(1, params_one, 0, Forge::k_whole_buffer, 1) == ErrorCode::Success);

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        device, k_bindless_constant_source, {.entry_point = "main_bindless_constants", .cache = GetShaderCache()}));
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(device, pipeline_desc));

    DispatchWithSet(device, queue, pipeline, descriptor_set, k_element_count / k_group_size);

    Opal::DynamicArray<u32> values(k_element_count);
    REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
    for (i32 i = 0; i < k_element_count; ++i)
    {
        const u32 expected = (i % 2) == 0 ? k_value_at_zero : k_value_at_one;
        INFO("invocation " << i);
        REQUIRE(values[i] == expected);
    }

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge barrier vocabulary", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 256;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 77);
    const Opal::DynamicArray<u8> zeros(k_size);

    constexpr Forge::BufferUsageBits k_both_ways = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination;
    const Forge::Buffer source = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written));
    const Forge::Buffer destination = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                    {.size = k_size, .usage = k_both_ways, .host_access = Forge::HostAccess::Random}));
    REQUIRE(destination.Update(zeros) == ErrorCode::Success);

    SECTION("A copy ordered against the host with the narrow stages and access")
    {
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                   // The stages synchronization2 split out, and the access that says which
                                   // write rather than any write at all.
                                   const Forge::BufferBarrier barrier{
                                       .stages_must_finish = Forge::PipelineStageBits::Copy,
                                       .stages_must_finish_access = Forge::PipelineStageAccessBits::TransferWrite,
                                       .before_stages_start = Forge::PipelineStageBits::Host,
                                       .before_stages_start_access = Forge::PipelineStageAccessBits::HostRead,
                                       .buffer = destination};
                                   REQUIRE(command_buffer.CmdBufferBarrier(barrier) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("A batch bigger than the in-place one still works")
    {
        // Sixteen barriers, past the eight the batch keeps on the stack, so the heap path is exercised.
        constexpr i32 k_barrier_count = 16;
        Opal::DynamicArray<Forge::BufferBarrier> barriers;
        for (i32 i = 0; i < k_barrier_count; ++i)
        {
            barriers.PushBack(Forge::BufferBarrier::WriteThenRead(destination, Forge::PipelineStageBits::Copy,
                                                                  Forge::PipelineStageBits::ComputeShader));
        }
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBufferBarriers(barriers) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("A barrier naming the mesh stage without the extension is refused")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        const Forge::MemoryBarrier barrier{.stages_must_finish = Forge::PipelineStageBits::MeshShader,
                                           .stages_must_finish_access = Forge::PipelineStageAccessBits::Write,
                                           .before_stages_start = Forge::PipelineStageBits::FragmentShader,
                                           .before_stages_start_access = Forge::PipelineStageAccessBits::Read};
        REQUIRE(command_buffer.CmdMemoryBarrier(barrier) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge texture layout tracking", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr u32 k_mip_count = 4;  // 8 -> 4 -> 2 -> 1
    Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                            .width = 8,
                                            .height = 8,
                                            .mip_level_count = k_mip_count,
                                            .usage = Forge::TextureUsageBits::TransferSource |
                                                     Forge::TextureUsageBits::TransferDestination |
                                                     Forge::TextureUsageBits::Sampled}));

    SECTION("A fresh texture is undefined and a transition moves every level of it")
    {
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::Undefined);
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout(k_mip_count - 1)) == Forge::ImageLayout::Undefined);

        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
                                   REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::ShaderReadOnly);
                                   // The second one has no old layout to be told: it reads ShaderReadOnly off
                                   // the texture, which is the whole point of tracking it.
                                   REQUIRE(command_buffer.CmdTransition(texture, Forge::ImageLayout::TransferSource) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferSource);

        for (u32 level = 0; level < k_mip_count; ++level)
        {
            REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout(level)) == Forge::ImageLayout::TransferSource);
        }
    }
    SECTION("A partial range splits the grid and the whole-texture answer stops existing")
    {
        Forge::TextureBarrier barrier = Forge::TextureBarrier::ToTransferDestination(texture, Forge::ImageLayout::Undefined);
        barrier.subresource_range.first_mip_level = 0;
        barrier.subresource_range.mip_level_count = 2;
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(), [&](Forge::CommandBuffer& command_buffer)
                                       { REQUIRE(command_buffer.CmdTextureBarrier(barrier) == ErrorCode::Success); }) ==
                ErrorCode::Success);

        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout(0)) == Forge::ImageLayout::TransferDestination);
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout(1)) == Forge::ImageLayout::TransferDestination);
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout(2)) == Forge::ImageLayout::Undefined);
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout(3)) == Forge::ImageLayout::Undefined);
        REQUIRE(texture.GetCurrentLayout().GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        // A transition of the whole texture cannot say what it is coming from either.
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A subresource the texture does not have is refused")
    {
        REQUIRE(texture.GetCurrentLayout(k_mip_count).GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
        REQUIRE(texture.GetCurrentLayout(0, 1).GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
    }
    SECTION("A transfer out of a layout the role does not allow is refused")
    {
        if (!fixture.device.GetPhysicalDevice().SupportsBlit(PixelFormat::R8G8B8A8_UNORM, true) ||
            !fixture.device.GetPhysicalDevice().SupportsBlit(PixelFormat::R8G8B8A8_UNORM, false))
        {
            SKIP("This device cannot blit R8G8B8A8_UNORM either way.");
        }
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
        // Halving mip 0 into mip 1, which is what mip generation does, but with both levels left where a
        // shader reads them rather than where a transfer does.
        const Forge::TextureBlitRegion region{.source = {.mip_level = 0}, .destination = {.mip_level = 1}};
        REQUIRE(command_buffer.CmdBlitTexture(texture, texture, {&region, 1}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * A barrier whose range reaches past its resource, refused before anything is recorded. The texture half used
 * to be caught by the layout bookkeeping after the barrier was already in the command buffer, and the buffer
 * half was not caught at all - both reached the driver, which is what the validation assertion at the end
 * would have named.
 */
TEST_CASE("Forge barriers over a range their resource does not have", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr u32 k_mip_count = 2;
    constexpr u64 k_buffer_size = 256;

    auto make_texture = [&]
    {
        return ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                                         .width = 4,
                                                                         .height = 4,
                                                                         .mip_level_count = k_mip_count,
                                                                         .usage = Forge::TextureUsageBits::TransferDestination}));
    };
    const Forge::Buffer buffer =
        ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_buffer_size, .usage = Forge::BufferUsageBits::StorageBuffer}));
    Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    REQUIRE(command_buffer.Begin() == ErrorCode::Success);

    SECTION("A texture barrier over mips or layers the texture lacks is refused, and moves no tracked layout")
    {
        Forge::Texture texture = make_texture();
        Forge::TextureBarrier past_the_mips = Forge::TextureBarrier::ToTransferDestination(texture, Forge::ImageLayout::Undefined);
        past_the_mips.subresource_range.first_mip_level = k_mip_count;
        past_the_mips.subresource_range.mip_level_count = 1;
        REQUIRE(command_buffer.CmdTextureBarrier(past_the_mips) == ErrorCode::OutOfBounds);

        Forge::TextureBarrier past_the_layers = Forge::TextureBarrier::ToTransferDestination(texture, Forge::ImageLayout::Undefined);
        past_the_layers.subresource_range.first_array_layer = 1;
        past_the_layers.subresource_range.array_layer_count = 1;
        REQUIRE(command_buffer.CmdTextureBarrier(past_the_layers) == ErrorCode::OutOfBounds);

        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::Undefined);
        // The same range asked of the tracker directly is refused the same way.
        REQUIRE(texture.GetCurrentLayout(past_the_mips.subresource_range).GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
    }
    SECTION("A batch with one bad texture barrier moves none of the textures in it")
    {
        // The first barrier is fine and the second is not. Checked after recording, the first texture's tracked
        // layout had already moved to a layout the refused command never took it to.
        Forge::Texture good = make_texture();
        Forge::Texture bad = make_texture();
        Forge::TextureBarrier bad_barrier = Forge::TextureBarrier::ToTransferDestination(bad, Forge::ImageLayout::Undefined);
        bad_barrier.subresource_range.first_mip_level = k_mip_count;
        bad_barrier.subresource_range.mip_level_count = 1;
        const Forge::TextureBarrier batch[] = {Forge::TextureBarrier::ToTransferDestination(good, Forge::ImageLayout::Undefined),
                                               bad_barrier.Clone()};
        REQUIRE(command_buffer.CmdTextureBarriers({batch, 2}) == ErrorCode::OutOfBounds);
        REQUIRE(ForgeTest::Unwrap(good.GetCurrentLayout()) == Forge::ImageLayout::Undefined);
        REQUIRE(ForgeTest::Unwrap(bad.GetCurrentLayout()) == Forge::ImageLayout::Undefined);
    }
    SECTION("A buffer barrier past the end of the buffer is refused")
    {
        auto barrier_over = [&](u64 offset, u64 size)
        {
            return Forge::BufferBarrier{.stages_must_finish = Forge::PipelineStageBits::ComputeShader,
                                        .stages_must_finish_access = Forge::PipelineStageAccessBits::ShaderWrite,
                                        .before_stages_start = Forge::PipelineStageBits::ComputeShader,
                                        .before_stages_start_access = Forge::PipelineStageAccessBits::ShaderRead,
                                        .buffer = buffer,
                                        .offset = offset,
                                        .size = size};
        };
        // Starting at the end, starting past it, and a size that runs off it from an offset that is fine.
        REQUIRE(command_buffer.CmdBufferBarrier(barrier_over(k_buffer_size, Forge::k_whole_buffer)) == ErrorCode::OutOfBounds);
        REQUIRE(command_buffer.CmdBufferBarrier(barrier_over(k_buffer_size + 4, 4)) == ErrorCode::OutOfBounds);
        REQUIRE(command_buffer.CmdBufferBarrier(barrier_over(128, 132)) == ErrorCode::OutOfBounds);
        // A barrier over no bytes is not a range at all.
        REQUIRE(command_buffer.CmdBufferBarrier(barrier_over(0, 0)) == ErrorCode::InvalidArgument);
        // And the edges that fit are recorded.
        REQUIRE(command_buffer.CmdBufferBarrier(barrier_over(128, 128)) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdBufferBarrier(barrier_over(252, Forge::k_whole_buffer)) == ErrorCode::Success);
    }
    REQUIRE(command_buffer.End() == ErrorCode::Success);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge debug labels", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 256;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 17);

    const Forge::Buffer source = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written));
    const Forge::Buffer destination = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size,
                                                     .usage = Forge::BufferUsageBits::TransferDestination,
                                                     .host_access = Forge::HostAccess::Random}));

    SECTION("A labelled region records and the work inside it still runs")
    {
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           REQUIRE(command_buffer.CmdBeginDebugLabel("copy region", {0.2f, 0.6f, 1.0f, 1.0f}) ==
                                                   ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdInsertDebugLabel("about to copy") == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdEndDebugLabel() == ErrorCode::Success);
                                       }) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("Regions nest")
    {
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBeginDebugLabel("frame") == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBeginDebugLabel("copy pass") == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdEndDebugLabel() == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdEndDebugLabel() == ErrorCode::Success);
                               }) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("ScopedDebugLabel closes the region it opened")
    {
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   const Forge::ScopedDebugLabel scope(command_buffer, "copy pass", {1.0f, 0.5f, 0.0f, 1.0f});
                                   REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("The StringUtf8 overloads open, mark and close a region the same way")
    {
        // Every other section names its labels with a literal, which picks the const char* overloads. A name
        // built at run time arrives as a StringUtf8, and those are three overloads of their own.
        const Opal::StringUtf8 region_name("copy region");
        const Opal::StringUtf8 marker_name("about to copy");
        const Opal::StringUtf8 scope_name("scoped copy region");
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           REQUIRE(command_buffer.CmdBeginDebugLabel(region_name, {0.2f, 0.6f, 1.0f, 1.0f}) ==
                                                   ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdInsertDebugLabel(marker_name) == ErrorCode::Success);
                                           {
                                               const Forge::ScopedDebugLabel scope(command_buffer, scope_name);
                                               REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                           }
                                           REQUIRE(command_buffer.CmdEndDebugLabel() == ErrorCode::Success);
                                       }) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("A region left open by a refused command is still closed")
    {
        // The point of the guard: the copy below is rejected while it is recorded, and the region has to end
        // on the way out anyway. A region left open is what the layer would report at End().
        const Forge::Buffer no_transfer =
            ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::ConstantBuffer}));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        {
            const Forge::ScopedDebugLabel scope(command_buffer, "doomed pass");
            REQUIRE(command_buffer.CmdCopyBuffer(no_transfer, destination) == ErrorCode::InvalidArgument);
        }
        REQUIRE(command_buffer.End() == ErrorCode::Success);
        // Not submitted: work the layer rejected while it was recorded is undefined behaviour once it runs.
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge GPU event macros", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 256;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 23);

    const Forge::Buffer source = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written));
    const Forge::Buffer destination = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size,
                                                     .usage = Forge::BufferUsageBits::TransferDestination,
                                                     .host_access = Forge::HostAccess::Random}));

    // The layer reports a command buffer ended with a region still open, so an unbalanced macro fails these
    // through REQUIRE_NO_VALIDATION_ERROR rather than through the readback.
    SECTION("The scoped macro brackets the work and the work still runs")
    {
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           RNDR_GPU_EVENT_SCOPED(command_buffer, "copy pass");
                                           REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                       }) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("The begin and end macros are the same region written out, with or without the name on the end")
    {
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           RNDR_GPU_EVENT_BEGIN(command_buffer, "copy pass");
                                           REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                           RNDR_GPU_EVENT_END(command_buffer, "copy pass");
                                           // The end macro takes the name for symmetry with Canvas and
                                           // nothing reads it, so the same region closes without one.
                                           RNDR_GPU_EVENT_BEGIN(command_buffer, "copy pass again");
                                           REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                           RNDR_GPU_EVENT_END(command_buffer);
                                       }) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("Both arities of one macro compile side by side")
    {
        // One macro spells both backends, and which one a call site means is decided by its arguments. That
        // the single argument form still resolves is the half a change to these macros could break in
        // silence, so it is compiled here - inside a lambda that is never called, since the Canvas path
        // reaches for an OpenGL context this test does not have.
        const auto canvas_form = []()
        {
            RNDR_GPU_EVENT_SCOPED("a Canvas region");
            RNDR_GPU_EVENT_BEGIN("a Canvas region");
            RNDR_GPU_EVENT_END("a Canvas region");
        };
        RNDR_UNUSED(canvas_form);
        SUCCEED();
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge timestamp queries", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_element_count = 4096;
    constexpr i32 k_group_size = 64;

    const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                .usage = Forge::BufferUsageBits::StorageBuffer,
                                                .host_access = Forge::HostAccess::Random,
                                                .use_device_address = true}));
    const Forge::Shader compute_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
    const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, compute_shader);
    const VkDeviceAddress output_address = output.GetNativeDeviceAddress();

    // The dispatch every measurement below wraps, so what differs between them is only how it is timed.
    auto record_dispatch = [&](Forge::CommandBuffer& command_buffer)
    {
        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(output_address)) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
    };

    SECTION("A span around a dispatch comes back as a plausible duration")
    {
        Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
        REQUIRE(pool.IsValid());
        REQUIRE(pool.GetQueryCount() == 2);
        Forge::SetDebugName(fixture.device, pool, "dispatch timing");

        // A pool that has been reset and not yet written has nothing to read, which is what the first frames
        // of a per-frame pool look like and the reason a frame loop asks rather than blocking. Reading one
        // that was never reset at all is not this case: that is undefined, and the layer says so.
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(), [&](Forge::CommandBuffer& command_buffer)
                                       { REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success); }) == ErrorCode::Success);
        f64 too_early_ms = -1.0;
        REQUIRE_FALSE(ForgeTest::Unwrap(pool.TryGetElapsedMilliseconds(0, 1, too_early_ms)));
        REQUIRE(too_early_ms == -1.0);

        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineStart) ==
                                           ErrorCode::Success);
                                   record_dispatch(command_buffer);
                                   REQUIRE(command_buffer.CmdWriteTimestamp(pool, 1, Forge::PipelineStageBits::PipelineEnd) ==
                                           ErrorCode::Success);
                               }) == ErrorCode::Success);

        Opal::InPlaceArray<u64, 2> ticks;
        REQUIRE(pool.GetResults({ticks.GetData(), 2}) == ErrorCode::Success);
        INFO("ticks " << ticks[0] << " -> " << ticks[1] << ", period " << pool.GetTimestampPeriod() << " ns");
        REQUIRE(ticks[1] >= ticks[0]);

        // ImmediateSubmit has already waited, so the result is there without blocking.
        f64 elapsed_ms = -1.0;
        REQUIRE(ForgeTest::Unwrap(pool.TryGetElapsedMilliseconds(0, 1, elapsed_ms)));
        INFO("elapsed " << elapsed_ms << " ms");
        REQUIRE(elapsed_ms >= 0.0);
        // A dispatch this small cannot take a second on a device that finished it, so a figure above one
        // says the period or the valid bits were applied wrong rather than that the device was slow.
        REQUIRE(elapsed_ms < 1000.0);
        REQUIRE(elapsed_ms == ForgeTest::Unwrap(pool.GetElapsedMilliseconds(0, 1)));
    }
    SECTION("Two writes into a drained pipeline measure the dispatch on its own")
    {
        // The other pattern: PipelineEnd on both sides, so the write in front waits for everything before it
        // and the difference covers the dispatch and nothing else.
        Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineEnd) ==
                                           ErrorCode::Success);
                                   record_dispatch(command_buffer);
                                   REQUIRE(command_buffer.CmdWriteTimestamp(pool, 1, Forge::PipelineStageBits::PipelineEnd) ==
                                           ErrorCode::Success);
                               }) == ErrorCode::Success);
        const f64 elapsed_ms = ForgeTest::Unwrap(pool.GetElapsedMilliseconds(0, 1));
        INFO("isolated " << elapsed_ms << " ms");
        REQUIRE(elapsed_ms >= 0.0);
        REQUIRE(elapsed_ms < 1000.0);
    }
    SECTION("A pair at the ends of a pool is readable with the queries between them unwritten")
    {
        // Reading the two as one range would report the whole range unavailable, since the middle was never
        // written, and a measurement that never arrives is indistinguishable from a device that is behind.
        Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 4}));
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineStart) ==
                                           ErrorCode::Success);
                                   record_dispatch(command_buffer);
                                   REQUIRE(command_buffer.CmdWriteTimestamp(pool, 3, Forge::PipelineStageBits::PipelineEnd) ==
                                           ErrorCode::Success);
                               }) == ErrorCode::Success);
        f64 elapsed_ms = -1.0;
        REQUIRE(ForgeTest::Unwrap(pool.TryGetElapsedMilliseconds(0, 3, elapsed_ms)));
        REQUIRE(elapsed_ms >= 0.0);
    }
    SECTION("Resetting from the host needs the feature")
    {
        const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
        REQUIRE(pool.Reset() == ErrorCode::InvalidArgument);
    }
    SECTION("A pool that asks for no queries is refused")
    {
        REQUIRE(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 0}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A query past the end of the pool is refused")
    {
        Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdWriteTimestamp(pool, 2) == ErrorCode::OutOfBounds);
        REQUIRE(command_buffer.CmdResetQueryPool(pool, 1, 2) == ErrorCode::OutOfBounds);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A timestamp naming more than one stage is refused")
    {
        Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::VertexShader |
                                                                        Forge::PipelineStageBits::FragmentShader) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::None) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge timestamp queries reset from the host", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture({.host_query_reset = true});

    constexpr i32 k_size = 256;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 41);
    const Forge::Buffer source = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written));
    const Forge::Buffer destination =
        ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferDestination}));

    const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
    // The whole point of the host side: the pool is made ready without a command buffer having to carry it.
    REQUIRE(pool.Reset() == ErrorCode::Success);
    REQUIRE(Forge::ImmediateSubmit(
                fixture.device, fixture.GetQueue(),
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineStart) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdWriteTimestamp(pool, 1, Forge::PipelineStageBits::PipelineEnd) == ErrorCode::Success);
                }) == ErrorCode::Success);
    f64 elapsed_ms = -1.0;
    REQUIRE(ForgeTest::Unwrap(pool.TryGetElapsedMilliseconds(0, 1, elapsed_ms)));
    REQUIRE(elapsed_ms >= 0.0);

    // Reset again and the results are gone, which is what makes a per-frame pool reusable.
    REQUIRE(pool.Reset() == ErrorCode::Success);
    f64 after_reset_ms = -1.0;
    REQUIRE_FALSE(ForgeTest::Unwrap(pool.TryGetElapsedMilliseconds(0, 1, after_reset_ms)));
    REQUIRE(after_reset_ms == -1.0);

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/** Writes what the storage buffer bound at binding 0 says, so a descriptor written the short way is checkable. */
constexpr const char* k_descriptor_source = R"(
[shader("compute")]
[numthreads(64, 1, 1)]
void main_descriptor(uint3 thread_id : SV_DispatchThreadID, uniform RWStructuredBuffer<uint> output)
{
    output[thread_id.x] = thread_id.x + 7;
}
)";

TEST_CASE("Forge single resource descriptor updates", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_element_count = 128;
    constexpr i32 k_group_size = 64;

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 4) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 4) == ErrorCode::Success);
    pool_desc.max_sets = 4;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(2, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    SECTION("The descriptor type comes from the layout")
    {
        const Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(ForgeTest::Unwrap(set.GetBindingDescriptorType(0)) == Forge::DescriptorType::StorageBuffer);
        REQUIRE(ForgeTest::Unwrap(set.GetBindingDescriptorType(2)) == Forge::DescriptorType::CombinedImageSampler);
        // Binding 1 is a gap in this layout, which is a binding index the set has to reject rather than
        // guess a type for.
        REQUIRE(set.GetBindingDescriptorType(1).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A buffer written the short way reaches the shader")
    {
        const Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, output) == ErrorCode::Success);

        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(fixture.device, k_descriptor_source,
                                                                       {.entry_point = "main_descriptor", .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        DispatchWithSet(fixture.device, fixture.GetQueue(), pipeline, set, k_element_count / k_group_size);

        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            REQUIRE(values[i] == static_cast<u32>(i) + 7);
        }
    }
    SECTION("A range past the end of the buffer is refused")
    {
        const Forge::Buffer small = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                  {.size = 256, .usage = Forge::BufferUsageBits::StorageBuffer}));
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, small, 128, 256) == ErrorCode::OutOfBounds);
        REQUIRE(set.Update(0, small, 0, 0) == ErrorCode::InvalidArgument);
    }
    SECTION("Writing a binding the layout does not have is refused")
    {
        const Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                   {.size = 256, .usage = Forge::BufferUsageBits::StorageBuffer}));
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(3, buffer) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge rendering without a depth attachment", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    Forge::Texture color = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                          .width = k_side,
                                          .height = k_side,
                                          .usage = Forge::TextureUsageBits::ColorAttachment |
                                                   Forge::TextureUsageBits::TransferSource}));

    SECTION("An absent depth attachment renders colour only")
    {
        // No pipeline and no draw: the load operation is what writes the attachment, so what comes back says
        // the pass ran with a colour attachment and nothing else.
        const Opal::DynamicArray<u8> pixels =
            RenderRaster(fixture, color, k_side, [](Forge::CommandBuffer&) {}, Vector4f{1.0f, 0.0f, 1.0f, 1.0f});
        // Zero and one are the only channel values a UNORM format converts exactly, so this compares
        // what was cleared rather than how the driver rounds.
        for (i32 i = 0; i < pixels.GetSize(); i += 4)
        {
            REQUIRE(static_cast<i32>(pixels[i]) == 255);
            REQUIRE(static_cast<i32>(pixels[i + 1]) == 0);
            REQUIRE(static_cast<i32>(pixels[i + 2]) == 255);
            REQUIRE(static_cast<i32>(pixels[i + 3]) == 255);
        }
    }
    SECTION("A depth attachment that names no texture is refused")
    {
        // What the old convention expressed as "no depth". Now that absent says it, a present attachment
        // pointing at nothing is a filled-in desc somebody forgot to finish. The colour attachment is
        // transitioned first so that the refusal is the depth one and not the layout check on the colour.
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{
            .render_area_extent = {k_side, k_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}},
            .depth_attachment = Forge::RenderingAttachmentDesc{}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A colour clear value on a depth attachment is refused")
    {
        // The misuse a union could not catch and the validation layer cannot either, VkClearValue being the
        // same union: the depth attachment would have cleared to whatever the first two floats of the vector
        // mean as a depth and a stencil. A depth attachment with no clear value written is the same mistake,
        // since the default holds a colour and Clear is the default load operation.
        Forge::Texture depth = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::D32_SFLOAT,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::DepthStencilAttachment}));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth)) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{
            .render_area_extent = {k_side, k_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}},
            .depth_attachment = Forge::RenderingAttachmentDesc{.texture = depth,
                                                               .clear_value = Vector4f{0.0f, 0.0f, 0.0f, 1.0f}}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::InvalidArgument);

        // The same attachment loading instead of clearing is fine: nothing reads the value, so nothing can
        // read the wrong member of it.
        const Forge::RenderingDesc load_desc{
            .render_area_extent = {k_side, k_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}},
            .depth_attachment = Forge::RenderingAttachmentDesc{.texture = depth,
                                                               .load_operation = Forge::AttachmentLoadOperation::Load}};
        REQUIRE(command_buffer.CmdBeginRendering(load_desc) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A depth clear value on a colour attachment is refused")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{
            .render_area_extent = {k_side, k_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{
                .texture = color, .clear_value = Forge::DepthStencilClearValue{.depth = 1.0f, .stencil = 0}}}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("An attachment whose texture was never transitioned is refused")
    {
        // The check the old API could not make: a colour attachment naming a texture no barrier has moved
        // out of Undefined. Vulkan rejects an undefined attachment layout, but a layout that is legal and
        // wrong - ShaderReadOnly on a texture the barriers left in ColorAttachment, say - it accepts, and
        // reading the layout off the texture is what removes both.
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{.render_area_extent = {k_side, k_side},
                                                  .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}}};
        REQUIRE(ForgeTest::Unwrap(color.GetCurrentLayout()) == Forge::ImageLayout::Undefined);
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A colour attachment in a layout meant for something else is refused")
    {
        // TransferSource is a layout this texture legitimately reaches - ReadBackTexture leaves it there -
        // so this is the plausible-but-wrong case rather than the unconfigured one above.
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTransition(color, Forge::ImageLayout::TransferSource) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{.render_area_extent = {k_side, k_side},
                                                  .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A colour attachment in the General layout is accepted")
    {
        // General is legal for every role, which is what makes it the layout a texture used two ways sits in.
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   // Written by hand rather than through ToGeneral, whose access is the
                                   // shader read and write of a storage image: what follows here is the
                                   // colour attachment output, and an access that does not match its stage
                                   // is invalid whichever way round it is wrong.
                                   REQUIRE(command_buffer.CmdTextureBarrier(
                                       Forge::TextureBarrier{.stages_must_finish = Forge::PipelineStageBits::PipelineStart,
                                                             .before_stages_start = Forge::PipelineStageBits::ColorAttachmentOutput,
                                                             .before_stages_start_access = Forge::PipelineStageAccessBits::Write,
                                                             .old_layout = Forge::ImageLayout::Undefined,
                                                             .new_layout = Forge::ImageLayout::General,
                                                             .texture = color}) == ErrorCode::Success);
                                   const Forge::RenderingDesc rendering_desc{
                                       .render_area_extent = {k_side, k_side},
                                       .color_attachments = {Forge::RenderingAttachmentDesc{
                                           .texture = color,
                                           .load_operation = Forge::AttachmentLoadOperation::Clear,
                                           .store_operation = Forge::AttachmentStoreOperation::Store,
                                           .clear_value = Vector4f{0.0f, 1.0f, 0.0f, 1.0f}}}};
                                   REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                               }) == ErrorCode::Success);

        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, color, k_side);
        for (i32 i = 0; i < pixels.GetSize(); i += 4)
        {
            REQUIRE(static_cast<i32>(pixels[i]) == 0);
            REQUIRE(static_cast<i32>(pixels[i + 1]) == 255);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * A triangle that covers the whole target, writing one constant colour. Its positions come from a vertex
 * buffer rather than from SV_VertexID: that maps to gl_VertexIndex, whose SPIR-V DrawParameters
 * capability needs a device feature, and a test should not turn one on to draw a triangle.
 */
constexpr const char* k_fullscreen_source = R"(
struct VertexOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput main_vertex(float2 position : POSITION)
{
    VertexOutput output;
    output.position = float4(position, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 main_fragment() : SV_Target
{
    return float4(0.0, 1.0, 0.0, 0.0);
}
)";

/** Three vertices that cover the whole target, so every texel of it is written by the one draw. */
constexpr f32 k_fullscreen_vertices[] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};

TEST_CASE("Forge color write mask", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));

    /**
     * Clear the target to opaque red, draw the triangle through a pipeline with the given mask, and hand back
     * one texel. The shader writes (0, 1, 0, 0), so every channel differs from what the clear left, and a
     * channel that comes back red is one the mask kept the draw away from.
     */
    const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                 {.size = sizeof(k_fullscreen_vertices),
                                  .usage = Forge::BufferUsageBits::VertexBuffer},
                                 Opal::AsBytes(k_fullscreen_vertices)));

    auto draw_through_mask = [&](Forge::ColorWriteMaskBits mask)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);

        Forge::GraphicsPipelineDesc pipeline_desc = MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, k_format);
        pipeline_desc.color_blend_attachments[0].color_write_mask = mask;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                                                           });
        return Opal::DynamicArray<u8>{pixels[0], pixels[1], pixels[2], pixels[3]};
    };

    SECTION("Every channel is written when nothing is masked out")
    {
        const Opal::DynamicArray<u8> texel = draw_through_mask(Forge::ColorWriteMaskBits::All);
        INFO("rgba " << static_cast<i32>(texel[0]) << " " << static_cast<i32>(texel[1]) << " "
                     << static_cast<i32>(texel[2]) << " " << static_cast<i32>(texel[3]));
        REQUIRE(static_cast<i32>(texel[0]) == 0);
        REQUIRE(static_cast<i32>(texel[1]) == 255);
        REQUIRE(static_cast<i32>(texel[2]) == 0);
        REQUIRE(static_cast<i32>(texel[3]) == 0);
    }
    SECTION("A masked out channel keeps what the clear left")
    {
        // Green only: the two channels the clear set stay red and opaque, and green is the one the draw moved.
        const Opal::DynamicArray<u8> texel = draw_through_mask(Forge::ColorWriteMaskBits::Green);
        INFO("rgba " << static_cast<i32>(texel[0]) << " " << static_cast<i32>(texel[1]) << " "
                     << static_cast<i32>(texel[2]) << " " << static_cast<i32>(texel[3]));
        REQUIRE(static_cast<i32>(texel[0]) == 255);
        REQUIRE(static_cast<i32>(texel[1]) == 255);
        REQUIRE(static_cast<i32>(texel[2]) == 0);
        REQUIRE(static_cast<i32>(texel[3]) == 255);
    }
    SECTION("Masking every channel out leaves the attachment as it was cleared")
    {
        const Opal::DynamicArray<u8> texel = draw_through_mask(Forge::ColorWriteMaskBits::None);
        REQUIRE(static_cast<i32>(texel[0]) == 255);
        REQUIRE(static_cast<i32>(texel[1]) == 0);
        REQUIRE(static_cast<i32>(texel[2]) == 0);
        REQUIRE(static_cast<i32>(texel[3]) == 255);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * The setup every draw case below renders with, and why one readback answers two questions at once. The
 * target is split down the middle by the geometry: which half comes back written says which vertices the
 * draw reached, and which channel it is written in says which instance it fetched. Every channel is zero or
 * one, so nothing here depends on how a UNORM format rounds.
 */
namespace
{

/** The four channels of one texel of a tightly packed RGBA readback. */
struct Texel
{
    i32 r = 0;
    i32 g = 0;
    i32 b = 0;
    i32 a = 0;
};

bool operator==(const Texel& lhs, const Texel& rhs)
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

/** What the clear left, which is a half no draw covered. */
constexpr Texel k_untouched{0, 0, 0, 255};
/** The three instance values below, one channel each. */
constexpr Texel k_instance_one{255, 0, 0, 255};
constexpr Texel k_instance_two{0, 255, 0, 255};
constexpr Texel k_instance_three{0, 0, 255, 255};

/**
 * One channel per instance value rather than a value per instance: a channel is either full or empty, which a
 * UNORM target converts exactly, so a mismatch is a wrong instance and never a rounding step.
 *
 * The value is flat, so the whole triangle carries the provoking vertex's copy of it. It comes from a
 * per-instance binding rather than from SV_InstanceID, which spares this the DrawParameters capability the
 * builtin drags in - and it is what makes first_instance visible, since that is the index the per-instance
 * binding is fetched at.
 */
constexpr const char* k_halves_source = R"(
struct VertexOutput
{
    float4 position : SV_Position;
    nointerpolation uint value : VALUE;
};

[shader("vertex")]
VertexOutput main_vertex(float2 position : POSITION, uint value : VALUE)
{
    VertexOutput output;
    output.position = float4(position, 0.0, 1.0);
    output.value = value;
    return output;
}

[shader("fragment")]
float4 main_fragment(VertexOutput input) : SV_Target
{
    return float4(input.value == 1 ? 1.0 : 0.0, input.value == 2 ? 1.0 : 0.0, input.value == 3 ? 1.0 : 0.0, 1.0);
}
)";

/**
 * The indirect commands, written by the device rather than by the host. A buffer the host filled would not
 * tell an indirect draw apart from a direct one, since the same values would be in the same place either way.
 */
constexpr const char* k_indirect_command_source = R"(
// vertex_count, instance_count, first_vertex, first_instance - the left half at instance one, then the right
// half at instance two.
[shader("compute")]
[numthreads(1, 1, 1)]
void main_write_draws(uniform uint32_t *output)
{
    output[0] = 6; output[1] = 1; output[2] = 0; output[3] = 1;
    output[4] = 6; output[5] = 1; output[6] = 6; output[7] = 2;
}

// index_count, instance_count, first_index, vertex_offset, first_instance - the left corners displaced by
// four, which is the right half, at instance two.
[shader("compute")]
[numthreads(1, 1, 1)]
void main_write_indexed_draw(uniform uint32_t *output)
{
    output[0] = 6; output[1] = 1; output[2] = 0; output[3] = 4; output[4] = 2;
}

// The three group counts of the dispatch that follows this one.
[shader("compute")]
[numthreads(1, 1, 1)]
void main_write_dispatch(uniform uint32_t *output)
{
    output[0] = 4; output[1] = 1; output[2] = 1;
}
)";

/** The four corners of the left half of the target followed by the four of the right, for the index buffers. */
constexpr f32 k_half_corners[] = {
    -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f,  // left, corners 0 to 3
    0.0f,  -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 0.0f,  1.0f,  // right, corners 4 to 7
};

/** The same two halves with the corners already repeated, for the draws that use no index buffer. */
constexpr f32 k_half_vertices[] = {
    -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f,  // left, vertices 0 to 5
    0.0f,  -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 0.0f,  -1.0f, 1.0f, 1.0f, 0.0f,  1.0f,  // right, vertices 6 to 11
};

/**
 * Two triangles per half, naming the corners above. The first six are padding that draws nothing: a draw that
 * ignored first_index would read them, be handed three copies of one corner, and rasterize no pixel at all -
 * which is a different answer from ignoring the vertex offset, so one readback tells the two apart.
 */
constexpr u32 k_half_indices[] = {0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 2, 3};

/** One value per instance, so instance 0 is red, 1 is green and 2 is blue. */
constexpr u32 k_instance_values[] = {1, 2, 3};

/** The same indices as bytes of the requested width, so one list drives all three IndexSize values. */
Opal::DynamicArray<u8> ToIndexBytes(const u32* indices, i32 count, IndexSize index_size)
{
    const i32 stride = index_size == IndexSize::uint8 ? 1 : (index_size == IndexSize::uint16 ? 2 : 4);
    Opal::DynamicArray<u8> bytes(count * stride);
    for (i32 i = 0; i < count; ++i)
    {
        for (i32 byte = 0; byte < stride; ++byte)
        {
            bytes[i * stride + byte] = static_cast<u8>((indices[i] >> (byte * 8)) & 0xFF);
        }
    }
    return bytes;
}

/** Everything the two-halves target is rendered with, built once per case. */
struct HalvesFixture
{
    static constexpr i32 k_side = 4;
    static constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    ForgeFixture forge;
    Forge::Shader vertex_shader;
    Forge::Shader fragment_shader;
    Forge::Pipeline pipeline;
    /** The eight corners the index buffers name. */
    Forge::Buffer corners;
    /** The same halves with the corners repeated, for a draw that uses no index buffer. */
    Forge::Buffer vertices;
    /** One value per instance, bound to the per-instance binding. */
    Forge::Buffer instances;
    Forge::Texture color;

    explicit HalvesFixture(const Forge::DeviceFeatures& features = {}) : forge(features)
    {
        vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(forge.device, k_halves_source,
                                                          {.entry_point = "main_vertex", .cache = GetShaderCache()}));
        fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(forge.device, k_halves_source,
                                                            {.entry_point = "main_fragment", .cache = GetShaderCache()}));

        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        // Off, so that which way a triangle winds is never what a failing case is about.
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        pipeline_desc.vertex_input.AddBinding(1, sizeof(u32), DataRepetition::PerInstance);
        REQUIRE(pipeline_desc.vertex_input.AddAttribute(1, 1, PixelFormat::R32_UINT, 0) == ErrorCode::Success);
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
        pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(forge.device, pipeline_desc));

        corners = ForgeTest::Unwrap(Forge::Buffer::Create(
            forge.device, {.size = sizeof(k_half_corners), .usage = Forge::BufferUsageBits::VertexBuffer}, Opal::AsBytes(k_half_corners)));
        vertices = ForgeTest::Unwrap(Forge::Buffer::Create(forge.device,
                                                           {.size = sizeof(k_half_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                                                           Opal::AsBytes(k_half_vertices)));
        instances = ForgeTest::Unwrap(
            Forge::Buffer::Create(forge.device, {.size = sizeof(k_instance_values), .usage = Forge::BufferUsageBits::VertexBuffer},
                                  Opal::AsBytes(k_instance_values)));
        color = ForgeTest::Unwrap(Forge::Texture::Create(forge.device, {.format = k_format,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::ColorAttachment |
                                                       Forge::TextureUsageBits::TransferSource}));
    }

    Forge::DeviceQueue& GetQueue() { return forge.GetQueue(); }

    /**
     * Clear the target, record what the caller asks for and hand back the readback. The per-instance binding
     * is bound here and the per-vertex one is not: which of the two vertex buffers a case wants is the whole
     * difference between an indexed draw and one that is not.
     *
     * @param record_before Recorded ahead of the pass, for the compute dispatch an indirect case needs.
     * @param record_draw Recorded inside the pass, with the pipeline and the instance binding already bound.
     */
    template <typename RecordBefore, typename RecordDraw>
    Opal::DynamicArray<u8> Render(RecordBefore&& record_before, RecordDraw&& record_draw)
    {
        REQUIRE(Forge::ImmediateSubmit(forge.device, GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   record_before(command_buffer);
                                   REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) ==
                                           ErrorCode::Success);
                                   const Forge::RenderingDesc rendering_desc{
                                       .render_area_extent = {k_side, k_side},
                                       .color_attachments = {Forge::RenderingAttachmentDesc{
                                           .texture = color,
                                           .load_operation = Forge::AttachmentLoadOperation::Clear,
                                           .store_operation = Forge::AttachmentStoreOperation::Store,
                                           .clear_value = Vector4f{0.0f, 0.0f, 0.0f, 1.0f}}}};
                                   REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindVertexBuffer(instances, 1) == ErrorCode::Success);
                                   record_draw(command_buffer);
                                   REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                               }) == ErrorCode::Success);

        // Left in TransferSource:
        return ReadColorPixels(forge, color, k_side);
    }

    template <typename RecordDraw>
    Opal::DynamicArray<u8> Render(RecordDraw&& record_draw)
    {
        return Render([](Forge::CommandBuffer&) {}, record_draw);
    }
};

Texel GetTexel(const Opal::DynamicArray<u8>& pixels, i32 x, i32 y)
{
    const i32 base = (y * HalvesFixture::k_side + x) * 4;
    return {pixels[base], pixels[base + 1], pixels[base + 2], pixels[base + 3]};
}

/**
 * The colour of one half of the readback, which every texel of that half has to share. A draw that covered
 * part of a half rather than all of it is exactly the mistake these cases look for, so the half is checked to
 * be uniform before it is reduced to one value.
 */
Texel GetHalfColor(const Opal::DynamicArray<u8>& pixels, bool right_half)
{
    constexpr i32 k_half = HalvesFixture::k_side / 2;
    const i32 first_column = right_half ? k_half : 0;
    const Texel expected = GetTexel(pixels, first_column, 0);
    for (i32 y = 0; y < HalvesFixture::k_side; ++y)
    {
        for (i32 x = first_column; x < first_column + k_half; ++x)
        {
            INFO("texel " << x << "," << y << " differs from the rest of its half");
            REQUIRE(GetTexel(pixels, x, y) == expected);
        }
    }
    return expected;
}

}  // namespace

/** Fails with both colours spelled out when a half is not the colour it should be. */
#define REQUIRE_HALF_COLOR(pixels, right_half, expected)                                                          \
    do                                                                                                            \
    {                                                                                                             \
        const Texel half_color = GetHalfColor(pixels, right_half);                                                \
        INFO("expected rgba " << (expected).r << " " << (expected).g << " " << (expected).b << " " << (expected).a \
                              << ", got " << half_color.r << " " << half_color.g << " " << half_color.b << " "    \
                              << half_color.a);                                                                   \
        REQUIRE(half_color == (expected));                                                                        \
    } while (false)

namespace
{

/**
 * One draw writing two colour attachments, each a constant of its own. The two values share no channel, so
 * an attachment that came back as the other one says the targets were swapped and one that came back as the
 * clear says the draw never reached it.
 */
constexpr const char* k_two_target_source = R"(
struct TwoTargets
{
    float4 first : SV_Target0;
    float4 second : SV_Target1;
};

[shader("vertex")]
float4 main_two_target_vertex(float2 position : POSITION) : SV_Position
{
    return float4(position, 0.0, 1.0);
}

[shader("fragment")]
TwoTargets main_two_target_fragment()
{
    TwoTargets output;
    output.first = float4(0.0, 1.0, 0.0, 1.0);
    output.second = float4(0.0, 0.0, 1.0, 1.0);
    return output;
}
)";

/** What the two attachments are cleared to, and what each one holds once the draw has written it. */
constexpr Texel k_cleared{255, 0, 0, 255};
constexpr Texel k_first_target{0, 255, 0, 255};
constexpr Texel k_second_target{0, 0, 255, 255};

/** The square both two-target cases render into, and the format its attachments carry. */
constexpr i32 k_two_target_side = 4;
constexpr PixelFormat k_two_target_format = PixelFormat::R8G8B8A8_UNORM;

/** A colour attachment of the size the two-target cases render at. */
Forge::Texture MakeTwoTargetAttachment(const Forge::Device& device)
{
    return ForgeTest::Unwrap(Forge::Texture::Create(
        device,
        {.format = k_two_target_format,
         .width = k_two_target_side,
         .height = k_two_target_side,
         .usage = Forge::TextureUsageBits::ColorAttachment | Forge::TextureUsageBits::TransferSource}));
}

/** The first texel of an attachment, which the fullscreen triangle wrote the same as every other one. */
Texel ReadFirstTexel(ForgeFixture& fixture, Forge::Texture& texture)
{
    Opal::DynamicArray<u8> pixels(k_two_target_side * k_two_target_side * 4);
    REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0, Forge::ImageLayout::TransferSource) ==
            ErrorCode::Success);
    return Texel{static_cast<i32>(pixels[0]), static_cast<i32>(pixels[1]), static_cast<i32>(pixels[2]), static_cast<i32>(pixels[3])};
}

/** Clears both attachments and draws the triangle that writes both of them. */
void DrawTwoTargets(ForgeFixture& fixture, const Forge::Pipeline& pipeline, const Forge::Buffer& vertices, Forge::Texture& first,
                    Forge::Texture& second)
{
    REQUIRE(Forge::ImmediateSubmit(
                fixture.device, fixture.GetQueue(),
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(first)) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(second)) == ErrorCode::Success);
                    const Forge::RenderingDesc rendering_desc{
                        .render_area_extent = {k_two_target_side, k_two_target_side},
                        .color_attachments = {Forge::RenderingAttachmentDesc{.texture = first,
                                                                             .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                             .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                             .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}},
                                              Forge::RenderingAttachmentDesc{.texture = second,
                                                                             .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                             .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                             .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}}}};
                    REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_two_target_side, k_two_target_side}) ==
                            ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_two_target_side, k_two_target_side}) ==
                            ErrorCode::Success);
                    REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                }) == ErrorCode::Success);
}

}  // namespace

TEST_CASE("Forge several colour attachments", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_two_target_source, {.entry_point = "main_two_target_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_two_target_source, {.entry_point = "main_two_target_fragment", .cache = GetShaderCache()}));

    const Forge::Buffer vertices = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = sizeof(k_fullscreen_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                              Opal::AsBytes(k_fullscreen_vertices)));

    /** The pipeline the sections differ in, with the blend state each one wants. */
    auto make_pipeline_desc = [&]
    {
        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        pipeline_desc.color_attachment_formats.PushBack(k_two_target_format);
        pipeline_desc.color_attachment_formats.PushBack(k_two_target_format);
        return pipeline_desc;
    };

    SECTION("A draw writes every attachment the pipeline names")
    {
        Forge::GraphicsPipelineDesc pipeline_desc = make_pipeline_desc();
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture first = MakeTwoTargetAttachment(fixture.device);
        Forge::Texture second = MakeTwoTargetAttachment(fixture.device);
        DrawTwoTargets(fixture, pipeline, vertices, first, second);

        // Each attachment holds the value its own SV_Target wrote, and neither holds the clear.
        REQUIRE(ReadFirstTexel(fixture, first) == k_first_target);
        REQUIRE(ReadFirstTexel(fixture, second) == k_second_target);
    }
    SECTION("A pipeline with fewer blend attachments than colour formats is refused")
    {
        // Vulkan wants one blend state per colour attachment and reads the array by the format count, so a
        // desc that names two formats and one blend state is read off the end of what the caller wrote.
        Forge::GraphicsPipelineDesc pipeline_desc = make_pipeline_desc();
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        REQUIRE(Forge::Pipeline::Create(fixture.device, pipeline_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("Blend state that differs between attachments needs the device feature")
    {
        // This fixture asked for no features, so independent_blend is off and every attachment has to carry
        // the same blend state. Two masks that differ is the mistake, and the device cannot honour it.
        Forge::GraphicsPipelineDesc pipeline_desc = make_pipeline_desc();
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{.color_write_mask = Forge::ColorWriteMaskBits::All});
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{.color_write_mask = Forge::ColorWriteMaskBits::Red});
        REQUIRE(Forge::Pipeline::Create(fixture.device, pipeline_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge blend state per colour attachment", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_independent_blend{.independent_blend = true};
    if (!CanCreateDevice(k_independent_blend))
    {
        SKIP("This device cannot give each colour attachment its own blend state.");
    }
    ForgeFixture fixture(k_independent_blend);

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_two_target_source, {.entry_point = "main_two_target_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_two_target_source, {.entry_point = "main_two_target_fragment", .cache = GetShaderCache()}));

    const Forge::Buffer vertices = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = sizeof(k_fullscreen_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                              Opal::AsBytes(k_fullscreen_vertices)));

    // The first attachment takes everything the shader wrote; the second takes the red channel only, which
    // the shader writes as zero over a clear that set it to one.
    Forge::GraphicsPipelineDesc pipeline_desc = MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, k_two_target_format);
    pipeline_desc.color_blend_attachments[0].color_write_mask = Forge::ColorWriteMaskBits::All;
    pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{.color_write_mask = Forge::ColorWriteMaskBits::Red});
    pipeline_desc.color_attachment_formats.PushBack(k_two_target_format);
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    Forge::Texture first = MakeTwoTargetAttachment(fixture.device);
    Forge::Texture second = MakeTwoTargetAttachment(fixture.device);
    DrawTwoTargets(fixture, pipeline, vertices, first, second);

    // The mask of one attachment says nothing about the other: the first is whole, and the second kept every
    // channel but the one its own mask let through. A mask shared by both would have left the first black
    // and the second the same as it is here.
    const Texel written = ReadFirstTexel(fixture, first);
    INFO("first rgba " << written.r << " " << written.g << " " << written.b << " " << written.a);
    REQUIRE(written == k_first_target);

    const Texel masked = ReadFirstTexel(fixture, second);
    INFO("second rgba " << masked.r << " " << masked.g << " " << masked.b << " " << masked.a);
    REQUIRE(masked == Texel{0, k_cleared.g, k_cleared.b, k_cleared.a});

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge CmdDraw with a non-zero first vertex and first instance", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    HalvesFixture halves;

    // The right half is vertices 6 through 11 of the unindexed buffer; first_vertex is what has to reach
    // past the left half to draw it. first_instance is what picks which entry of the per-instance binding
    // the draw reads, the same as the two-halves cases prove through an offset into an index buffer instead.
    const Opal::DynamicArray<u8> pixels = halves.Render(
        [&](Forge::CommandBuffer& command_buffer)
        {
            REQUIRE(command_buffer.CmdBindVertexBuffer(halves.vertices, 0) == ErrorCode::Success);
            REQUIRE(command_buffer.CmdDraw(6, 1, 6, 2) == ErrorCode::Success);
        });
    REQUIRE_HALF_COLOR(pixels, true, k_instance_three);
    REQUIRE_HALF_COLOR(pixels, false, k_untouched);
    REQUIRE_NO_VALIDATION_ERROR(halves.forge);
}

TEST_CASE("Forge indexed draws", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // Under either of the two names the extension has, which Device::Create looks for on its own.
    const bool has_uint8 = CanCreateDevice({.index_type_uint8 = true});
    HalvesFixture halves({.index_type_uint8 = has_uint8});

    // The one draw every section below makes, with the index buffer built at the given width. Both offsets are
    // non-zero: first_index skips the six padding indices and vertex_offset moves the left corners onto the
    // right ones, so the right half comes back written and nothing else does.
    auto draw_right_half_through = [&](IndexSize index_size)
    {
        const Opal::DynamicArray<u8> index_bytes =
            ToIndexBytes(k_half_indices, static_cast<i32>(std::size(k_half_indices)), index_size);
        const Forge::Buffer indices = ForgeTest::Unwrap(Forge::Buffer::Create(halves.forge.device,
                                    {.size = index_bytes.GetSize(), .usage = Forge::BufferUsageBits::IndexBuffer}, index_bytes));
        return halves.Render(
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdBindVertexBuffer(halves.corners, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, index_size) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdDrawIndexed(6, 1, 6, 4, 0) == ErrorCode::Success);
            });
    };

    SECTION("An indexed draw follows the indices, the first index and the vertex offset")
    {
        const Opal::DynamicArray<u8> pixels = draw_right_half_through(IndexSize::uint32);
        // The right half, because both offsets landed. The left half would mean vertex_offset was dropped,
        // and an untouched target would mean first_index was, since the padding indices draw nothing.
        REQUIRE_HALF_COLOR(pixels, true, k_instance_one);
        REQUIRE_HALF_COLOR(pixels, false, k_untouched);
    }
    SECTION("16-bit indices name the same vertices")
    {
        const Opal::DynamicArray<u8> pixels = draw_right_half_through(IndexSize::uint16);
        REQUIRE_HALF_COLOR(pixels, true, k_instance_one);
        REQUIRE_HALF_COLOR(pixels, false, k_untouched);
    }
    SECTION("8-bit indices name the same vertices when the device has them")
    {
        INFO("8-bit indices supported: " << has_uint8);
        if (!has_uint8)
        {
            SKIP("This device has neither VK_KHR_index_type_uint8 nor VK_EXT_index_type_uint8.");
        }
        const Opal::DynamicArray<u8> pixels = draw_right_half_through(IndexSize::uint8);
        REQUIRE_HALF_COLOR(pixels, true, k_instance_one);
        REQUIRE_HALF_COLOR(pixels, false, k_untouched);
    }
    REQUIRE_NO_VALIDATION_ERROR(halves.forge);
}

/**
 * A case of its own rather than a section of the one above, because it needs a device that was created
 * without the feature the one above may have asked for - and a second fixture inside a section is two live
 * contexts, which docs/forge.md allows and asks nobody to rely on.
 */
TEST_CASE("Forge an 8-bit index buffer on a device without the feature is refused", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // The index type is a plain enum value in a core call, so nothing but this check stands between a device
    // that never enabled the extension and an index type it does not accept.
    ForgeFixture fixture;
    REQUIRE_FALSE(fixture.device.GetFeatures().index_type_uint8);
    const Opal::DynamicArray<u8> index_bytes = ToIndexBytes(k_half_indices, static_cast<i32>(std::size(k_half_indices)), IndexSize::uint8);
    const Forge::Buffer indices = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device, {.size = index_bytes.GetSize(), .usage = Forge::BufferUsageBits::IndexBuffer}, index_bytes));
    Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    REQUIRE(command_buffer.Begin() == ErrorCode::Success);
    REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint8) == ErrorCode::InvalidArgument);
    // The two widths that need no extension still bind on the same command buffer.
    REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint16) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint32) == ErrorCode::Success);
    REQUIRE(command_buffer.End() == ErrorCode::Success);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge indirect draws", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    if (!CanCreateDevice({.draw_indirect_first_instance = true}))
    {
        SKIP("This device cannot start an indirect draw at a non-zero instance.");
    }
    const bool has_multi_draw = CanCreateDevice({.multi_draw_indirect = true, .draw_indirect_first_instance = true});
    HalvesFixture halves({.multi_draw_indirect = has_multi_draw, .draw_indirect_first_instance = true});

    const Forge::Shader write_draws =
        ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(halves.forge.device, k_indirect_command_source,
                                          {.entry_point = "main_write_draws", .cache = GetShaderCache()}));
    const Forge::Shader write_indexed_draw =
        ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(halves.forge.device, k_indirect_command_source,
                                          {.entry_point = "main_write_indexed_draw", .cache = GetShaderCache()}));

    // The commands live in memory the host cannot touch, so nothing but the dispatch below can have put them
    // there - which is what separates this from a direct draw with the same numbers written into a buffer.
    const Forge::Buffer commands = ForgeTest::Unwrap(
        Forge::Buffer::Create(halves.forge.device, {.size = 2 * sizeof(Forge::DrawIndexedIndirectCommand),
                                                    .usage = Forge::BufferUsageBits::IndirectBuffer | Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::None,
                                                    .use_device_address = true}));

    // Dispatches the writer over the command buffer, then orders that write against the indirect read.
    auto record_write = [&](const Forge::Pipeline& write_pipeline)
    {
        const VkDeviceAddress address = commands.GetNativeDeviceAddress();
        return [&, address](Forge::CommandBuffer& command_buffer)
        {
            REQUIRE(command_buffer.CmdBindPipeline(write_pipeline) == ErrorCode::Success);
            REQUIRE(command_buffer.CmdPushConstants(write_pipeline, ShaderTypeBits::Compute, Opal::AsBytes(address)) == ErrorCode::Success);
            REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
            REQUIRE(command_buffer.CmdBufferBarrier(Forge::BufferBarrier::WriteThenRead(
                        commands, Forge::PipelineStageBits::ComputeShader, Forge::PipelineStageBits::IndirectDraw)) == ErrorCode::Success);
        };
    };

    SECTION("An indirect draw runs the command a compute shader wrote")
    {
        const Forge::Pipeline write_pipeline = MakeAddressPipeline(halves.forge.device, write_draws);
        const Opal::DynamicArray<u8> pixels =
            halves.Render(record_write(write_pipeline),
                          [&](Forge::CommandBuffer& command_buffer)
                          {
                              REQUIRE(command_buffer.CmdBindVertexBuffer(halves.vertices, 0) == ErrorCode::Success);
                              REQUIRE(command_buffer.CmdDrawIndirect(commands, 0, 1) == ErrorCode::Success);
                          });
        // The first command only: the left half, at the non-zero instance it named.
        REQUIRE_HALF_COLOR(pixels, false, k_instance_two);
        REQUIRE_HALF_COLOR(pixels, true, k_untouched);
    }
    SECTION("More than one command in one call draws all of them")
    {
        INFO("multi_draw_indirect supported: " << has_multi_draw);
        if (!has_multi_draw)
        {
            SKIP("This device cannot read more than one indirect command per call.");
        }
        const Forge::Pipeline write_pipeline = MakeAddressPipeline(halves.forge.device, write_draws);
        const Opal::DynamicArray<u8> pixels =
            halves.Render(record_write(write_pipeline),
                          [&](Forge::CommandBuffer& command_buffer)
                          {
                              REQUIRE(command_buffer.CmdBindVertexBuffer(halves.vertices, 0) == ErrorCode::Success);
                              REQUIRE(command_buffer.CmdDrawIndirect(commands, 0, 2) == ErrorCode::Success);
                          });
        // Both commands ran, and each fetched the instance its own first_instance named rather than one of
        // them deciding for both.
        REQUIRE_HALF_COLOR(pixels, false, k_instance_two);
        REQUIRE_HALF_COLOR(pixels, true, k_instance_three);
    }
    SECTION("A stride other than the default is honoured")
    {
        // The same two commands "An indirect draw runs the command a compute shader wrote" checks, written
        // by the host this time and spaced twice as far apart as DrawIndirectCommand is wide. A call that
        // still read them back to back would find the second one's padding instead of its data.
        if (!has_multi_draw)
        {
            SKIP("This device cannot read more than one indirect command per call.");
        }
        constexpr u32 k_stride = 2 * sizeof(Forge::DrawIndirectCommand);
        Opal::DynamicArray<u8> spaced(2 * k_stride);
        for (u8& byte : spaced)
        {
            byte = 0;
        }
        const Forge::DrawIndirectCommand left{.vertex_count = 6, .instance_count = 1, .first_vertex = 0, .first_instance = 1};
        const Forge::DrawIndirectCommand right{.vertex_count = 6, .instance_count = 1, .first_vertex = 6, .first_instance = 2};
        memcpy(spaced.GetData(), &left, sizeof(left));
        memcpy(spaced.GetData() + k_stride, &right, sizeof(right));
        const Forge::Buffer spaced_commands = ForgeTest::Unwrap(Forge::Buffer::Create(
            halves.forge.device, {.size = spaced.GetSize(), .usage = Forge::BufferUsageBits::IndirectBuffer}, spaced));

        const Opal::DynamicArray<u8> pixels = halves.Render(
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdBindVertexBuffer(halves.vertices, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdDrawIndirect(spaced_commands, 0, 2, k_stride) == ErrorCode::Success);
            });
        REQUIRE_HALF_COLOR(pixels, false, k_instance_two);
        REQUIRE_HALF_COLOR(pixels, true, k_instance_three);
    }
    SECTION("An indirect indexed draw follows the indices and the vertex offset it was given")
    {
        const Opal::DynamicArray<u8> index_bytes = ToIndexBytes(k_half_indices + 6, 6, IndexSize::uint32);
        const Forge::Buffer indices = ForgeTest::Unwrap(Forge::Buffer::Create(halves.forge.device,
                                    {.size = index_bytes.GetSize(), .usage = Forge::BufferUsageBits::IndexBuffer}, index_bytes));
        const Forge::Pipeline write_pipeline = MakeAddressPipeline(halves.forge.device, write_indexed_draw);
        const Opal::DynamicArray<u8> pixels =
            halves.Render(record_write(write_pipeline),
                          [&](Forge::CommandBuffer& command_buffer)
                          {
                              REQUIRE(command_buffer.CmdBindVertexBuffer(halves.corners, 0) == ErrorCode::Success);
                              REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint32) == ErrorCode::Success);
                              REQUIRE(command_buffer.CmdDrawIndexedIndirect(commands, 0, 1) == ErrorCode::Success);
                          });
        // The indices name the left corners; the vertex offset of four in the command is the only reason the
        // right half is what comes back.
        REQUIRE_HALF_COLOR(pixels, true, k_instance_three);
        REQUIRE_HALF_COLOR(pixels, false, k_untouched);
    }
    REQUIRE_NO_VALIDATION_ERROR(halves.forge);
}

/**
 * How many of the commands above to draw, written by the device beside the commands themselves. Two entry
 * points rather than one reading a push constant, so each pipeline pushes one address the way every writer in
 * this file does.
 */
constexpr const char* k_indirect_count_source = R"(
[shader("compute")]
[numthreads(1, 1, 1)]
void main_write_count_one(uniform uint32_t *output)
{
    output[0] = 1;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main_write_count_two(uniform uint32_t *output)
{
    output[0] = 2;
}
)";

/**
 * CmdDrawIndirectCount and CmdDrawIndexedIndirectCount: the commands and their count both written by compute
 * shaders, both read by the draw when it runs. The same two commands every time, so what differs between the
 * sections is only the count the device wrote and the maximum the draw was recorded with, and which halves of
 * the target come back written says which of those decided.
 */
TEST_CASE("Forge indirect draws whose count the device wrote", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_features{.draw_indirect_first_instance = true, .draw_indirect_count = true};
    if (!CanCreateDevice(k_features))
    {
        SKIP("This device cannot read an indirect draw count from a buffer.");
    }
    HalvesFixture halves(k_features);
    const Forge::Device& device = halves.forge.device;

    auto make_pipeline = [&](const char* source, const char* entry_point)
    {
        const Forge::Shader shader =
            ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(device, source, {.entry_point = entry_point, .cache = GetShaderCache()}));
        return MakeAddressPipeline(device, shader);
    };
    const Forge::Pipeline write_draws = make_pipeline(k_indirect_command_source, "main_write_draws");
    const Forge::Pipeline write_indexed_draw = make_pipeline(k_indirect_command_source, "main_write_indexed_draw");
    const Forge::Pipeline write_count_one = make_pipeline(k_indirect_count_source, "main_write_count_one");
    const Forge::Pipeline write_count_two = make_pipeline(k_indirect_count_source, "main_write_count_two");

    // Device-only, both of them, so neither the commands nor the count can have come from the host.
    auto make_device_buffer = [&](u64 size)
    {
        return ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = size,
                                                                .usage = Forge::BufferUsageBits::IndirectBuffer |
                                                                         Forge::BufferUsageBits::StorageBuffer,
                                                                .host_access = Forge::HostAccess::None,
                                                                .use_device_address = true}));
    };
    const Forge::Buffer commands = make_device_buffer(2 * sizeof(Forge::DrawIndexedIndirectCommand));
    const Forge::Buffer count = make_device_buffer(sizeof(u32));

    /** Dispatch the two writers, then order both writes against the indirect read. */
    auto record_writes = [&](const Forge::Pipeline& command_writer, const Forge::Pipeline& count_writer)
    {
        return [&](Forge::CommandBuffer& command_buffer)
        {
            const VkDeviceAddress addresses[] = {commands.GetNativeDeviceAddress(), count.GetNativeDeviceAddress()};
            const Forge::Pipeline* writers[] = {&command_writer, &count_writer};
            for (i32 i = 0; i < 2; ++i)
            {
                REQUIRE(command_buffer.CmdBindPipeline(*writers[i]) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(*writers[i], ShaderTypeBits::Compute, Opal::AsBytes(addresses[i])) ==
                        ErrorCode::Success);
                REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
            }
            const Forge::BufferBarrier barriers[] = {
                Forge::BufferBarrier::WriteThenRead(commands, Forge::PipelineStageBits::ComputeShader, Forge::PipelineStageBits::IndirectDraw),
                Forge::BufferBarrier::WriteThenRead(count, Forge::PipelineStageBits::ComputeShader, Forge::PipelineStageBits::IndirectDraw)};
            REQUIRE(command_buffer.CmdBufferBarriers({barriers, 2}) == ErrorCode::Success);
        };
    };

    /** Draw the unindexed commands with the count the device wrote, reading at most `max_draw_count` of them. */
    auto draw_counted = [&](const Forge::Pipeline& count_writer, u32 max_draw_count)
    {
        return halves.Render(record_writes(write_draws, count_writer),
                             [&](Forge::CommandBuffer& command_buffer)
                             {
                                 REQUIRE(command_buffer.CmdBindVertexBuffer(halves.vertices, 0) == ErrorCode::Success);
                                 REQUIRE(command_buffer.CmdDrawIndirectCount(commands, 0, count, 0, max_draw_count) == ErrorCode::Success);
                             });
    };

    SECTION("A count of one draws the first command and stops")
    {
        // Room for two, and the device said one: the left half at instance two, and nothing on the right.
        const Opal::DynamicArray<u8> pixels = draw_counted(write_count_one, 2);
        REQUIRE_HALF_COLOR(pixels, false, k_instance_two);
        REQUIRE_HALF_COLOR(pixels, true, k_untouched);
    }
    SECTION("A count of two draws both commands")
    {
        const Opal::DynamicArray<u8> pixels = draw_counted(write_count_two, 2);
        REQUIRE_HALF_COLOR(pixels, false, k_instance_two);
        REQUIRE_HALF_COLOR(pixels, true, k_instance_three);
    }
    SECTION("A count above the maximum draws the maximum")
    {
        // The device says two and the draw was recorded with room for one. The maximum wins, which is what
        // keeps a count gone wrong from reading past what the command buffer was sized for.
        const Opal::DynamicArray<u8> pixels = draw_counted(write_count_two, 1);
        REQUIRE_HALF_COLOR(pixels, false, k_instance_two);
        REQUIRE_HALF_COLOR(pixels, true, k_untouched);
    }
    SECTION("An indexed draw reads its count the same way")
    {
        const Opal::DynamicArray<u8> index_bytes = ToIndexBytes(k_half_indices + 6, 6, IndexSize::uint32);
        const Forge::Buffer indices = ForgeTest::Unwrap(Forge::Buffer::Create(
            device, {.size = index_bytes.GetSize(), .usage = Forge::BufferUsageBits::IndexBuffer}, index_bytes));
        const Opal::DynamicArray<u8> pixels =
            halves.Render(record_writes(write_indexed_draw, write_count_one),
                          [&](Forge::CommandBuffer& command_buffer)
                          {
                              REQUIRE(command_buffer.CmdBindVertexBuffer(halves.corners, 0) == ErrorCode::Success);
                              REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint32) == ErrorCode::Success);
                              REQUIRE(command_buffer.CmdDrawIndexedIndirectCount(commands, 0, count, 0, 1) == ErrorCode::Success);
                          });
        // The command the writer put first moves the left corners onto the right half at instance two.
        REQUIRE_HALF_COLOR(pixels, true, k_instance_three);
        REQUIRE_HALF_COLOR(pixels, false, k_untouched);
    }
    SECTION("A count buffer or stride the draw cannot use is refused")
    {
        const Forge::Buffer storage_only =
            ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = sizeof(u32), .usage = Forge::BufferUsageBits::StorageBuffer}));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(device, halves.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdDrawIndirectCount(commands, 0, storage_only, 0, 1) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDrawIndirectCount(commands, 0, count, 2, 1) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDrawIndirectCount(commands, 0, count, 4, 1) == ErrorCode::OutOfBounds);
        // A stride is read however many commands there turn out to be, so a short one is refused at a
        // maximum of one, where the plain indirect draw would not look at it.
        REQUIRE(command_buffer.CmdDrawIndirectCount(commands, 0, count, 0, 1, 8) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDrawIndexedIndirectCount(commands, 0, count, 0, 1, 12) == ErrorCode::InvalidArgument);
        // Every command up to the maximum has to fit, since which of them are read is not known here.
        REQUIRE(command_buffer.CmdDrawIndexedIndirectCount(commands, 0, count, 0, 3) == ErrorCode::OutOfBounds);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(halves.forge);
}

TEST_CASE("Forge an indirect count draw on a device without the feature is refused", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // The count draws are core Vulkan 1.2 commands with a trampoline behind them either way, so nothing but
    // this check stands between a device that never enabled drawIndirectCount and a draw it may not record.
    ForgeFixture fixture;
    REQUIRE_FALSE(fixture.device.GetFeatures().draw_indirect_count);
    const Forge::Buffer commands = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device, {.size = sizeof(Forge::DrawIndexedIndirectCommand), .usage = Forge::BufferUsageBits::IndirectBuffer}));
    const Forge::Buffer count =
        ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = sizeof(u32), .usage = Forge::BufferUsageBits::IndirectBuffer}));
    Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    REQUIRE(command_buffer.Begin() == ErrorCode::Success);
    REQUIRE(command_buffer.CmdDrawIndirectCount(commands, 0, count, 0, 1) == ErrorCode::InvalidArgument);
    REQUIRE(command_buffer.CmdDrawIndexedIndirectCount(commands, 0, count, 0, 1) == ErrorCode::InvalidArgument);
    REQUIRE(command_buffer.End() == ErrorCode::Success);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge indirect dispatch", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_group_size = 64;
    constexpr i32 k_group_count = 4;
    constexpr i32 k_element_count = k_group_size * k_group_count;

    const Forge::Shader write_dispatch =
        ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(fixture.device, k_indirect_command_source,
                                          {.entry_point = "main_write_dispatch", .cache = GetShaderCache()}));
    const Forge::Shader compute_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));

    const Forge::Pipeline write_pipeline = MakeAddressPipeline(fixture.device, write_dispatch);
    const Forge::Pipeline compute_pipeline = MakeAddressPipeline(fixture.device, compute_shader);

    // Device-only, so the group counts cannot have come from the host.
    const Forge::Buffer group_counts = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device,
        {.size = sizeof(Forge::DispatchIndirectCommand),
         .usage = Forge::BufferUsageBits::IndirectBuffer | Forge::BufferUsageBits::StorageBuffer | Forge::BufferUsageBits::TransferSource,
         .host_access = Forge::HostAccess::None,
         .use_device_address = true}));

    const Forge::Buffer indirect_output = MakeWipedOutput(fixture.device, k_element_count);
    const Forge::Buffer direct_output = MakeWipedOutput(fixture.device, k_element_count);

    const VkDeviceAddress group_counts_address = group_counts.GetNativeDeviceAddress();
    const VkDeviceAddress indirect_address = indirect_output.GetNativeDeviceAddress();
    const VkDeviceAddress direct_address = direct_output.GetNativeDeviceAddress();
    REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               REQUIRE(command_buffer.CmdBindPipeline(write_pipeline) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdPushConstants(write_pipeline, ShaderTypeBits::Compute,
                                                               Opal::AsBytes(group_counts_address)) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdBufferBarrier(Forge::BufferBarrier::WriteThenRead(
                                           group_counts, Forge::PipelineStageBits::ComputeShader,
                                           Forge::PipelineStageBits::IndirectDraw)) == ErrorCode::Success);

                               REQUIRE(command_buffer.CmdBindPipeline(compute_pipeline) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdPushConstants(compute_pipeline, ShaderTypeBits::Compute,
                                                               Opal::AsBytes(indirect_address)) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdDispatchIndirect(group_counts) == ErrorCode::Success);
                               // The two dispatches write different buffers, so nothing has to order them
                               // against each other - only the push constant between them, which records in
                               // order with the commands around it.
                               REQUIRE(command_buffer.CmdPushConstants(compute_pipeline, ShaderTypeBits::Compute,
                                                               Opal::AsBytes(direct_address)) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdDispatch(k_group_count) == ErrorCode::Success);
                           }) == ErrorCode::Success);

    SECTION("The group counts came off the device")
    {
        Forge::DispatchIndirectCommand written;
        REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), group_counts,
                              {reinterpret_cast<u8*>(&written), sizeof(written)}) == ErrorCode::Success);
        REQUIRE(written.group_count_x == k_group_count);
        REQUIRE(written.group_count_y == 1);
        REQUIRE(written.group_count_z == 1);
    }
    SECTION("An indirect dispatch of those counts matches a direct dispatch of the same ones")
    {
        Opal::DynamicArray<u32> from_indirect(k_element_count);
        Opal::DynamicArray<u32> from_direct(k_element_count);
        REQUIRE(indirect_output.Read({reinterpret_cast<u8*>(from_indirect.GetData()), from_indirect.GetSize() * sizeof(u32)}) ==
                ErrorCode::Success);
        REQUIRE(direct_output.Read({reinterpret_cast<u8*>(from_direct.GetData()), from_direct.GetSize() * sizeof(u32)}) ==
                ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            INFO("element " << i);
            // Compared against the value the shader computes as well as against each other: two dispatches
            // that both did nothing would agree with one another and with nothing else.
            REQUIRE(from_indirect[i] == static_cast<u32>(i) + 1000);
            REQUIRE(from_direct[i] == from_indirect[i]);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * The guards every indirect command shares, which the indirect cases above only ever pass: the usage, the
 * offset alignment, the stride, and commands that run off the end of the buffer. Each is refused before
 * anything is recorded, so no pipeline and no render pass are needed to ask.
 */
TEST_CASE("Forge indirect arguments the guards refuse", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // More than one command is a feature, and refused as InvalidArgument without it - the same code a bad
    // stride gets - so the stride section needs a device where the count is not the thing being refused.
    const bool has_multi_draw = CanCreateDevice({.multi_draw_indirect = true});
    ForgeFixture fixture({.multi_draw_indirect = has_multi_draw});
    constexpr u64 k_draw_size = sizeof(Forge::DrawIndirectCommand);
    constexpr u64 k_indexed_size = sizeof(Forge::DrawIndexedIndirectCommand);
    constexpr u64 k_dispatch_size = sizeof(Forge::DispatchIndirectCommand);

    auto make_buffer = [&](u64 size, Forge::BufferUsageBits usage = Forge::BufferUsageBits::IndirectBuffer)
    { return ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = size, .usage = usage})); };

    Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    REQUIRE(command_buffer.Begin() == ErrorCode::Success);

    SECTION("A buffer created without the indirect usage")
    {
        const Forge::Buffer storage_only = make_buffer(64, Forge::BufferUsageBits::StorageBuffer);
        REQUIRE(command_buffer.CmdDrawIndirect(storage_only) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDrawIndexedIndirect(storage_only) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDispatchIndirect(storage_only) == ErrorCode::InvalidArgument);
    }
    SECTION("An offset that is not a multiple of four")
    {
        const Forge::Buffer commands = make_buffer(64);
        REQUIRE(command_buffer.CmdDrawIndirect(commands, 2) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDrawIndexedIndirect(commands, 6) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDispatchIndirect(commands, 1) == ErrorCode::InvalidArgument);
    }
    SECTION("A command that runs off the end of the buffer")
    {
        // Exactly one command of each kind fits, so the same command one word further in does not.
        const Forge::Buffer one_draw = make_buffer(k_draw_size);
        REQUIRE(command_buffer.CmdDrawIndirect(one_draw, 4) == ErrorCode::OutOfBounds);
        const Forge::Buffer one_indexed = make_buffer(k_indexed_size);
        REQUIRE(command_buffer.CmdDrawIndexedIndirect(one_indexed, 4) == ErrorCode::OutOfBounds);
        const Forge::Buffer one_dispatch = make_buffer(k_dispatch_size);
        REQUIRE(command_buffer.CmdDispatchIndirect(one_dispatch, 4) == ErrorCode::OutOfBounds);
        // And an offset past the buffer altogether.
        REQUIRE(command_buffer.CmdDispatchIndirect(one_dispatch, k_dispatch_size + 4) == ErrorCode::OutOfBounds);
    }
    SECTION("More than one command, with a stride that is too short, misaligned, or reaches past the buffer")
    {
        INFO("multi_draw_indirect supported: " << has_multi_draw);
        if (!has_multi_draw)
        {
            SKIP("This device cannot read more than one indirect command per call.");
        }
        const Forge::Buffer two_draws = make_buffer(2 * k_draw_size);
        REQUIRE(command_buffer.CmdDrawIndirect(two_draws, 0, 2, static_cast<u32>(k_draw_size - 4)) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDrawIndirect(two_draws, 0, 2, static_cast<u32>(k_draw_size + 2)) == ErrorCode::InvalidArgument);
        // A stride twice the command is legal, and two of them no longer fit in a buffer sized for two packed.
        REQUIRE(command_buffer.CmdDrawIndirect(two_draws, 0, 2, static_cast<u32>(2 * k_draw_size)) == ErrorCode::OutOfBounds);
        const Forge::Buffer two_indexed = make_buffer(2 * k_indexed_size);
        REQUIRE(command_buffer.CmdDrawIndexedIndirect(two_indexed, 0, 2, static_cast<u32>(k_indexed_size - 4)) ==
                ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDrawIndexedIndirect(two_indexed, 0, 3) == ErrorCode::OutOfBounds);
    }
    REQUIRE(command_buffer.End() == ErrorCode::Success);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge pipeline sample count and dynamic state", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));

    auto make_desc = [&]() { return MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, k_format); };

    SECTION("A sample count this device supports builds a pipeline")
    {
        const VkPhysicalDeviceLimits& limits = fixture.device.GetPhysicalDevice().GetProperties().limits;
        const VkSampleCountFlags supported = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
        INFO("supported sample counts mask " << supported);
        // One is the only count every device has to support, so it is the only one this can require.
        REQUIRE((supported & VK_SAMPLE_COUNT_1_BIT) != 0);

        Forge::GraphicsPipelineDesc desc = make_desc();
        desc.sample_count = Forge::SampleCount::Count1;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, desc));
        REQUIRE(pipeline.IsValid());

        if ((supported & VK_SAMPLE_COUNT_4_BIT) != 0)
        {
            Forge::GraphicsPipelineDesc four = make_desc();
            four.sample_count = Forge::SampleCount::Count4;
            const Forge::Pipeline multisampled = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, four));
            REQUIRE(multisampled.IsValid());
        }
    }
    SECTION("A sample count this device does not support is refused")
    {
        const VkPhysicalDeviceLimits& limits = fixture.device.GetPhysicalDevice().GetProperties().limits;
        const VkSampleCountFlags supported = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
        if ((supported & VK_SAMPLE_COUNT_64_BIT) != 0)
        {
            SKIP("This device supports 64 samples, so there is no unsupported count to test with.");
        }
        Forge::GraphicsPipelineDesc desc = make_desc();
        desc.sample_count = Forge::SampleCount::Count64;
        REQUIRE(Forge::Pipeline::Create(fixture.device, desc).GetErrorOr(ErrorCode::Success) == ErrorCode::FeatureNotSupported);
    }
    SECTION("Dynamic state is recorded on a pipeline that asked for it")
    {
        Forge::GraphicsPipelineDesc desc = make_desc();
        desc.dynamic_state = Forge::DynamicStateBits::DepthBias | Forge::DynamicStateBits::StencilReference;
        desc.rasterizer.depth_bias_enabled = true;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, desc));

        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetDepthBias(1.0f) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetStencilReference(3) == ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("Dynamic state a feature gates is refused without the feature")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        // The fixture device asks for neither, so both of these are the guard rather than the driver.
        REQUIRE_FALSE(fixture.device.GetFeatures().wide_lines);
        REQUIRE_FALSE(fixture.device.GetFeatures().depth_bias_clamp);
        REQUIRE(command_buffer.CmdSetLineWidth(4.0f) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdSetDepthBias(1.0f, 0.5f) == ErrorCode::InvalidArgument);
        // The value every device draws, and a bias with no clamp, need no feature.
        REQUIRE(command_buffer.CmdSetLineWidth(1.0f) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetDepthBias(1.0f) == ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * Two triangles that cover the left quarter of the target, so the boundary runs through the middle of the
 * left column of texels rather than along a texel edge. With the standard four-sample pattern that puts two
 * samples of every texel in that column inside the geometry and two outside, and the resolve averages the
 * two halves - a value neither the clear nor the draw wrote, which one sample per texel cannot produce.
 */
constexpr f32 k_left_quarter_vertices[] = {-1.0f, -1.0f, -0.5f, -1.0f, -1.0f, 1.0f, -0.5f, -1.0f, -0.5f, 1.0f, -1.0f, 1.0f};

TEST_CASE("Forge multisampled draw resolved to one sample", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const VkPhysicalDeviceLimits& limits = fixture.device.GetPhysicalDevice().GetProperties().limits;
    if ((limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0)
    {
        SKIP("This device cannot render four samples per texel.");
    }
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr i32 k_side = 2;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer vertices = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = sizeof(k_left_quarter_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                              Opal::AsBytes(k_left_quarter_vertices)));

    Forge::GraphicsPipelineDesc pipeline_desc = MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, k_format);
    pipeline_desc.sample_count = Forge::SampleCount::Count4;
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    /** A texture of the size the case renders at, with the sample count and the usage a section wants. */
    auto make_texture = [&](Forge::SampleCount sample_count, Forge::TextureUsageBits usage, PixelFormat format)
    {
        return ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = format, .width = k_side, .height = k_side, .sample_count = sample_count, .usage = usage}));
    };
    constexpr Forge::TextureUsageBits k_attachment_usage =
        Forge::TextureUsageBits::ColorAttachment | Forge::TextureUsageBits::TransferSource;
    constexpr Forge::TextureUsageBits k_resolved_usage =
        Forge::TextureUsageBits::TransferDestination | Forge::TextureUsageBits::TransferSource;

    Forge::Texture multisampled = make_texture(Forge::SampleCount::Count4, k_attachment_usage, k_format);
    Forge::Texture resolved = make_texture(Forge::SampleCount::Count1, k_resolved_usage, k_format);

    SECTION("A resolve averages the samples of every texel")
    {
        if (limits.standardSampleLocations == VK_FALSE)
        {
            SKIP("This device places its samples somewhere of its own, so the coverage of a half-covered texel is unknown.");
        }
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(multisampled)) ==
                                ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = multisampled,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                                 .clear_value = Vector4f{0.0f, 0.0f, 0.0f, 1.0f}}}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(multisampled)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(resolved)) ==
                                ErrorCode::Success);
                        const Forge::TextureCopyRegion whole{};
                        REQUIRE(command_buffer.CmdResolveTexture(multisampled, resolved, {&whole, 1}) == ErrorCode::Success);
                    }) == ErrorCode::Success);

        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, resolved, k_side);

        // The draw writes green with alpha zero over a clear of black with alpha one, so a texel half inside the
        // geometry averages to the middle in both channels and one outside keeps the clear. Half of 255 is not
        // a whole number, and the format may round it either way.
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                const i32 offset = (y * k_side + x) * 4;
                const Texel texel{pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
                INFO("texel (" << x << ", " << y << ") rgba " << texel.r << " " << texel.g << " " << texel.b << " " << texel.a);
                REQUIRE(texel.r == 0);
                REQUIRE(texel.b == 0);
                if (x == 0)
                {
                    REQUIRE(Opal::Abs(texel.g - 128) <= 1);
                    REQUIRE(Opal::Abs(texel.a - 128) <= 1);
                }
                else
                {
                    REQUIRE(texel.g == 0);
                    REQUIRE(texel.a == 255);
                }
            }
        }
    }
    SECTION("A resolve works at every sample count this device supports")
    {
        // No geometry: every sample of the source is cleared to the same value, so the average has to be
        // that value exactly whatever the count is - which is what lets one section cover the sample counts
        // ToVkSampleCount maps and the averaging section above does not reach.
        constexpr Forge::SampleCount k_counts[] = {Forge::SampleCount::Count2,  Forge::SampleCount::Count4,  Forge::SampleCount::Count8,
                                                   Forge::SampleCount::Count16, Forge::SampleCount::Count32, Forge::SampleCount::Count64};
        constexpr VkSampleCountFlagBits k_native_counts[] = {VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
                                                             VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT};
        constexpr Vector4f k_clear{0.2f, 0.4f, 0.6f, 1.0f};
        bool tried_one = false;
        for (i32 i = 0; i < 6; ++i)
        {
            if ((limits.framebufferColorSampleCounts & k_native_counts[i]) == 0)
            {
                continue;
            }
            tried_one = true;
            INFO("sample count " << static_cast<i32>(k_counts[i]));
            Forge::Texture multi = make_texture(k_counts[i], k_attachment_usage, k_format);
            Forge::Texture resolved_texture = make_texture(Forge::SampleCount::Count1, k_resolved_usage, k_format);
            REQUIRE(Forge::ImmediateSubmit(
                        fixture.device, fixture.GetQueue(),
                        [&](Forge::CommandBuffer& command_buffer)
                        {
                            REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(multi)) ==
                                    ErrorCode::Success);
                            const Forge::RenderingDesc rendering_desc{
                                .render_area_extent = {k_side, k_side},
                                .color_attachments = {Forge::RenderingAttachmentDesc{
                                    .texture = multi,
                                    .load_operation = Forge::AttachmentLoadOperation::Clear,
                                    .store_operation = Forge::AttachmentStoreOperation::Store,
                                    .clear_value = k_clear}}};
                            REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                            REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                            REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(multi)) ==
                                    ErrorCode::Success);
                            REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(resolved_texture)) ==
                                    ErrorCode::Success);
                            const Forge::TextureCopyRegion whole{};
                            REQUIRE(command_buffer.CmdResolveTexture(multi, resolved_texture, {&whole, 1}) == ErrorCode::Success);
                        }) == ErrorCode::Success);

            const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, resolved_texture, k_side);
            // Every sample was cleared to the same value, so the average is exact but for rounding.
            REQUIRE(Opal::Abs(static_cast<i32>(pixels[0]) - 51) <= 1);
            REQUIRE(Opal::Abs(static_cast<i32>(pixels[1]) - 102) <= 1);
            REQUIRE(Opal::Abs(static_cast<i32>(pixels[2]) - 153) <= 1);
        }
        if (!tried_one)
        {
            SKIP("This device supports no sample count above one besides four.");
        }
    }
    SECTION("A resolve out of a texture with one sample is refused")
    {
        Forge::Texture single = make_texture(Forge::SampleCount::Count1, k_attachment_usage, k_format);
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        const Forge::TextureCopyRegion whole{};
        REQUIRE(command_buffer.CmdResolveTexture(single, resolved, {&whole, 1}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A resolve into a multisampled texture is refused")
    {
        Forge::Texture other =
            make_texture(Forge::SampleCount::Count4, k_attachment_usage | Forge::TextureUsageBits::TransferDestination, k_format);
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        const Forge::TextureCopyRegion whole{};
        REQUIRE(command_buffer.CmdResolveTexture(multisampled, other, {&whole, 1}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A resolve between two formats is refused")
    {
        Forge::Texture other = make_texture(Forge::SampleCount::Count1, k_resolved_usage, PixelFormat::B8G8R8A8_UNORM);
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        const Forge::TextureCopyRegion whole{};
        REQUIRE(command_buffer.CmdResolveTexture(multisampled, other, {&whole, 1}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * A fragment shader whose output is decided by two specialization constants, so what comes back says which
 * values the pipeline was built with. [SpecializationConstant] is the portable Slang spelling and lets the
 * compiler pick the ids; [vk::constant_id(N)] pins them, and both reflect the same way.
 */
constexpr const char* k_specialized_source = R"(
[SpecializationConstant]
const int RED_LEVEL = 64;

[SpecializationConstant]
const bool WRITE_BLUE = false;

struct VertexOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput main_vertex(float2 position : POSITION)
{
    VertexOutput output;
    output.position = float4(position, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 main_fragment() : SV_Target
{
    return float4(float(RED_LEVEL) / 255.0, 0.0, WRITE_BLUE ? 1.0 : 0.0, 1.0);
}
)";

/** The same idea for a compute pipeline, which takes its own path through pipeline creation. */
constexpr const char* k_specialized_compute_source = R"(
[SpecializationConstant]
const uint ADDEND = 5;

[shader("compute")]
[numthreads(64, 1, 1)]
void main_specialized(uint3 thread_id : SV_DispatchThreadID, uniform RWStructuredBuffer<uint> output)
{
    output[thread_id.x] = thread_id.x + ADDEND;
}
)";

/**
 * A Create that gives up part way leaves no object behind for a destructor to clean up, so anything it
 * built before the check that refused is its own to release. The pipeline layout is created first and the
 * checks that reject a description come after it, which made every rejected pipeline a leaked layout.
 * Nothing noticed: the layer only names an object that outlived its device, and that report arrives at
 * vkDestroyDevice, after the assertion at the end of a case has already passed.
 */
/**
 * RenderingAttachmentDesc::resolve_texture, resolve_view and resolve_mode: a resolve Vulkan runs as the pass
 * ends. Both attachments rendered into are transient, which CmdResolveTexture cannot read at all, so what
 * reaches the one-sample texture can only have come through the pass.
 */
TEST_CASE("Forge a resolve at the end of the pass", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const VkPhysicalDeviceLimits& limits = fixture.device.GetPhysicalDevice().GetProperties().limits;
    if ((limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0 || (limits.framebufferDepthSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0)
    {
        SKIP("This device cannot render four samples per texel.");
    }
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr PixelFormat k_depth_format = PixelFormat::D32_SFLOAT;
    constexpr i32 k_side = 2;

    auto make_texture = [&](PixelFormat format, Forge::SampleCount sample_count, Forge::TextureUsageBits usage)
    {
        return ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = format, .width = k_side, .height = k_side, .sample_count = sample_count, .usage = usage}));
    };
    constexpr Forge::TextureUsageBits k_transient_color = Forge::TextureUsageBits::ColorAttachment | Forge::TextureUsageBits::TransientAttachment;
    constexpr Forge::TextureUsageBits k_transient_depth =
        Forge::TextureUsageBits::DepthStencilAttachment | Forge::TextureUsageBits::TransientAttachment;
    constexpr Forge::TextureUsageBits k_resolved_color = Forge::TextureUsageBits::ColorAttachment | Forge::TextureUsageBits::TransferSource;
    constexpr Forge::TextureUsageBits k_resolved_depth =
        Forge::TextureUsageBits::DepthStencilAttachment | Forge::TextureUsageBits::TransferSource;

    SECTION("A transient colour attachment resolves to the average of its samples")
    {
        if (limits.standardSampleLocations == VK_FALSE)
        {
            SKIP("This device places its samples somewhere of its own, so the coverage of a half-covered texel is unknown.");
        }
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
        const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));
        const Forge::Buffer vertices = ForgeTest::Unwrap(
            Forge::Buffer::Create(fixture.device, {.size = sizeof(k_left_quarter_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                                  Opal::AsBytes(k_left_quarter_vertices)));
        Forge::GraphicsPipelineDesc pipeline_desc = MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, k_format);
        pipeline_desc.sample_count = Forge::SampleCount::Count4;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture multisampled = make_texture(k_format, Forge::SampleCount::Count4, k_transient_color);
        Forge::Texture resolved = make_texture(k_format, Forge::SampleCount::Count1, k_resolved_color);
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(multisampled)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(resolved)) == ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = multisampled,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::DontCare,
                                                                                 .clear_value = Vector4f{0.0f, 0.0f, 0.0f, 1.0f},
                                                                                 .resolve_texture = resolved}}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);
        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, resolved, k_side, Forge::ImageLayout::ColorAttachment);

        // The same picture the standalone resolve case reads: green with alpha zero over half of each left texel
        // on a clear of opaque black, averaged to the middle in both channels, and the right column untouched.
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                const i32 offset = (y * k_side + x) * 4;
                const Texel texel{pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
                INFO("texel (" << x << ", " << y << ") rgba " << texel.r << " " << texel.g << " " << texel.b << " " << texel.a);
                REQUIRE(texel.r == 0);
                REQUIRE(texel.b == 0);
                REQUIRE(Opal::Abs(texel.g - (x == 0 ? 128 : 0)) <= 1);
                REQUIRE(Opal::Abs(texel.a - (x == 0 ? 128 : 255)) <= 1);
            }
        }
    }
    SECTION("A transient depth attachment resolves its first sample")
    {
        // SampleZero is the one depth resolve every device has. A clear writes every sample, so the resolved
        // depth is the clear value exactly - and a pass that never resolved leaves the target undefined.
        constexpr f32 k_depth = 0.375f;
        Forge::Texture multisampled = make_texture(k_depth_format, Forge::SampleCount::Count4, k_transient_depth);
        Forge::Texture resolved = make_texture(k_depth_format, Forge::SampleCount::Count1, k_resolved_depth);
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(multisampled)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(resolved)) ==
                                ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .depth_attachment = Forge::RenderingAttachmentDesc{.texture = multisampled,
                                                                               .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                               .store_operation = Forge::AttachmentStoreOperation::DontCare,
                                                                               .clear_value = Forge::DepthStencilClearValue{.depth = k_depth},
                                                                               .resolve_texture = resolved,
                                                                               .resolve_mode = Forge::ResolveMode::SampleZero}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);
        f32 depths[k_side * k_side] = {};
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), resolved, Opal::AsWritableBytes(depths), 0,
                                       Forge::ImageLayout::DepthStencilAttachment) == ErrorCode::Success);
        for (i32 texel = 0; texel < k_side * k_side; ++texel)
        {
            INFO("texel " << texel << " depth " << depths[texel]);
            REQUIRE(depths[texel] == k_depth);
        }
    }
    SECTION("A resolve the textures or the device cannot take is refused")
    {
        Forge::Texture multisampled = make_texture(k_format, Forge::SampleCount::Count4, k_transient_color);
        Forge::Texture single = make_texture(k_format, Forge::SampleCount::Count1, k_resolved_color);
        Forge::Texture other_single = make_texture(k_format, Forge::SampleCount::Count1, k_resolved_color);
        Forge::Texture multisampled_target = make_texture(k_format, Forge::SampleCount::Count4, k_transient_color);
        Forge::Texture other_format = make_texture(PixelFormat::B8G8R8A8_UNORM, Forge::SampleCount::Count1, k_resolved_color);
        Forge::Texture unready = make_texture(k_format, Forge::SampleCount::Count1, k_resolved_color);
        Forge::Texture multisampled_depth = make_texture(k_depth_format, Forge::SampleCount::Count4, k_transient_depth);
        Forge::Texture single_depth = make_texture(k_depth_format, Forge::SampleCount::Count1, k_resolved_depth);

        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        for (Forge::Texture* texture : {&multisampled, &single, &other_single, &multisampled_target, &other_format})
        {
            REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(*texture)) == ErrorCode::Success);
        }
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(multisampled_depth)) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(single_depth)) == ErrorCode::Success);

        auto begin_color = [&](const Forge::Texture& attachment, const Forge::Texture& target, Forge::ResolveMode mode)
        {
            Forge::RenderingDesc rendering_desc{.render_area_extent = {k_side, k_side}};
            rendering_desc.color_attachments.PushBack(
                Forge::RenderingAttachmentDesc{.texture = attachment, .resolve_texture = target, .resolve_mode = mode});
            return command_buffer.CmdBeginRendering(rendering_desc);
        };
        REQUIRE(begin_color(multisampled, multisampled_target, Forge::ResolveMode::Average) == ErrorCode::InvalidArgument);
        REQUIRE(begin_color(single, other_single, Forge::ResolveMode::Average) == ErrorCode::InvalidArgument);
        REQUIRE(begin_color(multisampled, other_format, Forge::ResolveMode::Average) == ErrorCode::InvalidArgument);
        REQUIRE(begin_color(multisampled, unready, Forge::ResolveMode::Average) == ErrorCode::InvalidArgument);
        // A normalized colour format is averaged and nothing else.
        REQUIRE(begin_color(multisampled, single, Forge::ResolveMode::SampleZero) == ErrorCode::InvalidArgument);
        REQUIRE(begin_color(multisampled, single, static_cast<Forge::ResolveMode>(9)) == ErrorCode::InvalidArgument);

        // Depth takes the modes this device reports, and one it does not report is refused.
        const VkResolveModeFlags depth_modes = fixture.device.GetPhysicalDevice().GetDepthStencilResolveProperties().supportedDepthResolveModes;
        REQUIRE((depth_modes & VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) != 0);
        const struct
        {
            Forge::ResolveMode mode;
            VkResolveModeFlagBits bit;
        } k_depth_modes[] = {{Forge::ResolveMode::Average, VK_RESOLVE_MODE_AVERAGE_BIT},
                             {Forge::ResolveMode::Min, VK_RESOLVE_MODE_MIN_BIT},
                             {Forge::ResolveMode::Max, VK_RESOLVE_MODE_MAX_BIT}};
        for (const auto& depth_mode : k_depth_modes)
        {
            if ((depth_modes & depth_mode.bit) != 0)
            {
                continue;
            }
            const Forge::RenderingDesc rendering_desc{
                .render_area_extent = {k_side, k_side},
                .depth_attachment = Forge::RenderingAttachmentDesc{
                    .texture = multisampled_depth, .resolve_texture = single_depth, .resolve_mode = depth_mode.mode}};
            INFO("resolve mode " << static_cast<i32>(depth_mode.mode));
            REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::InvalidArgument);
        }
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("Stencil is never averaged, and a pass resolving both sides resolves them into one texture")
    {
        if ((limits.framebufferStencilSampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0)
        {
            SKIP("This device cannot render four stencil samples per texel.");
        }
        constexpr PixelFormat k_combined_format = PixelFormat::D32_SFLOAT_S8_UINT;
        Forge::Texture multisampled = make_texture(k_combined_format, Forge::SampleCount::Count4, k_transient_depth);
        Forge::Texture target = make_texture(k_combined_format, Forge::SampleCount::Count1, k_resolved_depth);
        Forge::Texture other_target = make_texture(k_combined_format, Forge::SampleCount::Count1, k_resolved_depth);
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        for (Forge::Texture* texture : {&multisampled, &target, &other_target})
        {
            REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(*texture)) == ErrorCode::Success);
        }
        auto side = [&](const Forge::Texture& resolve_target, Forge::ResolveMode mode)
        {
            return Forge::RenderingAttachmentDesc{.texture = multisampled,
                                                  .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                  .store_operation = Forge::AttachmentStoreOperation::DontCare,
                                                  .clear_value = Forge::DepthStencilClearValue{},
                                                  .resolve_texture = resolve_target,
                                                  .resolve_mode = mode};
        };

        const VkResolveModeFlags stencil_modes = fixture.device.GetPhysicalDevice().GetDepthStencilResolveProperties().supportedStencilResolveModes;
        REQUIRE((stencil_modes & VK_RESOLVE_MODE_AVERAGE_BIT) == 0);
        REQUIRE(command_buffer.CmdBeginRendering({.render_area_extent = {k_side, k_side},
                                                  .stencil_attachment = side(target, Forge::ResolveMode::Average)}) ==
                ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdBeginRendering({.render_area_extent = {k_side, k_side},
                                                  .depth_attachment = side(target, Forge::ResolveMode::SampleZero),
                                                  .stencil_attachment = side(other_target, Forge::ResolveMode::SampleZero)}) ==
                ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdBeginRendering({.render_area_extent = {k_side, k_side},
                                                  .depth_attachment = side(target, Forge::ResolveMode::SampleZero),
                                                  .stencil_attachment = side(target, Forge::ResolveMode::SampleZero)}) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/**
 * The colour the first draw of the pass left at this texel, read back through an input attachment and
 * written over it with its red and green swapped - so the result is neither what the first draw wrote nor
 * what the clear under it was, and each of those three reads back differently.
 */
constexpr const char* k_input_attachment_source = R"(
[[vk::input_attachment_index(0)]]
[[vk::binding(0, 0)]] SubpassInput<float4> previous;

[shader("fragment")]
float4 main_swap_fragment() : SV_Target
{
    const float4 color = previous.SubpassLoad();
    return float4(color.g, color.r, 1.0, 1.0);
}
)";

}  // namespace

/**
 * DescriptorType::InputAttachment and DeviceFeatures::dynamic_rendering_local_read: one pass draws green over
 * a red clear, orders that write before a read with a ByRegion barrier, and draws again reading what the first
 * draw left at each texel. Magenta means the second draw read green; cyan would mean it read the clear.
 */
TEST_CASE("Forge input attachments", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_features{.dynamic_rendering_local_read = true};
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr i32 k_side = 4;
    constexpr Forge::TextureUsageBits k_usage =
        Forge::TextureUsageBits::ColorAttachment | Forge::TextureUsageBits::InputAttachment | Forge::TextureUsageBits::TransferSource;

    auto make_layout_desc = [](ShaderTypeBits stages)
    {
        Forge::DescriptorSetLayoutDesc layout_desc;
        REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::InputAttachment, 1, stages) == ErrorCode::Success);
        return layout_desc;
    };

    SECTION("A draw reads what an earlier draw of the pass wrote at its texel")
    {
        if (!CanCreateDevice(k_features))
        {
            SKIP("This device cannot read an attachment inside a dynamic rendering pass.");
        }
        ForgeFixture fixture(k_features);
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
        const Forge::Shader green_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));
        const Forge::Shader swap_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_input_attachment_source, {.entry_point = "main_swap_fragment", .cache = GetShaderCache()}));

        // Checked against the shader, which is also what shows reflection reads the binding as an input attachment.
        Forge::DescriptorSetLayoutDesc layout_desc = make_layout_desc(ShaderTypeBits::Fragment);
        layout_desc.shaders.PushBack(Opal::Ref<const Forge::Shader>(swap_shader));
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));
        REQUIRE(layout.GetDesc().bindings[0].name == Opal::StringUtf8("previous"));

        const Forge::Pipeline green_pipeline =
            ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, MakeFullscreenPipelineDesc(vertex_shader, green_shader, k_format)));
        Forge::GraphicsPipelineDesc swap_desc = MakeFullscreenPipelineDesc(vertex_shader, swap_shader, k_format);
        swap_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline swap_pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, swap_desc));

        Forge::Texture color = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = k_format, .width = k_side, .height = k_side, .usage = k_usage}));
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {}));
        Forge::DescriptorPoolDesc pool_desc;
        REQUIRE(pool_desc.Add(Forge::DescriptorType::InputAttachment, 1) == ErrorCode::Success);
        const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, color, sampler, Forge::ImageLayout::General) == ErrorCode::Success);
        const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device, {.size = sizeof(k_fullscreen_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
            Opal::AsBytes(k_fullscreen_vertices)));

        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTransition(color, Forge::ImageLayout::General) == ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                                 .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}}}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(green_pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);

                        const Forge::MemoryBarrier write_before_read{
                            .stages_must_finish = Forge::PipelineStageBits::ColorAttachmentOutput,
                            .stages_must_finish_access = Forge::PipelineStageAccessBits::ColorAttachmentWrite,
                            .before_stages_start = Forge::PipelineStageBits::FragmentShader,
                            .before_stages_start_access = Forge::PipelineStageAccessBits::InputAttachmentRead};
                        REQUIRE(command_buffer.CmdBarriers({.memory = {&write_before_read, 1}, .flags = Forge::DependencyFlagBits::ByRegion}) ==
                                ErrorCode::Success);

                        REQUIRE(command_buffer.CmdBindPipeline(swap_pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindDescriptorSet(swap_pipeline, set) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);

        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, color, k_side, Forge::ImageLayout::General);
        for (i32 texel = 0; texel < k_side * k_side; ++texel)
        {
            const Texel actual{pixels[texel * 4], pixels[texel * 4 + 1], pixels[texel * 4 + 2], pixels[texel * 4 + 3]};
            INFO("texel " << texel << " rgba " << actual.r << " " << actual.g << " " << actual.b << " " << actual.a);
            REQUIRE((actual.r == 255 && actual.g == 0 && actual.b == 255 && actual.a == 255));
        }
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("An input attachment binding or descriptor the pass could not read is refused")
    {
        {
            // Without the feature there is no pass that could read one.
            ForgeFixture fixture;
            REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, make_layout_desc(ShaderTypeBits::Fragment))
                        .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
            REQUIRE_NO_VALIDATION_ERROR(fixture);
        }
        if (!CanCreateDevice(k_features))
        {
            SKIP("This device cannot read an attachment inside a dynamic rendering pass.");
        }
        ForgeFixture fixture(k_features);
        REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, make_layout_desc(ShaderTypeBits::Compute))
                    .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, make_layout_desc(ShaderTypeBits::AllGraphics))
                    .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);

        // A binding the shader reads as an input attachment and the layout declares as something else.
        const Forge::Shader swap_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_input_attachment_source, {.entry_point = "main_swap_fragment", .cache = GetShaderCache()}));
        Forge::DescriptorSetLayoutDesc mismatched;
        mismatched.shaders.PushBack(Opal::Ref<const Forge::Shader>(swap_shader));
        REQUIRE(mismatched.AddBinding(0, Forge::DescriptorType::SampledImage, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, mismatched).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);

        const Forge::DescriptorSetLayout layout =
            ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, make_layout_desc(ShaderTypeBits::Fragment)));
        Forge::DescriptorPoolDesc pool_desc;
        REQUIRE(pool_desc.Add(Forge::DescriptorType::InputAttachment, 1) == ErrorCode::Success);
        const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {}));
        const Forge::Texture readable = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = k_format, .width = k_side, .height = k_side, .usage = k_usage}));
        const Forge::Texture not_an_input = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = k_format, .width = k_side, .height = k_side, .usage = Forge::TextureUsageBits::ColorAttachment}));
        REQUIRE(set.Update(0, not_an_input, sampler, Forge::ImageLayout::General) == ErrorCode::InvalidArgument);
        REQUIRE(set.Update(0, readable, sampler, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::InvalidArgument);
        REQUIRE(set.Update(0, readable, sampler, Forge::ImageLayout::General) == ErrorCode::Success);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
}

TEST_CASE("Forge a pipeline that fails to build leaves nothing behind", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    SECTION("A rejected graphics pipeline releases the layout it had already created")
    {
        // A description with no vertex and no mesh shader, which is rejected after the layout exists. The
        // layout is what a push constant range and a set layout make non-trivial, so both are asked for.
        Forge::DescriptorSetLayoutDesc layout_desc;
        REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        const Forge::DescriptorSetLayout set_layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

        Forge::GraphicsPipelineDesc desc;
        desc.descriptor_set_layouts.PushBack(set_layout);
        desc.push_constant_ranges.PushBack({.shader_stages = ShaderTypeBits::Fragment, .offset = 0, .size = 4});
        REQUIRE(Forge::Pipeline::Create(fixture.device, desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A rejected compute pipeline releases the layout it had already created")
    {
        // The compute constructor is a second path to the same layout, so it is checked on its own. A value
        // for a specialization constant this shader does not declare is rejected after the layout exists.
        const Forge::Shader compute_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc desc;
        desc.shader = compute_shader;
        desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        desc.specialization.PushBack(Forge::SpecializationConstant{.name = "NOT_DECLARED", .value = 1u});
        REQUIRE(Forge::Pipeline::Create(fixture.device, desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

TEST_CASE("Forge specialization constants", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_specialized_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(fixture.device, k_specialized_source,
                                                                            {.entry_point = "main_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                 {.size = sizeof(k_fullscreen_vertices),
                                  .usage = Forge::BufferUsageBits::VertexBuffer},
                                 Opal::AsBytes(k_fullscreen_vertices)));

    /** Builds a pipeline with the given values, draws through it, and hands back one texel. */
    // Hands back what the pipeline creation reported, since three cases below are about values it refuses.
    auto draw_specialized = [&](Opal::ArrayView<const Forge::SpecializationConstant> values)
        -> Opal::Expected<Opal::DynamicArray<u8>, ErrorCode>
    {
        using Result = Opal::Expected<Opal::DynamicArray<u8>, ErrorCode>;

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        Forge::GraphicsPipelineDesc pipeline_desc = MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, k_format);
        for (i32 i = 0; i < values.GetSize(); ++i)
        {
            pipeline_desc.specialization.PushBack(
                Forge::SpecializationConstant{.name = values[i].name.Clone(), .value = values[i].value});
        }
        Opal::Expected<Forge::Pipeline, ErrorCode> pipeline_result = Forge::Pipeline::Create(fixture.device, pipeline_desc);
        if (!pipeline_result.HasValue())
        {
            return Result(pipeline_result.GetError());
        }
        const Forge::Pipeline& pipeline = pipeline_result.GetValue();

        const Opal::DynamicArray<u8> pixels = RenderRaster(
            fixture, color, k_side,
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
            },
            Vector4f{0.0f, 0.0f, 0.0f, 1.0f});
        return Result(Opal::DynamicArray<u8>{pixels[0], pixels[1], pixels[2], pixels[3]});
    };

    SECTION("The shader reports what it declares")
    {
        const auto constants = fragment_shader.GetSpecializationConstants();
        REQUIRE(constants.GetSize() == 2);
        bool found_red = false;
        bool found_blue = false;
        for (i32 i = 0; i < constants.GetSize(); ++i)
        {
            if (constants[i].name == Opal::StringUtf8("RED_LEVEL"))
            {
                found_red = true;
                REQUIRE(constants[i].type == Forge::SpecializationType::Int32);
                REQUIRE(constants[i].byte_size == 4);
                REQUIRE(constants[i].default_value.bits == 64);
            }
            if (constants[i].name == Opal::StringUtf8("WRITE_BLUE"))
            {
                found_blue = true;
                REQUIRE(constants[i].type == Forge::SpecializationType::Bool);
                // VkBool32, not the one byte a bool takes on this side.
                REQUIRE(constants[i].byte_size == 4);
                REQUIRE(constants[i].default_value.bits == 0);
            }
        }
        REQUIRE(found_red);
        REQUIRE(found_blue);
    }
    SECTION("One module becomes two pipelines that render differently")
    {
        // The point of the feature: same Shader objects, different values, different output.
        const Forge::SpecializationConstant dim[] = {{.name = "RED_LEVEL", .value = 32}};
        const Forge::SpecializationConstant bright[] = {{.name = "RED_LEVEL", .value = 200},
                                                       {.name = "WRITE_BLUE", .value = true}};
        const Opal::DynamicArray<u8> dim_texel = ForgeTest::Unwrap(draw_specialized({dim, 1}));
        const Opal::DynamicArray<u8> bright_texel = ForgeTest::Unwrap(draw_specialized({bright, 2}));
        INFO("dim r=" << static_cast<i32>(dim_texel[0]) << " b=" << static_cast<i32>(dim_texel[2])
                      << " bright r=" << static_cast<i32>(bright_texel[0]) << " b="
                      << static_cast<i32>(bright_texel[2]));
        REQUIRE(static_cast<i32>(dim_texel[0]) == 32);
        REQUIRE(static_cast<i32>(dim_texel[2]) == 0);
        REQUIRE(static_cast<i32>(bright_texel[0]) == 200);
        REQUIRE(static_cast<i32>(bright_texel[2]) == 255);
    }
    SECTION("A constant nothing supplies keeps the default the shader declared")
    {
        const Opal::DynamicArray<u8> texel = ForgeTest::Unwrap(draw_specialized({}));
        REQUIRE(static_cast<i32>(texel[0]) == 64);
        REQUIRE(static_cast<i32>(texel[2]) == 0);
    }
    SECTION("A name no stage declares is refused")
    {
        // The case Vulkan ignores in silence when the value is keyed by number, which is why it is keyed
        // by name here.
        const Forge::SpecializationConstant wrong[] = {{.name = "RED_LEVELL", .value = 1}};
        REQUIRE(draw_specialized({wrong, 1}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A value of the wrong type is refused")
    {
        const Forge::SpecializationConstant wrong[] = {{.name = "RED_LEVEL", .value = 1.0f}};
        REQUIRE(draw_specialized({wrong, 1}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("One constant given a value twice is refused")
    {
        // Two map entries with the same constantID, which the specification does not allow within one
        // VkSpecializationInfo - and which reads as nothing worse than a repeated name from out here.
        const Forge::SpecializationConstant twice[] = {{.name = "RED_LEVEL", .value = 32},
                                                       {.name = "RED_LEVEL", .value = 64}};
        REQUIRE(draw_specialized({twice, 2}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A compute pipeline specializes the same way")
    {
        constexpr i32 k_element_count = 128;
        const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random,
                                                    .use_device_address = true}));
        const Forge::Shader compute_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_specialized_compute_source, {.entry_point = "main_specialized", .cache = GetShaderCache()}));
        Forge::DescriptorPoolDesc pool_desc;
        REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1) == ErrorCode::Success);
        const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
        Forge::DescriptorSetLayoutDesc layout_desc;
        REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, output) == ErrorCode::Success);

        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = compute_shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        pipeline_desc.specialization.PushBack(Forge::SpecializationConstant{.name = "ADDEND", .value = 100u});
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        DispatchWithSet(fixture.device, fixture.GetQueue(), pipeline, set, k_element_count / 64);
        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        // 100 rather than the 5 the shader declares, so the value came from the pipeline.
        for (i32 i = 0; i < k_element_count; ++i)
        {
            REQUIRE(values[i] == static_cast<u32>(i) + 100);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * A shader whose vertex stage reads three attributes and a push constant block, and whose fragment stage
 * reads two textures. Deliberately shaped like the sample: a struct parameter Slang flattens into three
 * locations, and bindings only the fragment stage declares.
 */
constexpr const char* k_reflected_source = R"(
struct VSInput {
    float2 position;
    float3 tint;
    uint2 flags;
};

layout(set = 0, binding = 0) Sampler2D first_texture;
layout(set = 0, binding = 1) Sampler2D second_texture;

struct Offsets {
    float2 shift;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 tint;
};

[shader("vertex")]
VSOutput main_vertex(VSInput input, uniform Offsets *offsets) {
    VSOutput output;
    output.position = float4(input.position + offsets->shift, 0.0, 1.0);
    output.tint = input.tint * float(input.flags.x + input.flags.y);
    return output;
}

[shader("fragment")]
float4 main_fragment(VSOutput input) {
    return float4(input.tint, 1.0) * first_texture.Sample(float2(0, 0)) * second_texture.Sample(float2(0, 0));
}
)";

/** Two members, one of them never read - the reason an attribute nothing declares cannot be refused. */
constexpr const char* k_unused_input_source = R"(
struct PartialInput {
    float2 position;
    float3 unused_tint;
};

[shader("vertex")]
float4 main_vertex(PartialInput input) : SV_Position {
    return float4(input.position, 0.0, 1.0);
}
)";

/** A binding of a fixed size the shader declares and reflects, rather than the unbounded kind bindless uses. */
constexpr const char* k_fixed_array_source = R"(
[[vk::binding(0, 0)]] Sampler2D textures[4];
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_fixed_array()
{
    output[0] = textures[0].SampleLevel(float2(0.0, 0.0), 0.0);
}
)";

namespace
{

/**
 * Two constants whose widths are not a word: one sixteen bits and one sixty four. Both are written into the
 * output as plain words, the wide one in halves, so what comes back says how many bits of each value
 * reached the shader.
 */
constexpr const char* k_odd_width_specialized_source = R"(
[SpecializationConstant]
const int16_t NARROW = 2;

[SpecializationConstant]
const int64_t WIDE = 5;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_odd_widths(uniform uint32_t *output)
{
    output[0] = (uint)NARROW;
    output[1] = (uint)(WIDE & 0xFFFFFFFF);
    output[2] = (uint)((WIDE >> 32) & 0xFFFFFFFF);
}
)";

/** The reported constant of that name, which a case reads the declared type and width off. */
const Forge::SpecializationConstantInfo& SpecializationConstantNamed(const Forge::Shader& shader, const char* name)
{
    const Opal::ArrayView<const Forge::SpecializationConstantInfo> constants = shader.GetSpecializationConstants();
    for (i32 i = 0; i < constants.GetSize(); ++i)
    {
        if (constants[i].name == Opal::StringUtf8(name))
        {
            return constants[i];
        }
    }
    FAIL("the shader declares no specialization constant called " << name);
    return constants[0];
}

}  // namespace

TEST_CASE("Forge specialization constants that are not a word wide", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_odd_widths{.shader_int16 = true, .shader_int64 = true};
    if (!CanCreateDevice(k_odd_widths))
    {
        SKIP("This device has no sixteen or sixty four bit integers in shaders.");
    }
    ForgeFixture fixture(k_odd_widths);
    constexpr i32 k_element_count = 4;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_odd_width_specialized_source, {.entry_point = "main_odd_widths", .cache = GetShaderCache()}));

    /** Dispatch with the given values and hand back the three words the shader wrote, or what refused. */
    auto dispatch_with = [&](Opal::ArrayView<const Forge::SpecializationConstant> values)
        -> Opal::Expected<Opal::DynamicArray<u32>, ErrorCode>
    {
        using Result = Opal::Expected<Opal::DynamicArray<u32>, ErrorCode>;

        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        for (i32 i = 0; i < values.GetSize(); ++i)
        {
            pipeline_desc.specialization.PushBack(Forge::SpecializationConstant{.name = values[i].name.Clone(), .value = values[i].value});
        }
        Opal::Expected<Forge::Pipeline, ErrorCode> pipeline_result = Forge::Pipeline::Create(fixture.device, pipeline_desc);
        if (!pipeline_result.HasValue())
        {
            return Result(pipeline_result.GetError());
        }

        const Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);
        const VkDeviceAddress output_address = output.GetNativeDeviceAddress();
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdBindPipeline(pipeline_result.GetValue()) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(pipeline_result.GetValue(), ShaderTypeBits::Compute,
                                                                Opal::AsBytes(output_address)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                    }) == ErrorCode::Success);
        Opal::DynamicArray<u32> values_read(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values_read.GetData()), values_read.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        return Result(std::move(values_read));
    };

    SECTION("The declared width is what reflection reports beside the type")
    {
        // Anything narrower than a word is reported as its 32 bit counterpart so a caller can write a plain
        // integer for it, and byte_size is where the width it actually occupies survives.
        const Forge::SpecializationConstantInfo& narrow = SpecializationConstantNamed(shader, "NARROW");
        REQUIRE(narrow.type == Forge::SpecializationType::Int32);
        REQUIRE(narrow.byte_size == 2);

        const Forge::SpecializationConstantInfo& wide = SpecializationConstantNamed(shader, "WIDE");
        REQUIRE(wide.type == Forge::SpecializationType::Int64);
        REQUIRE(wide.byte_size == 8);
    }
    SECTION("A constant narrower than a word takes the value it was given")
    {
        const Forge::SpecializationConstant values[] = {{.name = "NARROW", .value = 1234}};
        const Opal::DynamicArray<u32> written = ForgeTest::Unwrap(dispatch_with({values, 1}));
        REQUIRE(written[0] == 1234u);
        // And the constant nobody specialized keeps what the shader declared.
        REQUIRE(written[1] == 5u);
    }
    SECTION("A sixty four bit constant arrives whole")
    {
        // A value with bits above the low word, which is the half a constant written as four bytes loses.
        constexpr i64 k_wide_value = (i64{0x1234} << 32) | 0x5678ABCD;
        const Forge::SpecializationConstant values[] = {{.name = "WIDE", .value = k_wide_value}};
        const Opal::DynamicArray<u32> written = ForgeTest::Unwrap(dispatch_with({values, 1}));
        REQUIRE(written[1] == 0x5678ABCDu);
        REQUIRE(written[2] == 0x1234u);
        REQUIRE(written[0] == 2u);
    }
    SECTION("A value too wide for the constant it is going into is refused")
    {
        // Forty thousand does not fit in a signed sixteen bit constant, and the type reflection reports for
        // it is Int32 - so nothing but the byte size stands between this and a silent truncation.
        const Forge::SpecializationConstant too_big[] = {{.name = "NARROW", .value = 40000}};
        REQUIRE(dispatch_with({too_big, 1}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);

        // The largest value that does fit is accepted, so the check is the width and not the sign.
        const Forge::SpecializationConstant fits[] = {{.name = "NARROW", .value = 32767}};
        const Opal::DynamicArray<u32> written = ForgeTest::Unwrap(dispatch_with({fits, 1}));
        REQUIRE(written[0] == 32767u);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** One float constant, written out as its bit pattern so what comes back can be compared exactly. */
constexpr const char* k_float_specialized_source = R"(
[SpecializationConstant]
const float SCALE = 0.5;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_float_constant(uniform uint32_t *output)
{
    output[0] = asuint(SCALE);
}
)";

/**
 * An unsigned sixty four bit constant written out in halves, and a double one read back through arithmetic
 * done in double: the whole part, and the fraction scaled by two to the fortieth. A fraction that fine lives
 * in the low word of the double's bit pattern, so it only comes back right when both words arrived.
 */
constexpr const char* k_wide_specialized_source = R"(
[SpecializationConstant]
const uint64_t WIDE_UNSIGNED = 7;

[SpecializationConstant]
const double PRECISE = 1.0;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_wide_constants(uniform uint32_t *output)
{
    output[0] = (uint)(WIDE_UNSIGNED & 0xFFFFFFFF);
    output[1] = (uint)((WIDE_UNSIGNED >> 32) & 0xFFFFFFFF);
    output[2] = (uint)PRECISE;
    output[3] = (uint)((PRECISE - 1.0) * 1099511627776.0);
}
)";

/**
 * Build a compute pipeline over `shader` with the given values, dispatch it once with the output's address
 * pushed, and hand back the words it wrote - or what refused to build it.
 */
Opal::Expected<Opal::DynamicArray<u32>, ErrorCode> DispatchSpecialized(ForgeFixture& fixture, const Forge::Shader& shader,
                                                                       Opal::ArrayView<const Forge::SpecializationConstant> values,
                                                                       i32 word_count)
{
    using Result = Opal::Expected<Opal::DynamicArray<u32>, ErrorCode>;

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.push_constant_ranges.PushBack({.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
    for (i32 i = 0; i < values.GetSize(); ++i)
    {
        pipeline_desc.specialization.PushBack(Forge::SpecializationConstant{.name = values[i].name.Clone(), .value = values[i].value});
    }
    Opal::Expected<Forge::Pipeline, ErrorCode> pipeline_result = Forge::Pipeline::Create(fixture.device, pipeline_desc);
    if (!pipeline_result.HasValue())
    {
        return Result(pipeline_result.GetError());
    }

    const Forge::Buffer output = MakeWipedOutput(fixture.device, word_count);
    const VkDeviceAddress output_address = output.GetNativeDeviceAddress();
    REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                   [&](Forge::CommandBuffer& command_buffer)
                                   {
                                       REQUIRE(command_buffer.CmdBindPipeline(pipeline_result.GetValue()) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdPushConstants(pipeline_result.GetValue(), ShaderTypeBits::Compute,
                                                                               Opal::AsBytes(output_address)) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                                   }) == ErrorCode::Success);
    Opal::DynamicArray<u32> words(word_count);
    REQUIRE(output.Read({reinterpret_cast<u8*>(words.GetData()), words.GetSize() * sizeof(u32)}) == ErrorCode::Success);
    return Result(std::move(words));
}

/** The bits of a float as a word, the way the shader's asuint hands them back. */
u32 FloatBits(f32 value)
{
    u32 bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

}  // namespace

/**
 * A Float32 specialization constant, which until now only ever reached a pipeline as the wrong type being
 * refused. The value is compared as a bit pattern, so a float stored through the integer path - converted
 * to 0 rather than copied - or read back from the wrong half of the eight bytes, shows as a different word.
 */
namespace
{

/** A one byte specialization constant, which needs the Int8 capability and so DeviceFeatures::shader_int8. */
constexpr const char* k_int8_constant_source = R"(
[SpecializationConstant]
const uint8_t TINY = 7;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_int8(uniform uint32_t *output)
{
    output[0] = (uint)TINY;
}
)";

/**
 * Arithmetic in half on a value read from the buffer, so nothing can be folded at compile time: the input
 * is 1 + 2^-12, which a half cannot hold and rounds to one, so the result tells half arithmetic from float.
 */
constexpr const char* k_float16_source = R"(
[shader("compute")]
[numthreads(1, 1, 1)]
void main_half(uniform float *values)
{
    const half narrowed = half(values[1]);
    values[0] = float(narrowed * half(2.0) + half(0.25));
}
)";

}  // namespace

/**
 * DeviceFeatures::shader_int8 and shader_float16. Each is used and read back, and each is shown to reach
 * its own Vulkan bit by a device with only the other one on, where the layer refuses the shader that needs
 * the missing one - the way the 64-bit atomics case pins its two fields.
 */
TEST_CASE("Forge 8 and 16 bit shader scalars", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_both{.shader_int8 = true, .shader_float16 = true};
    if (!CanCreateDevice(k_both))
    {
        SKIP("This device has no 8-bit integers or no half floats in shaders.");
    }

    /** Dispatch one of the two shaders over a buffer of two words holding the given second word. */
    auto dispatch = [](ForgeFixture& fixture, const Forge::Shader& shader, Opal::ArrayView<const Forge::SpecializationConstant> values,
                       u32 second_word) -> Opal::Expected<Opal::DynamicArray<u32>, ErrorCode>
    {
        using Result = Opal::Expected<Opal::DynamicArray<u32>, ErrorCode>;
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.push_constant_ranges.PushBack({.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        for (i32 i = 0; i < values.GetSize(); ++i)
        {
            pipeline_desc.specialization.PushBack(Forge::SpecializationConstant{.name = values[i].name.Clone(), .value = values[i].value});
        }
        Opal::Expected<Forge::Pipeline, ErrorCode> pipeline = Forge::Pipeline::Create(fixture.device, pipeline_desc);
        if (!pipeline.HasValue())
        {
            return Result(pipeline.GetError());
        }
        const Forge::Buffer buffer = MakeWipedOutput(fixture.device, 2);
        const u32 initial[] = {0, second_word};
        REQUIRE(buffer.Update(Opal::AsBytes(initial)) == ErrorCode::Success);
        const VkDeviceAddress address = buffer.GetNativeDeviceAddress();
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           REQUIRE(command_buffer.CmdBindPipeline(pipeline.GetValue()) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdPushConstants(pipeline.GetValue(), ShaderTypeBits::Compute,
                                                                                   Opal::AsBytes(address)) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                                       }) == ErrorCode::Success);
        Opal::DynamicArray<u32> words(2);
        REQUIRE(buffer.Read({reinterpret_cast<u8*>(words.GetData()), words.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        return Result(std::move(words));
    };

    SECTION("A one byte specialization constant reports its width and takes a value that fits it")
    {
        ForgeFixture fixture(k_both);
        const Forge::Shader shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_int8_constant_source, {.entry_point = "main_int8", .cache = GetShaderCache()}));
        const Opal::ArrayView<const Forge::SpecializationConstantInfo> constants = shader.GetSpecializationConstants();
        REQUIRE(constants.GetSize() == 1);
        REQUIRE(constants[0].type == Forge::SpecializationType::UInt32);
        REQUIRE(constants[0].byte_size == 1);

        REQUIRE(ForgeTest::Unwrap(dispatch(fixture, shader, {}, 0))[0] == 7u);
        const Forge::SpecializationConstant fits[] = {{.name = "TINY", .value = 200u}};
        REQUIRE(ForgeTest::Unwrap(dispatch(fixture, shader, {fits, 1}, 0))[0] == 200u);
        // One past what a byte holds, which only the declared width refuses.
        const Forge::SpecializationConstant too_wide[] = {{.name = "TINY", .value = 256u}};
        REQUIRE(dispatch(fixture, shader, {too_wide, 1}, 0).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("Arithmetic in half rounds the way a half does")
    {
        ForgeFixture fixture(k_both);
        const Forge::Shader shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_float16_source, {.entry_point = "main_half", .cache = GetShaderCache()}));
        const f32 input = 1.0f + 1.0f / 4096.0f;
        u32 input_bits = 0;
        memcpy(&input_bits, &input, sizeof(input_bits));
        const Opal::DynamicArray<u32> words = ForgeTest::Unwrap(dispatch(fixture, shader, {}, input_bits));
        f32 result = 0.0f;
        memcpy(&result, &words[0], sizeof(result));
        // In float the answer is 2.25 and a little; in half the input was one before anything was done to it.
        INFO("result " << result);
        REQUIRE(result == 2.25f);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("With only half on, the layer refuses the one byte constant")
    {
        // Nothing is built past the shader - a pipeline over a module the layer rejected is undefined.
        ForgeFixture fixture({.shader_float16 = true});
        REQUIRE(fixture.status == ErrorCode::Success);
        const Forge::Shader shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_int8_constant_source, {.entry_point = "main_int8", .cache = GetShaderCache()}));
        INFO(*fixture.GetValidationErrors());
        REQUIRE(fixture.GetValidationErrorCount() > 0);
    }
    SECTION("With only 8-bit integers on, the layer refuses the half arithmetic")
    {
        ForgeFixture fixture({.shader_int8 = true});
        REQUIRE(fixture.status == ErrorCode::Success);
        const Forge::Shader shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_float16_source, {.entry_point = "main_half", .cache = GetShaderCache()}));
        INFO(*fixture.GetValidationErrors());
        REQUIRE(fixture.GetValidationErrorCount() > 0);
    }
}

TEST_CASE("Forge a floating point specialization constant", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_word_count = 1;
    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_float_specialized_source, {.entry_point = "main_float_constant", .cache = GetShaderCache()}));

    SECTION("Reflection reports the type, the width and the declared default")
    {
        const Forge::SpecializationConstantInfo& scale = SpecializationConstantNamed(shader, "SCALE");
        REQUIRE(scale.type == Forge::SpecializationType::Float32);
        REQUIRE(scale.byte_size == 4);
        REQUIRE(scale.default_value.bits == FloatBits(0.5f));
    }
    SECTION("A float value reaches the shader bit for bit")
    {
        // A value with a mantissa that runs to the last bit, so nothing about it survives a conversion.
        constexpr f32 k_value = 0.1f;
        const Forge::SpecializationConstant values[] = {{.name = "SCALE", .value = k_value}};
        const Opal::DynamicArray<u32> written = ForgeTest::Unwrap(DispatchSpecialized(fixture, shader, {values, 1}, k_word_count));
        REQUIRE(written[0] == FloatBits(k_value));
    }
    SECTION("Nothing supplied leaves the declared default")
    {
        const Opal::DynamicArray<u32> written = ForgeTest::Unwrap(DispatchSpecialized(fixture, shader, {}, k_word_count));
        REQUIRE(written[0] == FloatBits(0.5f));
    }
    SECTION("A double for a float constant is refused")
    {
        // Twice the width, and the one mistake an unsuffixed literal makes easy to write.
        const Forge::SpecializationConstant wrong[] = {{.name = "SCALE", .value = 0.1}};
        REQUIRE(DispatchSpecialized(fixture, shader, {wrong, 1}, k_word_count).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * UInt64 and Float64 specialization constants, the two constructors of SpecializationValue nothing had
 * called. Both carry information in each of their two words, so a value stored or packed as four bytes
 * loses half of itself, and the unsigned one has its top bit set, so it is also not a signed value in
 * disguise.
 */
TEST_CASE("Forge sixty four bit specialization constants of unsigned and floating point type", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_wide_types{.shader_int64 = true, .shader_float64 = true};
    if (!CanCreateDevice(k_wide_types))
    {
        SKIP("This device has no sixty four bit integers or doubles in shaders.");
    }
    ForgeFixture fixture(k_wide_types);
    constexpr i32 k_word_count = 4;
    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_wide_specialized_source, {.entry_point = "main_wide_constants", .cache = GetShaderCache()}));

    SECTION("Reflection reports each type at eight bytes, with its declared default")
    {
        const Forge::SpecializationConstantInfo& wide = SpecializationConstantNamed(shader, "WIDE_UNSIGNED");
        REQUIRE(wide.type == Forge::SpecializationType::UInt64);
        REQUIRE(wide.byte_size == 8);
        REQUIRE(wide.default_value.bits == 7u);

        const Forge::SpecializationConstantInfo& precise = SpecializationConstantNamed(shader, "PRECISE");
        REQUIRE(precise.type == Forge::SpecializationType::Float64);
        REQUIRE(precise.byte_size == 8);
        f64 declared = 0.0;
        memcpy(&declared, &precise.default_value.bits, sizeof(declared));
        REQUIRE(declared == 1.0);
    }
    SECTION("Both constants arrive whole")
    {
        constexpr u64 k_wide_value = (u64{0x80001234} << 32) | 0x5678ABCDu;
        // One and three parts in two to the fortieth: the fraction sits in the low word of the bit pattern,
        // and the shader's (value - 1) * 2^40 turns it back into exactly three.
        const f64 precise_value = 1.0 + 3.0 / 1099511627776.0;
        const Forge::SpecializationConstant values[] = {{.name = "WIDE_UNSIGNED", .value = k_wide_value},
                                                        {.name = "PRECISE", .value = precise_value}};
        const Opal::DynamicArray<u32> written = ForgeTest::Unwrap(DispatchSpecialized(fixture, shader, {values, 2}, k_word_count));
        INFO("words " << written[0] << " " << written[1] << " " << written[2] << " " << written[3]);
        REQUIRE(written[0] == 0x5678ABCDu);
        REQUIRE(written[1] == 0x80001234u);
        REQUIRE(written[2] == 1u);
        REQUIRE(written[3] == 3u);
    }
    SECTION("Nothing supplied leaves the declared defaults")
    {
        const Opal::DynamicArray<u32> written = ForgeTest::Unwrap(DispatchSpecialized(fixture, shader, {}, k_word_count));
        REQUIRE(written[0] == 7u);
        REQUIRE(written[1] == 0u);
        REQUIRE(written[2] == 1u);
        REQUIRE(written[3] == 0u);
    }
    SECTION("A value of the right width and the wrong type is refused")
    {
        // Signed for unsigned and float for double: the first is the same eight bytes read differently, the
        // second is half of them.
        const Forge::SpecializationConstant signed_for_unsigned[] = {{.name = "WIDE_UNSIGNED", .value = i64{7}}};
        REQUIRE(DispatchSpecialized(fixture, shader, {signed_for_unsigned, 1}, k_word_count).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        const Forge::SpecializationConstant float_for_double[] = {{.name = "PRECISE", .value = 1.0f}};
        REQUIRE(DispatchSpecialized(fixture, shader, {float_for_double, 1}, k_word_count).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge shader reflection", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Forge::Shader vertex_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_reflected_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_reflected_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));

    SECTION("Each stage reports only what it reads")
    {
        // The reason the entry point scoped enumerators are the ones called: the vertex stage of this module
        // declares no bindings at all, and the module wide call would have handed it the fragment's two.
        REQUIRE(vertex_shader.GetInputs().GetSize() == 3);
        REQUIRE(vertex_shader.GetBindings().GetSize() == 0);
        REQUIRE(vertex_shader.GetPushConstants().GetSize() == 1);
        REQUIRE(fragment_shader.GetBindings().GetSize() == 2);
        REQUIRE(fragment_shader.GetPushConstants().GetSize() == 0);

        const Opal::ArrayView<const Forge::ShaderInputInfo> inputs = vertex_shader.GetInputs();
        for (i32 i = 0; i < inputs.GetSize(); ++i)
        {
            INFO("input " << inputs[i].name.GetData());
            if (inputs[i].location == 0)
            {
                REQUIRE(inputs[i].format == PixelFormat::R32G32_SFLOAT);
            }
            if (inputs[i].location == 1)
            {
                REQUIRE(inputs[i].format == PixelFormat::R32G32B32_SFLOAT);
            }
            if (inputs[i].location == 2)
            {
                REQUIRE(inputs[i].format == PixelFormat::R32G32_UINT);
            }
        }

        const Opal::ArrayView<const Forge::ShaderBindingInfo> bindings = fragment_shader.GetBindings();
        bool found_first = false;
        for (i32 i = 0; i < bindings.GetSize(); ++i)
        {
            REQUIRE(bindings[i].set == 0);
            REQUIRE(bindings[i].descriptor_type == Forge::DescriptorType::CombinedImageSampler);
            REQUIRE(bindings[i].descriptor_count == 1);
            if (bindings[i].name == Opal::StringUtf8("first_texture"))
            {
                found_first = true;
                REQUIRE(bindings[i].binding == 0);
            }
        }
        REQUIRE(found_first);

        // A pointer parameter is how Slang spells a push constant block; eight bytes of float2 here.
        REQUIRE(vertex_shader.GetPushConstants()[0].offset == 0);
        REQUIRE(vertex_shader.GetPushConstants()[0].size == 8);
    }
    SECTION("A vertex input built from the shader packs the attributes in location order")
    {
        const Forge::VertexInputDesc derived = ForgeTest::Unwrap(Forge::VertexInputDesc::FromShader(vertex_shader));
        REQUIRE(derived.bindings.GetSize() == 1);
        const Forge::VertexInputDesc::Binding& binding = derived.bindings[0];
        REQUIRE(binding.binding == 0);
        REQUIRE(binding.attributes.GetSize() == 3);
        // float2 then float3 then uint2, tightly packed: 8, 12 and 8 bytes.
        REQUIRE(binding.attributes[0].location == 0);
        REQUIRE(binding.attributes[0].offset == 0);
        REQUIRE(binding.attributes[1].location == 1);
        REQUIRE(binding.attributes[1].offset == 8);
        REQUIRE(binding.attributes[2].location == 2);
        REQUIRE(binding.attributes[2].offset == 20);
        REQUIRE(binding.stride == 28);
    }
    SECTION("A stage that reads no vertex buffer has no attributes to give")
    {
        REQUIRE(Forge::VertexInputDesc::FromShader(fragment_shader).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("FromShader honours the binding and the input rate it is given")
    {
        const Forge::VertexInputDesc derived =
            ForgeTest::Unwrap(Forge::VertexInputDesc::FromShader(vertex_shader, 3, DataRepetition::PerInstance));
        REQUIRE(derived.bindings.GetSize() == 1);
        const Forge::VertexInputDesc::Binding& binding = derived.bindings[0];
        REQUIRE(binding.binding == 3);
        REQUIRE(binding.input_rate == DataRepetition::PerInstance);
        // The layout of the attributes is the shader's business regardless of which binding or rate they
        // land on, so the packing is the same as the default case's.
        REQUIRE(binding.attributes.GetSize() == 3);
        REQUIRE(binding.stride == 28);
    }
    SECTION("Push constant ranges come back merged across the stages that declare them")
    {
        const Opal::Ref<const Forge::Shader> shaders[] = {vertex_shader, fragment_shader};
        const Opal::DynamicArray<Forge::PushConstantRange> ranges = Forge::PushConstantRangesFromShaders({shaders, 2});
        REQUIRE(ranges.GetSize() == 1);
        REQUIRE(ranges[0].offset == 0);
        REQUIRE(ranges[0].size == 8);
        REQUIRE(!!(ranges[0].shader_stages & ShaderTypeBits::Vertex));
    }

    const Opal::Ref<const Forge::Shader> pipeline_shaders[] = {vertex_shader, fragment_shader};
    const Opal::DynamicArray<Forge::PushConstantRange> derived_ranges =
        Forge::PushConstantRangesFromShaders({pipeline_shaders, 2});
    const Opal::ArrayView<const Forge::PushConstantRange> good_ranges(derived_ranges.GetData(), derived_ranges.GetSize());

    /** Builds a graphics pipeline around the two stages, with whatever vertex input and ranges are handed in. */
    auto build_pipeline = [&](const Forge::VertexInputDesc& vertex_input,
                              Opal::ArrayView<const Forge::PushConstantRange> ranges)
    {
        Forge::DescriptorSetLayoutDesc layout_desc;
        REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_input = vertex_input.Clone();
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        for (i32 i = 0; i < ranges.GetSize(); ++i)
        {
            pipeline_desc.push_constant_ranges.PushBack(ranges[i]);
        }
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_attachment_formats.PushBack(PixelFormat::R8G8B8A8_UNORM);
        return Forge::Pipeline::Create(fixture.device, pipeline_desc);
    };

    SECTION("The derived vertex input and ranges build a pipeline")
    {
        const Forge::VertexInputDesc derived = ForgeTest::Unwrap(Forge::VertexInputDesc::FromShader(vertex_shader));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(build_pipeline(derived, good_ranges));
        REQUIRE(pipeline.IsValid());
    }
    SECTION("A location the shader reads that nothing feeds is refused")
    {
        Forge::VertexInputDesc incomplete;
        incomplete.AddBinding(0, 28);
        REQUIRE(incomplete.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        REQUIRE(incomplete.AddAttribute(0, 1, PixelFormat::R32G32B32_SFLOAT, 8) == ErrorCode::Success);
        REQUIRE(build_pipeline(incomplete, good_ranges).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("An attribute at a location the shader declares nothing at is accepted")
    {
        // Tempting to refuse, and wrong to: an input the shader does not read is optimised out of the
        // SPIR-V, so this is indistinguishable from a vertex struct with a field only some of its pipelines
        // read. The unused input further down proves the two really are the same case from out here.
        Forge::VertexInputDesc extra = ForgeTest::Unwrap(Forge::VertexInputDesc::FromShader(vertex_shader));
        REQUIRE(extra.AddAttribute(0, 7, PixelFormat::R32_SFLOAT, 28) == ErrorCode::Success);
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(build_pipeline(extra, good_ranges));
        REQUIRE(pipeline.IsValid());
    }
    SECTION("An input the shader never reads is not reported at all")
    {
        // Why the check above cannot exist. The struct has two members and reflection reports one.
        const Forge::Shader partial = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(fixture.device, k_unused_input_source,
                                                                        {.entry_point = "main_vertex", .cache = GetShaderCache()}));
        REQUIRE(partial.GetInputs().GetSize() == 1);
        REQUIRE(partial.GetInputs()[0].location == 0);
    }
    SECTION("An attribute of the wrong numeric class is refused")
    {
        // Location 2 is a uint2 in the shader; a float attribute of the same width is not the same thing.
        Forge::VertexInputDesc wrong_class;
        wrong_class.AddBinding(0, 28);
        REQUIRE(wrong_class.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        REQUIRE(wrong_class.AddAttribute(0, 1, PixelFormat::R32G32B32_SFLOAT, 8) == ErrorCode::Success);
        REQUIRE(wrong_class.AddAttribute(0, 2, PixelFormat::R32G32_SFLOAT, 20) == ErrorCode::Success);
        REQUIRE(build_pipeline(wrong_class, good_ranges).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A normalised attribute feeding a float input is accepted")
    {
        // UNORM arrives in the shader as a float, so the class agrees even though the format does not.
        Forge::VertexInputDesc normalised;
        normalised.AddBinding(0, 28);
        REQUIRE(normalised.AddAttribute(0, 0, PixelFormat::R8G8_UNORM, 0) == ErrorCode::Success);
        REQUIRE(normalised.AddAttribute(0, 1, PixelFormat::R32G32B32_SFLOAT, 8) == ErrorCode::Success);
        REQUIRE(normalised.AddAttribute(0, 2, PixelFormat::R32G32_UINT, 20) == ErrorCode::Success);
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(build_pipeline(normalised, good_ranges));
        REQUIRE(pipeline.IsValid());
    }
    SECTION("A push constant range that stops short of what the shader reads is refused")
    {
        const Forge::VertexInputDesc derived = ForgeTest::Unwrap(Forge::VertexInputDesc::FromShader(vertex_shader));
        const Forge::PushConstantRange too_small{.shader_stages = ShaderTypeBits::Vertex, .offset = 0, .size = 4};
        REQUIRE(build_pipeline(derived, {&too_small, 1}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("No push constant range at all, for a shader that reads one, is refused")
    {
        // The likeliest way to get this wrong, and the reason the check does not wait for a range to exist.
        const Forge::VertexInputDesc derived = ForgeTest::Unwrap(Forge::VertexInputDesc::FromShader(vertex_shader));
        REQUIRE(build_pipeline(derived, {}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge descriptor bindings checked against the shader", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Forge::Shader vertex_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_reflected_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_reflected_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));

    /** A layout desc naming both stages, so the check has the shader that declares the bindings. */
    auto make_desc = [&]()
    {
        Forge::DescriptorSetLayoutDesc desc;
        desc.shaders.PushBack(Opal::Ref<const Forge::Shader>(vertex_shader));
        desc.shaders.PushBack(Opal::Ref<const Forge::Shader>(fragment_shader));
        return desc;
    };

    SECTION("A layout that agrees with the shader is given the names")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, desc));
        REQUIRE(layout.GetDesc().bindings[0].name == Opal::StringUtf8("first_texture"));
        REQUIRE(layout.GetDesc().bindings[1].name == Opal::StringUtf8("second_texture"));
        // The caller's desc is untouched - the names went onto the layout's own copy.
        REQUIRE(desc.bindings[0].name.IsEmpty());
    }
    SECTION("A binding declared as the wrong kind is refused")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A binding sized for fewer descriptors than the shader indexes is refused")
    {
        // A fixed array of four, not the unbounded kind bindless writes - the shader's declared size is
        // what reflection reports back as descriptor_count, and a layout naming fewer of them is refused
        // rather than left for the driver to read past the end of what it was given.
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_fixed_array_source, {.entry_point = "main_fixed_array", .cache = GetShaderCache()}));
        Forge::DescriptorSetLayoutDesc desc;
        desc.shaders.PushBack(Opal::Ref<const Forge::Shader>(shader));
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 2, ShaderTypeBits::Compute) == ErrorCode::Success);
        REQUIRE(desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);

        // The same shader against a layout that names at least as many is accepted - four exactly, and more
        // than four, which is what a bindless array sized past what one shader happens to index looks like.
        Forge::DescriptorSetLayoutDesc exact_desc;
        exact_desc.shaders.PushBack(Opal::Ref<const Forge::Shader>(shader));
        REQUIRE(exact_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 4, ShaderTypeBits::Compute) == ErrorCode::Success);
        REQUIRE(exact_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, exact_desc)).IsValid());

        Forge::DescriptorSetLayoutDesc more_desc;
        more_desc.shaders.PushBack(Opal::Ref<const Forge::Shader>(shader));
        REQUIRE(more_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 8, ShaderTypeBits::Compute) == ErrorCode::Success);
        REQUIRE(more_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, more_desc)).IsValid());
    }
    SECTION("A binding whose stages leave out the one that reads it is refused")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Vertex) == ErrorCode::Success);
        REQUIRE(desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A binding the shaders read that the layout omits is refused")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A binding no shader reads is accepted and stays nameless")
    {
        // A descriptor nothing samples is optimised out of the SPIR-V, so reflection cannot tell this apart
        // from a binding that was never declared - the sample binds a metallic roughness texture its shader
        // does not read yet. It keeps an empty name, which is the whole of what it costs.
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(desc.AddBinding(5, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, desc));
        REQUIRE(layout.GetDesc().bindings[0].name == Opal::StringUtf8("first_texture"));
        REQUIRE(layout.GetDesc().bindings[2].name.IsEmpty());
    }
    SECTION("A set writes the same descriptor by name as by index")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, desc));

        Forge::DescriptorPoolDesc pool_desc;
        REQUIRE(pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 4) == ErrorCode::Success);
        pool_desc.max_sets = 2;
        const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

        const Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                      .width = 4,
                                                      .height = 4,
                                                      .usage = Forge::TextureUsageBits::Sampled}));
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.max_anisotropy = 1.0f}));

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(ForgeTest::Unwrap(set.GetBindingIndex("first_texture")) == 0);
        REQUIRE(ForgeTest::Unwrap(set.GetBindingIndex("second_texture")) == 1);
        REQUIRE(set.Update("first_texture", texture, sampler) == ErrorCode::Success);
        REQUIRE(set.Update("second_texture", texture, sampler) == ErrorCode::Success);
        REQUIRE(set.GetBindingIndex("third_texture").GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A set from a layout built without shaders carries no names")
    {
        Forge::DescriptorSetLayoutDesc desc;
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, desc));

        Forge::DescriptorPoolDesc pool_desc;
        REQUIRE(pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 1) == ErrorCode::Success);
        const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.GetBindingIndex("first_texture").GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * One binding of each of the six descriptor kinds Forge models, in the order of k_every_kind_declared below.
 * Every one of them is read or written, since a descriptor the entry point never touches is optimised out of
 * the SPIR-V and reflection would not report it.
 */
constexpr const char* k_every_kind_source = R"(
struct Scale
{
    float4 value;
};

[[vk::binding(0, 0)]] Sampler2D combined_texture;
[[vk::binding(1, 0)]] Texture2D<float4> sampled_texture;
[[vk::binding(2, 0)]] SamplerState plain_sampler;
[[vk::image_format("rgba8")]]
[[vk::binding(3, 0)]] RWTexture2D<float4> storage_image;
[[vk::binding(4, 0)]] ConstantBuffer<Scale> scale;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_every_kind()
{
    float4 sampled = combined_texture.SampleLevel(float2(0.5, 0.5), 0.0) +
                     sampled_texture.SampleLevel(plain_sampler, float2(0.5, 0.5), 0.0);
    storage_image[uint2(0, 0)] = sampled;
    output[0] = sampled * scale.value;
}
)";

/**
 * The layout check for the four kinds the case above never reaches - it only ever declares combined image
 * samplers, and the storage buffer cases elsewhere cover a fifth. Each of the six SPIR-V kinds
 * `ToDescriptorType` maps is read back through reflection, accepted when the layout agrees, and refused
 * against every other kind, so an entry of the table swapped with another shows up here and not at a draw.
 */
TEST_CASE("Forge descriptor bindings of every kind checked against the shader", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Forge::Shader shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_every_kind_source, {.entry_point = "main_every_kind", .cache = GetShaderCache()}));

    // Every kind a compute shader can declare. InputAttachment is read by the fragment stage alone, so it is
    // left to the input attachment case, which checks its reflection the same way; it is the last kind, which
    // is what lets the count stop short of it.
    static_assert(static_cast<i32>(Forge::DescriptorType::InputAttachment) + 1 == static_cast<i32>(Forge::DescriptorType::EnumCount));
    constexpr i32 k_kind_count = static_cast<i32>(Forge::DescriptorType::InputAttachment);
    // Indexed by binding. Every kind appears exactly once, which is what lets the wrong-kind section below
    // try each binding against all five kinds it is not.
    constexpr Forge::DescriptorType k_every_kind_declared[k_kind_count] = {
        Forge::DescriptorType::CombinedImageSampler, Forge::DescriptorType::SampledImage,   Forge::DescriptorType::Sampler,
        Forge::DescriptorType::StorageImage,         Forge::DescriptorType::ConstantBuffer, Forge::DescriptorType::StorageBuffer};
    constexpr const char* k_every_kind_names[k_kind_count] = {"combined_texture", "sampled_texture", "plain_sampler",
                                                              "storage_image",    "scale",           "output"};

    /** A layout checked against the shader, declaring binding i as kinds[i]. */
    auto make_desc = [&](const Forge::DescriptorType* kinds)
    {
        Forge::DescriptorSetLayoutDesc desc;
        desc.shaders.PushBack(Opal::Ref<const Forge::Shader>(shader));
        for (i32 i = 0; i < k_kind_count; ++i)
        {
            REQUIRE(desc.AddBinding(static_cast<u32>(i), kinds[i], 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        }
        return desc;
    };

    SECTION("Reflection reports each binding as the kind it is declared as")
    {
        const Opal::ArrayView<const Forge::ShaderBindingInfo> bindings = shader.GetBindings();
        REQUIRE(bindings.GetSize() == k_kind_count);
        bool seen[k_kind_count] = {};
        for (i32 i = 0; i < bindings.GetSize(); ++i)
        {
            const Forge::ShaderBindingInfo& binding = bindings[i];
            INFO("binding " << binding.binding << " named " << binding.name.GetData());
            REQUIRE(binding.set == 0);
            REQUIRE(binding.binding < static_cast<u32>(k_kind_count));
            REQUIRE_FALSE(seen[binding.binding]);
            seen[binding.binding] = true;
            REQUIRE(binding.descriptor_type == k_every_kind_declared[binding.binding]);
            REQUIRE(binding.descriptor_count == 1);
            REQUIRE(binding.name == Opal::StringUtf8(k_every_kind_names[binding.binding]));
        }
    }
    SECTION("A layout that agrees on every kind is given the names and builds a pipeline")
    {
        const Forge::DescriptorSetLayout layout =
            ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, make_desc(k_every_kind_declared)));
        for (i32 i = 0; i < k_kind_count; ++i)
        {
            REQUIRE(layout.GetDesc().bindings[i].name == Opal::StringUtf8(k_every_kind_names[i]));
        }

        // The validation layer compares the pipeline layout against the SPIR-V on its own, which makes it a
        // second opinion on the table above that does not go through Forge's reflection at all.
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
        REQUIRE(pipeline.IsValid());
    }
    SECTION("A binding declared as any other kind is refused")
    {
        for (i32 binding = 0; binding < k_kind_count; ++binding)
        {
            for (i32 kind = 0; kind < k_kind_count; ++kind)
            {
                const auto wrong_kind = static_cast<Forge::DescriptorType>(kind);
                if (wrong_kind == k_every_kind_declared[binding])
                {
                    continue;
                }
                Forge::DescriptorType kinds[k_kind_count];
                for (i32 i = 0; i < k_kind_count; ++i)
                {
                    kinds[i] = k_every_kind_declared[i];
                }
                kinds[binding] = wrong_kind;
                INFO("binding " << binding << " declared as kind " << kind);
                REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, make_desc(kinds)).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
            }
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

constexpr const char* k_named_storage_source = R"(
[[vk::binding(0, 0)]] RWStructuredBuffer<uint> value_buffer;

[shader("compute")]
[numthreads(64, 1, 1)]
void main_named(uint3 thread_id : SV_DispatchThreadID)
{
    value_buffer[thread_id.x] = thread_id.x + 1000;
}
)";

/**
 * DescriptorSet::Update(name, buffer, ...), which had no caller - only the texture by-name overload was ever
 * called. Writes the descriptor by name and then actually dispatches through it, rather than only checking
 * that the call reports Success: a name resolved to the wrong binding index would still return Success and
 * only show up in what the shader read.
 */
TEST_CASE("Forge descriptor set update by name for a buffer", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_element_count = 256;
    constexpr i32 k_group_size = 64;

    const Forge::Shader shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_named_storage_source, {.entry_point = "main_named", .cache = GetShaderCache()}));

    Forge::DescriptorSetLayoutDesc layout_desc;
    layout_desc.shaders.PushBack(Opal::Ref<const Forge::Shader>(shader));
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));
    REQUIRE(layout.GetDesc().bindings[0].name == Opal::StringUtf8("value_buffer"));

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1) == ErrorCode::Success);
    pool_desc.max_sets = 1;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
    Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));

    Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);
    REQUIRE(ForgeTest::Unwrap(set.GetBindingIndex("value_buffer")) == 0);
    REQUIRE(set.Update("value_buffer", output) == ErrorCode::Success);
    // A name the layout does not carry is refused rather than silently doing nothing.
    REQUIRE(set.Update("no_such_buffer", output) == ErrorCode::InvalidArgument);

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    DispatchWithSet(fixture.device, fixture.GetQueue(), pipeline, set, k_element_count / k_group_size);
    RequireComputeWrote(output, k_element_count);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/** Trivial and self-contained, so the cache tests are about the cache and not about what it holds. */
constexpr const char* k_cache_source = R"(
[shader("compute")]
[numthreads(1, 1, 1)]
void main_first(uint3 id : SV_DispatchThreadID, uniform RWStructuredBuffer<uint> output) {
    output[id.x] = 1;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main_second(uint3 id : SV_DispatchThreadID, uniform RWStructuredBuffer<uint> output) {
    output[id.x] = 2;
}
)";

/** The same source with one digit changed, which has to produce different SPIR-V and a different key. */
constexpr const char* k_cache_source_edited = R"(
[shader("compute")]
[numthreads(1, 1, 1)]
void main_first(uint3 id : SV_DispatchThreadID, uniform RWStructuredBuffer<uint> output) {
    output[id.x] = 7;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main_second(uint3 id : SV_DispatchThreadID, uniform RWStructuredBuffer<uint> output) {
    output[id.x] = 2;
}
)";

TEST_CASE("Forge shader cache", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    const Opal::StringUtf8 directory = ForgeTest::GetTestDataPath("shader-cache-test");
    Rndr::ShaderCache cache{directory};
    REQUIRE(!cache.GetDirectory().IsEmpty());

    const Rndr::ShaderCacheKey key =
        Rndr::ShaderCacheKey::Make(k_cache_source, "main_first", Rndr::ShaderOutputFormat::SpirV);
    const Rndr::ShaderCacheKey edited_key =
        Rndr::ShaderCacheKey::Make(k_cache_source_edited, "main_first", Rndr::ShaderOutputFormat::SpirV);
    const Rndr::ShaderCacheKey second_key =
        Rndr::ShaderCacheKey::Make(k_cache_source, "main_second", Rndr::ShaderOutputFormat::SpirV);

    /** Nothing left behind by an earlier run, or a stale hit would make all of this pass for free. */
    auto forget = [&](const Rndr::ShaderCacheKey& k)
    {
        const Opal::StringUtf8 path = cache.GetFilePath(k);
        if (!path.IsEmpty() && Opal::Exists(path))
        {
            REQUIRE(Opal::DeleteFile(path) == Opal::ErrorCode::Success);
        }
    };
    forget(key);
    forget(edited_key);
    forget(second_key);

    /** Compiles through the cache and hands back the bytes it kept, which is what the tests compare. */
    auto compile = [&](const char* source, const char* entry_point)
    {
        const Forge::Shader shader =
            ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(fixture.device, source, {.entry_point = entry_point, .cache = cache}));
        REQUIRE(shader.IsValid());
        return cache.Find(Rndr::ShaderCacheKey::Make(source, entry_point, Rndr::ShaderOutputFormat::SpirV));
    };

    SECTION("An edited source does not come back as the old one")
    {
        // The test that matters. One that only checks a hit is fast proves nothing about correctness.
        const Opal::DynamicArray<u8> original = compile(k_cache_source, "main_first");
        const Opal::DynamicArray<u8> edited = compile(k_cache_source_edited, "main_first");
        REQUIRE(!original.IsEmpty());
        REQUIRE(!edited.IsEmpty());
        REQUIRE(original != edited);
    }
    SECTION("Two entry points of one source do not share an entry")
    {
        const Opal::DynamicArray<u8> first = compile(k_cache_source, "main_first");
        const Opal::DynamicArray<u8> second = compile(k_cache_source, "main_second");
        REQUIRE(first != second);
    }
    SECTION("The same source twice is one compile and the same bytes")
    {
        const Opal::DynamicArray<u8> once = compile(k_cache_source, "main_first");
        const u32 misses_after_first = cache.GetMissCount();
        const Opal::DynamicArray<u8> twice = compile(k_cache_source, "main_first");
        REQUIRE(once == twice);
        // The second call found it, so it did not have to go looking for it again.
        REQUIRE(cache.GetMissCount() == misses_after_first);
    }
    SECTION("A fresh cache over the same directory finds what the last one wrote")
    {
        const Opal::DynamicArray<u8> written = compile(k_cache_source, "main_first");
        // No memory tier to answer from, so a hit here came off the disk.
        Rndr::ShaderCache reopened{directory};
        const Opal::DynamicArray<u8> read_back = reopened.Find(key);
        REQUIRE(read_back == written);
        REQUIRE(reopened.GetHitCount() == 1);
    }
    SECTION("A cache directory names the compiler that filled it")
    {
        // What a build without the compiler keys its lookups on. Removed first, so a file an earlier run left
        // cannot stand in for the one this Store has to write.
        const Opal::StringUtf8 tag_path = Opal::Paths::Combine(directory, Opal::StringUtf8("build-tag")).GetValue().Clone();
        if (Opal::Exists(tag_path))
        {
            REQUIRE(Opal::DeleteFile(tag_path) == Opal::ErrorCode::Success);
        }
        const Opal::DynamicArray<u8> written = compile(k_cache_source, "main_first");
        REQUIRE(!key.build_tag.IsEmpty());
        REQUIRE(Opal::Exists(tag_path));

        Rndr::ShaderCache reopened{directory};
        REQUIRE(reopened.GetDirectoryBuildTag() == key.build_tag);
        const Rndr::ShaderCacheKey reopened_key = reopened.MakeKey(k_cache_source, "main_first", Rndr::ShaderOutputFormat::SpirV);
        REQUIRE(reopened_key == key);
        REQUIRE(reopened.Find(reopened_key) == written);
    }
    SECTION("A blob from a different Slang is not used")
    {
        compile(k_cache_source, "main_first");
        Rndr::ShaderCacheKey wrong_tag = key.Clone();
        wrong_tag.build_tag = Opal::StringUtf8("some-other-slang");
        // Same source and entry point, so only the tag can turn this into a miss.
        Rndr::ShaderCache reopened{directory};
        REQUIRE(reopened.Find(wrong_tag).IsEmpty());
    }
    SECTION("A corrupt blob is recompiled over rather than trusted")
    {
        const Opal::DynamicArray<u8> written = compile(k_cache_source, "main_first");
        const Opal::StringUtf8 path = cache.GetFilePath(key);
        REQUIRE(Opal::Exists(path));

        constexpr u8 k_garbage[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
        REQUIRE(Opal::WriteBytesToFile(path, {k_garbage, 6}) == Opal::ErrorCode::Success);

        // A fresh cache, so the memory tier cannot cover for the file.
        Rndr::ShaderCache reopened{directory};
        REQUIRE(reopened.Find(key).IsEmpty());

        // And compiling through it puts a usable entry back where the garbage was.
        const Forge::Shader shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_cache_source, {.entry_point = "main_first", .cache = reopened}));
        REQUIRE(shader.IsValid());
        Rndr::ShaderCache third{directory};
        REQUIRE(third.Find(key) == written);
    }
    SECTION("A cache with no directory still answers within the process")
    {
        Rndr::ShaderCache memory_only;
        REQUIRE(memory_only.GetDirectory().IsEmpty());
        REQUIRE(memory_only.GetFilePath(key).IsEmpty());
        const Forge::Shader first = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_cache_source, {.entry_point = "main_first", .cache = memory_only}));
        const Forge::Shader second = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_cache_source, {.entry_point = "main_first", .cache = memory_only}));
        REQUIRE(first.IsValid());
        REQUIRE(second.IsValid());
        REQUIRE(memory_only.GetHitCount() == 1);
        REQUIRE(memory_only.GetMissCount() == 1);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * The contract docs/forge.md fixes in its first section, checked for every type that holds a handle. A move
 * that drops a member is invisible until something reaches for it, which is the shape of bug a smoke test is
 * worst at catching by accident - a move that dropped the VMA allocator or the enabled extension list found
 * three of them in Device and DeviceQueue alone - so each type is
 * put through the states and then asked to do its job afterwards.
 */
namespace
{

/**
 * The five states of the contract, in one place so that every type answers the same questions in the same
 * order: false when default constructed, true once built, false for the source of a move, false after
 * Destroy, and a second Destroy that changes nothing.
 *
 * The type is named only by what the factory returns, so adding a type here is adding two lambdas.
 *
 * @param type_name Named in the failure, since the assertions themselves look alike for every type.
 * @param make Builds one valid object. Called three times: a move needs a source, and assigning over a live
 *        object needs one to overwrite.
 * @param check_works Handed the object after both a move construction and a move assignment. Doing something
 *        that touches the members a move has to carry is the point - reporting itself valid is not.
 */
template <typename Make, typename CheckWorks>
void CheckLifetimeContract(const char* type_name, Make&& make, CheckWorks&& check_works)
{
    using T = decltype(make());
    INFO("type " << type_name);

    // Empty, owns nothing, and Destroy on it has nothing to release.
    T empty;
    REQUIRE_FALSE(empty.IsValid());
    empty.Destroy();
    REQUIRE_FALSE(empty.IsValid());

    T built = make();
    REQUIRE(built.IsValid());

    T move_constructed(std::move(built));
    REQUIRE(move_constructed.IsValid());
    REQUIRE_FALSE(built.IsValid());

    T move_assigned;
    move_assigned = std::move(move_constructed);
    REQUIRE(move_assigned.IsValid());
    REQUIRE_FALSE(move_constructed.IsValid());

    // Assigning over a live object has to release the one being overwritten rather than leak it, which is
    // what an earlier DeviceQueue::operator= got wrong: it moved into the target without releasing what it held.
    // The leak itself shows up in the validation layer
    // and in AddressSanitizer at teardown rather than in an assertion here.
    T overwritten = make();
    REQUIRE(overwritten.IsValid());
    overwritten = make();
    REQUIRE(overwritten.IsValid());

    // Self assignment has to leave the object alone rather than release it and then move from the wreck. The
    // alias is what keeps the compiler from seeing this as the obvious self move it is and warning about it.
    T& alias = overwritten;
    overwritten = std::move(alias);
    REQUIRE(overwritten.IsValid());

    check_works(move_assigned);

    move_assigned.Destroy();
    REQUIRE_FALSE(move_assigned.IsValid());
    // Idempotent, so releasing early is always safe.
    move_assigned.Destroy();
    REQUIRE_FALSE(move_assigned.IsValid());
}

/** How many elements the lifetime dispatches write. Small, since what they check is that the write happened at all. */
constexpr i32 k_lifetime_elements = 64;

/** Dispatch the address pipeline over one buffer and check every element it should have written. */
void RequireDispatchWrites(const Forge::Device& device, Forge::DeviceQueue& queue, const Forge::Pipeline& pipeline,
                           const Forge::Buffer& output)
{
    const VkDeviceAddress address = output.GetNativeDeviceAddress();
    REQUIRE(Forge::ImmediateSubmit(device, queue,
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(address)) ==
                                       ErrorCode::Success);
                               REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                           }) == ErrorCode::Success);
    RequireComputeWrote(output, k_lifetime_elements);
}

}  // namespace

TEST_CASE("Forge empty state and moves of the context", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // On its own, and deliberately: volkLoadInstance points one global table at whichever instance was
    // created last, so a case holding two contexts at once is where that stops being theoretical. Two of
    // them are live here for as long as the assignment below takes.
    CheckLifetimeContract(
        "GraphicsContext", [] { return ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc())); },
        [](const Forge::GraphicsContext& context)
        {
            REQUIRE(context.GetInstance() != VK_NULL_HANDLE);
            // Enumerating is the cheapest call that goes through the instance the move had to carry.
            REQUIRE(ForgeTest::Unwrap(context.EnumeratePhysicalDevices()).GetSize() > 0);
        });
}

TEST_CASE("Forge empty state and moves of the device stack", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));

    auto make_physical_device = [&context]
    {
        Opal::DynamicArray<Forge::PhysicalDevice> devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
        return std::move(devices[0]);
    };
    CheckLifetimeContract("PhysicalDevice", make_physical_device,
                          [](const Forge::PhysicalDevice& physical_device)
                          {
                              REQUIRE(physical_device.GetNativePhysicalDevice() != VK_NULL_HANDLE);
                              // Properties, features and the extension list are all read once at construction,
                              // so a move that dropped them hands back zeroes and an empty list rather than
                              // failing at the call.
                              REQUIRE(physical_device.GetProperties().apiVersion != 0);
                              REQUIRE_FALSE(physical_device.GetQueueFamilyProperties().IsEmpty());
                              REQUIRE_FALSE(physical_device.GetSupportedExtensions().IsEmpty());
                              REQUIRE(physical_device.GetMemoryProperties().memoryTypeCount > 0);
                          });

    auto make_device = [&context, &make_physical_device]
    { return ForgeTest::Unwrap(Forge::Device::Create(make_physical_device(), context, MakeHeadlessDeviceDesc())); };
    CheckLifetimeContract("Device", make_device,
                          [](Forge::Device& device)
                          {
                              // Allocating is what needs the VMA allocator, which is one of the two members
                              // Device's move used to drop; the other was the enabled extension list.
                              const Forge::Buffer buffer = MakeWipedOutput(device, k_lifetime_elements);
                              REQUIRE(buffer.IsValid());
                              REQUIRE(device.GetNativeDevice() != VK_NULL_HANDLE);
                              REQUIRE(device.GetPhysicalDevice().IsValid());
                              // Every queue holds a reference back to the device, which a move has to
                              // re-point, so reaching one through the moved device is the check for that.
                              REQUIRE(ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics)).IsValid());
                          });

    Forge::Device device = make_device();
    const u32 graphics_family = ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics)).GetQueueFamilyIndex();
    // A queue of its own rather than one from GetQueue: those belong to the device, and destroying one would
    // leave the device holding a queue with no command pool. See the note on DeviceQueue::Destroy.
    CheckLifetimeContract(
        "DeviceQueue", [&device, graphics_family] { return ForgeTest::Unwrap(Forge::DeviceQueue::Create(device, graphics_family)); },
        [&device](Forge::DeviceQueue& queue)
        {
            REQUIRE(queue.GetNativeQueue() != VK_NULL_HANDLE);
            REQUIRE(queue.GetNativeCommandPool() != VK_NULL_HANDLE);
            // Submitting needs every member at once: the device, the queue, the family
            // index and the command pool the command buffer is allocated out of.
            REQUIRE(Forge::ImmediateSubmit(device, queue, [](Forge::CommandBuffer&) {}) == ErrorCode::Success);
            REQUIRE(queue.WaitIdle() == ErrorCode::Success);
        });

    // The device goes before the check, because vkDestroyDevice is what names a queue's command pool that a
    // move assignment leaked. The context outlives it, so the message is still collected.
    device.Destroy();
    REQUIRE_NO_VALIDATION_ERROR_IN(context);
}

TEST_CASE("Forge empty state and moves of the resources", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    const Opal::DynamicArray<u8> written = MakeBytes(k_lifetime_elements * sizeof(u32), 41);

    CheckLifetimeContract("Buffer",
                          [&]
                          {
                              return ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                                   {.size = written.GetSize(),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random},
                                                   written));
                          },
                          [&](const Forge::Buffer& buffer)
                          {
                              // Read back through the mapped pointer, which is the member a move used to leave the source
                              // leaving behind.
                              Opal::DynamicArray<u8> read_back(written.GetSize());
                              REQUIRE(buffer.Read(read_back) == ErrorCode::Success);
                              REQUIRE(CountMismatches(written, read_back) == 0);
                          });

    CheckLifetimeContract("Texture",
                          [&]
                          {
                              return ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                                     .width = k_side,
                                                                     .height = k_side,
                                                                     .usage = Forge::TextureUsageBits::ColorAttachment |
                                                                              Forge::TextureUsageBits::TransferSource}));
                          },
                          [&](Forge::Texture& texture)
                          {
                              REQUIRE(texture.GetNativeImageView() != VK_NULL_HANDLE);
                              REQUIRE(texture.GetDesc().width == k_side);
                              // Forge tracks the layout per subresource itself, so the move has to carry that
                              // array; a readback transitions the texture and then asks where it ended up.
                              REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::Undefined);
                              const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, texture, k_side);
                              REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferSource);
                          });

    // A sampler holds nothing but its device and its handle, so writing it into a descriptor is the cheapest
    // thing that uses both. Sampling through one is "Forge sampler filtering, LOD clamp and immutable samplers".
    Forge::DescriptorPoolDesc sampler_pool_desc;
    REQUIRE(sampler_pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 4) == ErrorCode::Success);
    sampler_pool_desc.max_sets = 4;
    Forge::DescriptorPool sampler_pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, sampler_pool_desc));
    Forge::DescriptorSetLayoutDesc sampler_layout_desc;
    REQUIRE(sampler_layout_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) ==
            ErrorCode::Success);
    Forge::DescriptorSetLayout sampler_layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, sampler_layout_desc));
    Forge::Texture sampled = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                  .width = k_side,
                                                  .height = k_side,
                                                  .usage = Forge::TextureUsageBits::Sampled}));

    // A view holds its device, its texture and its handle; writing it into a descriptor uses all three.
    CheckLifetimeContract("TextureView", [&] { return ForgeTest::Unwrap(Forge::TextureView::Create(fixture.device, sampled)); },
                          [&](const Forge::TextureView& view)
                          {
                              REQUIRE(view.GetNativeImageView() != VK_NULL_HANDLE);
                              REQUIRE(&view.GetTexture() == &sampled);
                              const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.max_anisotropy = 1.0f}));
                              Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(sampler_pool, sampler_layout));
                              REQUIRE(set.Update(0, view, sampler) == ErrorCode::Success);
                          });

    CheckLifetimeContract("Sampler", [&] { return ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.max_anisotropy = 1.0f})); },
                          [&](const Forge::Sampler& sampler)
                          {
                              REQUIRE(sampler.GetNativeSampler() != VK_NULL_HANDLE);
                              Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(sampler_pool, sampler_layout));
                              REQUIRE(set.Update(0, sampled, sampler) == ErrorCode::Success);
                          });

    CheckLifetimeContract("Shader",
                          [&]
                          {
                              return ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source,
                                                                       {.entry_point = "main_compute", .cache = GetShaderCache()}));
                          },
                          [&](const Forge::Shader& shader)
                          {
                              // The stage and the entry point are read out of the reflection at construction,
                              // and a pipeline built from the moved shader needs both of them plus the module.
                              REQUIRE(shader.GetShaderStage() == ShaderTypeBits::Compute);
                              REQUIRE(shader.GetEntryPoint() == Opal::StringUtf8("main_compute"));
                              const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, shader);
                              const Forge::Buffer output = MakeWipedOutput(fixture.device, k_lifetime_elements);
                              RequireDispatchWrites(fixture.device, fixture.GetQueue(), pipeline, output);
                          });

    Forge::Shader compute_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
    CheckLifetimeContract("Pipeline", [&] { return MakeAddressPipeline(fixture.device, compute_shader); },
                          [&](const Forge::Pipeline& pipeline)
                          {
                              // Binding needs the layout and the bind point beside the pipeline, and the push
                              // constant the dispatch depends on goes through the layout.
                              REQUIRE(pipeline.GetNativePipelineLayout() != VK_NULL_HANDLE);
                              REQUIRE(pipeline.GetBindPoint() == VK_PIPELINE_BIND_POINT_COMPUTE);
                              const Forge::Buffer output = MakeWipedOutput(fixture.device, k_lifetime_elements);
                              RequireDispatchWrites(fixture.device, fixture.GetQueue(), pipeline, output);
                          });

    // What the case built for itself, released so the device can go before the check below. Nothing here is
    // what the case is about; they are the arguments the checks above needed.
    sampler_pool.Destroy();
    sampler_layout.Destroy();
    sampled.Destroy();
    compute_shader.Destroy();
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

TEST_CASE("Forge empty state and moves of the descriptor objects", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 16) == ErrorCode::Success);
    pool_desc.max_sets = 16;
    // On, so that DescriptorSet::Destroy returns the set to its pool rather than only dropping the handle,
    // which is the half of Destroy nothing else in the suite runs.
    pool_desc.free_individual_sets = true;

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);

    CheckLifetimeContract("DescriptorPool", [&] { return ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc)); },
                          [&](const Forge::DescriptorPool& pool)
                          {
                              REQUIRE(pool.GetNativeDescriptorPool() != VK_NULL_HANDLE);
                              // Allocating out of the moved pool is what needs its device and its desc, and
                              // the set it hands back holds the pool by reference.
                              const Forge::DescriptorSetLayout layout =
                                  ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));
                              const Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
                              REQUIRE(set.IsValid());
                          });

    CheckLifetimeContract(
        "DescriptorSetLayout", [&] { return ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc)); },
        [&](const Forge::DescriptorSetLayout& layout)
        {
            REQUIRE(layout.GetNativeDescriptorSetLayout() != VK_NULL_HANDLE);
            // The desc is what a set reads its binding types out of, so a layout that lost
            // it allocates a set that then knows about no binding at all.
            REQUIRE(layout.GetDesc().bindings.GetSize() == 1);
            const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
            const Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
            REQUIRE(ForgeTest::Unwrap(set.GetBindingDescriptorType(0)) == Forge::DescriptorType::StorageBuffer);
        });

    Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
    Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));
    Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_descriptor_source, {.entry_point = "main_descriptor", .cache = GetShaderCache()}));
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    CheckLifetimeContract("DescriptorSet", [&] { return ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout)); },
                          [&](Forge::DescriptorSet& set)
                          {
                              REQUIRE(ForgeTest::Unwrap(set.GetBindingDescriptorType(0)) == Forge::DescriptorType::StorageBuffer);
                              const Forge::Buffer output = MakeWipedOutput(fixture.device, k_lifetime_elements);
                              REQUIRE(set.Update(0, output) == ErrorCode::Success);
                              DispatchWithSet(fixture.device, fixture.GetQueue(), pipeline, set);
                              Opal::DynamicArray<u32> values(k_lifetime_elements);
                              REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) ==
                                      ErrorCode::Success);
                              for (i32 i = 0; i < k_lifetime_elements; ++i)
                              {
                                  INFO("element " << i);
                                  REQUIRE(values[i] == static_cast<u32>(i) + 7);
                              }
                          });

    // What the case built for itself, released so the device can go before the check below.
    pipeline.Destroy();
    shader.Destroy();
    layout.Destroy();
    pool.Destroy();
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

TEST_CASE("Forge empty state and moves of the command and synchronization objects", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    Forge::DeviceQueue& queue = fixture.GetQueue();

    CheckLifetimeContract("CommandBuffer", [&] { return ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, queue)); },
                          [&](Forge::CommandBuffer& command_buffer)
                          {
                              // Recording and submitting is what needs the queue the buffer was allocated on
                              // beside the handle itself.
                              REQUIRE(command_buffer.Begin() == ErrorCode::Success);
                              REQUIRE(command_buffer.End() == ErrorCode::Success);
                              const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
                              REQUIRE(queue.Submit(command_buffer, fence) == ErrorCode::Success);
                              REQUIRE(fence.Wait() == ErrorCode::Success);
                          });

    CheckLifetimeContract("Fence", [&] { return ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false)); },
                          [&](Forge::Fence& fence)
                          {
                              Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, queue));
                              REQUIRE(command_buffer.Begin() == ErrorCode::Success);
                              REQUIRE(command_buffer.End() == ErrorCode::Success);
                              REQUIRE(queue.Submit(command_buffer, fence) == ErrorCode::Success);
                              REQUIRE(fence.Wait() == ErrorCode::Success);
                              // Signalled now, and Reset has to reach the device the move carried.
                              REQUIRE(ForgeTest::Unwrap(fence.TryWait(0)));
                              REQUIRE(fence.Reset() == ErrorCode::Success);
                              REQUIRE_FALSE(ForgeTest::Unwrap(fence.TryWait(0)));
                          });

    CheckLifetimeContract("Semaphore",
                          [&] {
                              return ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device,
                                                      {.type = Forge::SemaphoreType::Timeline, .initial_value = 3}));
                          },
                          [](const Forge::Semaphore& semaphore)
                          {
                              // The type is a member of its own, and every host side call reports on a binary
                              // semaphore, so a move that dropped it would fail here rather than answer wrong.
                              REQUIRE(semaphore.IsTimeline());
                              REQUIRE(ForgeTest::Unwrap(semaphore.GetValue()) == 3);
                              REQUIRE(semaphore.Signal(7) == ErrorCode::Success);
                              REQUIRE(ForgeTest::Unwrap(semaphore.GetValue()) == 7);
                          });

    // Four rather than the two TimestampQueryPoolDesc defaults to: a check that asks for the default value
    // cannot tell a desc that came through the move from one that was never assigned.
    CheckLifetimeContract(
        "TimestampQueryPool", [&] { return ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 4})); },
        [&](Forge::TimestampQueryPool& pool)
        {
            REQUIRE(pool.GetQueryCount() == 4);
            // Read off the device once at construction and used by every elapsed helper.
            // Compared against what the device reports rather than against zero, because the
            // member defaults to one: a move that dropped it would otherwise keep answering
            // a plausible number and turn every measurement into ticks.
            REQUIRE(pool.GetTimestampPeriod() == fixture.device.GetPhysicalDevice().GetProperties().limits.timestampPeriod);
            REQUIRE(Forge::ImmediateSubmit(fixture.device, queue,
                                           [&](Forge::CommandBuffer& command_buffer)
                                           {
                                               REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
                                               REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineStart) ==
                                                       ErrorCode::Success);
                                               REQUIRE(command_buffer.CmdWriteTimestamp(pool, 1, Forge::PipelineStageBits::PipelineEnd) ==
                                                       ErrorCode::Success);
                                           }) == ErrorCode::Success);
            Opal::InPlaceArray<u64, 2> ticks;
            REQUIRE(pool.GetResults({ticks.GetData(), 2}) == ErrorCode::Success);
            REQUIRE(ticks[1] >= ticks[0]);
        });

    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

/**
 * The transfer, barrier and binding calls that existed with nothing running them. Every case here ends in a
 * readback compared against bytes worked out on the CPU, because the mistakes these calls make - a row
 * stride off by one, a box copied from the wrong corner - land in the middle of an image and produce a
 * result that is the right size and the right shape and wrong.
 */
namespace
{

/**
 * Texel (x, y) of every content texture below. The red channel names the column and the green names the row,
 * so a copy that moved a texel, mirrored an axis or dropped a row says which one it got wrong rather than
 * only that something differs. The blue channel names the texture, which is what tells two of them apart.
 *
 * Nothing here goes through a shader, so these are bytes a copy moves rather than values a format rounds.
 */
Opal::DynamicArray<u8> MakeTexelGrid(i32 width, i32 height, u8 seed)
{
    Opal::DynamicArray<u8> bytes(width * height * 4);
    for (i32 y = 0; y < height; ++y)
    {
        for (i32 x = 0; x < width; ++x)
        {
            const i32 base = (y * width + x) * 4;
            bytes[base + 0] = static_cast<u8>(10 + x * 20);
            bytes[base + 1] = static_cast<u8>(10 + y * 20);
            bytes[base + 2] = seed;
            bytes[base + 3] = 255;
        }
    }
    return bytes;
}

/** One texel of a tightly packed RGBA readback, as four ints so a failure prints something readable. */
Opal::InPlaceArray<i32, 4> GridTexel(Opal::ArrayView<const u8> pixels, i32 width, i32 x, i32 y)
{
    const i32 base = (y * width + x) * 4;
    return {pixels[base], pixels[base + 1], pixels[base + 2], pixels[base + 3]};
}

#define REQUIRE_TEXEL_EQUALS(actual, expected)                                                                  \
    do                                                                                                          \
    {                                                                                                           \
        INFO("expected rgba " << (expected)[0] << " " << (expected)[1] << " " << (expected)[2] << " "            \
                              << (expected)[3] << ", got " << (actual)[0] << " " << (actual)[1] << " "           \
                              << (actual)[2] << " " << (actual)[3]);                                            \
        REQUIRE((actual)[0] == (expected)[0]);                                                                  \
        REQUIRE((actual)[1] == (expected)[1]);                                                                  \
        REQUIRE((actual)[2] == (expected)[2]);                                                                  \
        REQUIRE((actual)[3] == (expected)[3]);                                                                  \
    } while (false)

/** Put pixels into every array layer of a texture and leave it where a transfer read can find it. */
void UploadGrid(const Forge::Device& device, Forge::DeviceQueue& queue, Forge::Texture& texture, Opal::ArrayView<const u8> pixels)
{
    const Forge::Buffer staging = ForgeTest::Unwrap(
        Forge::Buffer::Create(device, {.size = pixels.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, pixels));
    const Forge::BufferTextureCopyRegion region{
        .texture_subresource = {.array_layer_count = texture.GetDesc().array_layer_count}};
    REQUIRE(Forge::ImmediateSubmit(device, queue,
                                   [&](Forge::CommandBuffer& command_buffer)
                                   {
                                       REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(texture)) ==
                                               ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdCopyBufferToTexture(staging, texture, {&region, 1}) == ErrorCode::Success);
                                   }) == ErrorCode::Success);
}

/** A texture of the given size holding MakeTexelGrid, ready to be copied out of and into. */
Forge::Texture MakeGridTexture(const Forge::Device& device, Forge::DeviceQueue& queue, i32 width, i32 height, u8 seed,
                               PixelFormat format = PixelFormat::R8G8B8A8_UNORM)
{
    Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(device, {.format = format,
                                    .width = static_cast<u32>(width),
                                    .height = static_cast<u32>(height),
                                    .usage = Forge::TextureUsageBits::TransferSource |
                                             Forge::TextureUsageBits::TransferDestination}));
    const Opal::DynamicArray<u8> pixels = MakeTexelGrid(width, height, seed);
    UploadGrid(device, queue, texture, pixels);
    return texture;
}

/** An empty texture a blit or a copy writes into. */
Forge::Texture MakeTransferTarget(const Forge::Device& device, i32 width, i32 height,
                                  PixelFormat format = PixelFormat::R8G8B8A8_UNORM)
{
    return ForgeTest::Unwrap(Forge::Texture::Create(device, {.format = format,
                                   .width = static_cast<u32>(width),
                                   .height = static_cast<u32>(height),
                                   .usage = Forge::TextureUsageBits::TransferSource |
                                            Forge::TextureUsageBits::TransferDestination}));
}

/** The byte every buffer below is filled with before a copy, so anything the copy did not write says so. */
constexpr u8 k_sentinel = 0xAB;

}  // namespace

TEST_CASE("Forge blits", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    const Forge::PhysicalDevice& physical_device = fixture.device.GetPhysicalDevice();
    if (!physical_device.SupportsBlit(k_format, true) || !physical_device.SupportsBlit(k_format, false))
    {
        SKIP("This device cannot blit the format these cases use.");
    }
    constexpr i32 k_side = 4;
    constexpr u8 k_seed = 90;

    SECTION("A blit scales the source up, and a nearest filter repeats its texels exactly")
    {
        constexpr i32 k_target_side = k_side * 2;
        Forge::Texture source = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, k_seed);
        Forge::Texture destination = MakeTransferTarget(fixture.device, k_target_side, k_target_side);

        const Forge::TextureBlitRegion region{};
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(source)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(destination)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBlitTexture(source, destination, {&region, 1}, ImageFilter::Nearest) ==
                                ErrorCode::Success);
                    }) == ErrorCode::Success);

        Opal::DynamicArray<u8> pixels(k_target_side * k_target_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        // Exactly two to one with no filtering, so every destination texel is the source texel above it and
        // nothing has been averaged with a neighbour.
        const Opal::DynamicArray<u8> expected = MakeTexelGrid(k_side, k_side, k_seed);
        for (i32 y = 0; y < k_target_side; ++y)
        {
            for (i32 x = 0; x < k_target_side; ++x)
            {
                INFO("texel " << x << "," << y);
                REQUIRE_TEXEL_EQUALS(GridTexel(pixels, k_target_side, x, y), GridTexel(expected, k_side, x / 2, y / 2));
            }
        }
    }
    SECTION("A negative extent runs an axis backwards, which mirrors it")
    {
        Forge::Texture source = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, k_seed);
        Forge::Texture destination = MakeTransferTarget(fixture.device, k_side, k_side);

        // The far corner sits before the near one on x, so the destination is written right to left.
        const Forge::TextureBlitRegion region{.destination_offset = {k_side, 0, 0}, .destination_extent = {-k_side, k_side, 1}};
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(source)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(destination)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBlitTexture(source, destination, {&region, 1}, ImageFilter::Nearest) ==
                                ErrorCode::Success);
                    }) == ErrorCode::Success);

        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, destination, k_side);
        const Opal::DynamicArray<u8> expected = MakeTexelGrid(k_side, k_side, k_seed);
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                INFO("texel " << x << "," << y);
                REQUIRE_TEXEL_EQUALS(GridTexel(pixels, k_side, x, y), GridTexel(expected, k_side, k_side - 1 - x, y));
            }
        }
    }
    SECTION("A blit converts between formats")
    {
        constexpr PixelFormat k_swapped = PixelFormat::B8G8R8A8_UNORM;
        if (!physical_device.SupportsBlit(k_swapped, false))
        {
            SKIP("This device cannot blit into B8G8R8A8_UNORM.");
        }
        Forge::Texture source = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, k_seed);
        Forge::Texture destination = MakeTransferTarget(fixture.device, k_side, k_side, k_swapped);

        const Forge::TextureBlitRegion region{};
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(source)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(destination)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBlitTexture(source, destination, {&region, 1}, ImageFilter::Nearest) ==
                                ErrorCode::Success);
                    }) == ErrorCode::Success);

        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, destination, k_side);
        // Read back as the bytes of a BGRA image, so the red the source wrote is now the third byte. A blit
        // that had copied rather than converted would leave it first, which is what separates this from
        // CmdCopyTexture.
        const Opal::DynamicArray<u8> source_pixels = MakeTexelGrid(k_side, k_side, k_seed);
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                const Opal::InPlaceArray<i32, 4> from_source = GridTexel(source_pixels, k_side, x, y);
                const Opal::InPlaceArray<i32, 4> actual = GridTexel(pixels, k_side, x, y);
                INFO("texel " << x << "," << y);
                const Opal::InPlaceArray<i32, 4> expected{from_source[2], from_source[1], from_source[0], from_source[3]};
                REQUIRE_TEXEL_EQUALS(actual, expected);
            }
        }
    }
    SECTION("A linear filter averages where a nearest one repeats")
    {
        if (!physical_device.SupportsLinearFilter(k_format))
        {
            SKIP("This device cannot filter the test format linearly.");
        }
        constexpr i32 k_target_side = k_side * 2;
        Forge::Texture source = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, k_seed);
        Forge::Texture destination = MakeTransferTarget(fixture.device, k_target_side, k_target_side);

        const Forge::TextureBlitRegion region{};
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(source)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(destination)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBlitTexture(source, destination, {&region, 1}, ImageFilter::Linear) ==
                                ErrorCode::Success);
                    }) == ErrorCode::Success);

        Opal::DynamicArray<u8> pixels(k_target_side * k_target_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        // Where exactly a filtered sample lands is the driver's business, so this asserts the one thing the
        // filter has to change: somewhere across the row a value appears that is not one of the four the
        // source holds, which a nearest filter can never produce.
        const Opal::DynamicArray<u8> source_pixels = MakeTexelGrid(k_side, k_side, k_seed);
        bool found_blend = false;
        for (i32 x = 0; x < k_target_side && !found_blend; ++x)
        {
            const i32 red = GridTexel(pixels, k_target_side, x, 0)[0];
            bool matches_a_source_texel = false;
            for (i32 source_x = 0; source_x < k_side; ++source_x)
            {
                matches_a_source_texel = matches_a_source_texel || red == GridTexel(source_pixels, k_side, source_x, 0)[0];
            }
            found_blend = !matches_a_source_texel;
        }
        REQUIRE(found_blend);
    }

    /**
     * Blit one region of a fresh grid into a fresh target of the same size with a nearest filter, and hand
     * back what the target holds. Every section above reads the whole source; these read part of it.
     */
    auto blit_part = [&](const Forge::TextureBlitRegion& region)
    {
        Forge::Texture source = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, k_seed);
        Forge::Texture destination = MakeTransferTarget(fixture.device, k_side, k_side);
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(source)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(destination)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBlitTexture(source, destination, {&region, 1}, ImageFilter::Nearest) ==
                                ErrorCode::Success);
                    }) == ErrorCode::Success);
        return ReadColorPixels(fixture, destination, k_side);
    };

    /** Every texel of the target is the source texel two to one above it, counted from `corner` of the source. */
    auto require_quarter_scaled_up = [&](const Opal::DynamicArray<u8>& pixels, i32 corner_x, i32 corner_y)
    {
        const Opal::DynamicArray<u8> expected = MakeTexelGrid(k_side, k_side, k_seed);
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                INFO("texel " << x << "," << y);
                REQUIRE_TEXEL_EQUALS(GridTexel(pixels, k_side, x, y), GridTexel(expected, k_side, corner_x + x / 2, corner_y + y / 2));
            }
        }
    };

    SECTION("A blit reads only the box its source offset and extent name")
    {
        // The top right quarter into the whole destination. The offset is on x alone, so an offset read with
        // its axes exchanged lands on the bottom left quarter, and the grid tells every texel apart - red
        // counts columns and green counts rows.
        constexpr i32 k_half = k_side / 2;
        const Forge::TextureBlitRegion region{.source_offset = {k_half, 0, 0}, .source_extent = {k_half, k_half, 1}};
        require_quarter_scaled_up(blit_part(region), k_half, 0);
    }
    SECTION("A zero source extent past an offset reads the rest of the source")
    {
        // What the header promises for a zero on an axis: the rest of the mip level past the offset, not the
        // whole level and not nothing. From the middle, the rest is the bottom right quarter.
        constexpr i32 k_half = k_side / 2;
        const Forge::TextureBlitRegion region{.source_offset = {k_half, k_half, 0}};
        require_quarter_scaled_up(blit_part(region), k_half, k_half);
    }
    SECTION("A source box that reaches past the source is refused")
    {
        // The source half of the bounds check, which a blit of the whole source can never trip.
        Forge::Texture source = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, k_seed);
        Forge::Texture destination = MakeTransferTarget(fixture.device, k_side, k_side);
        const Forge::TextureBlitRegion past_the_edge{.source_offset = {k_side / 2, 0, 0}, .source_extent = {k_side, k_side, 1}};
        ErrorCode blit_status = ErrorCode::Success;
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(source)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(destination)) ==
                                ErrorCode::Success);
                        blit_status = command_buffer.CmdBlitTexture(source, destination, {&past_the_edge, 1}, ImageFilter::Nearest);
                    }) == ErrorCode::Success);
        REQUIRE(blit_status == ErrorCode::OutOfBounds);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge copies from a texture into a buffer", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr u8 k_seed = 55;
    constexpr i32 k_buffer_size = 256;
    const Opal::DynamicArray<u8> source_pixels = MakeTexelGrid(k_side, k_side, k_seed);

    // Filled with the sentinel first, so every byte the copy did not write says so rather than reading as a
    // zero that could have come from anywhere.
    auto make_sentinel_buffer = [&]
    {
        Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_buffer_size,
                                              .usage = Forge::BufferUsageBits::TransferDestination,
                                              .host_access = Forge::HostAccess::Random}));
        Opal::DynamicArray<u8> filler(k_buffer_size);
        for (i32 i = 0; i < k_buffer_size; ++i)
        {
            filler[i] = k_sentinel;
        }
        REQUIRE(buffer.Update(filler) == ErrorCode::Success);
        return buffer;
    };

    // Runs one region and hands back the whole buffer, so a case can check what was written and what was not.
    auto copy_region = [&](const Forge::BufferTextureCopyRegion& region)
    {
        Forge::Texture source = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, k_seed);
        const Forge::Buffer buffer = make_sentinel_buffer();
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(source)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdCopyTextureToBuffer(source, buffer, {&region, 1}) == ErrorCode::Success);
                    }) == ErrorCode::Success);
        Opal::DynamicArray<u8> out(k_buffer_size);
        REQUIRE(buffer.Read(out) == ErrorCode::Success);
        return out;
    };

    SECTION("A sub-box copies the texels it names and no others")
    {
        const Forge::BufferTextureCopyRegion region{.texture_offset = {1, 1, 0}, .texture_extent = {2, 2, 1}};
        const Opal::DynamicArray<u8> out = copy_region(region);
        // Four texels packed from byte zero: the box at (1, 1), row major.
        REQUIRE_TEXEL_EQUALS(GridTexel(out, 2, 0, 0), GridTexel(source_pixels, k_side, 1, 1));
        REQUIRE_TEXEL_EQUALS(GridTexel(out, 2, 1, 0), GridTexel(source_pixels, k_side, 2, 1));
        REQUIRE_TEXEL_EQUALS(GridTexel(out, 2, 0, 1), GridTexel(source_pixels, k_side, 1, 2));
        REQUIRE_TEXEL_EQUALS(GridTexel(out, 2, 1, 1), GridTexel(source_pixels, k_side, 2, 2));
        // Nothing past the sixteen bytes those four take.
        for (i32 i = 2 * 2 * 4; i < k_buffer_size; ++i)
        {
            INFO("byte " << i);
            REQUIRE(out[i] == k_sentinel);
        }
    }
    SECTION("A buffer offset starts the copy further into the buffer")
    {
        constexpr i32 k_offset = 32;
        const Forge::BufferTextureCopyRegion region{.buffer_offset = k_offset, .texture_extent = {k_side, k_side, 1}};
        const Opal::DynamicArray<u8> out = copy_region(region);
        for (i32 i = 0; i < k_offset; ++i)
        {
            INFO("byte " << i);
            REQUIRE(out[i] == k_sentinel);
        }
        const Opal::ArrayView<const u8> written{out.GetData() + k_offset, source_pixels.GetSize()};
        REQUIRE(CountMismatches(source_pixels, written) == 0);
    }
    SECTION("A row length spaces the rows out in the buffer")
    {
        // Two texels wide out of a four wide row length, so every row leaves two texels of the buffer alone.
        // This is the parameter an off-by-one turns into an image that shears one texel per row.
        const Forge::BufferTextureCopyRegion region{.buffer_row_length = 4, .texture_extent = {2, 2, 1}};
        const Opal::DynamicArray<u8> out = copy_region(region);
        for (i32 y = 0; y < 2; ++y)
        {
            for (i32 x = 0; x < 2; ++x)
            {
                INFO("texel " << x << "," << y);
                REQUIRE_TEXEL_EQUALS(GridTexel(out, 4, x, y), GridTexel(source_pixels, k_side, x, y));
            }
            // The two texels of the row the copy stepped over.
            for (i32 x = 2; x < 4; ++x)
            {
                for (i32 byte = 0; byte < 4; ++byte)
                {
                    INFO("padding texel " << x << "," << y << " byte " << byte);
                    REQUIRE(out[(y * 4 + x) * 4 + byte] == k_sentinel);
                }
            }
        }
    }
    SECTION("A row length and a layer height space out the rows and the layers")
    {
        constexpr i32 k_layer_count = 2;
        constexpr i32 k_box = 2;
        constexpr i32 k_row_length = 4;
        constexpr i32 k_layer_height = 3;
        Forge::Texture source = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                               .width = k_side,
                                               .height = k_side,
                                               .array_layer_count = k_layer_count,
                                               .usage = Forge::TextureUsageBits::TransferSource |
                                                        Forge::TextureUsageBits::TransferDestination,
                                               .view_type = Forge::TextureViewType::Texture2DArray}));
        // One grid per layer, with a different blue channel, so a layer stride that is wrong reads as the
        // other layer rather than as noise.
        Opal::DynamicArray<u8> both_layers(k_side * k_side * 4 * k_layer_count);
        const Opal::DynamicArray<u8> layer_zero = MakeTexelGrid(k_side, k_side, 11);
        const Opal::DynamicArray<u8> layer_one = MakeTexelGrid(k_side, k_side, 99);
        for (i32 i = 0; i < layer_zero.GetSize(); ++i)
        {
            both_layers[i] = layer_zero[i];
            both_layers[layer_zero.GetSize() + i] = layer_one[i];
        }
        UploadGrid(fixture.device, fixture.GetQueue(), source, {both_layers.GetData(), both_layers.GetSize()});

        const Forge::Buffer buffer = make_sentinel_buffer();
        const Forge::BufferTextureCopyRegion region{.buffer_row_length = k_row_length,
                                                    .buffer_layer_height = k_layer_height,
                                                    .texture_subresource = {.array_layer_count = k_layer_count},
                                                    .texture_extent = {k_box, k_box, 1}};
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(source)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdCopyTextureToBuffer(source, buffer, {&region, 1}) == ErrorCode::Success);
                    }) == ErrorCode::Success);
        Opal::DynamicArray<u8> out(k_buffer_size);
        REQUIRE(buffer.Read(out) == ErrorCode::Success);

        // One layer is row_length * layer_height texels apart from the next, which is larger than the box
        // the copy actually wrote - so the gap between them has to still hold the sentinel.
        constexpr i32 k_layer_stride = k_row_length * k_layer_height;
        for (i32 layer = 0; layer < k_layer_count; ++layer)
        {
            const Opal::DynamicArray<u8>& expected = layer == 0 ? layer_zero : layer_one;
            for (i32 y = 0; y < k_box; ++y)
            {
                for (i32 x = 0; x < k_box; ++x)
                {
                    const i32 texel = layer * k_layer_stride + y * k_row_length + x;
                    INFO("layer " << layer << " texel " << x << "," << y);
                    REQUIRE_TEXEL_EQUALS(GridTexel(out, 1, texel, 0), GridTexel(expected, k_side, x, y));
                }
            }
        }
        // The last row of the first layer's layer height is past its box and was never written.
        for (i32 byte = 0; byte < 4 * k_row_length; ++byte)
        {
            const i32 index = (2 * k_row_length) * 4 + byte;
            INFO("byte " << index << " of the gap between the layers");
            REQUIRE(out[index] == k_sentinel);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** Pixels whose every texel differs from every other, so a copy that shifted or mirrored shows up. */
Opal::DynamicArray<u8> MakeTexelGradient(i32 width, i32 height, i32 layer_count = 1)
{
    Opal::DynamicArray<u8> pixels(width * height * layer_count * 4);
    for (i32 texel = 0; texel < width * height * layer_count; ++texel)
    {
        pixels[texel * 4 + 0] = static_cast<u8>(texel * 16 + 1);
        pixels[texel * 4 + 1] = static_cast<u8>(255 - texel * 16);
        pixels[texel * 4 + 2] = static_cast<u8>(texel * 8 + 3);
        pixels[texel * 4 + 3] = 255;
    }
    return pixels;
}

/** Every texel of a level, one colour, for a destination whose untouched parts have to be recognisable. */
Opal::DynamicArray<u8> MakeFlatPixels(i32 texel_count, u8 red, u8 green, u8 blue)
{
    Opal::DynamicArray<u8> pixels(texel_count * 4);
    for (i32 texel = 0; texel < texel_count; ++texel)
    {
        pixels[texel * 4 + 0] = red;
        pixels[texel * 4 + 1] = green;
        pixels[texel * 4 + 2] = blue;
        pixels[texel * 4 + 3] = 255;
    }
    return pixels;
}

}  // namespace

TEST_CASE("Forge copies one texture into another", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr Forge::TextureUsageBits k_both_ways = Forge::TextureUsageBits::TransferSource | Forge::TextureUsageBits::TransferDestination;

    /** A copy of the regions given, with both textures barriered into the layouts a copy reads them in. */
    auto copy_regions = [&](Forge::Texture& source, Forge::Texture& destination, Opal::ArrayView<const Forge::TextureCopyRegion> regions)
    {
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(source)) ==
                                                   ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdTextureBarrier(
                                                       Forge::TextureBarrier::ToTransferDestination(destination)) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdCopyTexture(source, destination, regions) == ErrorCode::Success);
                                       }) == ErrorCode::Success);
    };

    SECTION("A whole mip level arrives texel for texel")
    {
        const Opal::DynamicArray<u8> source_pixels = MakeTexelGradient(k_side, k_side);
        Forge::Texture source = ForgeTest::Unwrap(
            Forge::Texture::Create(fixture.device, {.format = k_format, .width = k_side, .height = k_side, .usage = k_both_ways}));
        UploadMip(fixture.device, fixture.GetQueue(), source, {source_pixels.GetData(), source_pixels.GetSize()}, 0,
                  Forge::PipelineStageBits::None);
        Forge::Texture destination = ForgeTest::Unwrap(
            Forge::Texture::Create(fixture.device, {.format = k_format, .width = k_side, .height = k_side, .usage = k_both_ways}));

        // A zero extent is the rest of the source level, which for a region starting at the origin is all
        // of it.
        const Forge::TextureCopyRegion region{};
        copy_regions(source, destination, {&region, 1});

        Opal::DynamicArray<u8> read_back(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, read_back, 0,
                                       Forge::ImageLayout::TransferSource) == ErrorCode::Success);
        REQUIRE(CountMismatches({source_pixels.GetData(), source_pixels.GetSize()}, {read_back.GetData(), read_back.GetSize()}) == 0);
    }
    SECTION("A sub box lands at the offset the destination names")
    {
        // The top right quarter of the source into the bottom left quarter of the destination. Both offsets
        // differ and neither is zero, so a copy that ignored one of them lands somewhere this can see.
        const Opal::DynamicArray<u8> source_pixels = MakeTexelGradient(k_side, k_side);
        Forge::Texture source = ForgeTest::Unwrap(
            Forge::Texture::Create(fixture.device, {.format = k_format, .width = k_side, .height = k_side, .usage = k_both_ways}));
        UploadMip(fixture.device, fixture.GetQueue(), source, {source_pixels.GetData(), source_pixels.GetSize()}, 0,
                  Forge::PipelineStageBits::None);

        const Opal::DynamicArray<u8> filler = MakeFlatPixels(k_side * k_side, 7, 7, 7);
        Forge::Texture destination = ForgeTest::Unwrap(
            Forge::Texture::Create(fixture.device, {.format = k_format, .width = k_side, .height = k_side, .usage = k_both_ways}));
        UploadMip(fixture.device, fixture.GetQueue(), destination, {filler.GetData(), filler.GetSize()}, 0,
                  Forge::PipelineStageBits::None);

        constexpr i32 k_box = k_side / 2;
        const Forge::TextureCopyRegion region{
            .source_offset = {k_box, 0, 0}, .destination_offset = {0, k_box, 0}, .extent = {k_box, k_box, 1}};
        copy_regions(source, destination, {&region, 1});

        Opal::DynamicArray<u8> read_back(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, read_back, 0,
                                       Forge::ImageLayout::TransferSource) == ErrorCode::Success);
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                INFO("texel " << x << " " << y);
                const i32 destination_texel = (y * k_side + x) * 4;
                const bool inside_box = x < k_box && y >= k_box;
                if (inside_box)
                {
                    const i32 source_texel = ((y - k_box) * k_side + x + k_box) * 4;
                    REQUIRE(read_back[destination_texel + 0] == source_pixels[source_texel + 0]);
                    REQUIRE(read_back[destination_texel + 1] == source_pixels[source_texel + 1]);
                    REQUIRE(read_back[destination_texel + 2] == source_pixels[source_texel + 2]);
                }
                else
                {
                    REQUIRE(static_cast<i32>(read_back[destination_texel + 0]) == 7);
                    REQUIRE(static_cast<i32>(read_back[destination_texel + 1]) == 7);
                    REQUIRE(static_cast<i32>(read_back[destination_texel + 2]) == 7);
                }
            }
        }
    }
    SECTION("The mip level is named on each side of the copy")
    {
        // The whole of the source's top level into the second level of the destination, which is the same
        // size. A copy that took the level from the wrong side would be copying between a 4 and a 2.
        constexpr i32 k_half = k_side / 2;
        const Opal::DynamicArray<u8> source_pixels = MakeTexelGradient(k_half, k_half);
        Forge::Texture source = ForgeTest::Unwrap(
            Forge::Texture::Create(fixture.device, {.format = k_format, .width = k_half, .height = k_half, .usage = k_both_ways}));
        UploadMip(fixture.device, fixture.GetQueue(), source, {source_pixels.GetData(), source_pixels.GetSize()}, 0,
                  Forge::PipelineStageBits::None);

        Forge::Texture destination = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = k_format, .width = k_side, .height = k_side, .mip_level_count = 2, .usage = k_both_ways}));
        const Opal::DynamicArray<u8> top_filler = MakeFlatPixels(k_side * k_side, 9, 9, 9);
        UploadMip(fixture.device, fixture.GetQueue(), destination, {top_filler.GetData(), top_filler.GetSize()}, 0,
                  Forge::PipelineStageBits::None);
        const Opal::DynamicArray<u8> second_filler = MakeFlatPixels(k_half * k_half, 9, 9, 9);
        UploadMip(fixture.device, fixture.GetQueue(), destination, {second_filler.GetData(), second_filler.GetSize()}, 1,
                  Forge::PipelineStageBits::None);

        const Forge::TextureCopyRegion region{.destination = {.mip_level = 1}};
        copy_regions(source, destination, {&region, 1});

        Opal::DynamicArray<u8> second_level(k_half * k_half * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, second_level, 1,
                                       Forge::ImageLayout::TransferSource) == ErrorCode::Success);
        REQUIRE(CountMismatches({source_pixels.GetData(), source_pixels.GetSize()}, {second_level.GetData(), second_level.GetSize()}) == 0);

        // And the level beside it is the filler still, so the copy went to the level it named and not to
        // whichever one the destination starts at.
        Opal::DynamicArray<u8> top_level(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, top_level, 0,
                                       Forge::ImageLayout::TransferSource) == ErrorCode::Success);
        REQUIRE(CountMismatches({top_filler.GetData(), top_filler.GetSize()}, {top_level.GetData(), top_level.GetSize()}) == 0);
    }
    SECTION("One array layer is copied into another")
    {
        // Two layers on each side, and the copy crosses them: the second layer of the source into the first
        // of the destination. The layers of the source differ from one another, so a copy that took layer
        // zero comes back as the wrong half.
        const Opal::DynamicArray<u8> source_pixels = MakeTexelGradient(k_side, k_side, 2);
        Forge::Texture source = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = k_format,
                             .width = k_side,
                             .height = k_side,
                             .array_layer_count = 2,
                             .usage = k_both_ways,
                             .view_type = Forge::TextureViewType::Texture2DArray}));
        UploadMip(fixture.device, fixture.GetQueue(), source, {source_pixels.GetData(), source_pixels.GetSize()}, 0,
                  Forge::PipelineStageBits::None);

        const Opal::DynamicArray<u8> filler = MakeFlatPixels(k_side * k_side * 2, 5, 5, 5);
        Forge::Texture destination = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = k_format,
                             .width = k_side,
                             .height = k_side,
                             .array_layer_count = 2,
                             .usage = k_both_ways,
                             .view_type = Forge::TextureViewType::Texture2DArray}));
        UploadMip(fixture.device, fixture.GetQueue(), destination, {filler.GetData(), filler.GetSize()}, 0,
                  Forge::PipelineStageBits::None);

        const Forge::TextureCopyRegion region{.source = {.first_array_layer = 1}, .destination = {.first_array_layer = 0}};
        copy_regions(source, destination, {&region, 1});

        // Both layers come back at once, packed one after the other, which is what a readback of a mip level
        // of an array hands over.
        const i32 layer_size = k_side * k_side * 4;
        Opal::DynamicArray<u8> read_back(layer_size * 2);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, read_back, 0,
                                       Forge::ImageLayout::TransferSource) == ErrorCode::Success);
        REQUIRE(CountMismatches({source_pixels.GetData() + layer_size, layer_size}, {read_back.GetData(), layer_size}) == 0);
        REQUIRE(CountMismatches({filler.GetData() + layer_size, layer_size}, {read_back.GetData() + layer_size, layer_size}) == 0);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * Every guard between a copy, blit or resolve region and the driver, each asked on its own and answered with
 * the code the guard returns. The readback cases above only ever pass them; a guard deleted or loosened would
 * leave every one of those green and hand the driver a region past the end of an image.
 *
 * Nothing here is submitted, and nothing refused is recorded - which is what the validation assertion at the
 * end checks, since a refusal that recorded anyway would reach the layer.
 */
TEST_CASE("Forge transfer regions the guards refuse", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr u64 k_level_size = k_side * k_side * 4;
    constexpr Forge::TextureUsageBits k_both_ways = Forge::TextureUsageBits::TransferSource | Forge::TextureUsageBits::TransferDestination;

    auto make_texture = [&](Forge::TextureUsageBits usage, Forge::SampleCount sample_count = Forge::SampleCount::Count1, u32 mips = 1)
    {
        return ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                                         .width = k_side,
                                                                         .height = k_side,
                                                                         .mip_level_count = mips,
                                                                         .sample_count = sample_count,
                                                                         .usage = usage}));
    };
    auto make_buffer = [&](u64 size, Forge::BufferUsageBits usage)
    { return ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = size, .usage = usage})); };

    Forge::Texture texture = make_texture(k_both_ways);
    Forge::Texture other = make_texture(k_both_ways);
    const Forge::Buffer source = make_buffer(k_level_size, Forge::BufferUsageBits::TransferSource);
    const Forge::Buffer destination = make_buffer(k_level_size, Forge::BufferUsageBits::TransferDestination);

    Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    REQUIRE(command_buffer.Begin() == ErrorCode::Success);

    /** One buffer to texture region, refused with the code given. */
    auto upload_refuses = [&](const Forge::BufferTextureCopyRegion& region, const Forge::Buffer& buffer = Forge::Buffer{})
    { return command_buffer.CmdCopyBufferToTexture(buffer.IsValid() ? buffer : source, texture, {&region, 1}); };

    SECTION("A region naming a mip level or array layers the texture does not have")
    {
        REQUIRE(upload_refuses({.texture_subresource = {.mip_level = 1}}) == ErrorCode::OutOfBounds);
        REQUIRE(upload_refuses({.texture_subresource = {.first_array_layer = 1}}) == ErrorCode::OutOfBounds);
        REQUIRE(upload_refuses({.texture_subresource = {.array_layer_count = 0}}) == ErrorCode::OutOfBounds);
        REQUIRE(upload_refuses({.texture_subresource = {.array_layer_count = 2}}) == ErrorCode::OutOfBounds);
    }
    SECTION("A negative offset or extent")
    {
        REQUIRE(upload_refuses({.texture_offset = {-1, 0, 0}}) == ErrorCode::InvalidArgument);
        REQUIRE(upload_refuses({.texture_extent = {0, -1, 0}}) == ErrorCode::InvalidArgument);
        const Forge::TextureCopyRegion negative{.destination_offset = {0, 0, -1}, .extent = {1, 1, 1}};
        REQUIRE(command_buffer.CmdCopyTexture(texture, other, {&negative, 1}) == ErrorCode::InvalidArgument);
    }
    SECTION("A box that starts outside the level or reaches past it")
    {
        REQUIRE(upload_refuses({.texture_offset = {k_side + 1, 0, 0}}) == ErrorCode::OutOfBounds);
        REQUIRE(upload_refuses({.texture_offset = {2, 0, 0}, .texture_extent = {k_side, 1, 1}}) == ErrorCode::OutOfBounds);
        REQUIRE(upload_refuses({.texture_offset = {0, 0, 1}, .texture_extent = {1, 1, 1}}) == ErrorCode::OutOfBounds);
        // A copy's extent is resolved against the source and then has to fit the destination as well.
        const Forge::TextureCopyRegion past_the_destination{.destination_offset = {2, 0, 0}};
        REQUIRE(command_buffer.CmdCopyTexture(texture, other, {&past_the_destination, 1}) == ErrorCode::OutOfBounds);
        const Forge::BufferTextureCopyRegion past_on_readback{.texture_offset = {0, 3, 0}, .texture_extent = {1, 2, 1}};
        REQUIRE(command_buffer.CmdCopyTextureToBuffer(texture, destination, {&past_on_readback, 1}) == ErrorCode::OutOfBounds);
    }
    SECTION("A blit box with nothing in it")
    {
        if (!fixture.device.GetPhysicalDevice().SupportsBlit(k_format, true) ||
            !fixture.device.GetPhysicalDevice().SupportsBlit(k_format, false))
        {
            SKIP("This device cannot blit R8G8B8A8_UNORM, and that refusal comes first.");
        }
        // An offset at the far edge with a zero extent is the rest of the level past it, which is nothing.
        const Forge::TextureBlitRegion empty_source{.source_offset = {k_side, 0, 0}};
        REQUIRE(command_buffer.CmdBlitTexture(texture, other, {&empty_source, 1}, ImageFilter::Nearest) == ErrorCode::InvalidArgument);
        const Forge::TextureBlitRegion empty_destination{.destination_offset = {0, k_side, 0}};
        REQUIRE(command_buffer.CmdBlitTexture(texture, other, {&empty_destination, 1}, ImageFilter::Nearest) ==
                ErrorCode::InvalidArgument);
        // The destination box past its level is the half the blit case above never reaches.
        const Forge::TextureBlitRegion past_the_destination{.destination_offset = {2, 0, 0}, .destination_extent = {k_side, k_side, 1}};
        REQUIRE(command_buffer.CmdBlitTexture(texture, other, {&past_the_destination, 1}, ImageFilter::Nearest) ==
                ErrorCode::OutOfBounds);
    }
    SECTION("A buffer offset that is not a multiple of four")
    {
        REQUIRE(upload_refuses({.buffer_offset = 2, .texture_extent = {1, 1, 1}}) == ErrorCode::InvalidArgument);
        const Forge::BufferTextureCopyRegion unaligned{.buffer_offset = 6, .texture_extent = {1, 1, 1}};
        REQUIRE(command_buffer.CmdCopyTextureToBuffer(texture, destination, {&unaligned, 1}) == ErrorCode::InvalidArgument);
    }
    SECTION("A buffer too small for the rows and layers the region describes")
    {
        const Forge::Buffer small_source = make_buffer(k_level_size / 2, Forge::BufferUsageBits::TransferSource);
        REQUIRE(upload_refuses({}, small_source) == ErrorCode::OutOfBounds);
        // A region that fits, pushed past the end by its offset.
        REQUIRE(upload_refuses({.buffer_offset = 4}) == ErrorCode::OutOfBounds);
        // And one pushed past it by a row length that spaces its two rows out further than the buffer runs.
        REQUIRE(upload_refuses({.buffer_row_length = 64, .texture_extent = {1, 2, 1}}) == ErrorCode::OutOfBounds);
        const Forge::Buffer small_destination = make_buffer(k_level_size / 2, Forge::BufferUsageBits::TransferDestination);
        const Forge::BufferTextureCopyRegion whole{};
        REQUIRE(command_buffer.CmdCopyTextureToBuffer(texture, small_destination, {&whole, 1}) == ErrorCode::OutOfBounds);
    }
    SECTION("Every transfer command refuses a resource created without the usage it needs")
    {
        Forge::Texture sampled_only = make_texture(Forge::TextureUsageBits::Sampled);
        const Forge::Buffer storage_only = make_buffer(k_level_size, Forge::BufferUsageBits::StorageBuffer);
        const Forge::BufferTextureCopyRegion buffer_region{};
        const Forge::TextureCopyRegion texture_region{};

        REQUIRE(command_buffer.CmdCopyBufferToTexture(storage_only, texture, {&buffer_region, 1}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdCopyBufferToTexture(source, sampled_only, {&buffer_region, 1}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdCopyTextureToBuffer(sampled_only, destination, {&buffer_region, 1}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdCopyTextureToBuffer(texture, storage_only, {&buffer_region, 1}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdCopyTexture(sampled_only, other, {&texture_region, 1}) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdCopyTexture(texture, sampled_only, {&texture_region, 1}) == ErrorCode::InvalidArgument);

        const Forge::TextureBlitRegion blit_region{};
        REQUIRE(command_buffer.CmdBlitTexture(sampled_only, other, {&blit_region, 1}, ImageFilter::Nearest) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdBlitTexture(texture, sampled_only, {&blit_region, 1}, ImageFilter::Nearest) == ErrorCode::InvalidArgument);

        // Four samples on a colour attachment is a count every device supports, so the only thing wrong with
        // the source of this resolve is the usage it lacks.
        Forge::Texture multisampled = make_texture(Forge::TextureUsageBits::ColorAttachment, Forge::SampleCount::Count4);
        REQUIRE(command_buffer.CmdResolveTexture(multisampled, other, {&texture_region, 1}) == ErrorCode::InvalidArgument);

        Forge::Texture mipped_without_source = make_texture(Forge::TextureUsageBits::TransferDestination, Forge::SampleCount::Count1, 2);
        REQUIRE(command_buffer.CmdGenerateMips(mipped_without_source) == ErrorCode::InvalidArgument);
        // And a texture with one level, which has no chain to generate.
        REQUIRE(command_buffer.CmdGenerateMips(texture) == ErrorCode::InvalidArgument);
    }
    REQUIRE(command_buffer.End() == ErrorCode::Success);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge mip level sizes", "[forge]")
{
    // No device: GetMipLevelSize reads a desc and nothing else, so this is the one case here that needs
    // nothing on the machine.
    SECTION("Every level is a quarter of the one above it")
    {
        const Forge::TextureDesc desc{.format = PixelFormat::R8G8B8A8_UNORM, .width = 8, .height = 8, .mip_level_count = 4};
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(desc, 0)) == 8 * 8 * 4);
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(desc, 1)) == 4 * 4 * 4);
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(desc, 2)) == 2 * 2 * 4);
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(desc, 3)) == 1 * 1 * 4);
    }
    SECTION("An odd extent halves down rather than rounding up, and never below one")
    {
        const Forge::TextureDesc desc{.format = PixelFormat::R8G8B8A8_UNORM, .width = 5, .height = 3, .mip_level_count = 4};
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(desc, 0)) == 5 * 3 * 4);
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(desc, 1)) == 2 * 1 * 4);
        // Both axes are already at one here, and a level below that is still one texel rather than none.
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(desc, 2)) == 1 * 1 * 4);
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(desc, 3)) == 1 * 1 * 4);
    }
    SECTION("Array layers and depth both multiply the level")
    {
        const Forge::TextureDesc layered{
            .format = PixelFormat::R8G8B8A8_UNORM, .width = 4, .height = 4, .mip_level_count = 2, .array_layer_count = 3};
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(layered, 0)) == 4 * 4 * 4 * 3);
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(layered, 1)) == 2 * 2 * 4 * 3);

        const Forge::TextureDesc volume{.dimension = Forge::TextureDimension::Texture3D,
                                        .format = PixelFormat::R8G8B8A8_UNORM,
                                        .width = 4,
                                        .height = 4,
                                        .depth = 4,
                                        .mip_level_count = 2};
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(volume, 0)) == 4 * 4 * 4 * 4);
        // Depth halves with the other two axes.
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(volume, 1)) == 2 * 2 * 2 * 4);
    }
    SECTION("A depth format is sized by its own texel, not by four bytes of colour")
    {
        const Forge::TextureDesc half{.format = PixelFormat::D16_UNORM, .width = 4, .height = 4};
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(half, 0)) == 4 * 4 * 2);
        const Forge::TextureDesc full{.format = PixelFormat::D32_SFLOAT, .width = 4, .height = 4};
        REQUIRE(ForgeTest::Unwrap(Forge::GetMipLevelSize(full, 0)) == 4 * 4 * 4);
    }
    SECTION("A block compressed format is refused rather than answering as if it were packed texels")
    {
        // The size of a compressed level is a count of blocks, not of texels, and answering with the texel
        // arithmetic would hand a readback a buffer of the wrong size and no reason to notice.
        const Forge::TextureDesc desc{.format = PixelFormat::BC1_RGBA_UNORM_BLOCK, .width = 8, .height = 8, .mip_level_count = 2};
        REQUIRE(Forge::GetMipLevelSize(desc, 0).GetErrorOr(ErrorCode::Success) == ErrorCode::UnsupportedFormat);
    }
    SECTION("A level the texture does not have is refused")
    {
        const Forge::TextureDesc desc{.format = PixelFormat::R8G8B8A8_UNORM, .width = 8, .height = 8, .mip_level_count = 2};
        REQUIRE(Forge::GetMipLevelSize(desc, 2).GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
    }
}

/** Two storage buffers in two different sets, so binding more than one set at a time is checkable. */
constexpr const char* k_two_set_source = R"(
[[vk::binding(0, 0)]] RWStructuredBuffer<uint> first;
[[vk::binding(0, 1)]] RWStructuredBuffer<uint> second;

[shader("compute")]
[numthreads(64, 1, 1)]
void main_two_sets(uint3 thread_id : SV_DispatchThreadID)
{
    second[thread_id.x] = first[thread_id.x] * 2;
}
)";

TEST_CASE("Forge barrier batches", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_element_count = 64;
    constexpr i32 k_side = 4;

    SECTION("One call carries barriers of all three kinds")
    {
        // A compute shader fills a buffer, the batch orders that write against the copy that reads it and
        // against the texture transition beside it, and the readback says both halves arrived.
        const Forge::Shader compute_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
        const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, compute_shader);

        const Forge::Buffer written = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                     .usage = Forge::BufferUsageBits::StorageBuffer |
                                                              Forge::BufferUsageBits::TransferSource,
                                                     .host_access = Forge::HostAccess::None,
                                                     .use_device_address = true}));
        const Forge::Buffer copied = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::TransferDestination,
                                                    .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        REQUIRE(copied.Update(zeros) == ErrorCode::Success);
        Forge::Texture texture = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, 33);

        const VkDeviceAddress address = written.GetNativeDeviceAddress();
        REQUIRE(Forge::ImmediateSubmit(
            fixture.device, fixture.GetQueue(),
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(address)) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);

                const Forge::MemoryBarrier memory{.stages_must_finish = Forge::PipelineStageBits::ComputeShader,
                                                  .stages_must_finish_access = Forge::PipelineStageAccessBits::Write,
                                                  .before_stages_start = Forge::PipelineStageBits::Transfer,
                                                  .before_stages_start_access = Forge::PipelineStageAccessBits::Read};
                const Forge::BufferBarrier buffer = Forge::BufferBarrier::WriteThenRead(
                    written, Forge::PipelineStageBits::ComputeShader, Forge::PipelineStageBits::Transfer);
                const Forge::TextureBarrier texture_barrier = ForgeTest::Unwrap(Forge::TextureBarrier::ToTransferSource(texture));
                // CmdBarriers is what every other Cmd*Barrier delegates to, and the only way to put all
                // three kinds into one dependency.
                REQUIRE(command_buffer.CmdBarriers({.memory = {&memory, 1}, .buffer = {&buffer, 1}, .texture = {&texture_barrier, 1}}) ==
                        ErrorCode::Success);

                REQUIRE(command_buffer.CmdCopyBuffer(written, copied) == ErrorCode::Success);
            }) == ErrorCode::Success);

        RequireComputeWrote(copied, k_element_count);
        // The texture barrier in the same batch moved the texture, which is what makes this readback legal.
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferSource);
        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, texture, k_side);
        const Opal::DynamicArray<u8> expected = MakeTexelGrid(k_side, k_side, 33);
        REQUIRE(CountMismatches(expected, pixels) == 0);
    }
    SECTION("Several texture barriers go down in one call")
    {
        // Three textures in different layouts, transitioned together, each one read back after. The plural
        // overload forwards to CmdBarriers like the singular one, so what this catches is the forwarding
        // itself - a call that dropped all but the first would leave two of the three where they were, and
        // ReadBackTexture would be reading a layout the texture is not in.
        constexpr i32 k_texture_count = 3;
        Opal::DynamicArray<Forge::Texture> textures;
        Opal::DynamicArray<Forge::TextureBarrier> barriers;
        for (i32 i = 0; i < k_texture_count; ++i)
        {
            textures.PushBack(MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, static_cast<u8>(60 + i)));
        }
        for (i32 i = 0; i < k_texture_count; ++i)
        {
            barriers.PushBack(ForgeTest::Unwrap(Forge::TextureBarrier::ToTransferSource(textures[i])));
        }
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(), [&](Forge::CommandBuffer& command_buffer)
                    { REQUIRE(command_buffer.CmdTextureBarriers({barriers.GetData(), barriers.GetSize()}) == ErrorCode::Success); }) ==
                ErrorCode::Success);

        for (i32 i = 0; i < k_texture_count; ++i)
        {
            INFO("texture " << i);
            REQUIRE(ForgeTest::Unwrap(textures[i].GetCurrentLayout()) == Forge::ImageLayout::TransferSource);
            const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, textures[i], k_side);
            const Opal::DynamicArray<u8> expected = MakeTexelGrid(k_side, k_side, static_cast<u8>(60 + i));
            REQUIRE(CountMismatches(expected, pixels) == 0);
        }
    }
    SECTION("A by-region dependency reaches the dependency flags")
    {
        // Not inside a rendering pass, which is where the flag would actually mean something. Forge is
        // written entirely on dynamic rendering, and vkCmdPipelineBarrier2 may not be called inside a render
        // pass instance begun by CmdBeginRendering at all unless the device enabled
        // VK_KHR_dynamic_rendering_local_read - the validation layer says so in as many words. So what is
        // checkable here is that the flag reaches dependencyFlags rather than being dropped on the way;
        // whether a tiled device then kept the work in tile memory is neither observable from here nor
        // reachable without that extension.
        Forge::Texture texture = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, 44);
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        const Forge::TextureBarrier texture_barrier = ForgeTest::Unwrap(Forge::TextureBarrier::ToTransferSource(texture));
                        REQUIRE(command_buffer.CmdBarriers({.texture = {&texture_barrier, 1},
                                                            .flags = Forge::DependencyFlagBits::ByRegion}) == ErrorCode::Success);
                    }) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferSource);
        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, texture, k_side);
        const Opal::DynamicArray<u8> expected = MakeTexelGrid(k_side, k_side, 44);
        REQUIRE(CountMismatches(expected, pixels) == 0);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge barrier presets", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr u8 k_seed = 77;
    const Opal::DynamicArray<u8> expected = MakeTexelGrid(k_side, k_side, k_seed);

    /**
     * Record the preset over a texture holding known content and read that content back afterwards. A preset
     * naming the wrong source layout either trips the validation layer or discards what the texture holds -
     * Undefined as an old layout is a discard - so content that survives says the preset named the layout the
     * texture was actually in.
     */
    auto run_preset = [&](Forge::TextureUsageBits usage, auto&& make_barrier, Forge::ImageLayout expected_layout)
    {
        Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                .width = k_side,
                                                .height = k_side,
                                                .usage = Forge::TextureUsageBits::TransferSource |
                                                         Forge::TextureUsageBits::TransferDestination | usage}));
        UploadGrid(fixture.device, fixture.GetQueue(), texture, expected);
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(), [&](Forge::CommandBuffer& command_buffer)
                                       { REQUIRE(command_buffer.CmdTextureBarrier(make_barrier(texture)) == ErrorCode::Success); }) ==
                ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == expected_layout);

        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, texture, k_side);
        REQUIRE(CountMismatches(expected, pixels) == 0);
    };

    SECTION("The three argument To is told both layouts")
    {
        // No short form on purpose: with both, dropping an argument would leave a call that compiles and
        // means the opposite, since the layout in the middle is the source and the one at the end is not.
        run_preset(
            Forge::TextureUsageBits::Sampled,
            [](Forge::Texture& texture)
            {
                return ForgeTest::Unwrap(Forge::TextureBarrier::To(texture, ForgeTest::Unwrap(texture.GetCurrentLayout()),
                                                                   Forge::ImageLayout::TransferDestination));
            },
            Forge::ImageLayout::TransferDestination);
    }
    SECTION("A layout with no preset is refused rather than guessed at")
    {
        Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                .width = k_side,
                                                .height = k_side,
                                                .usage = Forge::TextureUsageBits::TransferSource}));
        // DepthStencilReadOnly is a real layout with no preset behind it, which is the near miss worth
        // checking: the dispatch reports rather than picking whichever preset is closest. General used to be
        // the example here and stopped being one once General got a preset of its own.
        REQUIRE(
            Forge::TextureBarrier::To(texture, Forge::ImageLayout::Undefined, Forge::ImageLayout::DepthStencilReadOnly).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("ToDepthStencilAttachment moves a depth texture")
    {
        Forge::Texture depth = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::D32_SFLOAT,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::DepthStencilAttachment}));
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           REQUIRE(command_buffer.CmdTextureBarrier(
                                                       Forge::TextureBarrier::ToDepthStencilAttachment(depth)) == ErrorCode::Success);
                                       }) == ErrorCode::Success);
        // Rendering with one is "Forge depth testing"'s business; what this says is that the preset picks the depth aspect off the
        // format rather than the colour aspect a colour texture would have given it.
        REQUIRE(ForgeTest::Unwrap(depth.GetCurrentLayout()) == Forge::ImageLayout::DepthStencilAttachment);
    }
    SECTION("BufferBarrier::ReadThenWrite orders a read before the write that follows it")
    {
        // The write is a transfer over the same range a compute shader just read, so without the barrier the
        // copy would be free to land before the dispatch finished reading.
        const Forge::Shader compute_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
        const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, compute_shader);

        constexpr i32 k_element_count = 64;
        const Opal::DynamicArray<u8> replacement = MakeBytes(k_element_count * sizeof(u32), 13);
        const Forge::Buffer shared = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer |
                                                             Forge::BufferUsageBits::TransferDestination,
                                                    .host_access = Forge::HostAccess::Random,
                                                    .use_device_address = true}));
        const Forge::Buffer source = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                   {.size = replacement.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, replacement));

        const VkDeviceAddress address = shared.GetNativeDeviceAddress();
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(address)) ==
                                           ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBufferBarrier(Forge::BufferBarrier::ReadThenWrite(
                                               shared, Forge::PipelineStageBits::ComputeShader, Forge::PipelineStageBits::Transfer)) ==
                                           ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdCopyBuffer(source, shared) == ErrorCode::Success);
                               }) == ErrorCode::Success);

        // The copy is last, so what is in the buffer is what it wrote.
        Opal::DynamicArray<u8> out(replacement.GetSize());
        REQUIRE(shared.Read(out) == ErrorCode::Success);
        REQUIRE(CountMismatches(replacement, out) == 0);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge barrier preset for presenting", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // Present is a swap chain layout, so naming it needs the device to have the extension even though
    // nothing here presents. Asking for it without a surface is legal, and is what makes ToPresent
    // checkable in a file that never opens a window.
    const char* const k_swap_chain_extension[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (!CanCreateDevice({}, {}, k_swap_chain_extension))
    {
        SKIP("This device has no swap chain extension.");
    }
    ForgeFixture fixture({}, {}, k_swap_chain_extension);
    Forge::Device& device = fixture.device;
    Forge::DeviceQueue& queue = fixture.GetQueue();

    constexpr i32 k_side = 4;
    Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                    .width = k_side,
                                    .height = k_side,
                                    .usage = Forge::TextureUsageBits::ColorAttachment |
                                             Forge::TextureUsageBits::TransferSource}));
    REQUIRE(Forge::ImmediateSubmit(
                device, queue,
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(texture)) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToPresent(texture)) == ErrorCode::Success);
                }) == ErrorCode::Success);
    REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::Present);

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a descriptor written after the set was bound", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_late_update{.update_after_bind_descriptors = true,
                                                  .update_unused_while_pending_descriptors = true};
    if (!CanCreateDevice(k_late_update))
    {
        SKIP("This device cannot have its descriptors written once the set is bound.");
    }
    ForgeFixture fixture(k_late_update);
    constexpr i32 k_element_count = 64;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_descriptor_source, {.entry_point = "main_descriptor", .cache = GetShaderCache()}));

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 8) == ErrorCode::Success);
    pool_desc.max_sets = 4;
    pool_desc.use_update_after_bind = true;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    /** The buffer the dispatch wrote, element by element, against what the shader computes. */
    auto require_written = [&](const Forge::Buffer& output, bool expect_written)
    {
        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            INFO("element " << i);
            REQUIRE(values[i] == (expect_written ? static_cast<u32>(i) + 7 : 0u));
        }
    };

    SECTION("The descriptor the dispatch reads is the one written after the bind")
    {
        // The set is written once, bound, and written again with another buffer before anything is
        // submitted. With UpdateAfterBind the descriptor the dispatch reads is the one standing when it
        // runs, so the second buffer is the one that comes back written and the first is left as it was.
        Forge::DescriptorSetLayoutDesc layout_desc;
        REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute, {},
                                       Forge::DescriptorBindingFlagBits::UpdateAfterBind) == ErrorCode::Success);
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        const Forge::Buffer bound_at_record = MakeWipedOutput(fixture.device, k_element_count);
        const Forge::Buffer written_after = MakeWipedOutput(fixture.device, k_element_count);
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, bound_at_record) == ErrorCode::Success);

        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);

        // After the bind was recorded and before the queue ever sees it.
        REQUIRE(set.Update(0, written_after) == ErrorCode::Success);

        const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        REQUIRE(fixture.GetQueue().Submit(command_buffer, fence) == ErrorCode::Success);
        REQUIRE(fence.Wait() == ErrorCode::Success);

        require_written(written_after, true);
        require_written(bound_at_record, false);
    }
    SECTION("A binding no recorded command reads can be written while the submit is pending")
    {
        // Two bindings, and the shader reads the first. The second is written after the set is bound and
        // before the submit, which is what UpdateUnusedWhilePending allows and what a plain binding forbids.
        Forge::DescriptorSetLayoutDesc layout_desc;
        REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute, {},
                                       Forge::DescriptorBindingFlagBits::UpdateUnusedWhilePending) == ErrorCode::Success);
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        const Forge::Buffer read_by_the_shader = MakeWipedOutput(fixture.device, k_element_count);
        const Forge::Buffer unused = MakeWipedOutput(fixture.device, k_element_count);
        const Forge::Buffer written_while_pending = MakeWipedOutput(fixture.device, k_element_count);
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, read_by_the_shader) == ErrorCode::Success);
        REQUIRE(set.Update(1, unused) == ErrorCode::Success);

        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);

        REQUIRE(set.Update(1, written_while_pending) == ErrorCode::Success);

        const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        REQUIRE(fixture.GetQueue().Submit(command_buffer, fence) == ErrorCode::Success);
        REQUIRE(fence.Wait() == ErrorCode::Success);

        // The dispatch wrote through the binding it reads, and the two buffers behind the binding it does
        // not are both untouched.
        require_written(read_by_the_shader, true);
        require_written(unused, false);
        require_written(written_while_pending, false);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a late update binding on a device without the feature is refused", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // The fixture of every other case here: a device that asked for no descriptor features beyond the
    // defaults, which is where both of these flags are a mistake rather than a capability.
    ForgeFixture fixture;
    REQUIRE_FALSE(fixture.device.GetFeatures().update_after_bind_descriptors);
    REQUIRE_FALSE(fixture.device.GetFeatures().update_unused_while_pending_descriptors);

    Forge::DescriptorSetLayoutDesc after_bind_desc;
    REQUIRE(after_bind_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute, {},
                                       Forge::DescriptorBindingFlagBits::UpdateAfterBind) == ErrorCode::Success);
    REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, after_bind_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);

    Forge::DescriptorSetLayoutDesc while_pending_desc;
    REQUIRE(while_pending_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute, {},
                                          Forge::DescriptorBindingFlagBits::UpdateUnusedWhilePending) == ErrorCode::Success);
    REQUIRE(Forge::DescriptorSetLayout::Create(fixture.device, while_pending_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a layout checked against a set index other than zero", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_element_count = 64;

    // Two sets, one binding each, both named by the shader: `first` at set zero and `second` at set one.
    const Forge::Shader shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_two_set_source, {.entry_point = "main_two_sets", .cache = GetShaderCache()}));

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 8) == ErrorCode::Success);
    pool_desc.max_sets = 8;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    auto make_checked_layout = [&](u32 set_index, u32 binding)
    {
        Forge::DescriptorSetLayoutDesc layout_desc;
        REQUIRE(layout_desc.AddBinding(binding, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        layout_desc.shaders.PushBack(Opal::Ref<const Forge::Shader>(shader));
        layout_desc.set_index = set_index;
        return Forge::DescriptorSetLayout::Create(fixture.device, layout_desc);
    };

    SECTION("The names come from the set the layout says it is")
    {
        const Forge::DescriptorSetLayout second_layout = ForgeTest::Unwrap(make_checked_layout(1, 0));
        Forge::DescriptorSet second_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, second_layout));
        const Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);

        // `second` is what set one calls its binding, and `first` is set zero's name for the binding at the
        // same index - so a layout that had read set zero would take the other name and refuse this one.
        REQUIRE(second_set.Update("second", output) == ErrorCode::Success);
        REQUIRE(second_set.Update("first", output) == ErrorCode::InvalidArgument);
    }
    SECTION("Both sets drive the dispatch that reads them")
    {
        const Forge::DescriptorSetLayout first_layout = ForgeTest::Unwrap(make_checked_layout(0, 0));
        const Forge::DescriptorSetLayout second_layout = ForgeTest::Unwrap(make_checked_layout(1, 0));

        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(first_layout));
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(second_layout));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Opal::DynamicArray<u32> input_values(k_element_count);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            input_values[i] = static_cast<u32>(i) + 500;
        }
        const Forge::Buffer input = ForgeTest::Unwrap(
            Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                   .usage = Forge::BufferUsageBits::StorageBuffer,
                                                   .host_access = Forge::HostAccess::Random},
                                  {reinterpret_cast<const u8*>(input_values.GetData()), input_values.GetSize() * sizeof(u32)}));
        const Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);

        Forge::DescriptorSet first_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, first_layout));
        Forge::DescriptorSet second_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, second_layout));
        REQUIRE(first_set.Update("first", input) == ErrorCode::Success);
        REQUIRE(second_set.Update("second", output) == ErrorCode::Success);

        const Opal::InPlaceArray<Opal::Ref<const Forge::DescriptorSet>, 2> sets{Opal::Ref<const Forge::DescriptorSet>(first_set),
                                                                                Opal::Ref<const Forge::DescriptorSet>(second_set)};
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdBindDescriptorSets(pipeline, {sets.GetData(), 2}) ==
                                                   ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                                       }) == ErrorCode::Success);

        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            INFO("element " << i);
            REQUIRE(values[i] == input_values[i] * 2);
        }
    }
    SECTION("A layout that leaves a binding of its set undeclared is refused")
    {
        // Set one declares binding zero, and this layout declares binding three instead - so the binding the
        // shader reads has nothing behind it, which is what naming the set index catches.
        REQUIRE(make_checked_layout(1, 3).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);

        // The other way round is deliberately allowed: a set index the shader declares nothing at leaves
        // every binding of this layout unmatched, which is the same as a descriptor no shader reads. The
        // layout is built, and the binding simply has no name for the by-name update to find.
        const Forge::DescriptorSetLayout unused_set = ForgeTest::Unwrap(make_checked_layout(2, 0));
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, unused_set));
        const Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);
        REQUIRE(set.Update("second", output) == ErrorCode::InvalidArgument);
        REQUIRE(set.Update(0, output) == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge binding several descriptor sets at once", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_element_count = 64;

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 8) == ErrorCode::Success);
    pool_desc.max_sets = 8;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout first_layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));
    const Forge::DescriptorSetLayout second_layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(fixture.device, k_two_set_source,
                                                                   {.entry_point = "main_two_sets", .cache = GetShaderCache()}));
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(first_layout));
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(second_layout));
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    // Distinct values per element, so a shader that read the wrong set would produce a pattern rather than
    // one wrong number.
    Opal::DynamicArray<u32> input_values(k_element_count);
    for (i32 i = 0; i < k_element_count; ++i)
    {
        input_values[i] = static_cast<u32>(i) + 500;
    }
    const Forge::Buffer input = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                               .usage = Forge::BufferUsageBits::StorageBuffer,
                                               .host_access = Forge::HostAccess::Random},
                              {reinterpret_cast<const u8*>(input_values.GetData()), input_values.GetSize() * sizeof(u32)}));

    auto require_doubled = [&](const Forge::Buffer& output)
    {
        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            INFO("element " << i);
            REQUIRE(values[i] == input_values[i] * 2);
        }
    };

    SECTION("Both sets go down in one call")
    {
        Forge::DescriptorSet first = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, first_layout));
        Forge::DescriptorSet second = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, second_layout));
        const Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);
        REQUIRE(first.Update(0, input) == ErrorCode::Success);
        REQUIRE(second.Update(0, output) == ErrorCode::Success);

        const Opal::InPlaceArray<Opal::Ref<const Forge::DescriptorSet>, 2> sets{Opal::Ref<const Forge::DescriptorSet>(first),
                                                                                Opal::Ref<const Forge::DescriptorSet>(second)};
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSets(pipeline, {sets.GetData(), 2}) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        require_doubled(output);
    }
    SECTION("A non-zero first set binds into the slot it names")
    {
        // Set zero goes down on its own and set one through the plural call at first_set one, so a call that
        // ignored first_set would overwrite set zero and the shader would read its output as its input.
        Forge::DescriptorSet first = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, first_layout));
        Forge::DescriptorSet second = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, second_layout));
        const Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);
        REQUIRE(first.Update(0, input) == ErrorCode::Success);
        REQUIRE(second.Update(0, output) == ErrorCode::Success);

        const Opal::InPlaceArray<Opal::Ref<const Forge::DescriptorSet>, 1> sets{Opal::Ref<const Forge::DescriptorSet>(second)};
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, first, 0) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSets(pipeline, {sets.GetData(), 1}, 1) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        require_doubled(output);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * The rasterizer state, the topologies and the per-instance vertex rate. Everything here ends in a readback
 * that counts texels, because the mistakes this state makes - a triangle culled by the wrong winding, a
 * scissor that did not take - produce an image rather than a failure.
 */
namespace
{

/** Positions with a z of their own, for the two cases that care where a fragment lands in depth. */
constexpr const char* k_depth_position_source = R"(
[shader("vertex")]
float4 main_depth_vertex(float3 position : POSITION) : SV_Position
{
    return float4(position, 1.0);
}

[shader("fragment")]
float4 main_depth_fragment() : SV_Target
{
    return float4(0.0, 1.0, 0.0, 0.0);
}
)";

/**
 * A point topology leaves the point size undefined unless the vertex stage writes it, so this one does. One
 * pixel is the only size every device draws without the large_points feature.
 */
constexpr const char* k_point_source = R"(
struct PointOutput
{
    float4 position : SV_Position;
    float size : SV_PointSize;
};

[shader("vertex")]
PointOutput main_point_vertex(float2 position : POSITION)
{
    PointOutput output;
    output.position = float4(position, 0.0, 1.0);
    output.size = 1.0;
    return output;
}
)";

/**
 * Two bindings at two rates: the position advances per vertex and the offset and the value advance per
 * instance, so four instances of one quad land in four places in four colours. The value is spread over the
 * channels a bit at a time, which keeps every channel at zero or one and out of the way of UNORM rounding.
 */
constexpr const char* k_instanced_source = R"(
struct InstancedOutput
{
    float4 position : SV_Position;
    nointerpolation uint value : VALUE;
};

[shader("vertex")]
InstancedOutput main_instanced_vertex(float2 position : POSITION, float2 offset : OFFSET, uint value : VALUE)
{
    InstancedOutput output;
    output.position = float4(position + offset, 0.0, 1.0);
    output.value = value;
    return output;
}

[shader("fragment")]
float4 main_instanced_fragment(InstancedOutput input) : SV_Target
{
    return float4((input.value & 1) != 0 ? 1.0 : 0.0,
                  (input.value & 2) != 0 ? 1.0 : 0.0,
                  (input.value & 4) != 0 ? 1.0 : 0.0,
                  1.0);
}
)";

/** What one call to RenderWithDepth leaves behind: the colour attachment and the depth attachment, both read back. */
struct DepthPassResult
{
    Opal::DynamicArray<u8> pixels;
    Opal::DynamicArray<f32> depths;
};

/**
 * Clear a colour attachment and a depth attachment, run one recorded pass over both, and hand back what each
 * ended up holding. The viewport and scissor cover the whole target before `record` runs, which is every
 * case's default and is still a case's to override - the last CmdSetViewport before a draw is the one that
 * takes.
 */
template <typename Record>
DepthPassResult RenderWithDepth(ForgeFixture& fixture, Forge::Texture& color, Forge::Texture& depth, i32 side,
                                const Vector4f& color_clear, const Forge::DepthStencilClearValue& depth_clear, Record&& record)
{
    REQUIRE(Forge::ImmediateSubmit(
                fixture.device, fixture.GetQueue(),
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth)) ==
                            ErrorCode::Success);
                    const Forge::RenderingDesc rendering_desc{
                        .render_area_extent = {side, side},
                        .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                             .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                             .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                             .clear_value = color_clear}},
                        .depth_attachment = Forge::RenderingAttachmentDesc{.texture = depth,
                                                                           .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                           .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                           .clear_value = depth_clear}};
                    REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {static_cast<f32>(side), static_cast<f32>(side)}) ==
                            ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {side, side}) == ErrorCode::Success);
                    record(command_buffer);
                    REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                }) == ErrorCode::Success);

    DepthPassResult result;
    result.pixels = ReadColorPixels(fixture, color, side);
    Opal::DynamicArray<u8> depth_bytes(side * side * static_cast<i32>(sizeof(f32)));
    REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), depth, depth_bytes, 0, Forge::ImageLayout::TransferSource) ==
            ErrorCode::Success);
    result.depths = Opal::DynamicArray<f32>(side * side);
    memcpy(result.depths.GetData(), depth_bytes.GetData(), depth_bytes.GetSize());
    return result;
}

/** Whether a fragment reached this texel, which the shaders above say by writing green over a red clear. */
bool IsCovered(const Opal::DynamicArray<u8>& pixels, i32 side, i32 x, i32 y)
{
    const i32 base = (y * side + x) * 4;
    return pixels[base] == 0 && pixels[base + 1] == 255;
}

i32 CountCovered(const Opal::DynamicArray<u8>& pixels, i32 side)
{
    i32 covered = 0;
    for (i32 y = 0; y < side; ++y)
    {
        for (i32 x = 0; x < side; ++x)
        {
            covered += IsCovered(pixels, side, x, y) ? 1 : 0;
        }
    }
    return covered;
}

/** The NDC position of the centre of one texel, which is where a point has to sit to land on it. */
Vector2f TexelCentre(i32 side, i32 x, i32 y)
{
    const f32 extent = static_cast<f32>(side);
    return {(static_cast<f32>(x) + 0.5f) / extent * 2.0f - 1.0f, (static_cast<f32>(y) + 0.5f) / extent * 2.0f - 1.0f};
}

/** One pipeline over two shaders, with everything these cases vary spelled out as arguments. */
Opal::Expected<Forge::Pipeline, ErrorCode> MakeRasterPipeline(const Forge::Device& device, const Forge::Shader& vertex_shader,
                                                              const Forge::Shader& fragment_shader, PixelFormat format,
                                                              const Forge::RasterizerDesc& rasterizer,
                                                              PrimitiveTopology topology = PrimitiveTopology::Triangle,
                                                              Forge::DynamicStateBits dynamic_state = Forge::DynamicStateBits::None)
{
    Forge::GraphicsPipelineDesc pipeline_desc = MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, format);
    pipeline_desc.rasterizer = rasterizer;
    pipeline_desc.topology = topology;
    pipeline_desc.dynamic_state = dynamic_state;
    return Forge::Pipeline::Create(device, pipeline_desc);
}

}  // namespace

TEST_CASE("Forge culling and winding", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                 {.size = sizeof(k_fullscreen_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                                 Opal::AsBytes(k_fullscreen_vertices)));

    // Whether the one triangle survived, for a given cull mode and winding. The geometry never changes, so
    // what the answers differ by is only the state.
    auto is_drawn = [&](Face cull_mode, WindingOrder front_face)
    {
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(MakeRasterPipeline(fixture.device, vertex_shader, fragment_shader, k_format,
                                                            {.cull_mode = cull_mode, .front_face = front_face}));
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                                                           });
        return CountCovered(pixels, k_side) == k_side * k_side;
    };

    SECTION("Culling nothing draws the triangle whichever way it is wound")
    {
        REQUIRE(is_drawn(Face::None, WindingOrder::CCW));
        REQUIRE(is_drawn(Face::None, WindingOrder::CW));
    }
    SECTION("Flipping the winding, not the geometry, is what brings a culled triangle back")
    {
        // Which winding this triangle actually has is not asserted, and deliberately: it depends on the
        // viewport transform as much as on the vertex order. What has to hold is that the two answers
        // differ, since that is the whole of what front_face does.
        const bool drawn_ccw = is_drawn(Face::Back, WindingOrder::CCW);
        const bool drawn_cw = is_drawn(Face::Back, WindingOrder::CW);
        INFO("back-culled, CCW front: " << drawn_ccw << ", CW front: " << drawn_cw);
        REQUIRE(drawn_ccw != drawn_cw);
    }
    SECTION("Culling the other face is the opposite answer")
    {
        const bool back_culled = is_drawn(Face::Back, WindingOrder::CCW);
        const bool front_culled = is_drawn(Face::Front, WindingOrder::CCW);
        INFO("back-culled: " << back_culled << ", front-culled: " << front_culled);
        REQUIRE(back_culled != front_culled);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/** A triangle well inside the target, so that a wireframe of it has an interior to leave alone. */
constexpr f32 k_inset_triangle[] = {-0.8f, -0.8f, 0.8f, -0.8f, 0.0f, 0.8f};

TEST_CASE("Forge fill modes", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const bool has_wireframe = CanCreateDevice({.fill_mode_non_solid = true});
    ForgeFixture fixture({.fill_mode_non_solid = has_wireframe});
    constexpr i32 k_side = 16;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                 {.size = sizeof(k_inset_triangle), .usage = Forge::BufferUsageBits::VertexBuffer},
                                 Opal::AsBytes(k_inset_triangle)));

    auto draw_with_fill_mode = [&](FillMode fill_mode)
    {
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(MakeRasterPipeline(fixture.device, vertex_shader, fragment_shader, k_format,
                                                            {.fill_mode = fill_mode, .cull_mode = Face::None}));
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        return RenderRaster(fixture, color, k_side,
                            [&](Forge::CommandBuffer& command_buffer)
                            {
                                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                                REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                            });
    };

    SECTION("A wireframe leaves the interior of the triangle as the clear left it")
    {
        INFO("fill_mode_non_solid supported: " << has_wireframe);
        if (!has_wireframe)
        {
            SKIP("This device cannot draw anything but solid.");
        }
        const Opal::DynamicArray<u8> solid = draw_with_fill_mode(FillMode::Solid);
        const Opal::DynamicArray<u8> wireframe = draw_with_fill_mode(FillMode::Wireframe);

        // The centroid of the triangle above, which is several texels clear of every edge on a target this
        // size, so no line width the device picks can reach it.
        const i32 centre_x = k_side / 2;
        const i32 centre_y = k_side / 3;
        INFO("centroid texel " << centre_x << "," << centre_y);
        REQUIRE(IsCovered(solid, k_side, centre_x, centre_y));
        REQUIRE_FALSE(IsCovered(wireframe, k_side, centre_x, centre_y));

        // Where exactly the edges land is the device's business; that they are drawn and that they are less
        // than the filled triangle is not.
        const i32 solid_covered = CountCovered(solid, k_side);
        const i32 wireframe_covered = CountCovered(wireframe, k_side);
        INFO("solid covered " << solid_covered << ", wireframe covered " << wireframe_covered);
        REQUIRE(wireframe_covered > 0);
        REQUIRE(wireframe_covered < solid_covered);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a wireframe on a device without the feature is refused", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // The polygon mode is a plain enum in the create info, so without this the device is handed a mode it
    // never agreed to and the validation layer is the only thing that notices.
    ForgeFixture fixture({.fill_mode_non_solid = false});
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));
    REQUIRE(MakeRasterPipeline(fixture.device, vertex_shader, fragment_shader, k_format,
                                     {.fill_mode = FillMode::Wireframe, .cull_mode = Face::None}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    // Solid on the same device is fine, so what was refused was the fill mode and not the pipeline.
    const Forge::Pipeline solid_pipeline =
        ForgeTest::Unwrap(MakeRasterPipeline(fixture.device, vertex_shader, fragment_shader, k_format, {.cull_mode = Face::None}));
    REQUIRE(solid_pipeline.IsValid());
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge topologies", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 8;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr i32 k_line_row = 4;

    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));

    SECTION("A line topology puts pixels along one row and nowhere else")
    {
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
        // Along the centres of one row of texels rather than along the boundary between two, so which row
        // the line lands on is not left to a rounding rule.
        const Vector2f left = TexelCentre(k_side, 0, k_line_row);
        const Vector2f right = TexelCentre(k_side, k_side - 1, k_line_row);
        const f32 line_vertices[] = {left.x, left.y, right.x, right.y};
        const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                     {.size = sizeof(line_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                                     Opal::AsBytes(line_vertices)));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(MakeRasterPipeline(fixture.device, vertex_shader, fragment_shader, k_format,
                                                            {.cull_mode = Face::None}, PrimitiveTopology::Line));
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDraw(2) == ErrorCode::Success);
                                                           });
        // A triangle over the same two vertices would have covered nothing at all; a filled one would have
        // covered far more than a single row.
        for (i32 y = 0; y < k_side; ++y)
        {
            const bool row_has_pixels = [&]
            {
                for (i32 x = 0; x < k_side; ++x)
                {
                    if (IsCovered(pixels, k_side, x, y))
                    {
                        return true;
                    }
                }
                return false;
            }();
            INFO("row " << y);
            REQUIRE(row_has_pixels == (y == k_line_row));
        }
    }
    SECTION("A point topology puts one pixel per vertex")
    {
        const Forge::Shader point_vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_point_source, {.entry_point = "main_point_vertex", .cache = GetShaderCache()}));
        // Three texels no triangle over them would fill, since they are not adjacent.
        const Vector2f first = TexelCentre(k_side, 1, 1);
        const Vector2f second = TexelCentre(k_side, 5, 2);
        const Vector2f third = TexelCentre(k_side, 3, 6);
        const f32 point_vertices[] = {first.x, first.y, second.x, second.y, third.x, third.y};
        const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                     {.size = sizeof(point_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                                     Opal::AsBytes(point_vertices)));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(MakeRasterPipeline(
            fixture.device, point_vertex_shader, fragment_shader, k_format, {.cull_mode = Face::None}, PrimitiveTopology::Point));
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                                                           });
        REQUIRE(CountCovered(pixels, k_side) == 3);
        REQUIRE(IsCovered(pixels, k_side, 1, 1));
        REQUIRE(IsCovered(pixels, k_side, 5, 2));
        REQUIRE(IsCovered(pixels, k_side, 3, 6));
    }
    SECTION("A line strip covers a bend a line list of the same vertices does not")
    {
        // Three vertices bent once: the first two share a row, the last two share a column. A list of three
        // draws only the first pair - the third vertex has no partner - so it covers the row and nothing at
        // the far corner. A strip draws both pairs, which is what reaches the far corner's row as well.
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
        constexpr i32 k_row_a = 2;
        constexpr i32 k_row_b = 6;
        // Strictly between the two rows, so it is covered only by the vertical segment and only when that
        // segment is actually rasterized - not a matter of whether a line rule includes its own endpoint.
        constexpr i32 k_row_between = 4;
        const Vector2f v0 = TexelCentre(k_side, 0, k_row_a);
        const Vector2f v1 = TexelCentre(k_side, k_side - 1, k_row_a);
        const Vector2f v2 = TexelCentre(k_side, k_side - 1, k_row_b);
        const f32 bend_vertices[] = {v0.x, v0.y, v1.x, v1.y, v2.x, v2.y};
        const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                     {.size = sizeof(bend_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                                     Opal::AsBytes(bend_vertices)));

        auto draw_with = [&](PrimitiveTopology topology)
        {
            const Forge::Pipeline pipeline =
                ForgeTest::Unwrap(MakeRasterPipeline(fixture.device, vertex_shader, fragment_shader, k_format, {.cull_mode = Face::None},
                                                     topology));
            Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
            return RenderRaster(fixture, color, k_side,
                                [&](Forge::CommandBuffer& command_buffer)
                                {
                                    REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                    REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                                    REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                                });
        };

        const Opal::DynamicArray<u8> list_pixels = draw_with(PrimitiveTopology::Line);
        REQUIRE_FALSE(IsCovered(list_pixels, k_side, k_side - 1, k_row_between));

        const Opal::DynamicArray<u8> strip_pixels = draw_with(PrimitiveTopology::LineStrip);
        REQUIRE(IsCovered(strip_pixels, k_side, k_side - 1, k_row_between));
    }
    SECTION("A triangle strip fills the quad a triangle list of the same vertices only half covers")
    {
        // Four corners in strip order. A list of four draws only the first three - the fourth has no
        // triangle of its own - which is half the quad; a strip draws both triangles the four corners make,
        // which is the whole of it.
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
        constexpr f32 k_quad_vertices[] = {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
        const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                     {.size = sizeof(k_quad_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                                     Opal::AsBytes(k_quad_vertices)));

        auto draw_with = [&](PrimitiveTopology topology)
        {
            const Forge::Pipeline pipeline =
                ForgeTest::Unwrap(MakeRasterPipeline(fixture.device, vertex_shader, fragment_shader, k_format, {.cull_mode = Face::None},
                                                     topology));
            Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
            return RenderRaster(fixture, color, k_side,
                                [&](Forge::CommandBuffer& command_buffer)
                                {
                                    REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                    REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                                    REQUIRE(command_buffer.CmdDraw(4) == ErrorCode::Success);
                                });
        };

        const i32 list_covered = CountCovered(draw_with(PrimitiveTopology::Triangle), k_side);
        INFO("triangle list covered " << list_covered << " of " << k_side * k_side);
        REQUIRE(list_covered < k_side * k_side);

        const i32 strip_covered = CountCovered(draw_with(PrimitiveTopology::TriangleStrip), k_side);
        REQUIRE(strip_covered == k_side * k_side);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge wide lines", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_wide_lines{.wide_lines = true};
    if (!CanCreateDevice(k_wide_lines))
    {
        SKIP("This device draws lines one pixel wide only.");
    }
    ForgeFixture fixture(k_wide_lines);
    constexpr i32 k_side = 8;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr i32 k_line_row = 4;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));

    // Along the centres of one row, the way the topology case draws it, so a line one pixel wide lands on
    // that row and the width is the only thing that moves the rest.
    const Vector2f left = TexelCentre(k_side, 0, k_line_row);
    const Vector2f right = TexelCentre(k_side, k_side - 1, k_line_row);
    const f32 line_vertices[] = {left.x, left.y, right.x, right.y};
    const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device, {.size = sizeof(line_vertices), .usage = Forge::BufferUsageBits::VertexBuffer}, Opal::AsBytes(line_vertices)));
    const Forge::Pipeline pipeline =
        ForgeTest::Unwrap(MakeRasterPipeline(fixture.device, vertex_shader, fragment_shader, k_format, {.cull_mode = Face::None},
                                             PrimitiveTopology::Line, Forge::DynamicStateBits::LineWidth));

    /** Which rows the line covered when drawn that wide. */
    auto rows_covered = [&](f32 width)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdSetLineWidth(width) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDraw(2) == ErrorCode::Success);
                                                           });
        Opal::DynamicArray<i32> rows;
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                if (IsCovered(pixels, k_side, x, y))
                {
                    rows.PushBack(y);
                    break;
                }
            }
        }
        return rows;
    };

    const Opal::DynamicArray<i32> thin = rows_covered(1.0f);
    REQUIRE(thin.GetSize() == 1);
    REQUIRE(thin[0] == k_line_row);

    // Three pixels wide, which a device with the feature draws as a band centred on the line. How the band
    // is placed is the driver's to decide - the rule covers a rectangle around the segment and the rounding
    // at its edges is not pinned down - so this asks for more rows than the thin line had, all of them
    // within one of it, rather than naming the three.
    const Opal::DynamicArray<i32> wide = rows_covered(3.0f);
    REQUIRE(wide.GetSize() > thin.GetSize());
    for (i32 i = 0; i < wide.GetSize(); ++i)
    {
        INFO("covered row " << wide[i]);
        REQUIRE(wide[i] >= k_line_row - 1);
        REQUIRE(wide[i] <= k_line_row + 1);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge instancing through a second vertex binding", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr i32 k_half = k_side / 2;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_instanced_source, {.entry_point = "main_instanced_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_instanced_source, {.entry_point = "main_instanced_fragment", .cache = GetShaderCache()}));

    // One quad over the top left quarter of the target, as two triangles. Every instance draws this and only
    // this, so where the four end up is entirely what the second binding fed them.
    constexpr f32 k_quarter_quad[] = {-1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f};
    const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                 {.size = sizeof(k_quarter_quad), .usage = Forge::BufferUsageBits::VertexBuffer},
                                 Opal::AsBytes(k_quarter_quad)));

    /** Per instance: where to move the quad, and the value whose bits become its colour. */
    struct InstanceData
    {
        f32 offset_x = 0.0f;
        f32 offset_y = 0.0f;
        u32 value = 0;
    };
    const InstanceData instances[] = {
        {0.0f, 0.0f, 1},  // top left, red
        {1.0f, 0.0f, 2},  // top right, green
        {0.0f, 1.0f, 3},  // bottom left, red and green
        {1.0f, 1.0f, 4},  // bottom right, blue
    };
    const Forge::Buffer instance_buffer = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                        {.size = sizeof(instances), .usage = Forge::BufferUsageBits::VertexBuffer},
                                        Opal::AsBytes(instances)));

    Forge::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.vertex_shader = vertex_shader;
    pipeline_desc.fragment_shader = fragment_shader;
    pipeline_desc.rasterizer.cull_mode = Face::None;
    pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
    REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
    pipeline_desc.vertex_input.AddBinding(1, sizeof(InstanceData), DataRepetition::PerInstance);
    REQUIRE(pipeline_desc.vertex_input.AddAttribute(1, 1, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
    REQUIRE(pipeline_desc.vertex_input.AddAttribute(1, 2, PixelFormat::R32_UINT, 2 * sizeof(f32)) == ErrorCode::Success);
    pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
    pipeline_desc.color_attachment_formats.PushBack(k_format);
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    // Each quarter has to carry the bits of its own instance value, so an instance that read the wrong entry
    // of the second binding shows up as the wrong quarter rather than as a missing one.
    auto require_quadrants = [&](const Opal::DynamicArray<u8>& pixels)
    {
        for (i32 instance = 0; instance < 4; ++instance)
        {
            const u32 value = instances[instance].value;
            const i32 first_x = instances[instance].offset_x == 0.0f ? 0 : k_half;
            const i32 first_y = instances[instance].offset_y == 0.0f ? 0 : k_half;
            for (i32 y = first_y; y < first_y + k_half; ++y)
            {
                for (i32 x = first_x; x < first_x + k_half; ++x)
                {
                    const i32 base = (y * k_side + x) * 4;
                    INFO("instance " << instance << " texel " << x << "," << y);
                    REQUIRE(static_cast<i32>(pixels[base + 0]) == ((value & 1) != 0 ? 255 : 0));
                    REQUIRE(static_cast<i32>(pixels[base + 1]) == ((value & 2) != 0 ? 255 : 0));
                    REQUIRE(static_cast<i32>(pixels[base + 2]) == ((value & 4) != 0 ? 255 : 0));
                }
            }
        }
    };

    SECTION("Through CmdDraw")
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(instance_buffer, 1) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDraw(6, 4) == ErrorCode::Success);
                                                           });
        require_quadrants(pixels);
    }
    SECTION("Through CmdDrawIndexed with an instance count above one")
    {
        // The identity index, so an indexed draw of the same six vertices lands exactly where CmdDraw put
        // them - what is under test is instance_count on the indexed call, not a different shape.
        constexpr u16 k_identity_indices[] = {0, 1, 2, 3, 4, 5};
        const Forge::Buffer indices = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device, {.size = sizeof(k_identity_indices), .usage = Forge::BufferUsageBits::IndexBuffer},
            Opal::AsBytes(k_identity_indices)));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(instance_buffer, 1) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint16) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDrawIndexed(6, 4) == ErrorCode::Success);
                                                           });
        require_quadrants(pixels);
    }
    SECTION("Through CmdDrawIndirect with an instance count above one")
    {
        // One command naming all four instances, host-written since the value under test is a field of the
        // command rather than proof that a device wrote it - that half of indirect drawing has its own case.
        const Forge::DrawIndirectCommand command{.vertex_count = 6, .instance_count = 4, .first_vertex = 0, .first_instance = 0};
        const Forge::Buffer commands = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device, {.size = sizeof(command), .usage = Forge::BufferUsageBits::IndirectBuffer}, Opal::AsBytes(command)));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(instance_buffer, 1) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDrawIndirect(commands, 0, 1) ==
                                                                       ErrorCode::Success);
                                                           });
        require_quadrants(pixels);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge viewport and scissor", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr i32 k_half = k_side / 2;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                 {.size = sizeof(k_fullscreen_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                                 Opal::AsBytes(k_fullscreen_vertices)));
    const Forge::Pipeline pipeline =
        ForgeTest::Unwrap(MakeRasterPipeline(fixture.device, vertex_shader, fragment_shader, k_format, {.cull_mode = Face::None}));

    /** Which half of the target the covered texels are in, as two counts. */
    auto count_halves = [&](const Opal::DynamicArray<u8>& pixels)
    {
        Opal::InPlaceArray<i32, 2> halves{0, 0};
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                halves[x < k_half ? 0 : 1] += IsCovered(pixels, k_side, x, y) ? 1 : 0;
            }
        }
        return halves;
    };

    SECTION("A scissor smaller than the viewport leaves the texels outside it untouched")
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels =
            RenderRaster(fixture, color, k_side,
                         [&](Forge::CommandBuffer& command_buffer)
                         {
                             // The viewport stays the whole target; only the scissor moves, so what the
                             // right half is missing is the scissor and not the transform.
                             REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_half, k_side}) == ErrorCode::Success);
                             REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                             REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                             REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                         });
        const Opal::InPlaceArray<i32, 2> halves = count_halves(pixels);
        INFO("left " << halves[0] << ", right " << halves[1]);
        REQUIRE(halves[0] == k_half * k_side);
        REQUIRE(halves[1] == 0);
    }
    SECTION("A viewport over half the target squeezes the triangle into that half")
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels =
            RenderRaster(fixture, color, k_side,
                         [&](Forge::CommandBuffer& command_buffer)
                         {
                             REQUIRE(command_buffer.CmdSetViewport({static_cast<f32>(k_half), 0.0f},
                                                           {static_cast<f32>(k_half), static_cast<f32>(k_side)}) == ErrorCode::Success);
                             REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                             REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                             REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                         });
        const Opal::InPlaceArray<i32, 2> halves = count_halves(pixels);
        INFO("left " << halves[0] << ", right " << halves[1]);
        REQUIRE(halves[0] == 0);
        REQUIRE(halves[1] == k_half * k_side);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** The fullscreen triangle again, with a z the case picks, since both cases below are about where z lands. */
Opal::DynamicArray<f32> MakeFullscreenTriangleAt(f32 z)
{
    Opal::DynamicArray<f32> vertices(9);
    const f32 positions[] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    for (i32 corner = 0; corner < 3; ++corner)
    {
        vertices[corner * 3 + 0] = positions[corner * 2 + 0];
        vertices[corner * 3 + 1] = positions[corner * 2 + 1];
        vertices[corner * 3 + 2] = z;
    }
    return vertices;
}

/** A pipeline over three-component positions, which is what both depth cases feed it. */
Opal::Expected<Forge::Pipeline, ErrorCode> MakeDepthPipeline(const Forge::Device& device, const Forge::Shader& vertex_shader,
                                                             const Forge::Shader& fragment_shader, PixelFormat color_format,
                                                             PixelFormat depth_format, bool depth_clamp)
{
    Forge::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.vertex_shader = vertex_shader;
    pipeline_desc.fragment_shader = fragment_shader;
    pipeline_desc.rasterizer.cull_mode = Face::None;
    pipeline_desc.rasterizer.depth_clamp = depth_clamp;
    pipeline_desc.vertex_input.AddBinding(0, 3 * sizeof(f32), DataRepetition::PerVertex);
    REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32B32_SFLOAT, 0) == ErrorCode::Success);
    pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
    pipeline_desc.color_attachment_formats.PushBack(color_format);
    if (depth_format != PixelFormat::Undefined)
    {
        // Writes need the test on: Vulkan only writes depth for a fragment that passed it, so a comparator
        // of Always is how a case that is not about the comparator still fills the buffer.
        pipeline_desc.depth_stencil.depth_test_enabled = true;
        pipeline_desc.depth_stencil.depth_write_enabled = true;
        pipeline_desc.depth_stencil.depth_comparator = Comparator::Always;
        pipeline_desc.depth_attachment_format = depth_format;
    }
    return Forge::Pipeline::Create(device, pipeline_desc);
}

}  // namespace

TEST_CASE("Forge viewport depth range", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_color_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr PixelFormat k_depth_format = PixelFormat::D32_SFLOAT;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_depth_position_source, {.entry_point = "main_depth_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_depth_position_source, {.entry_point = "main_depth_fragment", .cache = GetShaderCache()}));
    const Forge::Pipeline pipeline =
        ForgeTest::Unwrap(MakeDepthPipeline(fixture.device, vertex_shader, fragment_shader, k_color_format, k_depth_format, false));

    // A z of zero, so the depth that gets written is min_depth itself and the mapping is readable off the
    // result rather than having to be undone. A z of one half would land on the same number either way.
    const Opal::DynamicArray<f32> triangle = MakeFullscreenTriangleAt(0.0f);
    const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                 {.size = triangle.GetSize() * sizeof(f32), .usage = Forge::BufferUsageBits::VertexBuffer},
                                 {reinterpret_cast<const u8*>(triangle.GetData()), triangle.GetSize() * sizeof(f32)}));

    // Render once through the given depth range and hand back what the depth buffer holds.
    auto depth_through_range = [&](f32 min_depth, f32 max_depth)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        Forge::Texture depth = MakeDepthTarget(fixture.device, k_side, k_depth_format);

        return RenderWithDepth(fixture, color, depth, k_side, Vector4f{1.0f, 0.0f, 0.0f, 1.0f}, Forge::DepthStencilClearValue{1.0f, 0},
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   // Overrides the whole-target default RenderWithDepth already set, which
                                   // is what lets this case ask for a depth range other than [0, 1].
                                   REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}, min_depth, max_depth) ==
                                           ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                               })
            .depths;
    };

    SECTION("The whole range maps a z of zero onto zero")
    {
        const Opal::DynamicArray<f32> values = depth_through_range(0.0f, 1.0f);
        for (i32 i = 0; i < values.GetSize(); ++i)
        {
            INFO("texel " << i << " depth " << values[i]);
            REQUIRE(values[i] == Catch::Approx(0.0f).margin(0.001));
        }
    }
    SECTION("A narrowed range maps the same z onto the near end of it")
    {
        // The mapping is min + z * (max - min), so a z of zero lands exactly on min_depth. A viewport that
        // ignored the range would still be writing zero here, which is what makes this readable.
        const Opal::DynamicArray<f32> values = depth_through_range(0.25f, 0.75f);
        for (i32 i = 0; i < values.GetSize(); ++i)
        {
            INFO("texel " << i << " depth " << values[i]);
            REQUIRE(values[i] == Catch::Approx(0.25f).margin(0.001));
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge depth clamp", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const bool has_depth_clamp = CanCreateDevice({.depth_clamp = true});
    ForgeFixture fixture({.depth_clamp = has_depth_clamp});
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_depth_position_source, {.entry_point = "main_depth_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_depth_position_source, {.entry_point = "main_depth_fragment", .cache = GetShaderCache()}));

    // Past the far plane, which is the whole point: without clamping the triangle is clipped away, and with
    // it the fragments are flattened onto the plane and drawn.
    const Opal::DynamicArray<f32> triangle = MakeFullscreenTriangleAt(1.5f);
    const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                 {.size = triangle.GetSize() * sizeof(f32), .usage = Forge::BufferUsageBits::VertexBuffer},
                                 {reinterpret_cast<const u8*>(triangle.GetData()), triangle.GetSize() * sizeof(f32)}));

    auto is_drawn = [&](bool depth_clamp)
    {
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(MakeDepthPipeline(fixture.device, vertex_shader, fragment_shader, k_format,
                                                           PixelFormat::Undefined, depth_clamp));
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                                                           });
        return CountCovered(pixels, k_side) == k_side * k_side;
    };

    SECTION("Geometry past the far plane is clipped away without clamping")
    {
        REQUIRE_FALSE(is_drawn(false));
    }
    SECTION("Clamping draws it instead of cutting it away")
    {
        INFO("depth_clamp supported: " << has_depth_clamp);
        if (!has_depth_clamp)
        {
            SKIP("This device cannot clamp depth.");
        }
        REQUIRE(is_drawn(true));
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge depth clamping on a device without the feature is refused", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture({.depth_clamp = false});
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_depth_position_source, {.entry_point = "main_depth_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_depth_position_source, {.entry_point = "main_depth_fragment", .cache = GetShaderCache()}));
    REQUIRE(MakeDepthPipeline(fixture.device, vertex_shader, fragment_shader, k_format, PixelFormat::Undefined, true).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * Depth, stencil and blending: the three pieces of fixed-function state a pipeline carries that nothing had
 * ever switched on. Each case draws twice and reads back the colour, because what all three decide is which
 * of two fragments reaches the attachment.
 */
namespace
{

/** A colour handed in as a push constant, so two draws can differ by nothing but what they write. */
constexpr const char* k_pushed_color_source = R"(
struct ColorPush
{
    float4 color;
};
[[vk::push_constant]] ColorPush push;

[shader("vertex")]
float4 main_color_vertex(float3 position : POSITION) : SV_Position
{
    return float4(position, 1.0);
}

[shader("fragment")]
float4 main_color_fragment() : SV_Target
{
    return push.color;
}
)";

/** Two triangles covering the whole target at one depth, which is what every draw below is. */
Opal::DynamicArray<f32> MakeFullTargetQuad(f32 z)
{
    const f32 corners[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
    Opal::DynamicArray<f32> vertices(18);
    for (i32 corner = 0; corner < 6; ++corner)
    {
        vertices[corner * 3 + 0] = corners[corner * 2 + 0];
        vertices[corner * 3 + 1] = corners[corner * 2 + 1];
        vertices[corner * 3 + 2] = z;
    }
    return vertices;
}

/** The same, over the left half only, for the draw that writes a stencil mask into part of the target. */
Opal::DynamicArray<f32> MakeLeftHalfQuad(f32 z)
{
    const f32 corners[] = {-1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f};
    Opal::DynamicArray<f32> vertices(18);
    for (i32 corner = 0; corner < 6; ++corner)
    {
        vertices[corner * 3 + 0] = corners[corner * 2 + 0];
        vertices[corner * 3 + 1] = corners[corner * 2 + 1];
        vertices[corner * 3 + 2] = z;
    }
    return vertices;
}

Forge::Buffer MakeQuadBuffer(const Forge::Device& device, const Opal::DynamicArray<f32>& vertices)
{
    return ForgeTest::Unwrap(
        Forge::Buffer::Create(device, {.size = vertices.GetSize() * sizeof(f32), .usage = Forge::BufferUsageBits::VertexBuffer},
                              {reinterpret_cast<const u8*>(vertices.GetData()), vertices.GetSize() * sizeof(f32)}));
}

/** A colour as the shader wants it, built from the byte values a UNORM target round trips exactly. */
Vector4f ByteColor(i32 r, i32 g, i32 b, i32 a)
{
    return {static_cast<f32>(r) / 255.0f, static_cast<f32>(g) / 255.0f, static_cast<f32>(b) / 255.0f,
            static_cast<f32>(a) / 255.0f};
}

/** Everything a depth, stencil or blend pipeline shares, so each case only spells out what it varies. */
Forge::GraphicsPipelineDesc MakePushedColorPipelineDesc(const Forge::Shader& vertex_shader, const Forge::Shader& fragment_shader,
                                                        PixelFormat color_format)
{
    Forge::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.vertex_shader = vertex_shader;
    pipeline_desc.fragment_shader = fragment_shader;
    pipeline_desc.rasterizer.cull_mode = Face::None;
    pipeline_desc.vertex_input.AddBinding(0, 3 * sizeof(f32), DataRepetition::PerVertex);
    REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32B32_SFLOAT, 0) == ErrorCode::Success);
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Fragment, .offset = 0, .size = sizeof(Vector4f)});
    pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
    pipeline_desc.color_attachment_formats.PushBack(color_format);
    return pipeline_desc;
}

}  // namespace

namespace
{

/**
 * One push constant block read by both stages: the vertex stage shifts the geometry by the first half of it
 * and the fragment stage colours it with the second. What comes back therefore says which half of the block
 * arrived, and the two halves are pushed together or one at a time depending on the section.
 */
constexpr const char* k_two_stage_push_source = R"(
struct StagePush
{
    float2 shift;
    float2 padding;
    float4 color;
};
[[vk::push_constant]] StagePush push;

[shader("vertex")]
float4 main_push_vertex(float3 position : POSITION) : SV_Position
{
    return float4(position.xy + push.shift, position.z, 1.0);
}

[shader("fragment")]
float4 main_push_fragment() : SV_Target
{
    return push.color;
}
)";

/** What that block holds, laid out the way the shader reads it. */
struct StagePush
{
    Vector2f shift{0.0f, 0.0f};
    Vector2f padding{0.0f, 0.0f};
    Vector4f color = Vector4f{0.0f, 0.0f, 0.0f, 1.0f};
};

}  // namespace

TEST_CASE("Forge push constants read by two stages", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr ShaderTypeBits k_both_stages = ShaderTypeBits::Vertex | ShaderTypeBits::Fragment;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_two_stage_push_source, {.entry_point = "main_push_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_two_stage_push_source, {.entry_point = "main_push_fragment", .cache = GetShaderCache()}));

    // Built from reflection rather than written out: both stages declare the block, which is what makes the
    // merge worth asking about.
    const Opal::Ref<const Forge::Shader> shaders[] = {vertex_shader, fragment_shader};
    const Opal::DynamicArray<Forge::PushConstantRange> ranges = Forge::PushConstantRangesFromShaders({shaders, 2});
    REQUIRE(ranges.GetSize() == 1);
    INFO("range offset " << ranges[0].offset << " size " << ranges[0].size);
    REQUIRE(ranges[0].offset == 0);
    REQUIRE(ranges[0].size == sizeof(StagePush));
    REQUIRE(!!(ranges[0].shader_stages & ShaderTypeBits::Vertex));
    REQUIRE(!!(ranges[0].shader_stages & ShaderTypeBits::Fragment));

    Forge::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.vertex_shader = vertex_shader;
    pipeline_desc.fragment_shader = fragment_shader;
    pipeline_desc.rasterizer.cull_mode = Face::None;
    pipeline_desc.vertex_input.AddBinding(0, 3 * sizeof(f32), DataRepetition::PerVertex);
    REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32B32_SFLOAT, 0) == ErrorCode::Success);
    pipeline_desc.push_constant_ranges.PushBack(ranges[0]);
    pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
    pipeline_desc.color_attachment_formats.PushBack(k_format);
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    // The left half of the target, which the shift in the block moves to the right half when it is read.
    const Forge::Buffer quad = MakeQuadBuffer(fixture.device, MakeLeftHalfQuad(0.0f));

    /** Draw the quad once, pushing the block however the section says, and hand back the target. */
    auto draw_pushing = [&](auto&& push_the_block)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        return RenderRaster(
            fixture, color, k_side,
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                push_the_block(command_buffer);
                REQUIRE(command_buffer.CmdBindVertexBuffer(quad, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
            },
            Vector4f{0.0f, 0.0f, 0.0f, 1.0f});
    };

    // llvmpipe on Mesa 25.2 hands the fragment stage zeros for a push constant it declares past offset zero,
    // while the vertex stage reads the same offset correctly and the fragment stage reads offset zero
    // correctly. CI's Windows lavapipe (Mesa 24.3) and hardware drivers read it right, and Forge forwards the
    // push to vkCmdPushConstants unchanged. The zeros are measured rather than the version read off the device,
    // so a Mesa that fixes it gets the coverage back: a quad drawn with a transparent black nobody pushed is
    // the bug and nothing else, since the clear and every colour pushed here are opaque. Only on a software
    // device, so that the same picture from hardware still fails.
    auto fragment_read_zeros = [&](const Opal::DynamicArray<u8>& pixels, i32 x, i32 y)
    { return IsSoftwareDevice() && GetTexel(pixels, x, y) == Texel{0, 0, 0, 0}; };

    SECTION("One push feeds the stage that reads each half of the block")
    {
        const StagePush block{.shift = {0.0f, 0.0f}, .color = ByteColor(0, 255, 0, 255)};
        const Opal::DynamicArray<u8> pixels = draw_pushing(
            [&](Forge::CommandBuffer& command_buffer)
            { REQUIRE(command_buffer.CmdPushConstants(pipeline, k_both_stages, Opal::AsBytes(block)) == ErrorCode::Success); });
        if (fragment_read_zeros(pixels, 0, 0))
        {
            SKIP("This software driver reads a push constant the fragment stage declares past offset zero as zero.");
        }
        // The quad stayed where it was, in the colour the fragment stage read out of the same block.
        REQUIRE(GetTexel(pixels, 0, 0) == Texel{0, 255, 0, 255});
        REQUIRE(GetTexel(pixels, 1, 3) == Texel{0, 255, 0, 255});
        REQUIRE(GetTexel(pixels, 2, 0) == Texel{0, 0, 0, 255});
        REQUIRE(GetTexel(pixels, 3, 3) == Texel{0, 0, 0, 255});
    }
    SECTION("Each half of the block is written at the offset it sits at")
    {
        // Two calls instead of one, the second at a non-zero offset. The shift moves the quad to the right
        // half, so a colour written at the wrong offset would land in the shift and move it somewhere else
        // again - the two halves cannot be confused without the picture changing.
        const Vector2f shift[2] = {{1.0f, 0.0f}, {0.0f, 0.0f}};
        const Vector4f color = ByteColor(255, 0, 0, 255);
        const Opal::DynamicArray<u8> pixels = draw_pushing(
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdPushConstants(pipeline, k_both_stages, Opal::AsBytes(shift), 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(pipeline, k_both_stages, Opal::AsBytes(color),
                                                        static_cast<u32>(offsetof(StagePush, color))) == ErrorCode::Success);
            });
        if (fragment_read_zeros(pixels, 2, 0))
        {
            SKIP("This software driver reads a push constant the fragment stage declares past offset zero as zero.");
        }
        REQUIRE(GetTexel(pixels, 0, 0) == Texel{0, 0, 0, 255});
        REQUIRE(GetTexel(pixels, 1, 3) == Texel{0, 0, 0, 255});
        REQUIRE(GetTexel(pixels, 2, 0) == Texel{255, 0, 0, 255});
        REQUIRE(GetTexel(pixels, 3, 3) == Texel{255, 0, 0, 255});
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge attachment load and store operations", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_color_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Pipeline pipeline =
        ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_color_format)));

    const Forge::Buffer left_quad = MakeQuadBuffer(fixture.device, MakeLeftHalfQuad(0.0f));
    const Forge::Buffer full_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.0f));
    const Vector4f red = ByteColor(255, 0, 0, 255);
    const Vector4f green = ByteColor(0, 255, 0, 255);

    /** What one pass does: how it treats what is there, what it draws, and whether it keeps the result. */
    struct Pass
    {
        Forge::AttachmentLoadOperation load_operation = Forge::AttachmentLoadOperation::Clear;
        Forge::AttachmentStoreOperation store_operation = Forge::AttachmentStoreOperation::Store;
        Vector4f clear_value = Vector4f{0.0f, 0.0f, 1.0f, 1.0f};
        /** Null for a pass that only clears, which is what makes the load operation the only thing writing. */
        const Forge::Buffer* quad = nullptr;
        Vector4f draw_color = Vector4f{0.0f, 0.0f, 0.0f, 1.0f};
    };

    /** Run the two passes over one attachment, in order, and hand back what the second one left. */
    auto render_two_passes = [&](const Pass& first, const Pass& second)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        auto record_pass = [&](Forge::CommandBuffer& command_buffer, const Pass& pass)
        {
            const Forge::RenderingDesc rendering_desc{
                .render_area_extent = {k_side, k_side},
                .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                     .load_operation = pass.load_operation,
                                                                     .store_operation = pass.store_operation,
                                                                     .clear_value = pass.clear_value}}};
            REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
            if (pass.quad != nullptr)
            {
                REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindVertexBuffer(*pass.quad, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(pass.draw_color)) ==
                        ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
            }
            REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
        };

        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                        record_pass(command_buffer, first);
                        // Between the two passes, since the second one reads and writes what the first wrote
                        // and the layout it sits in is the same on both sides.
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier{
                                    .stages_must_finish = Forge::PipelineStageBits::ColorAttachmentOutput,
                                    .stages_must_finish_access = Forge::PipelineStageAccessBits::ColorAttachmentWrite,
                                    .before_stages_start = Forge::PipelineStageBits::ColorAttachmentOutput,
                                    .before_stages_start_access = Forge::PipelineStageAccessBits::ColorAttachmentRead |
                                                                  Forge::PipelineStageAccessBits::ColorAttachmentWrite,
                                    .old_layout = Forge::ImageLayout::ColorAttachment,
                                    .new_layout = Forge::ImageLayout::ColorAttachment,
                                    .texture = color}) == ErrorCode::Success);
                        record_pass(command_buffer, second);
                    }) == ErrorCode::Success);

        return ReadColorPixels(fixture, color, k_side);
    };

    SECTION("A pass that loads keeps what the one before it stored")
    {
        // The first pass clears the whole target to red and stores it; the second loads that and draws green
        // over the left half only. The right half is the first pass's colour seen through the second one.
        const Opal::DynamicArray<u8> pixels =
            render_two_passes({.load_operation = Forge::AttachmentLoadOperation::Clear, .clear_value = red},
                              {.load_operation = Forge::AttachmentLoadOperation::Load, .quad = &left_quad, .draw_color = green});
        REQUIRE(GetTexel(pixels, 0, 0) == Texel{0, 255, 0, 255});
        REQUIRE(GetTexel(pixels, 1, 2) == Texel{0, 255, 0, 255});
        REQUIRE(GetTexel(pixels, 2, 0) == Texel{255, 0, 0, 255});
        REQUIRE(GetTexel(pixels, 3, 3) == Texel{255, 0, 0, 255});
    }
    SECTION("A pass that loads nothing shows only what it drew")
    {
        // DontCare on the load: whatever the first pass left is gone, and the draw covers the whole target,
        // so every texel is the second pass's colour and none of them is the first pass's red.
        const Opal::DynamicArray<u8> pixels =
            render_two_passes({.load_operation = Forge::AttachmentLoadOperation::Clear, .clear_value = red},
                              {.load_operation = Forge::AttachmentLoadOperation::DontCare, .quad = &full_quad, .draw_color = green});
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                INFO("texel " << x << "," << y);
                REQUIRE(GetTexel(pixels, x, y) == Texel{0, 255, 0, 255});
            }
        }
    }
    SECTION("A pass whose result is not stored leaves the next one to write the attachment")
    {
        // What DontCare on the store leaves behind is undefined by definition, so nothing here asserts on it:
        // what is checked is that the second pass, which clears and draws over the whole target, ends up with
        // exactly what it wrote.
        const Opal::DynamicArray<u8> pixels =
            render_two_passes({.load_operation = Forge::AttachmentLoadOperation::Clear,
                               .store_operation = Forge::AttachmentStoreOperation::DontCare,
                               .clear_value = red},
                              {.load_operation = Forge::AttachmentLoadOperation::Clear,
                               .clear_value = Vector4f{0.0f, 0.0f, 1.0f, 1.0f},
                               .quad = &full_quad,
                               .draw_color = green});
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                INFO("texel " << x << "," << y);
                REQUIRE(GetTexel(pixels, x, y) == Texel{0, 255, 0, 255});
            }
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge depth testing", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_color_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr PixelFormat k_depth_format = PixelFormat::D32_SFLOAT;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));

    // Near is drawn first and far second, so a pass with no working depth test would end on the far colour
    // whatever the comparator said - which is what makes the two answers below tell them apart.
    const Forge::Buffer near_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.25f));
    const Forge::Buffer far_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.75f));
    const Vector4f near_color = ByteColor(0, 255, 0, 255);
    const Vector4f far_color = ByteColor(255, 0, 0, 255);

    auto make_pipeline = [&](Comparator comparator, bool depth_write)
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_color_format);
        pipeline_desc.depth_stencil.depth_test_enabled = true;
        pipeline_desc.depth_stencil.depth_write_enabled = depth_write;
        pipeline_desc.depth_stencil.depth_comparator = comparator;
        pipeline_desc.depth_attachment_format = k_depth_format;
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    };

    /** Draw the near quad then the far one and hand back both the colour and the depth that survived. */
    auto draw_both = [&](Comparator comparator, bool depth_write, f32 clear_depth)
    {
        const Forge::Pipeline pipeline = make_pipeline(comparator, depth_write);
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        Forge::Texture depth = MakeDepthTarget(fixture.device, k_side, k_depth_format);

        // Cleared to whichever end of the range the comparator counts as furthest, so the first draw passes
        // either way. Clearing to one under Greater would reject both.
        return RenderWithDepth(fixture, color, depth, k_side, Vector4f{0.0f, 0.0f, 1.0f, 1.0f},
                               Forge::DepthStencilClearValue{clear_depth, 0},
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindVertexBuffer(near_quad, 0) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(near_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindVertexBuffer(far_quad, 0) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(far_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                               });
    };

    SECTION("The nearer fragment is the one that survives")
    {
        const auto result = draw_both(Comparator::Less, true, 1.0f);
        for (i32 i = 0; i < k_side * k_side; ++i)
        {
            INFO("texel " << i);
            // Green, which the near draw wrote first and the far draw was rejected over.
            REQUIRE(static_cast<i32>(result.pixels[i * 4 + 0]) == 0);
            REQUIRE(static_cast<i32>(result.pixels[i * 4 + 1]) == 255);
            REQUIRE(result.depths[i] == Catch::Approx(0.25f).margin(0.001));
        }
    }
    SECTION("Flipping the comparator flips which one survives")
    {
        // Same two draws in the same order. Only the comparator moved, so the far quad passing is the test
        // running rather than the depth buffer being ignored.
        const auto result = draw_both(Comparator::Greater, true, 0.0f);
        for (i32 i = 0; i < k_side * k_side; ++i)
        {
            INFO("texel " << i);
            REQUIRE(static_cast<i32>(result.pixels[i * 4 + 0]) == 255);
            REQUIRE(static_cast<i32>(result.pixels[i * 4 + 1]) == 0);
            REQUIRE(result.depths[i] == Catch::Approx(0.75f).margin(0.001));
        }
    }
    SECTION("Depth writes turned off leave the buffer as the clear left it")
    {
        // The buffer stays at the clear, which is the assertion. The colour follows from it: with nothing
        // ever written, the far quad is still compared against one and still passes, so it lands on top -
        // the opposite of the first case, and only because the write was what rejected it there.
        const auto result = draw_both(Comparator::Less, false, 1.0f);
        for (i32 i = 0; i < k_side * k_side; ++i)
        {
            INFO("texel " << i);
            REQUIRE(result.depths[i] == Catch::Approx(1.0f).margin(0.001));
            REQUIRE(static_cast<i32>(result.pixels[i * 4 + 0]) == 255);
            REQUIRE(static_cast<i32>(result.pixels[i * 4 + 1]) == 0);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** The depth format the bias cases render into, and the one depth every draw below starts from. */
constexpr PixelFormat k_bias_depth_format = PixelFormat::D32_SFLOAT;
constexpr f32 k_bias_quad_depth = 0.5f;

/**
 * A constant factor big enough that the bias it produces is worth measuring. Vulkan scales it by the
 * smallest difference the depth format resolves near the primitive, which for a float depth around a half is
 * about six times ten to the minus eight - so a factor of a hundred thousand moves the depth by a few
 * thousandths and a factor of one would move it by nothing a test could see.
 */
constexpr f32 k_bias_constant_factor = 100000.0f;

}  // namespace

TEST_CASE("Forge depth bias", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_color_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(k_bias_quad_depth));
    const Vector4f draw_color = ByteColor(0, 255, 0, 255);

    /** A pipeline that always writes depth, with whatever bias state the case is about. */
    auto make_pipeline = [&](const Forge::RasterizerDesc& rasterizer, Forge::DynamicStateBits dynamic_state)
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_color_format);
        pipeline_desc.rasterizer = rasterizer;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.dynamic_state = dynamic_state;
        pipeline_desc.depth_stencil.depth_test_enabled = true;
        pipeline_desc.depth_stencil.depth_write_enabled = true;
        pipeline_desc.depth_stencil.depth_comparator = Comparator::Always;
        pipeline_desc.depth_attachment_format = k_bias_depth_format;
        return Forge::Pipeline::Create(fixture.device, pipeline_desc);
    };

    /**
     * Draw the quad once and hand back the depth it left. The quad is flat and faces the viewer, so its
     * slope is zero and the constant factor is the only term of the bias that can move anything.
     */
    auto depth_after_draw = [&](const Forge::Pipeline& pipeline, auto&& before_draw)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        Forge::Texture depth = MakeDepthTarget(fixture.device, k_side, k_bias_depth_format);
        const DepthPassResult result =
            RenderWithDepth(fixture, color, depth, k_side, Vector4f{0.0f, 0.0f, 1.0f, 1.0f}, Forge::DepthStencilClearValue{1.0f, 0},
                            [&](Forge::CommandBuffer& command_buffer)
                            {
                                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                before_draw(command_buffer);
                                REQUIRE(command_buffer.CmdBindVertexBuffer(quad, 0) == ErrorCode::Success);
                                REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(draw_color)) ==
                                        ErrorCode::Success);
                                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                            });
        return result.depths[0];
    };

    auto no_setup = [](Forge::CommandBuffer&) {};

    SECTION("A quad drawn without bias lands at the depth it was given")
    {
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(make_pipeline({}, Forge::DynamicStateBits::None));
        REQUIRE(depth_after_draw(pipeline, no_setup) == Catch::Approx(k_bias_quad_depth).margin(0.0001));
    }
    SECTION("A constant bias moves the depth in the direction of its sign")
    {
        const Forge::Pipeline pushed_back = ForgeTest::Unwrap(make_pipeline(
            {.depth_bias_enabled = true, .depth_bias_constant_factor = k_bias_constant_factor}, Forge::DynamicStateBits::None));
        const f32 further = depth_after_draw(pushed_back, no_setup);
        INFO("depth with a positive bias " << further);
        REQUIRE(further > k_bias_quad_depth + 0.0005f);
        REQUIRE(further < 1.0f);

        const Forge::Pipeline pulled_forward = ForgeTest::Unwrap(make_pipeline(
            {.depth_bias_enabled = true, .depth_bias_constant_factor = -k_bias_constant_factor}, Forge::DynamicStateBits::None));
        const f32 nearer = depth_after_draw(pulled_forward, no_setup);
        INFO("depth with a negative bias " << nearer);
        REQUIRE(nearer < k_bias_quad_depth - 0.0005f);
        REQUIRE(nearer > 0.0f);
    }
    SECTION("A pipeline that leaves the bias dynamic takes it from the command")
    {
        // The desc carries no factor at all, so a depth that moved is the command's doing and nothing else.
        const Forge::Pipeline pipeline =
            ForgeTest::Unwrap(make_pipeline({.depth_bias_enabled = true}, Forge::DynamicStateBits::DepthBias));
        const f32 unbiased = depth_after_draw(pipeline, [](Forge::CommandBuffer& command_buffer)
                                              { REQUIRE(command_buffer.CmdSetDepthBias(0.0f) == ErrorCode::Success); });
        INFO("depth with a dynamic bias of zero " << unbiased);
        REQUIRE(unbiased == Catch::Approx(k_bias_quad_depth).margin(0.0001));

        const f32 biased = depth_after_draw(pipeline,
                                            [](Forge::CommandBuffer& command_buffer) {
                                                REQUIRE(command_buffer.CmdSetDepthBias(k_bias_constant_factor) == ErrorCode::Success);
                                            });
        INFO("depth with a dynamic bias " << biased);
        REQUIRE(biased > k_bias_quad_depth + 0.0005f);
    }
    SECTION("A bias clamp on a device without the feature is refused")
    {
        // The static counterpart of the CmdSetDepthBias guard: the fixture asked for no features, and a
        // non-zero clamp is one.
        REQUIRE_FALSE(fixture.device.GetFeatures().depth_bias_clamp);
        REQUIRE(make_pipeline({.depth_bias_enabled = true,
                                     .depth_bias_constant_factor = k_bias_constant_factor,
                                     .depth_bias_clamp = 0.001f},
                                    Forge::DynamicStateBits::None)
                          .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a clamped depth bias", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_bias_clamp{.depth_bias_clamp = true};
    if (!CanCreateDevice(k_bias_clamp))
    {
        SKIP("This device cannot clamp the depth bias.");
    }
    ForgeFixture fixture(k_bias_clamp);
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_color_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr f32 k_clamp = 0.002f;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(k_bias_quad_depth));
    const Vector4f draw_color = ByteColor(0, 255, 0, 255);

    /** The same draw as the case above, with the clamp the pipeline was built with. */
    auto depth_with_clamp = [&](f32 clamp)
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_color_format);
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.rasterizer.depth_bias_enabled = true;
        // Far more bias than the clamp allows, so what comes back is the clamp rather than the factor.
        pipeline_desc.rasterizer.depth_bias_constant_factor = 10.0f * k_bias_constant_factor;
        pipeline_desc.rasterizer.depth_bias_clamp = clamp;
        pipeline_desc.depth_stencil.depth_test_enabled = true;
        pipeline_desc.depth_stencil.depth_write_enabled = true;
        pipeline_desc.depth_stencil.depth_comparator = Comparator::Always;
        pipeline_desc.depth_attachment_format = k_bias_depth_format;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        Forge::Texture depth = MakeDepthTarget(fixture.device, k_side, k_bias_depth_format);
        const DepthPassResult result =
            RenderWithDepth(fixture, color, depth, k_side, Vector4f{0.0f, 0.0f, 1.0f, 1.0f}, Forge::DepthStencilClearValue{1.0f, 0},
                            [&](Forge::CommandBuffer& command_buffer)
                            {
                                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                REQUIRE(command_buffer.CmdBindVertexBuffer(quad, 0) == ErrorCode::Success);
                                REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(draw_color)) ==
                                        ErrorCode::Success);
                                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                            });
        return result.depths[0];
    };

    // Unclamped, the bias is large enough to push the quad to the far end of the range. The clamp is the
    // largest bias allowed, so the same draw through it stops a couple of thousandths past where it started.
    const f32 unclamped = depth_with_clamp(0.0f);
    INFO("depth with no clamp " << unclamped);
    const f32 clamped = depth_with_clamp(k_clamp);
    INFO("depth with a clamp of " << k_clamp << " is " << clamped);
    REQUIRE(clamped < unclamped);
    REQUIRE(clamped > k_bias_quad_depth);
    REQUIRE(clamped <= k_bias_quad_depth + k_clamp + 0.0001f);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** The full-target quad tilted in depth along x: `z_left` along the left edge, `z_right` along the right, flat in y. */
Opal::DynamicArray<f32> MakeTiltedQuad(f32 z_left, f32 z_right)
{
    Opal::DynamicArray<f32> vertices = MakeFullTargetQuad(0.0f);
    for (i32 corner = 0; corner < 6; ++corner)
    {
        vertices[corner * 3 + 2] = vertices[corner * 3 + 0] < 0.0f ? z_left : z_right;
    }
    return vertices;
}

}  // namespace

/**
 * The slope term of the depth bias, which the flat quads above can never move: their slope is zero, so
 * RasterizerDesc::depth_bias_slope_factor and the third argument of CmdSetDepthBias could be dropped or
 * exchanged with another factor and nothing would notice.
 *
 * The specification gives the bias as o = m * slope_factor + r * constant_factor, where m is the largest
 * slope of the depth in framebuffer coordinates - either sqrt((dz/dx)^2 + (dz/dy)^2) or the approximation
 * max(|dz/dx|, |dz/dy|), whichever the implementation picks. A quad tilted along x alone has dz/dy of zero,
 * where the two agree, so m is exactly the change in depth across the quad divided by its width in texels.
 * With no constant factor the bias is m times the slope factor, and it is the same at every texel of the
 * primitive: two texels far apart both moving by that one amount is what separates it from a bias that grew
 * with depth or with position.
 */
TEST_CASE("Forge the slope factor of the depth bias", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 8;
    constexpr PixelFormat k_color_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr f32 k_z_left = 0.25f;
    constexpr f32 k_z_right = 0.75f;
    // Depth per texel along x. The quad spans the whole target, so the change from edge to edge is spread
    // over k_side texels.
    constexpr f32 k_slope = (k_z_right - k_z_left) / static_cast<f32>(k_side);
    // Picked so the bias, k_slope times this, is an eighth of the way across the quad's range: well clear of
    // any rounding, and small enough that neither end is pushed out of [0, 1].
    constexpr f32 k_slope_factor = 0.5f;
    // Two texels in the same row, one near each edge, so their depths without bias differ by most of the tilt.
    constexpr i32 k_row = 3;
    constexpr i32 k_columns[] = {1, 6};

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer tilted_quad = MakeQuadBuffer(fixture.device, MakeTiltedQuad(k_z_left, k_z_right));
    const Forge::Buffer flat_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(k_bias_quad_depth));
    const Vector4f draw_color = ByteColor(0, 255, 0, 255);

    /** The depth the tilted quad is interpolated to at a texel's centre, before any bias. */
    auto unbiased_depth = [&](i32 column) { return k_z_left + k_slope * (static_cast<f32>(column) + 0.5f); };

    /** The same pipeline as the constant bias case above, over a target k_side texels wide. */
    auto make_pipeline = [&](const Forge::RasterizerDesc& rasterizer, Forge::DynamicStateBits dynamic_state)
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_color_format);
        pipeline_desc.rasterizer = rasterizer;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.dynamic_state = dynamic_state;
        pipeline_desc.depth_stencil.depth_test_enabled = true;
        pipeline_desc.depth_stencil.depth_write_enabled = true;
        pipeline_desc.depth_stencil.depth_comparator = Comparator::Always;
        pipeline_desc.depth_attachment_format = k_bias_depth_format;
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    };

    /** Draw `quad` once and hand back every depth it left, row by row. */
    auto depths_after_draw = [&](const Forge::Pipeline& pipeline, const Forge::Buffer& quad, auto&& before_draw)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        Forge::Texture depth = MakeDepthTarget(fixture.device, k_side, k_bias_depth_format);
        DepthPassResult result =
            RenderWithDepth(fixture, color, depth, k_side, Vector4f{0.0f, 0.0f, 1.0f, 1.0f}, Forge::DepthStencilClearValue{1.0f, 0},
                            [&](Forge::CommandBuffer& command_buffer)
                            {
                                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                before_draw(command_buffer);
                                REQUIRE(command_buffer.CmdBindVertexBuffer(quad, 0) == ErrorCode::Success);
                                REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(draw_color)) ==
                                        ErrorCode::Success);
                                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                            });
        return std::move(result.depths);
    };

    auto no_setup = [](Forge::CommandBuffer&) {};

    /** Both texels moved by `bias` from where the tilt alone puts them. */
    auto require_biased_by = [&](const Opal::DynamicArray<f32>& depths, f32 bias)
    {
        for (const i32 column : k_columns)
        {
            const f32 depth = depths[k_row * k_side + column];
            INFO("column " << column << " depth " << depth << " expected " << unbiased_depth(column) + bias);
            REQUIRE(depth == Catch::Approx(unbiased_depth(column) + bias).margin(0.0005));
        }
    };

    SECTION("The tilted quad without bias lands where the interpolation puts it")
    {
        // What every expectation below is measured from, and the proof the tilt is the one the slope assumes.
        const Forge::Pipeline pipeline = make_pipeline({}, Forge::DynamicStateBits::None);
        require_biased_by(depths_after_draw(pipeline, tilted_quad, no_setup), 0.0f);
    }
    SECTION("A slope factor moves a tilted quad by the slope times the factor, in the direction of its sign")
    {
        const Forge::Pipeline pushed_back = make_pipeline(
            {.depth_bias_enabled = true, .depth_bias_slope_factor = k_slope_factor}, Forge::DynamicStateBits::None);
        require_biased_by(depths_after_draw(pushed_back, tilted_quad, no_setup), k_slope * k_slope_factor);

        const Forge::Pipeline pulled_forward = make_pipeline(
            {.depth_bias_enabled = true, .depth_bias_slope_factor = -k_slope_factor}, Forge::DynamicStateBits::None);
        require_biased_by(depths_after_draw(pulled_forward, tilted_quad, no_setup), -k_slope * k_slope_factor);
    }
    SECTION("A slope factor leaves a flat quad where it was")
    {
        // The other half of what makes it a slope term: the same factor that moved the tilted quad has
        // nothing to multiply here. A factor applied as a constant would move this one as well.
        const Forge::Pipeline pipeline = make_pipeline(
            {.depth_bias_enabled = true, .depth_bias_slope_factor = k_slope_factor}, Forge::DynamicStateBits::None);
        const Opal::DynamicArray<f32> depths = depths_after_draw(pipeline, flat_quad, no_setup);
        INFO("depth " << depths[0]);
        REQUIRE(depths[0] == Catch::Approx(k_bias_quad_depth).margin(0.0001));
    }
    SECTION("A pipeline that leaves the bias dynamic takes the slope factor from the command")
    {
        // The desc carries no factor at all, and the command's constant factor and clamp are both zero, so a
        // depth that moved did so through the third argument and nothing else.
        const Forge::Pipeline pipeline = make_pipeline({.depth_bias_enabled = true}, Forge::DynamicStateBits::DepthBias);
        require_biased_by(depths_after_draw(pipeline, tilted_quad,
                                            [&](Forge::CommandBuffer& command_buffer) {
                                                REQUIRE(command_buffer.CmdSetDepthBias(0.0f, 0.0f, k_slope_factor) == ErrorCode::Success);
                                            }),
                          k_slope * k_slope_factor);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/**
 * What the enum table cases below render into: four texels wide, since none of them draws a shape and what
 * is read back is one value repeated. The stencil format is one every device offers with a stencil aspect in
 * it, so nothing here is conditional on the device.
 */
constexpr i32 k_table_side = 4;
constexpr PixelFormat k_table_color_format = PixelFormat::R8G8B8A8_UNORM;
constexpr PixelFormat k_table_depth_stencil_format = PixelFormat::D24_UNORM_S8_UINT;

/**
 * The stencil value the device ended up holding, read straight out of the stencil aspect rather than probed
 * with a comparison. Probing would make every answer here depend on the comparator table being right, which
 * is the next case along and has no business deciding this one.
 *
 * The staging buffer is sized by the whole format rather than by the aspect, which is four bytes a texel
 * instead of the one a stencil aspect writes. CmdCopyTextureToBuffer measures the region with GetPixelSize
 * and has no notion of an aspect narrower than the format, so a buffer sized to what the copy actually
 * writes is refused. Over-allocating is harmless - the copy still writes one byte per texel from the front.
 */
u8 ReadStencilValue(ForgeFixture& fixture, Forge::Texture& depth_stencil)
{
    constexpr i32 k_texel_count = k_table_side * k_table_side;
    const Forge::Buffer staging = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                {.size = k_texel_count * GetPixelSize(k_table_depth_stencil_format),
                                 .usage = Forge::BufferUsageBits::TransferDestination,
                                 .host_access = Forge::HostAccess::Random}));
    const Forge::BufferTextureCopyRegion region{.texture_subresource = {.aspect_mask = Forge::ImageAspectBits::Stencil},
                                                .texture_extent = {k_table_side, k_table_side, 1}};
    REQUIRE(Forge::ImmediateSubmit(
                fixture.device, fixture.GetQueue(),
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(depth_stencil)) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdCopyTextureToBuffer(depth_stencil, staging, {&region, 1}) == ErrorCode::Success);
                }) == ErrorCode::Success);
    Opal::DynamicArray<u8> values(k_texel_count);
    REQUIRE(staging.Read({values.GetData(), values.GetSize()}) == ErrorCode::Success);
    // Every texel was covered by the same draw, so a target that does not agree with itself means the draw
    // did not reach all of it and whichever value came back first would be an accident.
    for (i32 i = 1; i < k_texel_count; ++i)
    {
        REQUIRE(values[i] == values[0]);
    }
    return values[0];
}

/**
 * The depth value the device ended up holding, read out of the depth aspect of a combined format rather
 * than the whole-format read a plain D32_SFLOAT texture's tests use. D24_UNORM_S8_UINT copies its depth
 * aspect as X8_D24_UNORM_PACK32 - the same four bytes a whole-format copy would read, with the 24 bit value
 * in the low bits and the high byte unused - so unpacking it into [0, 1] is this function's own job.
 */
f32 ReadDepthValue(ForgeFixture& fixture, Forge::Texture& depth_stencil)
{
    constexpr i32 k_texel_count = k_table_side * k_table_side;
    const Forge::Buffer staging = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                {.size = k_texel_count * GetPixelSize(k_table_depth_stencil_format),
                                 .usage = Forge::BufferUsageBits::TransferDestination,
                                 .host_access = Forge::HostAccess::Random}));
    const Forge::BufferTextureCopyRegion region{.texture_subresource = {.aspect_mask = Forge::ImageAspectBits::Depth},
                                                .texture_extent = {k_table_side, k_table_side, 1}};
    REQUIRE(Forge::ImmediateSubmit(
                fixture.device, fixture.GetQueue(),
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferSource(depth_stencil)) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdCopyTextureToBuffer(depth_stencil, staging, {&region, 1}) == ErrorCode::Success);
                }) == ErrorCode::Success);
    Opal::DynamicArray<u32> words(k_texel_count);
    REQUIRE(staging.Read({reinterpret_cast<u8*>(words.GetData()), words.GetSize() * sizeof(u32)}) == ErrorCode::Success);
    for (i32 i = 1; i < k_texel_count; ++i)
    {
        REQUIRE(words[i] == words[0]);
    }
    return static_cast<f32>(words[0] & 0x00FFFFFF) / 16777215.0f;
}

/**
 * A pipeline that covers the whole target, writes no colour, and applies `pass_operation` to the stencil
 * buffer wherever it draws. Comparator::Always, so nothing about the test decides whether the operation runs.
 *
 * @param dynamic_state Which of the stencil state is left to the command buffer. StencilReference by
 *        default, which is every caller but the one that stamps a fixed reference and never calls
 *        CmdSetStencilReference at all.
 * @param static_reference The reference both faces write when it is not dynamic.
 */
Forge::Pipeline MakeStencilWritePipeline(const Forge::Device& device, const Forge::Shader& vertex_shader,
                                         const Forge::Shader& fragment_shader, StencilOperation pass_operation,
                                         Forge::DynamicStateBits dynamic_state = Forge::DynamicStateBits::StencilReference,
                                         u32 static_reference = 0)
{
    Forge::GraphicsPipelineDesc pipeline_desc =
        MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
    pipeline_desc.depth_stencil.stencil_test_enabled = true;
    pipeline_desc.depth_stencil.front_stencil_comparator = Comparator::Always;
    pipeline_desc.depth_stencil.back_stencil_comparator = Comparator::Always;
    pipeline_desc.depth_stencil.front_pass = pass_operation;
    pipeline_desc.depth_stencil.back_pass = pass_operation;
    pipeline_desc.depth_stencil.front_reference = static_reference;
    pipeline_desc.depth_stencil.back_reference = static_reference;
    pipeline_desc.color_blend_attachments[0].color_write_mask = Forge::ColorWriteMaskBits::None;
    pipeline_desc.depth_attachment_format = k_table_depth_stencil_format;
    pipeline_desc.stencil_attachment_format = k_table_depth_stencil_format;
    pipeline_desc.dynamic_state = dynamic_state;
    return ForgeTest::Unwrap(Forge::Pipeline::Create(device, pipeline_desc));
}

/** What one stencil operation does to a stored value, spelled out from the Vulkan definition of each. */
u8 ApplyStencilOperation(StencilOperation operation, u8 stored, u8 reference)
{
    switch (operation)
    {
        case StencilOperation::Keep:
            return stored;
        case StencilOperation::Zero:
            return 0;
        case StencilOperation::Replace:
            return reference;
        // Clamped at the ends of the eight bits a D24_UNORM_S8_UINT stencil aspect holds, which is the only
        // place these two differ from the wrapping pair below.
        case StencilOperation::Increment:
            return stored == 0xFF ? 0xFF : static_cast<u8>(stored + 1);
        case StencilOperation::IncrementWrap:
            return static_cast<u8>(stored + 1);
        case StencilOperation::Decrement:
            return stored == 0 ? 0 : static_cast<u8>(stored - 1);
        case StencilOperation::DecrementWrap:
            return static_cast<u8>(stored - 1);
        case StencilOperation::Invert:
            return static_cast<u8>(~stored);
        default:
            FAIL("Unhandled stencil operation");
            return 0;
    }
}

const char* StencilOperationName(StencilOperation operation)
{
    switch (operation)
    {
        case StencilOperation::Keep:
            return "Keep";
        case StencilOperation::Zero:
            return "Zero";
        case StencilOperation::Replace:
            return "Replace";
        case StencilOperation::Increment:
            return "Increment";
        case StencilOperation::IncrementWrap:
            return "IncrementWrap";
        case StencilOperation::Decrement:
            return "Decrement";
        case StencilOperation::DecrementWrap:
            return "DecrementWrap";
        case StencilOperation::Invert:
            return "Invert";
        default:
            return "?";
    }
}

/** The depth-stencil these cases render into and then read the raw stencil values out of. */
Forge::Texture MakeStencilTarget(const Forge::Device& device)
{
    return MakeDepthTarget(device, k_table_side, k_table_depth_stencil_format);
}

/**
 * Begin rendering into a colour target cleared to red and a depth-stencil cleared to `stencil_clear`, with
 * the viewport, the scissor and the one vertex buffer these cases use already set.
 */
void BeginTableRendering(Forge::CommandBuffer& command_buffer, Forge::Texture& color, Forge::Texture& depth_stencil,
                         const Forge::Buffer& quad, u32 stencil_clear)
{
    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth_stencil)) == ErrorCode::Success);
    const Forge::RenderingAttachmentDesc depth_stencil_attachment{
        .texture = depth_stencil,
        .load_operation = Forge::AttachmentLoadOperation::Clear,
        .store_operation = Forge::AttachmentStoreOperation::Store,
        .clear_value = Forge::DepthStencilClearValue{1.0f, stencil_clear}};
    const Forge::RenderingDesc rendering_desc{
        .render_area_extent = {k_table_side, k_table_side},
        .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                             .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                             .store_operation = Forge::AttachmentStoreOperation::Store,
                                                             .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}}},
        .depth_attachment = depth_stencil_attachment.Clone(),
        .stencil_attachment = depth_stencil_attachment.Clone()};
    REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdBindVertexBuffer(quad, 0) == ErrorCode::Success);
}

/**
 * Whether `quad` presents its front face under WindingOrder::CCW, decided by whether culling the back of it
 * leaves anything behind. The winding of a quad in clip space depends on the viewport transform as much as on
 * the order of its vertices, so it is measured rather than reasoned about - the same reason the culling case
 * never asserts which way its triangle is wound.
 */
bool IsQuadFrontFacing(ForgeFixture& fixture, const Forge::Shader& vertex_shader, const Forge::Shader& fragment_shader,
                       const Forge::Buffer& quad)
{
    Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
    pipeline_desc.rasterizer.cull_mode = Face::Back;
    pipeline_desc.rasterizer.front_face = WindingOrder::CCW;
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    const Vector4f paint_color = ByteColor(0, 255, 0, 255);
    Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
    const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_table_side,
                                                       [&](Forge::CommandBuffer& command_buffer)
                                                       {
                                                           REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                           REQUIRE(command_buffer.CmdBindVertexBuffer(quad, 0) == ErrorCode::Success);
                                                           REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment,
                                                                                                   Opal::AsBytes(paint_color)) ==
                                                                   ErrorCode::Success);
                                                           REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                                                       });
    return CountCovered(pixels, k_table_side) == k_table_side * k_table_side;
}

}  // namespace

TEST_CASE("Forge stencil testing", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr i32 k_half = k_side / 2;
    constexpr PixelFormat k_color_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr PixelFormat k_depth_stencil_format = PixelFormat::D24_UNORM_S8_UINT;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));

    // Both faces of every pipeline below are set to the same thing, and not for tidiness: culling is off, so
    // which of the two states a fragment uses depends on how the quad happens to wind, and setting only the
    // front would silently do nothing here. Mutating one face alone leaves these cases green, which is how
    // that was found.
    const Forge::Buffer left_quad = MakeQuadBuffer(fixture.device, MakeLeftHalfQuad(0.5f));
    const Forge::Buffer full_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.5f));
    const Vector4f mask_color = ByteColor(0, 0, 0, 255);
    const Vector4f paint_color = ByteColor(0, 255, 0, 255);

    // A fixed reference of one rather than a dynamic one: nothing here calls CmdSetStencilReference, so
    // the value has to be baked into the pipeline.
    const Forge::Pipeline mask_pipeline = MakeStencilWritePipeline(fixture.device, vertex_shader, fragment_shader,
                                                                    StencilOperation::Replace, Forge::DynamicStateBits::None, 1);

    /** The pipeline that paints, either only where the stencil says one or everywhere. */
    auto make_paint_pipeline = [&](Comparator comparator)
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_color_format);
        pipeline_desc.depth_stencil.stencil_test_enabled = true;
        pipeline_desc.depth_stencil.front_stencil_comparator = comparator;
        pipeline_desc.depth_stencil.front_reference = 1;
        // Reads the buffer and leaves it alone, which is what a masked draw does.
        pipeline_desc.depth_stencil.front_write_mask = 0;
        pipeline_desc.depth_stencil.back_stencil_comparator = comparator;
        pipeline_desc.depth_stencil.back_reference = 1;
        pipeline_desc.depth_stencil.back_write_mask = 0;
        pipeline_desc.depth_attachment_format = k_depth_stencil_format;
        pipeline_desc.stencil_attachment_format = k_depth_stencil_format;
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    };

    auto run_pass = [&](const Forge::Pipeline& paint_pipeline)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        Forge::Texture depth_stencil = MakeStencilTarget(fixture.device);

        REQUIRE(Forge::ImmediateSubmit(
            fixture.device, fixture.GetQueue(),
            [&](Forge::CommandBuffer& command_buffer)
            {
                // One texture carries both, so the same view is named twice - Vulkan takes the two sides
                // apart even then, and each gets its own load and store.
                BeginTableRendering(command_buffer, color, depth_stencil, left_quad, 0);

                REQUIRE(command_buffer.CmdBindPipeline(mask_pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(mask_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(mask_color)) ==
                        ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);

                REQUIRE(command_buffer.CmdBindPipeline(paint_pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindVertexBuffer(full_quad, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(paint_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(paint_color)) ==
                        ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
            }) == ErrorCode::Success);

        return ReadColorPixels(fixture, color, k_side);
    };

    SECTION("A second draw lands only where the first allowed it")
    {
        const Opal::DynamicArray<u8> pixels = run_pass(make_paint_pipeline(Comparator::Equal));
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                const i32 base = (y * k_side + x) * 4;
                const bool inside_mask = x < k_half;
                INFO("texel " << x << "," << y << " inside the mask: " << inside_mask);
                // Green where the first draw stamped a one, and the red clear everywhere else. The first
                // draw wrote no colour at all, so the red is the clear rather than anything it left.
                REQUIRE(static_cast<i32>(pixels[base + 0]) == (inside_mask ? 0 : 255));
                REQUIRE(static_cast<i32>(pixels[base + 1]) == (inside_mask ? 255 : 0));
            }
        }
    }
    SECTION("A stencil attachment that names no texture is refused")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        // Transitioned first so that the refusal is the stencil one and not the layout check on the colour.
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{
            .render_area_extent = {k_side, k_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}},
            .stencil_attachment = Forge::RenderingAttachmentDesc{}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge stencil masks set per draw", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr i32 k_half = k_side / 2;
    constexpr PixelFormat k_color_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr PixelFormat k_depth_stencil_format = PixelFormat::D24_UNORM_S8_UINT;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));

    const Forge::Buffer left_quad = MakeQuadBuffer(fixture.device, MakeLeftHalfQuad(0.5f));
    const Forge::Buffer full_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.5f));

    // All three values are dynamic on both pipelines, so what the desc holds for them is ignored and every
    // number below comes from a CmdSet call. The static half of the same three is "Forge stencil testing"'s
    // business.
    constexpr Forge::DynamicStateBits k_dynamic_stencil = Forge::DynamicStateBits::StencilCompareMask |
                                                          Forge::DynamicStateBits::StencilWriteMask |
                                                          Forge::DynamicStateBits::StencilReference;

    /** Stamps into the stencil buffer wherever it draws, writing no colour. */
    const Forge::Pipeline mask_pipeline =
        MakeStencilWritePipeline(fixture.device, vertex_shader, fragment_shader, StencilOperation::Replace, k_dynamic_stencil);

    /** Paints where the stencil buffer compares equal, leaving it alone. */
    const Forge::Pipeline paint_pipeline = [&]
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_color_format);
        pipeline_desc.depth_stencil.stencil_test_enabled = true;
        pipeline_desc.depth_stencil.front_stencil_comparator = Comparator::Equal;
        pipeline_desc.depth_stencil.back_stencil_comparator = Comparator::Equal;
        pipeline_desc.depth_attachment_format = k_depth_stencil_format;
        pipeline_desc.stencil_attachment_format = k_depth_stencil_format;
        pipeline_desc.dynamic_state = k_dynamic_stencil;
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    }();

    const Vector4f mask_color = ByteColor(0, 0, 0, 255);
    const Vector4f paint_color = ByteColor(0, 255, 0, 255);

    /**
     * Stamp the left half with one set of values, then paint the whole target with another, and hand back
     * what survived. Each value is set for the two faces separately rather than through the FrontAndBack
     * default: culling is off here, so which face a fragment counts as depends on how the quad winds, and
     * setting both by name is what keeps that from deciding the answer.
     */
    auto run = [&](u32 mask_write_mask, u32 mask_reference, u32 paint_compare_mask, u32 paint_reference)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        Forge::Texture depth_stencil = MakeStencilTarget(fixture.device);

        auto set_stencil = [](Forge::CommandBuffer& command_buffer, u32 compare_mask, u32 write_mask, u32 reference)
        {
            for (const Forge::StencilFaceBits face : {Forge::StencilFaceBits::Front, Forge::StencilFaceBits::Back})
            {
                REQUIRE(command_buffer.CmdSetStencilCompareMask(compare_mask, face) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetStencilWriteMask(write_mask, face) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetStencilReference(reference, face) == ErrorCode::Success);
            }
        };

        REQUIRE(Forge::ImmediateSubmit(
            fixture.device, fixture.GetQueue(),
            [&](Forge::CommandBuffer& command_buffer)
            {
                BeginTableRendering(command_buffer, color, depth_stencil, left_quad, 0);

                // Comparator Always, so the compare mask decides nothing here and the write mask is what
                // picks which bits of the reference land in the buffer.
                REQUIRE(command_buffer.CmdBindPipeline(mask_pipeline) == ErrorCode::Success);
                set_stencil(command_buffer, 0xFF, mask_write_mask, mask_reference);
                REQUIRE(command_buffer.CmdPushConstants(mask_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(mask_color)) ==
                        ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);

                // Write mask zero, so this reads the buffer and leaves it as it found it.
                REQUIRE(command_buffer.CmdBindPipeline(paint_pipeline) == ErrorCode::Success);
                set_stencil(command_buffer, paint_compare_mask, 0, paint_reference);
                REQUIRE(command_buffer.CmdBindVertexBuffer(full_quad, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(paint_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(paint_color)) ==
                        ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
            }) == ErrorCode::Success);

        return ReadColorPixels(fixture, color, k_side);
    };

    /** Which texels the paint reached: green where it landed, the red clear where it did not. */
    auto require_painted = [](const Opal::DynamicArray<u8>& pixels, bool left, bool right)
    {
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                const i32 base = (y * k_side + x) * 4;
                const bool painted = x < k_half ? left : right;
                INFO("texel " << x << "," << y << " expected painted: " << painted);
                REQUIRE(static_cast<i32>(pixels[base + 0]) == (painted ? 0 : 255));
                REQUIRE(static_cast<i32>(pixels[base + 1]) == (painted ? 255 : 0));
            }
        }
    };

    SECTION("A full write mask stamps the whole reference")
    {
        // Three stamped into the left half and three compared against it, which is the baseline every
        // section below moves one value away from.
        require_painted(run(0xFF, 3, 0xFF, 3), true, false);
    }
    SECTION("A write mask keeps the bits it leaves out of the buffer")
    {
        // The same reference of three through a write mask of one: only the low bit lands, so the buffer
        // holds one and a test against three matches nowhere - including the half that was drawn.
        require_painted(run(0x01, 3, 0xFF, 3), false, false);
        // And against one it matches exactly where the stamp went, which is what rules out the stamp
        // having been dropped altogether rather than narrowed.
        require_painted(run(0x01, 3, 0xFF, 1), true, false);
    }
    SECTION("A compare mask of zero makes the test read no bits at all")
    {
        // Nothing about the buffer changed from the first section; the test now compares zero against zero
        // everywhere, so the half that was never stamped passes too.
        require_painted(run(0xFF, 3, 0x00, 3), true, true);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge blending", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.5f));

    // Byte values rather than round numbers, so what comes back is the blend equation and not a coincidence
    // of the clear. Both round trip a UNORM target exactly.
    constexpr i32 k_dst[4] = {64, 32, 16, 255};
    constexpr i32 k_src[4] = {128, 96, 48, 128};

    /** The pipeline that lays the destination down, with blending off so it arrives untouched. */
    const Forge::Pipeline opaque_pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(
        fixture.device, MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_format)));

    auto blend_over_destination = [&](const Forge::ColorBlendDesc& blend)
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_format);
        pipeline_desc.color_blend_attachments[0] = blend;
        const Forge::Pipeline blend_pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Vector4f destination = ByteColor(k_dst[0], k_dst[1], k_dst[2], k_dst[3]);
        const Vector4f source = ByteColor(k_src[0], k_src[1], k_src[2], k_src[3]);
        return RenderRaster(
            fixture, color, k_side,
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdBindVertexBuffer(quad, 0) == ErrorCode::Success);
                // The destination is drawn rather than cleared to, so it is exactly the bytes the shader wrote and
                // not a float the clear had to convert.
                REQUIRE(command_buffer.CmdBindPipeline(opaque_pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(opaque_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(destination)) ==
                        ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindPipeline(blend_pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(blend_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(source)) ==
                        ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
            },
            Vector4f{0.0f, 0.0f, 0.0f, 1.0f});
    };

    /** The same equation on the CPU, in the floats the device works in, rounded back to a byte at the end. */
    auto expected_channel = [](f32 src, f32 dst, f32 src_factor, f32 dst_factor, bool reverse_subtract)
    {
        const f32 blended = reverse_subtract ? dst * dst_factor - src * src_factor : src * src_factor + dst * dst_factor;
        const f32 clamped = blended < 0.0f ? 0.0f : (blended > 1.0f ? 1.0f : blended);
        return static_cast<i32>(clamped * 255.0f + 0.5f);
    };

    // The blend arithmetic is at least as precise as the format, but which way it rounds the last bit is the
    // device's business, so a byte either side is allowed and a wrong factor is nowhere near that close.
    auto require_channels = [&](const Opal::DynamicArray<u8>& pixels, const i32 expected[3])
    {
        for (i32 texel = 0; texel < k_side * k_side; ++texel)
        {
            for (i32 channel = 0; channel < 3; ++channel)
            {
                const i32 actual = pixels[texel * 4 + channel];
                INFO("texel " << texel << " channel " << channel << ", expected " << expected[channel] << " got " << actual);
                REQUIRE(actual >= expected[channel] - 1);
                REQUIRE(actual <= expected[channel] + 1);
            }
        }
    };

    /** The fourth channel, which both sections below are entirely about. */
    auto require_alpha = [&](const Opal::DynamicArray<u8>& pixels, i32 expected)
    {
        for (i32 texel = 0; texel < k_side * k_side; ++texel)
        {
            const i32 actual = pixels[texel * 4 + 3];
            INFO("texel " << texel << " alpha, expected " << expected << " got " << actual);
            REQUIRE(actual >= expected - 1);
            REQUIRE(actual <= expected + 1);
        }
    };

    SECTION("The alpha factors are read from their own fields rather than the colour ones")
    {
        // Opposite factors on the two halves, which is what makes a swap visible: the colour keeps the
        // source whole and the alpha keeps the destination whole, so a translation that fed the colour
        // fields into srcAlphaBlendFactor and dstAlphaBlendFactor would hand back exactly the other pair.
        const Opal::DynamicArray<u8> pixels = blend_over_destination({.blend_enabled = true,
                                                                      .src_color_factor = BlendFactor::One,
                                                                      .dst_color_factor = BlendFactor::Zero,
                                                                      .color_operation = BlendOperation::Add,
                                                                      .src_alpha_factor = BlendFactor::Zero,
                                                                      .dst_alpha_factor = BlendFactor::One,
                                                                      .alpha_operation = BlendOperation::Add});
        const i32 expected[3] = {k_src[0], k_src[1], k_src[2]};
        require_channels(pixels, expected);
        require_alpha(pixels, k_dst[3]);
    }
    SECTION("The alpha operation is its own as well")
    {
        // One and one on both halves, so the factors say nothing about which channel is which; only the
        // operation differs, and the two answers are far apart - the colour adds to more than it started
        // with while the alpha subtracts down to less.
        const Opal::DynamicArray<u8> pixels = blend_over_destination({.blend_enabled = true,
                                                                      .src_color_factor = BlendFactor::One,
                                                                      .dst_color_factor = BlendFactor::One,
                                                                      .color_operation = BlendOperation::Add,
                                                                      .src_alpha_factor = BlendFactor::One,
                                                                      .dst_alpha_factor = BlendFactor::One,
                                                                      .alpha_operation = BlendOperation::ReverseSubtract});
        i32 expected[3] = {};
        for (i32 channel = 0; channel < 3; ++channel)
        {
            expected[channel] = expected_channel(static_cast<f32>(k_src[channel]) / 255.0f,
                                                 static_cast<f32>(k_dst[channel]) / 255.0f, 1.0f, 1.0f, false);
        }
        require_channels(pixels, expected);
        require_alpha(pixels, expected_channel(static_cast<f32>(k_src[3]) / 255.0f,
                                               static_cast<f32>(k_dst[3]) / 255.0f, 1.0f, 1.0f, true));
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * Samplers, the texture shapes past a flat 2D one, and the descriptor kinds nothing had ever bound. Every
 * case samples in a compute shader and writes the result into a buffer as floats, so what comes back is the
 * value the sampler produced rather than a colour a UNORM attachment had to round on the way out.
 *
 * Sampling is always by explicit level: a compute shader has no derivatives, so there is no implicit LOD to
 * be had, and an explicit one is still clamped by the sampler - which is what makes the LOD clamp checkable.
 */
namespace
{

/** Where to sample and at what level, the same for every shader below. */
struct SampleParams
{
    Vector2f uv;
    f32 lod = 0.0f;
    f32 padding = 0.0f;
};

constexpr const char* k_combined_sample_source = R"(
struct SampleParams
{
    float2 uv;
    float lod;
    float padding;
};
[[vk::push_constant]] SampleParams params;

[[vk::binding(0, 0)]] Sampler2D combined;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_sample_combined()
{
    output[0] = combined.SampleLevel(params.uv, params.lod);
}
)";

constexpr const char* k_separate_sample_source = R"(
struct SampleParams
{
    float2 uv;
    float lod;
    float padding;
};
[[vk::push_constant]] SampleParams params;

[[vk::binding(0, 0)]] Texture2D<float4> separate_texture;
[[vk::binding(1, 0)]] SamplerState separate_sampler;
[[vk::binding(2, 0)]] RWStructuredBuffer<float4> output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_sample_separate()
{
    output[0] = separate_texture.SampleLevel(separate_sampler, params.uv, params.lod);
}
)";

constexpr const char* k_shape_sample_source = R"(
struct SampleParams
{
    float3 direction;
    float lod;
};
[[vk::push_constant]] SampleParams params;

[[vk::binding(0, 0)]] Sampler3D volume;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> volume_output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_sample_volume()
{
    volume_output[0] = volume.SampleLevel(params.direction, params.lod);
}
)";

constexpr const char* k_cube_sample_source = R"(
struct SampleParams
{
    float3 direction;
    float lod;
};
[[vk::push_constant]] SampleParams params;

[[vk::binding(0, 0)]] SamplerCube cube;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> cube_output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_sample_cube()
{
    cube_output[0] = cube.SampleLevel(params.direction, params.lod);
}
)";

/**
 * The shapes a sampler can take that the cases above do not: a line of texels, an array of such lines, an
 * array of flat textures, and an array of cubes. Each reads the one push constant every shape shader here
 * takes, so they all go through the same harness.
 */
constexpr const char* k_line_sample_source = R"(
[[vk::push_constant]] float4 params;

[[vk::binding(0, 0)]] Sampler1D line_texture;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> line_output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_sample_line()
{
    line_output[0] = line_texture.SampleLevel(params.x, params.w);
}
)";

constexpr const char* k_line_array_sample_source = R"(
[[vk::push_constant]] float4 params;

[[vk::binding(0, 0)]] Sampler1DArray line_array;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> line_array_output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_sample_line_array()
{
    line_array_output[0] = line_array.SampleLevel(float2(params.x, params.y), params.w);
}
)";

constexpr const char* k_flat_array_sample_source = R"(
[[vk::push_constant]] float4 params;

[[vk::binding(0, 0)]] Sampler2DArray flat_array;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> flat_array_output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_sample_flat_array()
{
    flat_array_output[0] = flat_array.SampleLevel(float3(params.x, params.y, params.z), params.w);
}
)";

constexpr const char* k_cube_array_sample_source = R"(
[[vk::push_constant]] float4 params;

[[vk::binding(0, 0)]] SamplerCubeArray cube_array;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> cube_array_output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_sample_cube_array()
{
    cube_array_output[0] = cube_array.SampleLevel(float4(params.xyz, params.w), 0.0);
}
)";

constexpr const char* k_storage_image_source = R"(
[[vk::image_format("rgba8")]]
[[vk::binding(0, 0)]] RWTexture2D<float4> storage_image;

[shader("compute")]
[numthreads(4, 4, 1)]
void main_write_storage(uint3 thread_id : SV_DispatchThreadID)
{
    storage_image[thread_id.xy] = float4(float(thread_id.x) / 4.0, float(thread_id.y) / 4.0, 0.0, 1.0);
}
)";

/**
 * Two texels side by side, the left one red and the right one green. Their centres sit at u = 0.25 and
 * u = 0.75, which is what every coordinate below is picked against.
 */
Opal::DynamicArray<u8> MakeTwoTexelRow()
{
    Opal::DynamicArray<u8> bytes(2 * 4);
    bytes[0] = 255;
    bytes[1] = 0;
    bytes[2] = 0;
    bytes[3] = 255;
    bytes[4] = 0;
    bytes[5] = 255;
    bytes[6] = 0;
    bytes[7] = 255;
    return bytes;
}

/**
 * The pool and the layout every sampling case below binds: one combined image sampler and one storage
 * buffer, both compute visible. What differs between them is only how many sets they need at once.
 */
struct SampleHarness
{
    Forge::DescriptorPool pool;
    Forge::DescriptorSetLayout layout;
};

SampleHarness MakeSampleHarness(const Forge::Device& device, u32 set_count)
{
    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, set_count) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, set_count) == ErrorCode::Success);
    pool_desc.max_sets = set_count;
    Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(device, layout_desc));

    return {std::move(pool), std::move(layout)};
}

/**
 * Dispatch a sampling pipeline once through a set of its own, with the texture and the sampler at binding
 * zero and a zeroed Vector4f at binding one, and hand back what the shader wrote there.
 *
 * @param push The push constant block, whatever shape the shader reads it in - SampleParams or a Vector4f.
 */
template <typename Push, typename Resource>
Vector4f SampleOnce(ForgeFixture& fixture, const SampleHarness& harness, const Forge::Pipeline& pipeline, const Resource& texture,
                    const Forge::Sampler& sampler, const Push& push)
{
    const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device, {.size = sizeof(Vector4f), .usage = Forge::BufferUsageBits::StorageBuffer, .host_access = Forge::HostAccess::Random}));
    const Opal::DynamicArray<u8> zeros(sizeof(Vector4f));
    REQUIRE(output.Update(zeros) == ErrorCode::Success);

    Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(harness.pool, harness.layout));
    REQUIRE(set.Update(0, texture, sampler, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
    REQUIRE(set.Update(1, output) == ErrorCode::Success);
    REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                   [&](Forge::CommandBuffer& command_buffer)
                                   {
                                       REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(push)) ==
                                               ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                                   }) == ErrorCode::Success);
    Vector4f result;
    REQUIRE(output.Read({reinterpret_cast<u8*>(&result), sizeof(result)}) == ErrorCode::Success);
    return result;
}

}  // namespace

TEST_CASE("Forge sampler filtering, LOD clamp and immutable samplers", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_combined_sample_source, {.entry_point = "main_sample_combined", .cache = GetShaderCache()}));

    const SampleHarness harness = MakeSampleHarness(fixture.device, 8);

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(harness.layout));
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(SampleParams)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    /** The one texture every section here samples: two texels, red then green. */
    Forge::Texture row = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                        .width = 2,
                                        .height = 1,
                                        .usage = Forge::TextureUsageBits::Sampled |
                                                 Forge::TextureUsageBits::TransferDestination}));
    const Opal::DynamicArray<u8> row_pixels = MakeTwoTexelRow();
    UploadMip(fixture.device, fixture.GetQueue(), row, {row_pixels.GetData(), row_pixels.GetSize()}, 0);

    /** Sample the texture through the given sampler and hand back the four floats it produced. */
    auto sample_with = [&](const Forge::Sampler& sampler, Forge::Texture& texture, const SampleParams& params)
    { return SampleOnce(fixture, harness, pipeline, texture, sampler, params); };

    SECTION("A linear and a nearest sampler differ between the two texels")
    {
        // Three tenths of the way from the left texel centre to the right one. A linear filter has to blend
        // in that proportion; a nearest one can only ever hand back one of the two texels whole.
        const SampleParams params{.uv = {0.4f, 0.5f}};
        const Forge::Sampler linear = ForgeTest::Unwrap(
            Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Linear, .mag_filter = ImageFilter::Linear}));
        const Forge::Sampler nearest = ForgeTest::Unwrap(
            Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest, .mag_filter = ImageFilter::Nearest}));

        const Vector4f blended = sample_with(linear, row, params);
        INFO("linear rgba " << blended.x << " " << blended.y << " " << blended.z << " " << blended.w);
        REQUIRE(blended.x == Catch::Approx(0.7f).margin(0.01));
        REQUIRE(blended.y == Catch::Approx(0.3f).margin(0.01));

        const Vector4f picked = sample_with(nearest, row, params);
        INFO("nearest rgba " << picked.x << " " << picked.y << " " << picked.z << " " << picked.w);
        REQUIRE(picked.x == Catch::Approx(1.0f).margin(0.01));
        REQUIRE(picked.y == Catch::Approx(0.0f).margin(0.01));
    }
    SECTION("A LOD clamp forces the level the sampler allows rather than the one asked for")
    {
        // Two levels with nothing in common: the top is red and the one below it is blue, so which level was
        // read is not a matter of degree.
        Forge::Texture mipped = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                               .width = 2,
                                               .height = 2,
                                               .mip_level_count = 2,
                                               .usage = Forge::TextureUsageBits::Sampled |
                                                        Forge::TextureUsageBits::TransferDestination}));
        Opal::DynamicArray<u8> top(2 * 2 * 4);
        for (i32 texel = 0; texel < 4; ++texel)
        {
            top[texel * 4 + 0] = 255;
            top[texel * 4 + 3] = 255;
        }
        Opal::DynamicArray<u8> bottom(4);
        bottom[2] = 255;
        bottom[3] = 255;
        UploadMip(fixture.device, fixture.GetQueue(), mipped, {top.GetData(), top.GetSize()}, 0);
        UploadMip(fixture.device, fixture.GetQueue(), mipped, {bottom.GetData(), bottom.GetSize()}, 1);

        // Nearest between levels, so the answer is one level and never a blend of two.
        const Forge::Sampler top_only = ForgeTest::Unwrap(
            Forge::Sampler::Create(fixture.device, {.mip_map_filter = ImageFilter::Nearest, .min_lod = 0.0f, .max_lod = 0.0f}));
        const Forge::Sampler bottom_only = ForgeTest::Unwrap(
            Forge::Sampler::Create(fixture.device, {.mip_map_filter = ImageFilter::Nearest, .min_lod = 1.0f, .max_lod = 1.0f}));

        // Asking for level one and being held at zero.
        const Vector4f held_at_top = sample_with(top_only, mipped, {.uv = {0.5f, 0.5f}, .lod = 1.0f});
        INFO("held at the top rgba " << held_at_top.x << " " << held_at_top.z);
        REQUIRE(held_at_top.x == Catch::Approx(1.0f).margin(0.01));
        REQUIRE(held_at_top.z == Catch::Approx(0.0f).margin(0.01));

        // And asking for level zero and being pushed down to one.
        const Vector4f pushed_to_bottom = sample_with(bottom_only, mipped, {.uv = {0.5f, 0.5f}, .lod = 0.0f});
        INFO("pushed to the bottom rgba " << pushed_to_bottom.x << " " << pushed_to_bottom.z);
        REQUIRE(pushed_to_bottom.x == Catch::Approx(0.0f).margin(0.01));
        REQUIRE(pushed_to_bottom.z == Catch::Approx(1.0f).margin(0.01));
    }
    SECTION("An immutable sampler is the one that samples, whatever the update said")
    {
        // The layout bakes a nearest sampler in. The update below hands it a linear one, which Vulkan
        // ignores for such a binding - so a result that blends would mean the write had been obeyed.
        const Forge::Sampler baked = ForgeTest::Unwrap(
            Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest, .mag_filter = ImageFilter::Nearest}));
        const Forge::Sampler ignored = ForgeTest::Unwrap(
            Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Linear, .mag_filter = ImageFilter::Linear}));

        const Opal::InPlaceArray<Opal::Ref<const Forge::Sampler>, 1> baked_samplers{Opal::Ref<const Forge::Sampler>(baked)};
        Forge::DescriptorSetLayoutDesc immutable_desc;
        REQUIRE(immutable_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Compute,
                                  {baked_samplers.GetData(), 1}) == ErrorCode::Success);
        REQUIRE(immutable_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        const Forge::DescriptorSetLayout immutable_layout =
            ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, immutable_desc));

        Forge::ComputePipelineDesc immutable_pipeline_desc;
        immutable_pipeline_desc.shader = shader;
        immutable_pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(immutable_layout));
        immutable_pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(SampleParams)});
        const Forge::Pipeline immutable_pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, immutable_pipeline_desc));

        const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = sizeof(Vector4f),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(sizeof(Vector4f));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(harness.pool, immutable_layout));
        REQUIRE(set.Update(0, row, ignored, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
        REQUIRE(set.Update(1, output) == ErrorCode::Success);
        const SampleParams params{.uv = {0.4f, 0.5f}};
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(immutable_pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSet(immutable_pipeline, set) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(immutable_pipeline, ShaderTypeBits::Compute,
                                                                   Opal::AsBytes(params)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        Vector4f result;
        REQUIRE(output.Read({reinterpret_cast<u8*>(&result), sizeof(result)}) == ErrorCode::Success);
        INFO("rgba " << result.x << " " << result.y);
        // The whole left texel, which is the baked nearest sampler. The linear one would have given 0.7.
        REQUIRE(result.x == Catch::Approx(1.0f).margin(0.01));
        REQUIRE(result.y == Catch::Approx(0.0f).margin(0.01));
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** One comparison through a shadow sampler, the reference in the slot SampleParams keeps its LOD in. */
constexpr const char* k_compare_sample_source = R"(
struct CompareParams
{
    float2 uv;
    float reference;
    float padding;
};
[[vk::push_constant]] CompareParams params;

[[vk::binding(0, 0)]] Sampler2DShadow shadow;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_sample_compare()
{
    output[0] = float4(shadow.SampleCmpLevelZero(params.uv, params.reference), 0.0, 0.0, 1.0);
}
)";

}  // namespace

/**
 * SamplerDesc::compare_enabled and compare_operator, over a two texel depth texture holding 0.25 and 0.75. A
 * comparison reads back as one or zero from a nearest filter, and as the share of the two texels that passed
 * from a linear one sampled halfway between them - which is the hardware doing percentage closer filtering.
 */
TEST_CASE("Forge comparison samplers", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::D32_SFLOAT;
    constexpr f32 k_depths[] = {0.25f, 0.75f};
    constexpr Vector2f k_first_texel{0.25f, 0.5f};
    constexpr Vector2f k_second_texel{0.75f, 0.5f};
    constexpr Vector2f k_between{0.5f, 0.5f};

    Forge::Texture depth = ForgeTest::Unwrap(Forge::Texture::Create(
        fixture.device, {.format = k_format,
                         .width = 2,
                         .height = 1,
                         .usage = Forge::TextureUsageBits::Sampled | Forge::TextureUsageBits::TransferDestination}));
    UploadMip(fixture.device, fixture.GetQueue(), depth, Opal::AsBytes(k_depths), 0);

    const SampleHarness harness = MakeSampleHarness(fixture.device, 8);
    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_compare_sample_source, {.entry_point = "main_sample_compare", .cache = GetShaderCache()}));
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(harness.layout));
    pipeline_desc.push_constant_ranges.PushBack({.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(SampleParams)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    auto make_sampler = [&](ImageFilter filter, Comparator comparator)
    {
        return ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = filter,
                                                                         .mag_filter = filter,
                                                                         .mip_map_filter = ImageFilter::Nearest,
                                                                         .address_mode_u = ImageAddressMode::Clamp,
                                                                         .address_mode_v = ImageAddressMode::Clamp,
                                                                         .address_mode_w = ImageAddressMode::Clamp,
                                                                         .compare_enabled = true,
                                                                         .compare_operator = comparator}));
    };
    /** The comparison of the reference against the texel at uv, which is all the shader writes. */
    auto compare = [&](const Forge::Sampler& sampler, const Vector2f& uv, f32 reference)
    {
        const Vector4f result = SampleOnce(fixture, harness, pipeline, depth, sampler, SampleParams{.uv = uv, .lod = reference});
        INFO("uv " << uv.x << "," << uv.y << " reference " << reference << " compared to " << result.x);
        return result.x;
    };

    SECTION("A nearest comparison passes or fails against the one texel it reads")
    {
        const Forge::Sampler less_equal = make_sampler(ImageFilter::Nearest, Comparator::LessEqual);
        REQUIRE(compare(less_equal, k_first_texel, 0.2f) == 1.0f);
        REQUIRE(compare(less_equal, k_first_texel, 0.3f) == 0.0f);
        REQUIRE(compare(less_equal, k_second_texel, 0.5f) == 1.0f);
        REQUIRE(compare(less_equal, k_second_texel, 0.8f) == 0.0f);
    }
    SECTION("The operator is the one the desc named")
    {
        // The opposite answers to the section above at the same references, so an operator that did not
        // reach the sampler would give the answers of the default.
        const Forge::Sampler greater = make_sampler(ImageFilter::Nearest, Comparator::Greater);
        REQUIRE(compare(greater, k_first_texel, 0.2f) == 0.0f);
        REQUIRE(compare(greater, k_first_texel, 0.3f) == 1.0f);
        const Forge::Sampler never = make_sampler(ImageFilter::Nearest, Comparator::Never);
        REQUIRE(compare(never, k_first_texel, 0.0f) == 0.0f);
    }
    SECTION("A linear comparison is the share of the texels it reads that passed")
    {
        if (!fixture.device.GetPhysicalDevice().SupportsLinearFilter(k_format))
        {
            SKIP("This device cannot filter the depth format linearly.");
        }
        const Forge::Sampler less_equal = make_sampler(ImageFilter::Linear, Comparator::LessEqual);
        // Halfway between the two texels a reference of one half passes the second and fails the first.
        REQUIRE(compare(less_equal, k_between, 0.5f) == Catch::Approx(0.5f).margin(0.01));
        REQUIRE(compare(less_equal, k_between, 0.1f) == Catch::Approx(1.0f).margin(0.01));
        REQUIRE(compare(less_equal, k_between, 0.9f) == Catch::Approx(0.0f).margin(0.01));
    }
    SECTION("A comparison operator out of range is refused")
    {
        REQUIRE(Forge::Sampler::Create(fixture.device, {.compare_operator = Comparator::EnumCount}).GetErrorOr(ErrorCode::Success) ==
                ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * SamplerDesc::reduction and DeviceFeatures::sampler_filter_minmax, over a two texel float texture holding
 * 0.25 and 0.75 sampled halfway between them: the average there is one half, the least one quarter and the
 * greatest three quarters, so each mode reads back as a value no other one gives.
 */
TEST_CASE("Forge min and max samplers", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_features{.sampler_filter_minmax = true};
    constexpr PixelFormat k_format = PixelFormat::R32_SFLOAT;
    constexpr f32 k_values[] = {0.25f, 0.75f};

    SECTION("A reduction returns the least or the greatest texel the filter reads")
    {
        if (!CanCreateDevice(k_features))
        {
            SKIP("This device has no min or max sampler reduction.");
        }
        ForgeFixture fixture(k_features);
        Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = k_format,
                             .width = 2,
                             .height = 1,
                             .usage = Forge::TextureUsageBits::Sampled | Forge::TextureUsageBits::TransferDestination}));
        UploadMip(fixture.device, fixture.GetQueue(), texture, Opal::AsBytes(k_values), 0);

        const SampleHarness harness = MakeSampleHarness(fixture.device, 4);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_combined_sample_source, {.entry_point = "main_sample_combined", .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(harness.layout));
        pipeline_desc.push_constant_ranges.PushBack({.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(SampleParams)});
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        auto sample_between = [&](SamplerReduction reduction)
        {
            const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Linear,
                                                                                                      .mag_filter = ImageFilter::Linear,
                                                                                                      .mip_map_filter = ImageFilter::Nearest,
                                                                                                      .address_mode_u = ImageAddressMode::Clamp,
                                                                                                      .address_mode_v = ImageAddressMode::Clamp,
                                                                                                      .address_mode_w = ImageAddressMode::Clamp,
                                                                                                      .reduction = reduction}));
            const f32 value = SampleOnce(fixture, harness, pipeline, texture, sampler, SampleParams{.uv = {0.5f, 0.5f}}).x;
            INFO("reduction " << static_cast<i32>(reduction) << " sampled " << value);
            return value;
        };
        REQUIRE(sample_between(SamplerReduction::Max) == 0.75f);
        REQUIRE(sample_between(SamplerReduction::Min) == 0.25f);
        // The ordinary filter at the same point, which only a format that filters linearly can give - and which
        // shows the two above are not what any linear sample there reads.
        if (fixture.device.GetPhysicalDevice().SupportsLinearFilter(k_format))
        {
            REQUIRE(sample_between(SamplerReduction::WeightedAverage) == Catch::Approx(0.5f).margin(0.01));
        }
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("A reduction the device or the rest of the desc cannot take is refused")
    {
        {
            ForgeFixture fixture;
            REQUIRE(Forge::Sampler::Create(fixture.device, {.reduction = SamplerReduction::Max}).GetErrorOr(ErrorCode::Success) ==
                    ErrorCode::InvalidArgument);
            REQUIRE_NO_VALIDATION_ERROR(fixture);
        }
        if (!CanCreateDevice(k_features))
        {
            SKIP("This device has no min or max sampler reduction.");
        }
        ForgeFixture fixture(k_features);
        REQUIRE(Forge::Sampler::Create(fixture.device, {.compare_enabled = true, .reduction = SamplerReduction::Min})
                    .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        REQUIRE(Forge::Sampler::Create(fixture.device, {.reduction = SamplerReduction::EnumCount}).GetErrorOr(ErrorCode::Success) ==
                ErrorCode::InvalidArgument);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
}

TEST_CASE("Forge separate sampler and sampled image", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_separate_sample_source, {.entry_point = "main_sample_separate", .cache = GetShaderCache()}));

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::SampledImage, 4) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::Sampler, 4) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 4) == ErrorCode::Success);
    pool_desc.max_sets = 4;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    // The image and the sampler in bindings of their own, which is the pair the combined descriptor bundles.
    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::SampledImage, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::Sampler, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(2, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(SampleParams)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    Forge::Texture row = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                        .width = 2,
                                        .height = 1,
                                        .usage = Forge::TextureUsageBits::Sampled |
                                                 Forge::TextureUsageBits::TransferDestination}));
    const Opal::DynamicArray<u8> row_pixels = MakeTwoTexelRow();
    UploadMip(fixture.device, fixture.GetQueue(), row, {row_pixels.GetData(), row_pixels.GetSize()}, 0);

    const Forge::Sampler linear =
        ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Linear, .mag_filter = ImageFilter::Linear}));
    const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = sizeof(Vector4f),
                                                .usage = Forge::BufferUsageBits::StorageBuffer,
                                                .host_access = Forge::HostAccess::Random}));
    const Opal::DynamicArray<u8> zeros(sizeof(Vector4f));
    REQUIRE(output.Update(zeros) == ErrorCode::Success);

    Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
    // The sampler of the image binding and the image of the sampler binding are each the half Vulkan ignores
    // for that descriptor type, which is what makes one Update overload serve all three kinds.
    REQUIRE(set.Update(0, row, linear, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
    REQUIRE(set.Update(1, row, linear, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
    REQUIRE(set.Update(2, output) == ErrorCode::Success);

    const SampleParams params{.uv = {0.4f, 0.5f}};
    REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(params)) ==
                                       ErrorCode::Success);
                               REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                           }) == ErrorCode::Success);
    Vector4f result;
    REQUIRE(output.Read({reinterpret_cast<u8*>(&result), sizeof(result)}) == ErrorCode::Success);
    INFO("rgba " << result.x << " " << result.y << " " << result.z << " " << result.w);
    // The same numbers the combined descriptor produces from the same texture, sampler and coordinate.
    REQUIRE(result.x == Catch::Approx(0.7f).margin(0.01));
    REQUIRE(result.y == Catch::Approx(0.3f).margin(0.01));
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge storage image writes", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_storage_image_source, {.entry_point = "main_write_storage", .cache = GetShaderCache()}));

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageImage, 4) == ErrorCode::Success);
    pool_desc.max_sets = 4;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageImage, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    Forge::Texture storage = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                            .width = k_side,
                                            .height = k_side,
                                            .usage = Forge::TextureUsageBits::Storage |
                                                     Forge::TextureUsageBits::TransferSource}));
    const Forge::Sampler unused = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {}));

    Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
    // General is the layout a storage image is bound in, which is what ToGeneral exists for.
    REQUIRE(set.Update(0, storage, unused, Forge::ImageLayout::General) == ErrorCode::Success);

    REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToGeneral(storage)) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                           }) == ErrorCode::Success);
    REQUIRE(ForgeTest::Unwrap(storage.GetCurrentLayout()) == Forge::ImageLayout::General);

    const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, storage, k_side);
    for (i32 y = 0; y < k_side; ++y)
    {
        for (i32 x = 0; x < k_side; ++x)
        {
            const i32 base = (y * k_side + x) * 4;
            // The shader writes its own coordinates over four, so every texel is different and a write that
            // landed at the wrong one says which.
            const i32 expected_red = static_cast<i32>(static_cast<f32>(x) / 4.0f * 255.0f + 0.5f);
            const i32 expected_green = static_cast<i32>(static_cast<f32>(y) / 4.0f * 255.0f + 0.5f);
            INFO("texel " << x << "," << y);
            REQUIRE(static_cast<i32>(pixels[base + 0]) >= expected_red - 1);
            REQUIRE(static_cast<i32>(pixels[base + 0]) <= expected_red + 1);
            REQUIRE(static_cast<i32>(pixels[base + 1]) >= expected_green - 1);
            REQUIRE(static_cast<i32>(pixels[base + 1]) <= expected_green + 1);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge texture shapes past a flat two dimensional one", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const SampleHarness harness = MakeSampleHarness(fixture.device, 4);

    const Forge::Sampler nearest =
        ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest, .mag_filter = ImageFilter::Nearest}));

    /** Sample one texture through one shader at one direction, and hand back what came out. */
    auto sample_shape = [&](const char* source, const char* entry_point, Forge::Texture& texture, const Vector4f& direction)
    {
        const Forge::Shader shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, source, {.entry_point = entry_point, .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(harness.layout));
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(Vector4f)});
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
        return SampleOnce(fixture, harness, pipeline, texture, nearest, direction);
    };

    SECTION("A three dimensional texture is sampled along its depth")
    {
        // Two slices, red in front and green behind, so which slice was read is not a matter of degree.
        Forge::Texture volume = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.dimension = Forge::TextureDimension::Texture3D,
                                               .format = k_format,
                                               .width = 1,
                                               .height = 1,
                                               .depth = 2,
                                               .usage = Forge::TextureUsageBits::Sampled |
                                                        Forge::TextureUsageBits::TransferDestination,
                                               .view_type = Forge::TextureViewType::Texture3D}));
        const Opal::DynamicArray<u8> slices = MakeTwoTexelRow();
        UploadMip(fixture.device, fixture.GetQueue(), volume, {slices.GetData(), slices.GetSize()}, 0);

        const Vector4f front = sample_shape(k_shape_sample_source, "main_sample_volume", volume, {0.5f, 0.5f, 0.25f, 0.0f});
        INFO("front rgba " << front.x << " " << front.y);
        REQUIRE(front.x == Catch::Approx(1.0f).margin(0.01));
        REQUIRE(front.y == Catch::Approx(0.0f).margin(0.01));

        const Vector4f back = sample_shape(k_shape_sample_source, "main_sample_volume", volume, {0.5f, 0.5f, 0.75f, 0.0f});
        INFO("back rgba " << back.x << " " << back.y);
        REQUIRE(back.x == Catch::Approx(0.0f).margin(0.01));
        REQUIRE(back.y == Catch::Approx(1.0f).margin(0.01));
    }
    SECTION("A cube view is sampled by direction")
    {
        // Six faces, each one texel, each a different value. A cube view needs the image to have been made
        // cube compatible, which nothing but the view type in the desc asks for.
        Forge::Texture cube = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                             .width = 1,
                                             .height = 1,
                                             .array_layer_count = 6,
                                             .usage = Forge::TextureUsageBits::Sampled |
                                                      Forge::TextureUsageBits::TransferDestination,
                                             .view_type = Forge::TextureViewType::Cube}));
        // Layer order is +X, -X, +Y, -Y, +Z, -Z, so a direction of positive x has to come back as the first.
        // From one rather than from zero: a face whose value is zero reads the same as one that was never
        // written, so the first face would be asserting nothing.
        Opal::DynamicArray<u8> faces(6 * 4);
        for (i32 face = 0; face < 6; ++face)
        {
            faces[face * 4 + 0] = static_cast<u8>((face + 1) * 36);
            faces[face * 4 + 3] = 255;
        }
        UploadMip(fixture.device, fixture.GetQueue(), cube, {faces.GetData(), faces.GetSize()}, 0);

        const Vector4f positive_x = sample_shape(k_cube_sample_source, "main_sample_cube", cube, {1.0f, 0.0f, 0.0f, 0.0f});
        INFO("+x red " << positive_x.x);
        REQUIRE(positive_x.x == Catch::Approx(36.0f / 255.0f).margin(0.01));

        const Vector4f negative_x = sample_shape(k_cube_sample_source, "main_sample_cube", cube, {-1.0f, 0.0f, 0.0f, 0.0f});
        INFO("-x red " << negative_x.x);
        REQUIRE(negative_x.x == Catch::Approx(2.0f * 36.0f / 255.0f).margin(0.01));

        const Vector4f positive_z = sample_shape(k_cube_sample_source, "main_sample_cube", cube, {0.0f, 0.0f, 1.0f, 0.0f});
        INFO("+z red " << positive_z.x);
        REQUIRE(positive_z.x == Catch::Approx(5.0f * 36.0f / 255.0f).margin(0.01));
    }
    SECTION("A one dimensional texture is sampled along its width")
    {
        // Two texels, red then green, in an image with no height at all: the shape is the point, and what
        // comes back says which of the two the coordinate landed on.
        Forge::Texture line = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                                                                       {.dimension = Forge::TextureDimension::Texture1D,
                                                                        .format = k_format,
                                                                        .width = 2,
                                                                        .height = 1,
                                                                        .usage = Forge::TextureUsageBits::Sampled |
                                                                                 Forge::TextureUsageBits::TransferDestination,
                                                                        .view_type = Forge::TextureViewType::Texture1D}));
        const Opal::DynamicArray<u8> texels = MakeTwoTexelRow();
        UploadMip(fixture.device, fixture.GetQueue(), line, {texels.GetData(), texels.GetSize()}, 0);

        const Vector4f left = sample_shape(k_line_sample_source, "main_sample_line", line, {0.25f, 0.0f, 0.0f, 0.0f});
        INFO("left rgba " << left.x << " " << left.y);
        REQUIRE(left.x == Catch::Approx(1.0f).margin(0.01));
        REQUIRE(left.y == Catch::Approx(0.0f).margin(0.01));

        const Vector4f right = sample_shape(k_line_sample_source, "main_sample_line", line, {0.75f, 0.0f, 0.0f, 0.0f});
        INFO("right rgba " << right.x << " " << right.y);
        REQUIRE(right.x == Catch::Approx(0.0f).margin(0.01));
        REQUIRE(right.y == Catch::Approx(1.0f).margin(0.01));
    }
    SECTION("A one dimensional array is sampled by layer")
    {
        // One texel per layer, so the coordinate along the width says nothing and the layer says everything.
        Forge::Texture lines = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                                                                        {.dimension = Forge::TextureDimension::Texture1D,
                                                                         .format = k_format,
                                                                         .width = 1,
                                                                         .height = 1,
                                                                         .array_layer_count = 2,
                                                                         .usage = Forge::TextureUsageBits::Sampled |
                                                                                  Forge::TextureUsageBits::TransferDestination,
                                                                         .view_type = Forge::TextureViewType::Texture1DArray}));
        const Opal::DynamicArray<u8> layers = MakeTwoTexelRow();
        UploadMip(fixture.device, fixture.GetQueue(), lines, {layers.GetData(), layers.GetSize()}, 0);

        const Vector4f first = sample_shape(k_line_array_sample_source, "main_sample_line_array", lines, {0.5f, 0.0f, 0.0f, 0.0f});
        INFO("layer zero rgba " << first.x << " " << first.y);
        REQUIRE(first.x == Catch::Approx(1.0f).margin(0.01));
        REQUIRE(first.y == Catch::Approx(0.0f).margin(0.01));

        const Vector4f second = sample_shape(k_line_array_sample_source, "main_sample_line_array", lines, {0.5f, 1.0f, 0.0f, 0.0f});
        INFO("layer one rgba " << second.x << " " << second.y);
        REQUIRE(second.x == Catch::Approx(0.0f).margin(0.01));
        REQUIRE(second.y == Catch::Approx(1.0f).margin(0.01));
    }
    SECTION("A two dimensional array is sampled rather than only copied")
    {
        Forge::Texture layers_texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                                                                                 {.format = k_format,
                                                                                  .width = 1,
                                                                                  .height = 1,
                                                                                  .array_layer_count = 2,
                                                                                  .usage = Forge::TextureUsageBits::Sampled |
                                                                                           Forge::TextureUsageBits::TransferDestination,
                                                                                  .view_type = Forge::TextureViewType::Texture2DArray}));
        const Opal::DynamicArray<u8> layers = MakeTwoTexelRow();
        UploadMip(fixture.device, fixture.GetQueue(), layers_texture, {layers.GetData(), layers.GetSize()}, 0);

        const Vector4f first =
            sample_shape(k_flat_array_sample_source, "main_sample_flat_array", layers_texture, {0.5f, 0.5f, 0.0f, 0.0f});
        INFO("layer zero rgba " << first.x << " " << first.y);
        REQUIRE(first.x == Catch::Approx(1.0f).margin(0.01));
        REQUIRE(first.y == Catch::Approx(0.0f).margin(0.01));

        const Vector4f second =
            sample_shape(k_flat_array_sample_source, "main_sample_flat_array", layers_texture, {0.5f, 0.5f, 1.0f, 0.0f});
        INFO("layer one rgba " << second.x << " " << second.y);
        REQUIRE(second.x == Catch::Approx(0.0f).margin(0.01));
        REQUIRE(second.y == Catch::Approx(1.0f).margin(0.01));
    }
    SECTION("A view over one mip level samples that level as its own level zero")
    {
        // Two levels with nothing in common, and a view that covers the second one only. The shader asks for
        // level zero, which through this view is the image's level one - so a view that ignored the range
        // would hand back red.
        Forge::Texture mipped = ForgeTest::Unwrap(
            Forge::Texture::Create(fixture.device, {.format = k_format,
                                                    .width = 2,
                                                    .height = 2,
                                                    .mip_level_count = 2,
                                                    .usage = Forge::TextureUsageBits::Sampled |
                                                             Forge::TextureUsageBits::TransferDestination,
                                                    .subresource_range = {.first_mip_level = 1, .mip_level_count = 1}}));
        Opal::DynamicArray<u8> top(2 * 2 * 4);
        for (i32 texel = 0; texel < 4; ++texel)
        {
            top[texel * 4 + 0] = 255;
            top[texel * 4 + 3] = 255;
        }
        Opal::DynamicArray<u8> bottom(4);
        bottom[1] = 255;
        bottom[3] = 255;
        UploadMip(fixture.device, fixture.GetQueue(), mipped, {top.GetData(), top.GetSize()}, 0);
        UploadMip(fixture.device, fixture.GetQueue(), mipped, {bottom.GetData(), bottom.GetSize()}, 1);

        const Vector4f sampled = sample_shape(k_combined_sample_source, "main_sample_combined", mipped, {0.5f, 0.5f, 0.0f, 0.0f});
        INFO("rgba " << sampled.x << " " << sampled.y);
        REQUIRE(sampled.x == Catch::Approx(0.0f).margin(0.01));
        REQUIRE(sampled.y == Catch::Approx(1.0f).margin(0.01));
    }
    SECTION("A view over one array layer samples it as a flat texture of its own")
    {
        // Two layers with nothing in common, and a flat Texture2D view over the range that names the second
        // one only. A view that ignored the range, or read the image's total layer count instead, would
        // hand back the first layer's colour.
        Forge::Texture layered = ForgeTest::Unwrap(
            Forge::Texture::Create(fixture.device, {.format = k_format,
                                                    .width = 1,
                                                    .height = 1,
                                                    .array_layer_count = 2,
                                                    .usage = Forge::TextureUsageBits::Sampled |
                                                             Forge::TextureUsageBits::TransferDestination,
                                                    .subresource_range = {.first_array_layer = 1, .array_layer_count = 1}}));
        const Opal::DynamicArray<u8> layers = MakeTwoTexelRow();
        UploadMip(fixture.device, fixture.GetQueue(), layered, {layers.GetData(), layers.GetSize()}, 0);

        const Vector4f sampled = sample_shape(k_combined_sample_source, "main_sample_combined", layered, {0.5f, 0.5f, 0.0f, 0.0f});
        INFO("rgba " << sampled.x << " " << sampled.y);
        REQUIRE(sampled.x == Catch::Approx(0.0f).margin(0.01));
        REQUIRE(sampled.y == Catch::Approx(1.0f).margin(0.01));
    }
    SECTION("A cube view over a layer count that is not a multiple of six is refused")
    {
        REQUIRE(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                          .width = 1,
                                                          .height = 1,
                                                          .array_layer_count = 4,
                                                          .usage = Forge::TextureUsageBits::Sampled,
                                                          .view_type = Forge::TextureViewType::Cube}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A cube view over twelve layers is refused, and one over six of them is not")
    {
        // Twelve is a multiple of six, which is all a cube compatible image needs - the texture's own view is
        // what cannot be a cube over all of them.
        REQUIRE(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                          .width = 1,
                                                          .height = 1,
                                                          .array_layer_count = 12,
                                                          .usage = Forge::TextureUsageBits::Sampled,
                                                          .view_type = Forge::TextureViewType::Cube}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        const Forge::Texture cube = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                                                              .width = 1,
                                                                                              .height = 1,
                                                                                              .array_layer_count = 12,
                                                                                              .usage = Forge::TextureUsageBits::Sampled,
                                                                                              .view_type = Forge::TextureViewType::Cube,
                                                                                              .subresource_range = {.first_array_layer = 6,
                                                                                                                    .array_layer_count = 6}}));
        REQUIRE(cube.IsValid());
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** Fills one mip level through a storage view, a constant colour over the texels the level has. */
constexpr const char* k_write_level_source = R"(
struct LevelParams
{
    float4 color;
    uint side;
};
[[vk::push_constant]] LevelParams params;

[[vk::image_format("rgba8")]]
[[vk::binding(0, 0)]] RWTexture2D<float4> level_image;

[shader("compute")]
[numthreads(4, 4, 1)]
void main_write_level(uint3 thread_id : SV_DispatchThreadID)
{
    if (thread_id.x < params.side && thread_id.y < params.side)
    {
        level_image[thread_id.xy] = params.color;
    }
}
)";

/** What that shader reads out of its push constant block. */
struct LevelParams
{
    Vector4f color;
    u32 side = 0;
    u32 padding[3] = {};
};

}  // namespace

/**
 * TextureView: more than one view of one image, each over part of it, all reading the layout the texture
 * tracks. The three shapes that need one - a mip chain written by level and sampled whole, one level rendered
 * into, one layer of an array sampled flat - and what a view refuses.
 */
TEST_CASE("Forge views of one texture", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr i32 k_side = 4;
    constexpr u32 k_level_count = 3;
    const Vector4f k_level_colors[k_level_count] = {{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}};

    /** A range of one mip level, or one layer, of every aspect. */
    auto level = [](u32 mip_level) { return Forge::ImageSubresourceRange{.first_mip_level = mip_level, .mip_level_count = 1}; };
    auto layer = [](u32 array_layer) { return Forge::ImageSubresourceRange{.first_array_layer = array_layer, .array_layer_count = 1}; };

    const Forge::Shader sample_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_combined_sample_source, {.entry_point = "main_sample_combined", .cache = GetShaderCache()}));
    const SampleHarness harness = MakeSampleHarness(fixture.device, 4);
    Forge::ComputePipelineDesc sample_pipeline_desc;
    sample_pipeline_desc.shader = sample_shader;
    sample_pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(harness.layout));
    sample_pipeline_desc.push_constant_ranges.PushBack({.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(SampleParams)});
    const Forge::Pipeline sample_pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, sample_pipeline_desc));
    const Forge::Sampler nearest = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest,
                                                                                             .mag_filter = ImageFilter::Nearest,
                                                                                             .mip_map_filter = ImageFilter::Nearest,
                                                                                             .min_lod = 0.0f,
                                                                                             .max_lod = static_cast<f32>(k_level_count)}));

    auto require_color = [](const Vector4f& measured, const Vector4f& expected)
    {
        INFO("measured " << measured.x << " " << measured.y << " " << measured.z << " " << measured.w);
        REQUIRE(measured.x == Catch::Approx(expected.x).margin(0.01));
        REQUIRE(measured.y == Catch::Approx(expected.y).margin(0.01));
        REQUIRE(measured.z == Catch::Approx(expected.z).margin(0.01));
        REQUIRE(measured.w == Catch::Approx(expected.w).margin(0.01));
    };

    SECTION("A mip chain written a level at a time through storage views is sampled whole")
    {
        // A depth pyramid's shape: each level a storage image of its own while it is written, the whole chain
        // one sampled texture afterwards. The texture's own view covers every level; the three written
        // through are views of one level each.
        Forge::Texture chain = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                                                         .width = k_side,
                                                                                         .height = k_side,
                                                                                         .mip_level_count = k_level_count,
                                                                                         .usage = Forge::TextureUsageBits::Storage |
                                                                                                  Forge::TextureUsageBits::Sampled}));
        const Forge::Shader write_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_write_level_source, {.entry_point = "main_write_level", .cache = GetShaderCache()}));
        Forge::DescriptorPoolDesc pool_desc;
        REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageImage, k_level_count) == ErrorCode::Success);
        pool_desc.max_sets = k_level_count;
        const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
        Forge::DescriptorSetLayoutDesc layout_desc;
        REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageImage, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));
        Forge::ComputePipelineDesc write_pipeline_desc;
        write_pipeline_desc.shader = write_shader;
        write_pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        write_pipeline_desc.push_constant_ranges.PushBack({.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(LevelParams)});
        const Forge::Pipeline write_pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, write_pipeline_desc));
        const Forge::Sampler unused = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {}));

        Opal::DynamicArray<Forge::TextureView> views;
        Opal::DynamicArray<Forge::DescriptorSet> sets;
        for (u32 mip = 0; mip < k_level_count; ++mip)
        {
            views.PushBack(ForgeTest::Unwrap(Forge::TextureView::Create(fixture.device, chain, {.subresource_range = level(mip)})));
            sets.PushBack(ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout)));
            REQUIRE(sets[mip].Update(0, views[mip], unused, Forge::ImageLayout::General) == ErrorCode::Success);
        }
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           REQUIRE(command_buffer.CmdBindPipeline(write_pipeline) == ErrorCode::Success);
                                           for (u32 mip = 0; mip < k_level_count; ++mip)
                                           {
                                               // One level at a time into General, the way a pyramid build moves
                                               // down it; the levels below are still Undefined while this runs.
                                               Forge::TextureBarrier to_general = Forge::TextureBarrier::ToGeneral(chain, Forge::ImageLayout::Undefined);
                                               to_general.subresource_range = level(mip);
                                               REQUIRE(command_buffer.CmdTextureBarrier(to_general) == ErrorCode::Success);
                                               REQUIRE(ForgeTest::Unwrap(chain.GetCurrentLayout(level(mip))) == Forge::ImageLayout::General);
                                               const LevelParams params{.color = k_level_colors[mip], .side = static_cast<u32>(k_side >> mip)};
                                               REQUIRE(command_buffer.CmdBindDescriptorSet(write_pipeline, sets[mip]) == ErrorCode::Success);
                                               REQUIRE(command_buffer.CmdPushConstants(write_pipeline, ShaderTypeBits::Compute,
                                                                                       Opal::AsBytes(params)) == ErrorCode::Success);
                                               REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                                           }
                                           REQUIRE(command_buffer.CmdTextureBarrier(
                                                       Forge::TextureBarrier::ToShaderRead(chain, Forge::PipelineStageBits::ComputeShader)) ==
                                                   ErrorCode::Success);
                                       }) == ErrorCode::Success);

        // The texture's own view, over every level, sees what each level's view wrote.
        for (u32 mip = 0; mip < k_level_count; ++mip)
        {
            INFO("level " << mip);
            require_color(SampleOnce(fixture, harness, sample_pipeline, chain, nearest,
                                     SampleParams{.uv = {0.5f, 0.5f}, .lod = static_cast<f32>(mip)}),
                          k_level_colors[mip]);
        }
    }
    SECTION("One mip level is rendered into through a view of it")
    {
        Forge::Texture target = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                                                          .width = k_side,
                                                                                          .height = k_side,
                                                                                          .mip_level_count = 2,
                                                                                          .usage = Forge::TextureUsageBits::ColorAttachment |
                                                                                                   Forge::TextureUsageBits::TransferSource}));
        const Forge::TextureView second_level =
            ForgeTest::Unwrap(Forge::TextureView::Create(fixture.device, target, {.subresource_range = level(1)}));
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(target)) ==
                                                   ErrorCode::Success);
                                           // The render area is the size of the level, not of the texture.
                                           const Forge::RenderingDesc rendering_desc{
                                               .render_area_extent = {k_side / 2, k_side / 2},
                                               .color_attachments = {Forge::RenderingAttachmentDesc{
                                                   .view = second_level,
                                                   .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                   .store_operation = Forge::AttachmentStoreOperation::Store,
                                                   .clear_value = Vector4f{0.0f, 1.0f, 0.0f, 1.0f}}}};
                                           REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                                       }) == ErrorCode::Success);
        Opal::DynamicArray<u8> pixels((k_side / 2) * (k_side / 2) * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), target, pixels, 1, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        for (i32 texel = 0; texel < (k_side / 2) * (k_side / 2); ++texel)
        {
            INFO("texel " << texel);
            REQUIRE(static_cast<i32>(pixels[texel * 4 + 0]) == 0);
            REQUIRE(static_cast<i32>(pixels[texel * 4 + 1]) == 255);
            REQUIRE(static_cast<i32>(pixels[texel * 4 + 2]) == 0);
        }
    }
    SECTION("Two layers of one array are sampled as two flat textures")
    {
        // The texture's own view is the array; each layer is a view of its own besides it, and the three
        // coexist over the one image.
        Forge::Texture layers = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                                                          .width = 1,
                                                                                          .height = 1,
                                                                                          .array_layer_count = 2,
                                                                                          .usage = Forge::TextureUsageBits::Sampled |
                                                                                                   Forge::TextureUsageBits::TransferDestination,
                                                                                          .view_type = Forge::TextureViewType::Texture2DArray}));
        const Opal::DynamicArray<u8> two_colors = MakeTwoTexelRow();
        UploadMip(fixture.device, fixture.GetQueue(), layers, {two_colors.GetData(), two_colors.GetSize()}, 0);
        const Forge::TextureView first = ForgeTest::Unwrap(Forge::TextureView::Create(fixture.device, layers, {.subresource_range = layer(0)}));
        const Forge::TextureView second = ForgeTest::Unwrap(Forge::TextureView::Create(fixture.device, layers, {.subresource_range = layer(1)}));
        require_color(SampleOnce(fixture, harness, sample_pipeline, first, nearest, SampleParams{.uv = {0.5f, 0.5f}}), k_level_colors[0]);
        require_color(SampleOnce(fixture, harness, sample_pipeline, second, nearest, SampleParams{.uv = {0.5f, 0.5f}}), k_level_colors[1]);
    }
    SECTION("A view the image cannot take, or of a texture that cannot have one, is refused")
    {
        constexpr Forge::TextureUsageBits k_sampled = Forge::TextureUsageBits::Sampled;
        const Forge::Texture flat = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = k_format, .width = k_side, .height = k_side, .mip_level_count = 2, .usage = k_sampled}));
        const Forge::Texture array = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                                                               .width = k_side,
                                                                                               .height = k_side,
                                                                                               .array_layer_count = 6,
                                                                                               .usage = k_sampled,
                                                                                               .view_type = Forge::TextureViewType::Texture2DArray}));
        auto refused = [&](const Forge::Texture& texture, const Forge::TextureViewDesc& desc)
        { return Forge::TextureView::Create(fixture.device, texture, desc).GetErrorOr(ErrorCode::Success); };

        REQUIRE(refused(flat, {.subresource_range = level(2)}) == ErrorCode::OutOfBounds);
        REQUIRE(refused(array, {.view_type = Forge::TextureViewType::Texture2DArray, .subresource_range = layer(6)}) == ErrorCode::OutOfBounds);
        // A flat view over the six layers, a cube over an image not made cube compatible, and views of a
        // dimension the image does not have.
        REQUIRE(refused(array, {}) == ErrorCode::InvalidArgument);
        REQUIRE(refused(array, {.view_type = Forge::TextureViewType::Cube}) == ErrorCode::InvalidArgument);
        REQUIRE(refused(flat, {.view_type = Forge::TextureViewType::Texture1D}) == ErrorCode::InvalidArgument);
        REQUIRE(refused(flat, {.view_type = Forge::TextureViewType::Texture3D}) == ErrorCode::InvalidArgument);

        // An image made cube compatible takes a cube view over its six layers, one of them flat, but not a cube
        // over fewer - and, on this device, no cube array, since the fixture did not ask for the feature.
        const Forge::Texture cube = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                                                              .width = 1,
                                                                                              .height = 1,
                                                                                              .array_layer_count = 6,
                                                                                              .usage = k_sampled,
                                                                                              .view_type = Forge::TextureViewType::Cube}));
        REQUIRE(refused(cube, {.view_type = Forge::TextureViewType::Cube}) == ErrorCode::Success);
        REQUIRE(refused(cube, {.subresource_range = layer(4)}) == ErrorCode::Success);
        REQUIRE(refused(cube, {.view_type = Forge::TextureViewType::Cube,
                               .subresource_range = {.first_array_layer = 3, .array_layer_count = 3}}) == ErrorCode::InvalidArgument);
        REQUIRE_FALSE(fixture.device.GetFeatures().image_cube_array);
        REQUIRE(refused(cube, {.view_type = Forge::TextureViewType::CubeArray}) == ErrorCode::InvalidArgument);

        const Forge::Texture transfer_only = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = k_format, .width = k_side, .height = k_side, .usage = Forge::TextureUsageBits::TransferSource}));
        REQUIRE(refused(transfer_only, {}) == ErrorCode::InvalidArgument);
        REQUIRE(refused(Forge::Texture{}, {}) == ErrorCode::InvalidArgument);
    }
    SECTION("An attachment view over more than one mip level is refused")
    {
        Forge::Texture target = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                                                          .width = k_side,
                                                                                          .height = k_side,
                                                                                          .mip_level_count = 2,
                                                                                          .usage = Forge::TextureUsageBits::ColorAttachment}));
        const Forge::TextureView both_levels = ForgeTest::Unwrap(Forge::TextureView::Create(fixture.device, target));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(target)) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{.render_area_extent = {k_side, k_side},
                                                  .color_attachments = {Forge::RenderingAttachmentDesc{.view = both_levels}}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a cube array view", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_cube_array{.image_cube_array = true};
    if (!CanCreateDevice(k_cube_array))
    {
        SKIP("This device cannot put more than one cube under a view.");
    }
    ForgeFixture fixture(k_cube_array);
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr i32 k_cube_count = 2;
    constexpr i32 k_layer_count = k_cube_count * 6;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_cube_array_sample_source, {.entry_point = "main_sample_cube_array", .cache = GetShaderCache()}));

    const SampleHarness harness = MakeSampleHarness(fixture.device, 4);

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(harness.layout));
    pipeline_desc.push_constant_ranges.PushBack({.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(Vector4f)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    // Two cubes of one texel a face, each face a red level of its own, counted from one so that a face
    // nobody wrote reads differently from every face that was.
    Forge::Texture cubes = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                                                                    {.format = k_format,
                                                                     .width = 1,
                                                                     .height = 1,
                                                                     .array_layer_count = k_layer_count,
                                                                     .usage = Forge::TextureUsageBits::Sampled |
                                                                              Forge::TextureUsageBits::TransferDestination,
                                                                     .view_type = Forge::TextureViewType::CubeArray}));
    Opal::DynamicArray<u8> faces(k_layer_count * 4);
    for (i32 layer = 0; layer < k_layer_count; ++layer)
    {
        faces[layer * 4 + 0] = static_cast<u8>((layer + 1) * 20);
        faces[layer * 4 + 3] = 255;
    }
    UploadMip(fixture.device, fixture.GetQueue(), cubes, {faces.GetData(), faces.GetSize()}, 0);

    const Forge::Sampler nearest =
        ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest, .mag_filter = ImageFilter::Nearest}));

    /** Sample one direction of one cube of the array, and hand back what came out. */
    auto sample_cube = [&](const Vector4f& direction_and_cube)
    { return SampleOnce(fixture, harness, pipeline, cubes, nearest, direction_and_cube); };

    // Layer order within a cube is +X, -X, +Y, -Y, +Z, -Z, and the cubes follow one another: the first face
    // of the second cube is layer six.
    const Vector4f first_cube = sample_cube({1.0f, 0.0f, 0.0f, 0.0f});
    INFO("first cube +x red " << first_cube.x);
    REQUIRE(first_cube.x == Catch::Approx(20.0f / 255.0f).margin(0.01));

    const Vector4f second_cube = sample_cube({1.0f, 0.0f, 0.0f, 1.0f});
    INFO("second cube +x red " << second_cube.x);
    REQUIRE(second_cube.x == Catch::Approx(140.0f / 255.0f).margin(0.01));

    // And a direction inside the second cube that is not the first face, so the cube index and the face are
    // both being read rather than one standing in for the other.
    const Vector4f second_cube_back = sample_cube({0.0f, 0.0f, 1.0f, 1.0f});
    INFO("second cube +z red " << second_cube_back.x);
    REQUIRE(second_cube_back.x == Catch::Approx(220.0f / 255.0f).margin(0.01));

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/**
 * A fullscreen triangle whose texture coordinates come out of a constant buffer and whose colour comes out
 * of a sampled texture, so one draw exercises a descriptor set from both graphics stages at once. The two
 * bindings are read by different stages on purpose: a layout that named the wrong stage would leave one of
 * them unreadable where it is used.
 */
constexpr const char* k_textured_draw_source = R"(
struct UvTransform
{
    float2 scale;
    float2 bias;
};

[[vk::binding(0, 0)]] ConstantBuffer<UvTransform> transform;
[[vk::binding(1, 0)]] Sampler2D albedo;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput main_textured_vertex(float2 position : POSITION)
{
    VertexOutput output;
    output.position = float4(position, 0.0, 1.0);
    output.uv = (position * 0.5 + 0.5) * transform.scale + transform.bias;
    return output;
}

[shader("fragment")]
float4 main_textured_fragment(VertexOutput input) : SV_Target
{
    return albedo.SampleLevel(input.uv, 0.0);
}
)";

/** What the constant buffer of that shader holds. Two float2, which std140 packs at 0 and 8 as written. */
struct UvTransform
{
    Vector2f scale{1.0f, 1.0f};
    Vector2f bias{0.0f, 0.0f};
};

/** The two texels of MakeTwoTexelRow, as a draw that sampled one of them whole reads back. */
constexpr Texel k_left_texel{255, 0, 0, 255};
constexpr Texel k_right_texel{0, 255, 0, 255};

}  // namespace

TEST_CASE("Forge a draw that reads a descriptor set", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_textured_draw_source, {.entry_point = "main_textured_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_textured_draw_source, {.entry_point = "main_textured_fragment", .cache = GetShaderCache()}));

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::ConstantBuffer, 4) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 4) == ErrorCode::Success);
    pool_desc.max_sets = 4;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::ConstantBuffer, 1, ShaderTypeBits::Vertex) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    Forge::GraphicsPipelineDesc pipeline_desc = MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, k_format);
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    const Forge::Buffer vertices = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = sizeof(k_fullscreen_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                              Opal::AsBytes(k_fullscreen_vertices)));

    /** Two texels, red then green, sampled in the fragment stage rather than in a dispatch. */
    Forge::Texture row = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                                                                 {.format = k_format,
                                                                  .width = 2,
                                                                  .height = 1,
                                                                  .usage = Forge::TextureUsageBits::Sampled |
                                                                           Forge::TextureUsageBits::TransferDestination}));
    const Opal::DynamicArray<u8> row_pixels = MakeTwoTexelRow();
    UploadMip(fixture.device, fixture.GetQueue(), row, {row_pixels.GetData(), row_pixels.GetSize()}, 0,
              Forge::PipelineStageBits::FragmentShader);

    const Forge::Sampler nearest =
        ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest, .mag_filter = ImageFilter::Nearest}));

    /**
     * Draw the triangle with that transform in the constant buffer and hand back the four columns of the
     * first row. The target is four texels wide and the texture two, so a column lands on the left texel
     * when the coordinate the vertex stage computed is below a half and on the right one above it: the four
     * values say what the vertex stage read out of the constant buffer and what the fragment stage sampled
     * with it.
     */
    auto draw_with = [&](const UvTransform& transform)
    {
        const Forge::Buffer transform_buffer = ForgeTest::Unwrap(
            Forge::Buffer::Create(fixture.device, {.size = sizeof(UvTransform), .usage = Forge::BufferUsageBits::ConstantBuffer},
                                  Opal::AsBytes(transform)));

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, transform_buffer) == ErrorCode::Success);
        REQUIRE(set.Update(1, row, nearest, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);

        // Cleared to blue, which the shader never writes: a texel that comes back blue is one the draw did not
        // reach.
        const Opal::DynamicArray<u8> pixels = RenderRaster(
            fixture, color, k_side,
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                // The same call the dispatch cases make. The bind point comes off the pipeline, so this is the one
                // line that says a set can be bound for graphics at all.
                REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
            },
            Vector4f{0.0f, 0.0f, 1.0f, 1.0f});
        Opal::DynamicArray<Texel> columns(k_side);
        for (i32 column = 0; column < k_side; ++column)
        {
            const i32 texel = column * 4;
            columns[column] = Texel{static_cast<i32>(pixels[texel]), static_cast<i32>(pixels[texel + 1]),
                                    static_cast<i32>(pixels[texel + 2]), static_cast<i32>(pixels[texel + 3])};
        }
        return columns;
    };

    SECTION("A set bound at the graphics bind point feeds the vertex and the fragment stage")
    {
        // The identity transform: the left half of the target samples the left texel and the right half the
        // right one.
        const Opal::DynamicArray<Texel> columns = draw_with({});
        for (i32 column = 0; column < k_side; ++column)
        {
            INFO("column " << column << " rgba " << columns[column].r << " " << columns[column].g << " " << columns[column].b << " "
                           << columns[column].a);
            REQUIRE(columns[column] == (column < 2 ? k_left_texel : k_right_texel));
        }
    }
    SECTION("What the constant buffer holds is what the vertex stage computed with")
    {
        // The same draw with the coordinate mirrored, which only the constant buffer says. Every column comes
        // back as the other texel, so nothing here could have been produced by a stage that ignored the set.
        const Opal::DynamicArray<Texel> columns = draw_with({.scale = {-1.0f, 1.0f}, .bias = {1.0f, 0.0f}});
        for (i32 column = 0; column < k_side; ++column)
        {
            INFO("column " << column << " rgba " << columns[column].r << " " << columns[column].g << " " << columns[column].b << " "
                           << columns[column].a);
            REQUIRE(columns[column] == (column < 2 ? k_right_texel : k_left_texel));
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge descriptor pool recycling", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_element_count = 64;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_descriptor_source, {.entry_point = "main_descriptor", .cache = GetShaderCache()}));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    /** Bind one set, dispatch through it, and check the shader wrote what it should have. */
    auto require_set_works = [&](Forge::DescriptorSet& set)
    {
        const Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);
        REQUIRE(set.Update(0, output) == ErrorCode::Success);
        DispatchWithSet(fixture.device, fixture.GetQueue(), pipeline, set);
        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            INFO("element " << i);
            REQUIRE(values[i] == static_cast<u32>(i) + 7);
        }
    };

    SECTION("Sets allocated after a reset work")
    {
        // A pool with room for exactly two, filled, then reset and filled again. Without the reset the third
        // allocation would have nothing left to come out of.
        Forge::DescriptorPoolDesc pool_desc;
        REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 2) == ErrorCode::Success);
        pool_desc.max_sets = 2;
        Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

        {
            Forge::DescriptorSet first = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
            Forge::DescriptorSet second = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
            require_set_works(first);
            require_set_works(second);
            // Every set the pool handed out is invalid the moment it is reset, so they go out of scope first.
        }
        REQUIRE(pool.Reset() == ErrorCode::Success);

        Forge::DescriptorSet after_reset = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        Forge::DescriptorSet also_after_reset = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        require_set_works(after_reset);
        require_set_works(also_after_reset);
    }
    SECTION("A destroyed set gives its space back when the pool allows it")
    {
        // free_individual_sets is what makes DescriptorSet::Destroy return the set rather than only drop the
        // handle, so the pool below runs out without it.
        Forge::DescriptorPoolDesc pool_desc;
        REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 2) == ErrorCode::Success);
        pool_desc.max_sets = 2;
        pool_desc.free_individual_sets = true;
        const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

        Forge::DescriptorSet first = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        Forge::DescriptorSet second = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        require_set_works(first);

        first.Destroy();
        REQUIRE_FALSE(first.IsValid());

        Forge::DescriptorSet reused = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        require_set_works(reused);
        // The one that was never destroyed is untouched by any of it.
        require_set_works(second);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a dispatch on the async compute queue", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr ForgeQueues k_queues{.async_compute = true};
    if (!CanCreateDevice({}, k_queues))
    {
        SKIP("This device has no async compute family.");
    }
    ForgeFixture fixture({}, k_queues);
    Forge::DeviceQueue& compute_queue = fixture.GetQueue(Forge::QueueFamily::AsyncCompute);
    // The point of the case: a family that is not the graphics one. A device that handed back the graphics
    // queue under another name would pass everything below while proving nothing.
    REQUIRE(compute_queue.GetQueueFamilyIndex() != fixture.GetQueue().GetQueueFamilyIndex());

    constexpr i32 k_element_count = 256;
    constexpr i32 k_group_size = 64;
    const Forge::Buffer output = MakeWipedOutput(fixture.device, k_element_count);

    const Forge::Shader compute_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
    const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, compute_shader);
    const VkDeviceAddress output_address = output.GetNativeDeviceAddress();

    // The command buffer comes out of the pool of the queue it is submitted to, which is the part a queue of
    // the wrong family gets wrong: a command buffer allocated on one family may not be submitted to another.
    REQUIRE(Forge::ImmediateSubmit(fixture.device, compute_queue,
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(output_address)) ==
                                       ErrorCode::Success);
                               REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
                           }) == ErrorCode::Success);

    RequireComputeWrote(output, k_element_count);

    SECTION("Timestamps on that family are read with its own valid bits")
    {
        const u32 family_index = compute_queue.GetQueueFamilyIndex();
        const VkQueueFamilyProperties& properties = fixture.device.GetPhysicalDevice().GetQueueFamilyProperties()[family_index];
        if (properties.timestampValidBits == 0)
        {
            // A family that can time nothing is named rather than answered with zeroes.
            REQUIRE(
                Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2, .queue_family = Forge::QueueFamily::AsyncCompute})
                    .GetErrorOr(ErrorCode::Success) == ErrorCode::FeatureNotSupported);
        }
        else
        {
            const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device,
                                                 {.query_count = 2, .queue_family = Forge::QueueFamily::AsyncCompute}));
            REQUIRE(Forge::ImmediateSubmit(fixture.device, compute_queue,
                                   [&](Forge::CommandBuffer& command_buffer)
                                   {
                                       REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineStart) ==
                                               ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute,
                                                                       Opal::AsBytes(output_address)) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdWriteTimestamp(pool, 1, Forge::PipelineStageBits::PipelineEnd) ==
                                               ErrorCode::Success);
                                   }) == ErrorCode::Success);
            f64 elapsed_ms = -1.0;
            REQUIRE(ForgeTest::Unwrap(pool.TryGetElapsedMilliseconds(0, 1, elapsed_ms)));
            INFO("elapsed " << elapsed_ms << " ms on family " << family_index);
            REQUIRE(elapsed_ms >= 0.0);
            // Ticks the mask of this family did not clear come back as an interval no dispatch this small
            // could have taken.
            REQUIRE(elapsed_ms < 1000.0);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge transfers on the dedicated transfer queue", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr ForgeQueues k_queues{.dedicated_transfer = true};
    if (!CanCreateDevice({}, k_queues))
    {
        SKIP("This device has no dedicated transfer family.");
    }
    ForgeFixture fixture({}, k_queues);
    Forge::DeviceQueue& transfer_queue = fixture.GetQueue(Forge::QueueFamily::Transfer);
    REQUIRE(transfer_queue.GetQueueFamilyIndex() != fixture.GetQueue().GetQueueFamilyIndex());

    constexpr i32 k_size = 256;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 41);
    const Opal::DynamicArray<u8> zeros(k_size);

    SECTION("A buffer copy")
    {
        const Forge::Buffer source = ForgeTest::Unwrap(
            Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written));
        const Forge::Buffer destination = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size,
                                                         .usage = Forge::BufferUsageBits::TransferDestination,
                                                         .host_access = Forge::HostAccess::Random}));
        REQUIRE(destination.Update(zeros) == ErrorCode::Success);
        REQUIRE(Forge::ImmediateSubmit(fixture.device, transfer_queue, [&](Forge::CommandBuffer& command_buffer)
                                       { REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success); }) ==
                ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("A texture upload and readback, in the layouts this family may transition into")
    {
        // The per-family trap. A transfer only family supports no shader stage, so it may not transition a
        // texture into ShaderReadOnly - which is what ReadBackTexture leaves a texture in by default. Undefined
        // as the final layout leaves it in TransferSource, and TransferSource and TransferDestination are the
        // two this family can reach.
        constexpr i32 k_side = 4;
        constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
        const Opal::DynamicArray<u8> pixels = MakeBytes(k_side * k_side * 4, 13);
        Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                .width = k_side,
                                                .height = k_side,
                                                .usage = Forge::TextureUsageBits::TransferSource |
                                                         Forge::TextureUsageBits::TransferDestination}));
        const Forge::Buffer staging = ForgeTest::Unwrap(
            Forge::Buffer::Create(fixture.device, {.size = pixels.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, pixels));
        const Forge::BufferTextureCopyRegion region;
        REQUIRE(
            Forge::ImmediateSubmit(fixture.device, transfer_queue,
                                   [&](Forge::CommandBuffer& command_buffer)
                                   {
                                       REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToTransferDestination(texture)) ==
                                               ErrorCode::Success);
                                       REQUIRE(command_buffer.CmdCopyBufferToTexture(staging, texture, {&region, 1}) == ErrorCode::Success);
                                   }) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferDestination);

        Opal::DynamicArray<u8> read_back(pixels.GetSize());
        REQUIRE(Forge::ReadBackTexture(fixture.device, transfer_queue, texture, read_back, 0, Forge::ImageLayout::Undefined) ==
                ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferSource);
        REQUIRE(CountMismatches(pixels, read_back) == 0);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a buffer handed from one queue family to another", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr ForgeQueues k_queues{.async_compute = true};
    if (!CanCreateDevice({}, k_queues))
    {
        SKIP("This device has no async compute family.");
    }
    ForgeFixture fixture({}, k_queues);
    Forge::DeviceQueue& compute_queue = fixture.GetQueue(Forge::QueueFamily::AsyncCompute);
    Forge::DeviceQueue& graphics_queue = fixture.GetQueue();
    const u32 compute_family = compute_queue.GetQueueFamilyIndex();
    const u32 graphics_family = graphics_queue.GetQueueFamilyIndex();
    REQUIRE(compute_family != graphics_family);

    constexpr i32 k_element_count = 256;
    constexpr i32 k_group_size = 64;
    constexpr u64 k_byte_size = k_element_count * sizeof(u32);
    // Device local on purpose: a buffer the host could read would let the copy on the other family be left
    // out, and handing a buffer between two families is what an ownership transfer is for.
    const Forge::Buffer shared = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_byte_size,
                                                .usage = Forge::BufferUsageBits::StorageBuffer |
                                                         Forge::BufferUsageBits::TransferSource,
                                                .use_device_address = true}));
    const Forge::Buffer host_visible = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_byte_size,
                                                      .usage = Forge::BufferUsageBits::TransferDestination,
                                                      .host_access = Forge::HostAccess::Random}));
    const Opal::DynamicArray<u8> zeros(k_byte_size);
    REQUIRE(host_visible.Update(zeros) == ErrorCode::Success);

    const Forge::Shader compute_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
    const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, compute_shader);
    const VkDeviceAddress shared_address = shared.GetNativeDeviceAddress();

    // The release half, on the family that wrote the buffer. Its destination stages and access are empty:
    // what happens on the other side of a release belongs to the acquiring family and is named there.
    Forge::CommandBuffer release_commands = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, compute_queue));
    REQUIRE(release_commands.Begin() == ErrorCode::Success);
    REQUIRE(release_commands.CmdBindPipeline(pipeline) == ErrorCode::Success);
    REQUIRE(release_commands.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(shared_address)) == ErrorCode::Success);
    REQUIRE(release_commands.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
    REQUIRE(release_commands.CmdBufferBarrier({.stages_must_finish = Forge::PipelineStageBits::ComputeShader,
                                       .stages_must_finish_access = Forge::PipelineStageAccessBits::ShaderWrite,
                                       .before_stages_start = Forge::PipelineStageBits::None,
                                       .before_stages_start_access = Forge::PipelineStageAccessBits::None,
                                       .source_queue_family = compute_family,
                                       .destination_queue_family = graphics_family,
                                       .buffer = shared}) == ErrorCode::Success);
    REQUIRE(release_commands.End() == ErrorCode::Success);

    // The acquire half, on the family that reads it, naming the same pair of families in the same order.
    Forge::CommandBuffer acquire_commands = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, graphics_queue));
    REQUIRE(acquire_commands.Begin() == ErrorCode::Success);
    REQUIRE(acquire_commands.CmdBufferBarrier({.stages_must_finish = Forge::PipelineStageBits::None,
                                       .stages_must_finish_access = Forge::PipelineStageAccessBits::None,
                                       .before_stages_start = Forge::PipelineStageBits::Copy,
                                       .before_stages_start_access = Forge::PipelineStageAccessBits::TransferRead,
                                       .source_queue_family = compute_family,
                                       .destination_queue_family = graphics_family,
                                       .buffer = shared}) == ErrorCode::Success);
    REQUIRE(acquire_commands.CmdCopyBuffer(shared, host_visible) == ErrorCode::Success);
    REQUIRE(acquire_commands.End() == ErrorCode::Success);

    // A semaphore between the two submits, which the transfer needs beyond the barriers: the acquire may not
    // run before the release, and two queues have no order of their own.
    const Forge::Semaphore handover = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
    const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
    const Opal::Ref<const Forge::CommandBuffer> release_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(release_commands)};
    const Opal::Ref<const Forge::CommandBuffer> acquire_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(acquire_commands)};
    const Forge::SemaphoreSubmit signal{.semaphore = handover, .stages = Forge::PipelineStageBits::ComputeShader};
    const Forge::SemaphoreSubmit wait{.semaphore = handover, .stages = Forge::PipelineStageBits::Transfer};
    REQUIRE(compute_queue.Submit({.command_buffers = {release_batch, 1}, .signal_semaphores = {&signal, 1}}) == ErrorCode::Success);
    REQUIRE(graphics_queue.Submit({.command_buffers = {acquire_batch, 1}, .wait_semaphores = {&wait, 1}, .fence = fence}) ==
            ErrorCode::Success);
    REQUIRE(fence.Wait() == ErrorCode::Success);

    RequireComputeWrote(host_visible, k_element_count);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a texture handed from one queue family to another", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr ForgeQueues k_queues{.async_compute = true};
    if (!CanCreateDevice({}, k_queues))
    {
        SKIP("This device has no async compute family.");
    }
    ForgeFixture fixture({}, k_queues);
    Forge::DeviceQueue& compute_queue = fixture.GetQueue(Forge::QueueFamily::AsyncCompute);
    Forge::DeviceQueue& graphics_queue = fixture.GetQueue();
    const u32 compute_family = compute_queue.GetQueueFamilyIndex();
    const u32 graphics_family = graphics_queue.GetQueueFamilyIndex();
    REQUIRE(compute_family != graphics_family);

    constexpr i32 k_side = 4;
    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_storage_image_source, {.entry_point = "main_write_storage", .cache = GetShaderCache()}));

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageImage, 1) == ErrorCode::Success);
    pool_desc.max_sets = 1;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageImage, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    // Written on one family and read on the other, with nothing host visible in between: a texture the host
    // could reach would let the read on the second family be left out, and handing it over is the point.
    Forge::Texture shared = ForgeTest::Unwrap(Forge::Texture::Create(
        fixture.device,
        {.format = PixelFormat::R8G8B8A8_UNORM,
         .width = k_side,
         .height = k_side,
         .usage = Forge::TextureUsageBits::Storage | Forge::TextureUsageBits::TransferSource}));
    const Forge::Sampler unused = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {}));
    Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
    REQUIRE(set.Update(0, shared, unused, Forge::ImageLayout::General) == ErrorCode::Success);

    const Forge::Buffer host_visible = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                                                               {.size = k_side * k_side * 4,
                                                                                .usage = Forge::BufferUsageBits::TransferDestination,
                                                                                .host_access = Forge::HostAccess::Random}));
    const Opal::DynamicArray<u8> zeros(k_side * k_side * 4);
    REQUIRE(host_visible.Update(zeros) == ErrorCode::Success);

    // The release half, on the family that wrote the image. Both halves name the same pair of layouts: a
    // transfer that carries a transition performs it once, and the two barriers have to agree on it.
    Forge::CommandBuffer release_commands = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, compute_queue));
    REQUIRE(release_commands.Begin() == ErrorCode::Success);
    REQUIRE(release_commands.CmdTextureBarrier(Forge::TextureBarrier::ToGeneral(shared)) == ErrorCode::Success);
    REQUIRE(release_commands.CmdBindPipeline(pipeline) == ErrorCode::Success);
    REQUIRE(release_commands.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
    REQUIRE(release_commands.CmdDispatch(1) == ErrorCode::Success);
    REQUIRE(release_commands.CmdTextureBarrier({.stages_must_finish = Forge::PipelineStageBits::ComputeShader,
                                                .stages_must_finish_access = Forge::PipelineStageAccessBits::ShaderWrite,
                                                .before_stages_start = Forge::PipelineStageBits::None,
                                                .before_stages_start_access = Forge::PipelineStageAccessBits::None,
                                                .old_layout = Forge::ImageLayout::General,
                                                .new_layout = Forge::ImageLayout::TransferSource,
                                                .source_queue_family = compute_family,
                                                .destination_queue_family = graphics_family,
                                                .texture = shared}) == ErrorCode::Success);
    REQUIRE(release_commands.End() == ErrorCode::Success);

    // The acquire half, on the family that reads it, naming the same families in the same order.
    Forge::CommandBuffer acquire_commands = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, graphics_queue));
    REQUIRE(acquire_commands.Begin() == ErrorCode::Success);
    REQUIRE(acquire_commands.CmdTextureBarrier({.stages_must_finish = Forge::PipelineStageBits::None,
                                                .stages_must_finish_access = Forge::PipelineStageAccessBits::None,
                                                .before_stages_start = Forge::PipelineStageBits::Copy,
                                                .before_stages_start_access = Forge::PipelineStageAccessBits::TransferRead,
                                                .old_layout = Forge::ImageLayout::General,
                                                .new_layout = Forge::ImageLayout::TransferSource,
                                                .source_queue_family = compute_family,
                                                .destination_queue_family = graphics_family,
                                                .texture = shared}) == ErrorCode::Success);
    const Forge::BufferTextureCopyRegion region{};
    REQUIRE(acquire_commands.CmdCopyTextureToBuffer(shared, host_visible, {&region, 1}) == ErrorCode::Success);
    REQUIRE(acquire_commands.End() == ErrorCode::Success);

    // A semaphore between the two submits, which the transfer needs beyond the barriers: the acquire may not
    // run before the release, and two queues have no order of their own.
    const Forge::Semaphore handover = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
    const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
    const Opal::Ref<const Forge::CommandBuffer> release_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(release_commands)};
    const Opal::Ref<const Forge::CommandBuffer> acquire_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(acquire_commands)};
    const Forge::SemaphoreSubmit signal{.semaphore = handover, .stages = Forge::PipelineStageBits::ComputeShader};
    const Forge::SemaphoreSubmit wait{.semaphore = handover, .stages = Forge::PipelineStageBits::Transfer};
    REQUIRE(compute_queue.Submit({.command_buffers = {release_batch, 1}, .signal_semaphores = {&signal, 1}}) == ErrorCode::Success);
    REQUIRE(graphics_queue.Submit({.command_buffers = {acquire_batch, 1}, .wait_semaphores = {&wait, 1}, .fence = fence}) ==
            ErrorCode::Success);
    REQUIRE(fence.Wait() == ErrorCode::Success);

    Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
    REQUIRE(host_visible.Read({pixels.GetData(), pixels.GetSize()}) == ErrorCode::Success);
    for (i32 y = 0; y < k_side; ++y)
    {
        for (i32 x = 0; x < k_side; ++x)
        {
            // What the shader writes at that texel, which is its own coordinates over four. The contents
            // crossing the handover is the whole question: an image whose ownership was dropped comes back
            // as whatever the allocation held.
            const i32 base = (y * k_side + x) * 4;
            const i32 expected_red = static_cast<i32>(static_cast<f32>(x) / 4.0f * 255.0f + 0.5f);
            const i32 expected_green = static_cast<i32>(static_cast<f32>(y) / 4.0f * 255.0f + 0.5f);
            INFO("texel " << x << "," << y);
            REQUIRE(static_cast<i32>(pixels[base + 0]) >= expected_red - 1);
            REQUIRE(static_cast<i32>(pixels[base + 0]) <= expected_red + 1);
            REQUIRE(static_cast<i32>(pixels[base + 1]) >= expected_green - 1);
            REQUIRE(static_cast<i32>(pixels[base + 1]) <= expected_green + 1);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}


/**
 * A pass that only reads the depth attachment and one where the stencil attachment stands on a texture of
 * its own, since nothing has checked either. DepthStencilReadOnly is a real layout with a preset of its
 * own, and a stencil-only pass is the one case Vulkan requires a separate image for.
 */
TEST_CASE("Forge a depth attachment that is read and not written", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr f32 k_stored_depth = 0.5f;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));

    const Forge::Buffer stored_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(k_stored_depth));
    const Forge::Buffer near_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.25f));
    const Forge::Buffer far_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.75f));
    const Vector4f green = ByteColor(0, 255, 0, 255);
    const Vector4f red = ByteColor(255, 0, 0, 255);
    const Vector4f blue = ByteColor(0, 0, 255, 255);

    SECTION("A pass that only reads the depth keeps the layout and the values")
    {
        constexpr PixelFormat k_depth_format = PixelFormat::D32_SFLOAT;
        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        Forge::Texture depth = MakeDepthTarget(fixture.device, k_table_side, k_depth_format);

        Forge::GraphicsPipelineDesc writing_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
        writing_desc.depth_stencil.depth_test_enabled = true;
        writing_desc.depth_stencil.depth_write_enabled = true;
        writing_desc.depth_stencil.depth_comparator = Comparator::Always;
        writing_desc.depth_attachment_format = k_depth_format;
        const Forge::Pipeline writing_pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, writing_desc));

        // The second pass tests against what the first left and writes nothing, which is what makes the
        // read-only layout legal for it.
        Forge::GraphicsPipelineDesc reading_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
        reading_desc.depth_stencil.depth_test_enabled = true;
        reading_desc.depth_stencil.depth_write_enabled = false;
        reading_desc.depth_stencil.depth_comparator = Comparator::Less;
        reading_desc.depth_attachment_format = k_depth_format;
        const Forge::Pipeline reading_pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, reading_desc));

        // The submit is kept in a variable rather than asserted on directly: REQUIRE stringizes what it is
        // given, and the recorder below is past what a compiler will take as one string literal.
        const ErrorCode submit_status = Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth)) ==
                                ErrorCode::Success);
                        const Forge::RenderingDesc writing_pass{
                            .render_area_extent = {k_table_side, k_table_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                                 .clear_value = Vector4f{0.0f, 0.0f, 0.0f, 1.0f}}},
                            .depth_attachment = Forge::RenderingAttachmentDesc{
                                .texture = depth,
                                .load_operation = Forge::AttachmentLoadOperation::Clear,
                                .store_operation = Forge::AttachmentStoreOperation::Store,
                                .clear_value = Forge::DepthStencilClearValue{1.0f, 0}}};
                        REQUIRE(command_buffer.CmdBeginRendering(writing_pass) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(writing_pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(stored_quad, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(writing_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(green)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);

                        // Written by hand rather than through CmdTransition: the preset for this layout names
                        // the fragment stage and a shader read, which is what a texture about to be sampled
                        // wants. What follows here is the depth test, so the stages and the access are the
                        // ones the fixed-function depth read happens in.
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier{
                                    .stages_must_finish =
                                        Forge::PipelineStageBits::EarlyFragmentTests | Forge::PipelineStageBits::LateFragmentTests,
                                    .stages_must_finish_access = Forge::PipelineStageAccessBits::DepthStencilAttachmentWrite,
                                    .before_stages_start =
                                        Forge::PipelineStageBits::EarlyFragmentTests | Forge::PipelineStageBits::LateFragmentTests,
                                    .before_stages_start_access = Forge::PipelineStageAccessBits::DepthStencilAttachmentRead,
                                    .old_layout = Forge::ImageLayout::DepthStencilAttachment,
                                    .new_layout = Forge::ImageLayout::DepthStencilReadOnly,
                                    .texture = depth}) == ErrorCode::Success);

                        // Load and a store of DontCare: the pass reads the depth and is not allowed to write
                        // it, so there is nothing for a store to keep.
                        const Forge::RenderingDesc reading_pass{
                            .render_area_extent = {k_table_side, k_table_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Load,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store}},
                            .depth_attachment =
                                Forge::RenderingAttachmentDesc{.texture = depth,
                                                               .load_operation = Forge::AttachmentLoadOperation::Load,
                                                               .store_operation = Forge::AttachmentStoreOperation::DontCare}};
                        REQUIRE(command_buffer.CmdBeginRendering(reading_pass) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(reading_pipeline) == ErrorCode::Success);
                        // Nearer than what is stored, so it passes, and then further, so it does not.
                        REQUIRE(command_buffer.CmdBindVertexBuffer(near_quad, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(reading_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(red)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(far_quad, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(reading_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(blue)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    });
        REQUIRE(submit_status == ErrorCode::Success);

        REQUIRE(ForgeTest::Unwrap(depth.GetCurrentLayout()) == Forge::ImageLayout::DepthStencilReadOnly);

        Opal::DynamicArray<u8> pixels(k_table_side * k_table_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        // Red, from the quad the test let through. Blue would mean the depth was not being read at all.
        REQUIRE(static_cast<i32>(pixels[0]) == 255);
        REQUIRE(static_cast<i32>(pixels[2]) == 0);

        Opal::DynamicArray<u8> depth_bytes(k_table_side * k_table_side * sizeof(f32));
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), depth, depth_bytes, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        f32 written_depth = 0.0f;
        memcpy(&written_depth, depth_bytes.GetData(), sizeof(f32));
        // What the first pass left. The pass that followed drew a fragment that passed the test and wrote
        // none of it, which is the whole point of the layout.
        REQUIRE(written_depth == Catch::Approx(k_stored_depth).margin(0.0001));
    }
    SECTION("A stencil attachment with no depth beside it stands on its own texture")
    {
        // The only way a stencil attachment gets a texture of its own: Vulkan requires one image where both
        // sides are present, so a pass that writes stencil and no depth is what a separate texture is for.
        constexpr u32 k_reference = 0x2A;
        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        Forge::Texture stencil = MakeStencilTarget(fixture.device);

        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
        pipeline_desc.depth_stencil.stencil_test_enabled = true;
        pipeline_desc.depth_stencil.front_stencil_comparator = Comparator::Always;
        pipeline_desc.depth_stencil.back_stencil_comparator = Comparator::Always;
        pipeline_desc.depth_stencil.front_pass = StencilOperation::Replace;
        pipeline_desc.depth_stencil.back_pass = StencilOperation::Replace;
        pipeline_desc.depth_stencil.front_reference = k_reference;
        pipeline_desc.depth_stencil.back_reference = k_reference;
        pipeline_desc.depth_stencil.front_write_mask = 0xFF;
        pipeline_desc.depth_stencil.back_write_mask = 0xFF;
        pipeline_desc.stencil_attachment_format = k_table_depth_stencil_format;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        const ErrorCode submit_status = Forge::ImmediateSubmit(
            fixture.device, fixture.GetQueue(),
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(stencil)) == ErrorCode::Success);
                const Forge::RenderingDesc rendering_desc{
                    .render_area_extent = {k_table_side, k_table_side},
                    .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                         .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                         .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                         .clear_value = Vector4f{0.0f, 0.0f, 0.0f, 1.0f}}},
                    .stencil_attachment =
                        Forge::RenderingAttachmentDesc{.texture = stencil,
                                                       .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                       .store_operation = Forge::AttachmentStoreOperation::Store,
                                                       .clear_value = Forge::DepthStencilClearValue{1.0f, 0}}};
                REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindVertexBuffer(stored_quad, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(green)) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
            });
        REQUIRE(submit_status == ErrorCode::Success);

        // The reference landed in the texture the stencil attachment named, and nothing else was attached to
        // carry it.
        REQUIRE(static_cast<i32>(ReadStencilValue(fixture, stencil)) == static_cast<i32>(k_reference));
    }
    SECTION("A stencil attachment on a texture other than the depth one is refused")
    {
        // Where both sides are present Vulkan wants one image for the two, so this pair of descs describes a
        // pass no device will begin.
        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        Forge::Texture depth = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                                                                        {.format = k_table_depth_stencil_format,
                                                                         .width = k_table_side,
                                                                         .height = k_table_side,
                                                                         .usage = Forge::TextureUsageBits::DepthStencilAttachment}));
        Forge::Texture stencil = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                                                                          {.format = k_table_depth_stencil_format,
                                                                           .width = k_table_side,
                                                                           .height = k_table_side,
                                                                           .usage = Forge::TextureUsageBits::DepthStencilAttachment}));

        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth)) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(stencil)) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{
            .render_area_extent = {k_table_side, k_table_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}},
            .depth_attachment = Forge::RenderingAttachmentDesc{.texture = depth,
                                                               .load_operation = Forge::AttachmentLoadOperation::Load},
            .stencil_attachment = Forge::RenderingAttachmentDesc{.texture = stencil,
                                                                 .load_operation = Forge::AttachmentLoadOperation::Load}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge reading the depth aspect of a combined depth-stencil format", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));

    constexpr f32 k_depth = 0.5f;
    const Forge::Buffer quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(k_depth));
    const Vector4f unused_color = ByteColor(0, 0, 0, 255);

    Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
    pipeline_desc.depth_stencil.depth_test_enabled = true;
    pipeline_desc.depth_stencil.depth_write_enabled = true;
    pipeline_desc.depth_stencil.depth_comparator = Comparator::Always;
    pipeline_desc.depth_attachment_format = k_table_depth_stencil_format;
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
    Forge::Texture depth_stencil = MakeStencilTarget(fixture.device);

    // Only the depth attachment is named - the stencil aspect of this same texture is never touched by the
    // pass, which is legal, and what makes the read below about the depth aspect specifically.
    REQUIRE(Forge::ImmediateSubmit(
                fixture.device, fixture.GetQueue(),
                [&](Forge::CommandBuffer& command_buffer)
                {
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth_stencil)) ==
                            ErrorCode::Success);
                    const Forge::RenderingDesc rendering_desc{
                        .render_area_extent = {k_table_side, k_table_side},
                        .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                             .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                             .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                             .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}}},
                        .depth_attachment = Forge::RenderingAttachmentDesc{
                            .texture = depth_stencil,
                            .load_operation = Forge::AttachmentLoadOperation::Clear,
                            .store_operation = Forge::AttachmentStoreOperation::Store,
                            .clear_value = Forge::DepthStencilClearValue{1.0f, 0}}};
                    REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdBindVertexBuffer(quad, 0) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(unused_color)) ==
                            ErrorCode::Success);
                    REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                }) == ErrorCode::Success);

    const f32 depth = ReadDepthValue(fixture, depth_stencil);
    INFO("depth " << depth);
    REQUIRE(depth == Catch::Approx(k_depth).margin(0.001));
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * Every StencilOperation, checked by what it leaves in the stencil buffer rather than by the pipeline having
 * been accepted. One entry of ToVkStencilOp swapped for another is a mistake nothing reports - both are legal
 * operations and the layer has no view on which one was meant - and the value that comes back is the only
 * witness there is.
 *
 * The clamping pair and the wrapping pair agree everywhere except at the end of the range they clamp at, so
 * each of the four is run at a stored value where it differs from its partner as well as at one where it
 * does not.
 */
TEST_CASE("Forge the stencil operations", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer full_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.5f));
    const Vector4f unused_color = ByteColor(0, 0, 0, 255);

    // Replace with a dynamic reference is how the buffer is seeded, so one pipeline serves every starting
    // value and the operation under test gets a pipeline of its own.
    const Forge::Pipeline seed_pipeline =
        MakeStencilWritePipeline(fixture.device, vertex_shader, fragment_shader, StencilOperation::Replace);

    /** Seed the stencil buffer with `stored`, apply `operation` against `reference`, and read what is left. */
    auto apply = [&](StencilOperation operation, u8 stored, u8 reference)
    {
        const Forge::Pipeline operation_pipeline =
            MakeStencilWritePipeline(fixture.device, vertex_shader, fragment_shader, operation);
        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        Forge::Texture depth_stencil = MakeStencilTarget(fixture.device);

        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   BeginTableRendering(command_buffer, color, depth_stencil, full_quad, 0);

                                   REQUIRE(command_buffer.CmdBindPipeline(seed_pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdSetStencilReference(stored) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(seed_pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(unused_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);

                                   REQUIRE(command_buffer.CmdBindPipeline(operation_pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdSetStencilReference(reference) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(operation_pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(unused_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                               }) == ErrorCode::Success);
        return ReadStencilValue(fixture, depth_stencil);
    };

    constexpr StencilOperation k_operations[] = {StencilOperation::Keep,          StencilOperation::Zero,
                                                 StencilOperation::Replace,       StencilOperation::Increment,
                                                 StencilOperation::IncrementWrap, StencilOperation::Decrement,
                                                 StencilOperation::DecrementWrap, StencilOperation::Invert};
    static_assert(sizeof(k_operations) / sizeof(k_operations[0]) == static_cast<i32>(StencilOperation::EnumCount),
                  "Every StencilOperation has to be in this table.");

    // A seed the clamping pair does not saturate at, so Increment and IncrementWrap agree here and what tells
    // the rest apart is what each one does rather than where the range ends.
    constexpr u8 k_middle_seed = 5;
    constexpr u8 k_reference = 200;

    SECTION("Each operation leaves what its definition says, away from the ends of the range")
    {
        for (const StencilOperation operation : k_operations)
        {
            INFO("operation " << StencilOperationName(operation));
            REQUIRE(apply(operation, k_middle_seed, k_reference) ==
                    ApplyStencilOperation(operation, k_middle_seed, k_reference));
        }
    }
    SECTION("Clamping and wrapping part company at the end of the range")
    {
        // The one place Increment differs from IncrementWrap, and the whole of what tells those two entries
        // apart: everywhere else they are the same function.
        REQUIRE(apply(StencilOperation::Increment, 0xFF, k_reference) == 0xFF);
        REQUIRE(apply(StencilOperation::IncrementWrap, 0xFF, k_reference) == 0x00);
        REQUIRE(apply(StencilOperation::Decrement, 0x00, k_reference) == 0x00);
        REQUIRE(apply(StencilOperation::DecrementWrap, 0x00, k_reference) == 0xFF);
    }
    SECTION("No two operations agree on all three seeds, so a swapped pair has somewhere to show")
    {
        // What the two sections above rest on. Two entries exchanged is only visible where the values they
        // produce differ, and this says they do rather than leaving it assumed.
        for (const StencilOperation left : k_operations)
        {
            for (const StencilOperation right : k_operations)
            {
                if (left == right)
                {
                    continue;
                }
                INFO(StencilOperationName(left) << " against " << StencilOperationName(right));
                const bool differ_in_the_middle = ApplyStencilOperation(left, k_middle_seed, k_reference) !=
                                                  ApplyStencilOperation(right, k_middle_seed, k_reference);
                const bool differ_at_the_top =
                    ApplyStencilOperation(left, 0xFF, k_reference) != ApplyStencilOperation(right, 0xFF, k_reference);
                const bool differ_at_the_bottom =
                    ApplyStencilOperation(left, 0x00, k_reference) != ApplyStencilOperation(right, 0x00, k_reference);
                REQUIRE((differ_in_the_middle || differ_at_the_top || differ_at_the_bottom));
            }
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** Whether one comparator passes for a reference on the left and a stored value on the right. */
bool ComparatorPasses(Comparator comparator, u8 reference, u8 stored)
{
    switch (comparator)
    {
        case Comparator::Never:
            return false;
        case Comparator::Always:
            return true;
        case Comparator::Less:
            return reference < stored;
        case Comparator::Greater:
            return reference > stored;
        case Comparator::Equal:
            return reference == stored;
        case Comparator::NotEqual:
            return reference != stored;
        case Comparator::LessEqual:
            return reference <= stored;
        case Comparator::GreaterEqual:
            return reference >= stored;
        default:
            FAIL("Unhandled comparator");
            return false;
    }
}

const char* ComparatorName(Comparator comparator)
{
    switch (comparator)
    {
        case Comparator::Never:
            return "Never";
        case Comparator::Always:
            return "Always";
        case Comparator::Less:
            return "Less";
        case Comparator::Greater:
            return "Greater";
        case Comparator::Equal:
            return "Equal";
        case Comparator::NotEqual:
            return "NotEqual";
        case Comparator::LessEqual:
            return "LessEqual";
        case Comparator::GreaterEqual:
            return "GreaterEqual";
        default:
            return "?";
    }
}

}  // namespace

/**
 * Every Comparator, through the stencil test, checked by whether the draw survived it. ToVkCompareOp is one
 * switch serving the depth test, the stencil test and a comparison sampler alike, and two of its entries
 * exchanged - Less for LessEqual, say, or Greater for Less - produces a pipeline the layer is perfectly happy
 * with and a picture that is subtly wrong.
 *
 * The stencil test compares the reference on the left against the stored value on the right, so a fixed
 * stored value and three references around it - below, equal, above - give each comparator a three way answer
 * of its own. All eight of those answers differ, which is what makes the check a check rather than a
 * demonstration; the last section is where that is asserted rather than assumed.
 */
TEST_CASE("Forge the comparators", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer full_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.5f));
    const Vector4f unused_color = ByteColor(0, 0, 0, 255);
    const Vector4f paint_color = ByteColor(0, 255, 0, 255);

    const Forge::Pipeline seed_pipeline =
        MakeStencilWritePipeline(fixture.device, vertex_shader, fragment_shader, StencilOperation::Replace);

    /** Paints green where the stencil test passes, and leaves the buffer exactly as it found it. */
    auto make_probe_pipeline = [&](Comparator comparator)
    {
        Forge::GraphicsPipelineDesc pipeline_desc =
            MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
        pipeline_desc.depth_stencil.stencil_test_enabled = true;
        pipeline_desc.depth_stencil.front_stencil_comparator = comparator;
        pipeline_desc.depth_stencil.back_stencil_comparator = comparator;
        // Keep on every outcome and a write mask of zero: this draw reads the buffer and never touches it,
        // so what it reports is the comparison and nothing downstream of it.
        pipeline_desc.depth_stencil.front_write_mask = 0;
        pipeline_desc.depth_stencil.back_write_mask = 0;
        pipeline_desc.depth_attachment_format = k_table_depth_stencil_format;
        pipeline_desc.stencil_attachment_format = k_table_depth_stencil_format;
        pipeline_desc.dynamic_state = Forge::DynamicStateBits::StencilReference;
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    };

    /** Seed the buffer with `stored`, test `reference` against it through `comparator`, and say if green landed. */
    auto passes = [&](Comparator comparator, u8 reference, u8 stored)
    {
        const Forge::Pipeline probe_pipeline = make_probe_pipeline(comparator);
        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        Forge::Texture depth_stencil = MakeStencilTarget(fixture.device);

        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   BeginTableRendering(command_buffer, color, depth_stencil, full_quad, 0);

                                   REQUIRE(command_buffer.CmdBindPipeline(seed_pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdSetStencilReference(stored) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(seed_pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(unused_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);

                                   REQUIRE(command_buffer.CmdBindPipeline(probe_pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdSetStencilReference(reference) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(probe_pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(paint_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                               }) == ErrorCode::Success);

        Opal::DynamicArray<u8> pixels(k_table_side * k_table_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        // The whole target was covered by one draw, so it is green everywhere or red everywhere.
        const bool painted = pixels[0] == 0 && pixels[1] == 255;
        for (i32 texel = 1; texel < k_table_side * k_table_side; ++texel)
        {
            REQUIRE((pixels[texel * 4] == 0 && pixels[texel * 4 + 1] == 255) == painted);
        }
        return painted;
    };

    constexpr Comparator k_comparators[] = {Comparator::Never,    Comparator::Always,    Comparator::Less,
                                            Comparator::Greater,  Comparator::Equal,     Comparator::NotEqual,
                                            Comparator::LessEqual, Comparator::GreaterEqual};
    static_assert(sizeof(k_comparators) / sizeof(k_comparators[0]) == static_cast<i32>(Comparator::EnumCount),
                  "Every Comparator has to be in this table.");

    constexpr u8 k_stored = 5;
    constexpr u8 k_references[] = {k_stored - 1, k_stored, k_stored + 1};

    SECTION("Each comparator answers the three references the way its definition says")
    {
        for (const Comparator comparator : k_comparators)
        {
            for (const u8 reference : k_references)
            {
                INFO(ComparatorName(comparator) << " with reference " << static_cast<i32>(reference) << " against stored "
                                                << static_cast<i32>(k_stored));
                REQUIRE(passes(comparator, reference, k_stored) == ComparatorPasses(comparator, reference, k_stored));
            }
        }
    }
    SECTION("No two comparators answer all three the same way, so a swapped pair has somewhere to show")
    {
        for (const Comparator left : k_comparators)
        {
            for (const Comparator right : k_comparators)
            {
                if (left == right)
                {
                    continue;
                }
                INFO(ComparatorName(left) << " against " << ComparatorName(right));
                bool differ_somewhere = false;
                for (const u8 reference : k_references)
                {
                    differ_somewhere = differ_somewhere || ComparatorPasses(left, reference, k_stored) !=
                                                               ComparatorPasses(right, reference, k_stored);
                }
                REQUIRE(differ_somewhere);
            }
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/**
 * What one blend factor multiplies its side of the equation by, per channel, spelled out from the Vulkan
 * definition. A Vector4f rather than a scalar because a factor is per channel, and the alpha equation uses
 * the alpha component of the same factor - so one value serves both halves as long as the pipeline names the
 * same factor for colour and for alpha, which the case below does.
 */
Vector4f BlendFactorValue(BlendFactor factor, const Vector4f& src, const Vector4f& dst, const Vector4f& constant)
{
    auto all = [](f32 value) { return Vector4f{value, value, value, value}; };
    auto invert = [](const Vector4f& value)
    { return Vector4f{1.0f - value.x, 1.0f - value.y, 1.0f - value.z, 1.0f - value.w}; };
    switch (factor)
    {
        case BlendFactor::Zero:
            return all(0.0f);
        case BlendFactor::One:
            return all(1.0f);
        case BlendFactor::SrcColor:
            return src;
        case BlendFactor::DstColor:
            return dst;
        case BlendFactor::InvSrcColor:
            return invert(src);
        case BlendFactor::InvDstColor:
            return invert(dst);
        case BlendFactor::SrcAlpha:
            return all(src.w);
        case BlendFactor::DstAlpha:
            return all(dst.w);
        case BlendFactor::InvSrcAlpha:
            return all(1.0f - src.w);
        case BlendFactor::InvDstAlpha:
            return all(1.0f - dst.w);
        case BlendFactor::ConstColor:
            return constant;
        case BlendFactor::InvConstColor:
            return invert(constant);
        case BlendFactor::ConstAlpha:
            return all(constant.w);
        case BlendFactor::InvConstAlpha:
            return all(1.0f - constant.w);
        default:
            FAIL("Unhandled blend factor");
            return all(0.0f);
    }
}

const char* BlendFactorName(BlendFactor factor)
{
    switch (factor)
    {
        case BlendFactor::Zero:
            return "Zero";
        case BlendFactor::One:
            return "One";
        case BlendFactor::SrcColor:
            return "SrcColor";
        case BlendFactor::DstColor:
            return "DstColor";
        case BlendFactor::InvSrcColor:
            return "InvSrcColor";
        case BlendFactor::InvDstColor:
            return "InvDstColor";
        case BlendFactor::SrcAlpha:
            return "SrcAlpha";
        case BlendFactor::DstAlpha:
            return "DstAlpha";
        case BlendFactor::InvSrcAlpha:
            return "InvSrcAlpha";
        case BlendFactor::InvDstAlpha:
            return "InvDstAlpha";
        case BlendFactor::ConstColor:
            return "ConstColor";
        case BlendFactor::InvConstColor:
            return "InvConstColor";
        case BlendFactor::ConstAlpha:
            return "ConstAlpha";
        case BlendFactor::InvConstAlpha:
            return "InvConstAlpha";
        default:
            return "?";
    }
}

const char* BlendOperationName(BlendOperation operation)
{
    switch (operation)
    {
        case BlendOperation::Add:
            return "Add";
        case BlendOperation::Subtract:
            return "Subtract";
        case BlendOperation::ReverseSubtract:
            return "ReverseSubtract";
        case BlendOperation::Min:
            return "Min";
        case BlendOperation::Max:
            return "Max";
        default:
            return "?";
    }
}

/** One channel of a blend equation, clamped the way a UNORM attachment clamps it. */
f32 BlendChannel(BlendOperation operation, f32 weighted_src, f32 weighted_dst, f32 src, f32 dst)
{
    f32 result = 0.0f;
    switch (operation)
    {
        case BlendOperation::Add:
            result = weighted_src + weighted_dst;
            break;
        case BlendOperation::Subtract:
            result = weighted_src - weighted_dst;
            break;
        case BlendOperation::ReverseSubtract:
            result = weighted_dst - weighted_src;
            break;
        // Min and Max ignore both factors, which is what Vulkan says of them and the one thing about these
        // two that a test naming factors either side of them could get wrong.
        case BlendOperation::Min:
            result = Opal::Min(src, dst);
            break;
        case BlendOperation::Max:
            result = Opal::Max(src, dst);
            break;
        default:
            FAIL("Unhandled blend operation");
            break;
    }
    return Opal::Clamp(result, 0.0f, 1.0f);
}

/** The whole blend equation on the CPU, as four channels of the same form. */
Vector4f BlendOnTheCpu(BlendFactor src_factor, BlendFactor dst_factor, BlendOperation operation, const Vector4f& src,
                       const Vector4f& dst, const Vector4f& constant)
{
    const Vector4f src_weight = BlendFactorValue(src_factor, src, dst, constant);
    const Vector4f dst_weight = BlendFactorValue(dst_factor, src, dst, constant);
    return {BlendChannel(operation, src.x * src_weight.x, dst.x * dst_weight.x, src.x, dst.x),
            BlendChannel(operation, src.y * src_weight.y, dst.y * dst_weight.y, src.y, dst.y),
            BlendChannel(operation, src.z * src_weight.z, dst.z * dst_weight.z, src.z, dst.z),
            BlendChannel(operation, src.w * src_weight.w, dst.w * dst_weight.w, src.w, dst.w)};
}

/** How far apart two blended results are, in the byte levels a UNORM attachment stores them at. */
f32 LargestChannelGap(const Vector4f& left, const Vector4f& right)
{
    const f32 gaps[] = {Opal::Abs(left.x - right.x), Opal::Abs(left.y - right.y), Opal::Abs(left.z - right.z),
                        Opal::Abs(left.w - right.w)};
    f32 largest = 0.0f;
    for (const f32 gap : gaps)
    {
        largest = Opal::Max(largest, gap);
    }
    return largest * 255.0f;
}

}  // namespace

/**
 * Every BlendFactor and every BlendOperation, checked against the equation each one stands for rather than
 * against the pipeline having been accepted. Both tables are switches over legal Vulkan values, so two
 * entries exchanged builds a pipeline nothing complains about and blends the wrong thing forever.
 *
 * The colours are byte values a UNORM attachment stores exactly, but a factor multiplies two of them together
 * and the product is not a byte value, so the comparison carries a tolerance of two levels. What makes that
 * safe is the last section: no two factors here land within ten levels of each other, so the tolerance can
 * never let one be read as another.
 *
 * The four constant factors were dead until this case: nothing set VkPipelineColorBlendStateCreateInfo's
 * blendConstants, so ConstColor and ConstAlpha meant zero and their inverses meant one, which is what
 * BlendFactor::Zero and ::One already say. GraphicsPipelineDesc::blend_constants is what makes them mean
 * anything, and this is the only caller of it.
 */
TEST_CASE("Forge the blend factors and operations", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer full_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.5f));

    /**
     * Clear the target to `dst`, draw `src` over it through the given blend state, and hand back what the
     * attachment ended up holding. The clear is what puts the destination there: the target is created fresh
     * for every run, so nothing carries over from the run before.
     */
    auto blend = [&](BlendFactor src_factor, BlendFactor dst_factor, BlendOperation operation, const Vector4f& src,
                     const Vector4f& dst, const Vector4f& constant)
    {
        Forge::GraphicsPipelineDesc pipeline_desc =
            MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
        // The alpha equation is given the same three, so all four channels follow one formula and the alpha
        // component of the factor is what the alpha channel gets - which is what BlendFactorValue models.
        pipeline_desc.color_blend_attachments[0] = Forge::ColorBlendDesc{.blend_enabled = true,
                                                                        .src_color_factor = src_factor,
                                                                        .dst_color_factor = dst_factor,
                                                                        .color_operation = operation,
                                                                        .src_alpha_factor = src_factor,
                                                                        .dst_alpha_factor = dst_factor,
                                                                        .alpha_operation = operation};
        pipeline_desc.blend_constants = constant;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(
            fixture, color, k_table_side,
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindVertexBuffer(full_quad, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(src)) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
            },
            dst);
        return ByteColor(pixels[0], pixels[1], pixels[2], pixels[3]);
    };

    /** Two colours agree to within the rounding a UNORM attachment does, and no further. */
    auto require_same_color = [](const Vector4f& measured, const Vector4f& expected)
    {
        INFO("measured " << measured.x << " " << measured.y << " " << measured.z << " " << measured.w);
        INFO("expected " << expected.x << " " << expected.y << " " << expected.z << " " << expected.w);
        REQUIRE(LargestChannelGap(measured, expected) <= 2.0f);
    };

    auto reads_the_constants = [](BlendFactor factor)
    {
        return factor == BlendFactor::ConstColor || factor == BlendFactor::InvConstColor ||
               factor == BlendFactor::ConstAlpha || factor == BlendFactor::InvConstAlpha;
    };

    constexpr BlendFactor k_factors[] = {
        BlendFactor::Zero,       BlendFactor::One,           BlendFactor::SrcColor,      BlendFactor::DstColor,
        BlendFactor::InvSrcColor, BlendFactor::InvDstColor,  BlendFactor::SrcAlpha,      BlendFactor::DstAlpha,
        BlendFactor::InvSrcAlpha, BlendFactor::InvDstAlpha,  BlendFactor::ConstColor,    BlendFactor::InvConstColor,
        BlendFactor::ConstAlpha,  BlendFactor::InvConstAlpha};
    static_assert(sizeof(k_factors) / sizeof(k_factors[0]) == static_cast<i32>(BlendFactor::EnumCount),
                  "Every BlendFactor has to be in this table.");

    constexpr BlendOperation k_operations[] = {BlendOperation::Add, BlendOperation::Subtract,
                                               BlendOperation::ReverseSubtract, BlendOperation::Min,
                                               BlendOperation::Max};
    static_assert(sizeof(k_operations) / sizeof(k_operations[0]) == static_cast<i32>(BlendOperation::EnumCount),
                  "Every BlendOperation has to be in this table.");

    // Byte values, so the source and the destination themselves survive a UNORM attachment exactly and the
    // only rounding in the answer is what the blend introduced. Chosen so that no two factors below land on
    // the same colour - which the last section is what checks.
    const Vector4f src = ByteColor(204, 153, 102, 153);
    const Vector4f dst = ByteColor(77, 179, 26, 51);
    const Vector4f constant = ByteColor(26, 77, 128, 191);

    // llvmpipe somewhere between Mesa 24.3 and 25.2 started reading the blend constants rotated one channel
    // to the right - a constant of (r, g, b, a) blends as (a, r, g, b) - and all four constant factors are
    // wrong the same way whether the constants are static pipeline state or set on the command buffer. A
    // driver bug is not something a pipeline can express its way around, so where it is present the four
    // factors that read the constants sit out and the ten that never do keep running. Present is measured
    // rather than read off the device name, because CI's lavapipe predates the bug and a Mesa that fixes it
    // should get its coverage back without anyone editing a version check. The diagnosis, with the
    // measurements behind it, is in docs/linux-windowing-plan.md.
    auto constants_arrive_rotated = [&]
    {
        const Vector4f rotated = {constant.w, constant.x, constant.y, constant.z};
        const Vector4f measured = blend(BlendFactor::ConstColor, BlendFactor::Zero, BlendOperation::Add, src, dst, constant);
        const Vector4f if_rotated = BlendOnTheCpu(BlendFactor::ConstColor, BlendFactor::Zero, BlendOperation::Add, src, dst, rotated);
        return LargestChannelGap(measured, if_rotated) <= 2.0f;
    };

    SECTION("Each factor weights the source the way its definition says")
    {
        // The destination is multiplied by zero and added, so what lands is the source through the factor
        // under test and nothing else.
        const bool skip_constant_factors = constants_arrive_rotated();
        if (skip_constant_factors)
        {
            WARN("The constant factors sit this section out: this driver reads the blend constants rotated.");
        }
        for (const BlendFactor factor : k_factors)
        {
            if (skip_constant_factors && reads_the_constants(factor))
            {
                continue;
            }
            INFO("source factor " << BlendFactorName(factor));
            require_same_color(blend(factor, BlendFactor::Zero, BlendOperation::Add, src, dst, constant),
                               BlendOnTheCpu(factor, BlendFactor::Zero, BlendOperation::Add, src, dst, constant));
        }
    }
    SECTION("Each factor weights the destination the same way")
    {
        // The other side of the equation, which is a separate field reaching the same table - so a mistake
        // that reads dst_color_factor through the wrong translation shows here and not above.
        const bool skip_constant_factors = constants_arrive_rotated();
        if (skip_constant_factors)
        {
            WARN("The constant factors sit this section out: this driver reads the blend constants rotated.");
        }
        for (const BlendFactor factor : k_factors)
        {
            if (skip_constant_factors && reads_the_constants(factor))
            {
                continue;
            }
            INFO("destination factor " << BlendFactorName(factor));
            require_same_color(blend(BlendFactor::Zero, factor, BlendOperation::Add, src, dst, constant),
                               BlendOnTheCpu(BlendFactor::Zero, factor, BlendOperation::Add, src, dst, constant));
        }
    }
    SECTION("Each operation combines the two sides the way its definition says")
    {
        // Both factors One, so the operation is the whole of what differs. A dimmer source than the sections
        // above use, so that Add lands short of white and is a value rather than a clamp.
        const Vector4f dim_src = ByteColor(102, 51, 128, 153);
        for (const BlendOperation operation : k_operations)
        {
            INFO("operation " << BlendOperationName(operation));
            require_same_color(blend(BlendFactor::One, BlendFactor::One, operation, dim_src, dst, constant),
                               BlendOnTheCpu(BlendFactor::One, BlendFactor::One, operation, dim_src, dst, constant));
        }
    }
    SECTION("No two factors and no two operations land within the tolerance of each other")
    {
        // What the three sections above rest on. A tolerance of two levels is only safe while the values it
        // has to tell apart are further apart than that, and this is where that is asserted rather than
        // eyeballed once and left to rot.
        for (const BlendFactor left : k_factors)
        {
            for (const BlendFactor right : k_factors)
            {
                if (left == right)
                {
                    continue;
                }
                INFO(BlendFactorName(left) << " against " << BlendFactorName(right));
                const Vector4f left_color = BlendOnTheCpu(left, BlendFactor::Zero, BlendOperation::Add, src, dst, constant);
                const Vector4f right_color = BlendOnTheCpu(right, BlendFactor::Zero, BlendOperation::Add, src, dst, constant);
                REQUIRE(LargestChannelGap(left_color, right_color) > 8.0f);
            }
        }
        const Vector4f dim_src = ByteColor(102, 51, 128, 153);
        for (const BlendOperation left : k_operations)
        {
            for (const BlendOperation right : k_operations)
            {
                if (left == right)
                {
                    continue;
                }
                INFO(BlendOperationName(left) << " against " << BlendOperationName(right));
                const Vector4f left_color = BlendOnTheCpu(BlendFactor::One, BlendFactor::One, left, dim_src, dst, constant);
                const Vector4f right_color = BlendOnTheCpu(BlendFactor::One, BlendFactor::One, right, dim_src, dst, constant);
                REQUIRE(LargestChannelGap(left_color, right_color) > 8.0f);
            }
        }
    }
    SECTION("A constant factor is the constant the pipeline named, not zero")
    {
        // The regression this section exists for, and the one thing the sections above cannot catch on their
        // own: with blendConstants left unset, ConstColor weighs its side by zero and InvConstColor by one,
        // which is exactly what BlendFactor::Zero and ::One already mean. Compared against those two rather
        // than against the model, so a model that made the same mistake would not hide it.
        const Vector4f through_zero = blend(BlendFactor::Zero, BlendFactor::Zero, BlendOperation::Add, src, dst, constant);
        const Vector4f through_one = blend(BlendFactor::One, BlendFactor::Zero, BlendOperation::Add, src, dst, constant);
        const BlendFactor k_collapses_onto_zero[] = {BlendFactor::ConstColor, BlendFactor::ConstAlpha};
        const BlendFactor k_collapses_onto_one[] = {BlendFactor::InvConstColor, BlendFactor::InvConstAlpha};
        for (const BlendFactor factor : k_collapses_onto_zero)
        {
            INFO(BlendFactorName(factor) << " must not have collapsed onto Zero");
            REQUIRE(LargestChannelGap(blend(factor, BlendFactor::Zero, BlendOperation::Add, src, dst, constant),
                                      through_zero) > 8.0f);
        }
        for (const BlendFactor factor : k_collapses_onto_one)
        {
            INFO(BlendFactorName(factor) << " must not have collapsed onto One");
            REQUIRE(LargestChannelGap(blend(factor, BlendFactor::Zero, BlendOperation::Add, src, dst, constant),
                                      through_one) > 8.0f);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** The index WrapTexelIndex reports for a coordinate that fell outside the texture under Border. */
constexpr i32 k_border_texel = -1;

/** The `mirror` of the Vulkan wrapping table: the index folded about the left edge of the texture. */
i32 MirrorIndex(i32 index)
{
    return index >= 0 ? index : -(1 + index);
}

/**
 * Which texel of a row of `size` an unnormalized index lands on, straight out of the wrapping table in the
 * image operations chapter. `k_border_texel` when the coordinate fell outside and the mode is Border.
 *
 * A second expression of what ToVkSamplerAddressMode maps onto, written from the specification rather than
 * from that switch - which is what lets two of its entries being exchanged show up as a texel that is the
 * wrong colour instead of as a sampler that builds perfectly well.
 */
i32 WrapTexelIndex(ImageAddressMode mode, i32 index, i32 size)
{
    // The remainder Vulkan means, which is the non-negative one - C leaves the sign of a negative operand to
    // the implementation and every coordinate below the texture would come out on the wrong texel.
    auto wrapped_mod = [](i32 value, i32 divisor)
    {
        const i32 remainder = value % divisor;
        return remainder < 0 ? remainder + divisor : remainder;
    };
    switch (mode)
    {
        case ImageAddressMode::Repeat:
            return wrapped_mod(index, size);
        case ImageAddressMode::MirrorRepeat:
            return (size - 1) - MirrorIndex(wrapped_mod(index, 2 * size) - size);
        case ImageAddressMode::Clamp:
            return Opal::Clamp(index, 0, size - 1);
        case ImageAddressMode::Border:
            return index < 0 || index >= size ? k_border_texel : index;
        // Mirrored once about the left edge and then held there, which is where this parts company with
        // MirrorRepeat: the latter goes on folding.
        case ImageAddressMode::MirrorOnce:
            return Opal::Clamp(MirrorIndex(index), 0, size - 1);
        default:
            FAIL("Unhandled image address mode");
            return 0;
    }
}

const char* ImageAddressModeName(ImageAddressMode mode)
{
    switch (mode)
    {
        case ImageAddressMode::Clamp:
            return "Clamp";
        case ImageAddressMode::Border:
            return "Border";
        case ImageAddressMode::Repeat:
            return "Repeat";
        case ImageAddressMode::MirrorRepeat:
            return "MirrorRepeat";
        case ImageAddressMode::MirrorOnce:
            return "MirrorOnce";
        default:
            return "?";
    }
}

const char* BorderColorName(BorderColor border_color)
{
    switch (border_color)
    {
        case BorderColor::TransparentBlack:
            return "TransparentBlack";
        case BorderColor::OpaqueBlack:
            return "OpaqueBlack";
        case BorderColor::OpaqueWhite:
            return "OpaqueWhite";
        default:
            return "?";
    }
}

/** What each BorderColor stands for, which is the whole of what ToVkBorderColor has to get right. */
Vector4f BorderColorValue(BorderColor border_color)
{
    switch (border_color)
    {
        case BorderColor::TransparentBlack:
            return {0.0f, 0.0f, 0.0f, 0.0f};
        case BorderColor::OpaqueBlack:
            return {0.0f, 0.0f, 0.0f, 1.0f};
        case BorderColor::OpaqueWhite:
            return {1.0f, 1.0f, 1.0f, 1.0f};
        default:
            FAIL("Unhandled border color");
            return {0.0f, 0.0f, 0.0f, 0.0f};
    }
}

}  // namespace

/**
 * Every ImageAddressMode and every BorderColor, checked by which texel came back rather than by the sampler
 * having been created. Both are switches over legal Vulkan values, and until this case ToVkBorderColor had no
 * caller at all in the suite while ToVkSamplerAddressMode had two of its five.
 *
 * The texture is the two texel row the filtering case uses - red then green - and every coordinate below sits
 * outside it, since inside it every mode agrees and there is nothing to tell apart. Five coordinates, because
 * no single one separates all five modes: Clamp and MirrorOnce agree everywhere above the texture, and
 * MirrorRepeat and MirrorOnce agree everywhere until the second fold below it.
 *
 * Both of those switches used to fall through to a default that quietly answered Repeat and OpaqueBlack for a
 * value they did not know. They report now, which is what the rest of Forge does and what keeps an enumerator
 * added later from silently meaning something else.
 */
TEST_CASE("Forge the sampler address modes and border colours", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const bool has_mirror_once = CanCreateDevice({.sampler_mirror_clamp_to_edge = true});
    INFO("MIRROR_CLAMP_TO_EDGE available: " << has_mirror_once);
    ForgeFixture fixture({.sampler_mirror_clamp_to_edge = has_mirror_once});
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr i32 k_row_width = 2;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_combined_sample_source, {.entry_point = "main_sample_combined", .cache = GetShaderCache()}));

    const SampleHarness harness = MakeSampleHarness(fixture.device, 32);

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(harness.layout));
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(SampleParams)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    Forge::Texture row = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                        .width = k_row_width,
                                        .height = 1,
                                        .usage = Forge::TextureUsageBits::Sampled |
                                                 Forge::TextureUsageBits::TransferDestination}));
    const Opal::DynamicArray<u8> row_pixels = MakeTwoTexelRow();
    UploadMip(fixture.device, fixture.GetQueue(), row, {row_pixels.GetData(), row_pixels.GetSize()}, 0);

    /**
     * Sample at `u` through a nearest sampler wrapping every axis the given way. Nearest, so what comes back
     * is one whole texel and never a blend of two - which is what makes "which texel" a question with an
     * answer.
     */
    auto sample_at = [&](ImageAddressMode mode, BorderColor border_color, f32 u)
    {
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest,
                                                      .mag_filter = ImageFilter::Nearest,
                                                      .address_mode_u = mode,
                                                      .address_mode_v = mode,
                                                      .address_mode_w = mode,
                                                      .border_color = border_color}));
        return SampleOnce(fixture, harness, pipeline, row, sampler, SampleParams{.uv = {u, 0.5f}});
    };

    /** The colour a texel index stands for: the two the row holds, or the border colour for k_border_texel. */
    auto texel_color = [](i32 texel, BorderColor border_color)
    {
        if (texel == k_border_texel)
        {
            return BorderColorValue(border_color);
        }
        return texel == 0 ? Vector4f{1.0f, 0.0f, 0.0f, 1.0f} : Vector4f{0.0f, 1.0f, 0.0f, 1.0f};
    };

    auto require_same_color = [](const Vector4f& measured, const Vector4f& expected)
    {
        INFO("measured " << measured.x << " " << measured.y << " " << measured.z << " " << measured.w);
        INFO("expected " << expected.x << " " << expected.y << " " << expected.z << " " << expected.w);
        REQUIRE(measured.x == Catch::Approx(expected.x).margin(0.01));
        REQUIRE(measured.y == Catch::Approx(expected.y).margin(0.01));
        REQUIRE(measured.z == Catch::Approx(expected.z).margin(0.01));
        REQUIRE(measured.w == Catch::Approx(expected.w).margin(0.01));
    };

    // Every one of these is outside the texture, which is the only place the five modes differ from each
    // other. The three below zero are what separate the two mirroring modes from Clamp and from each other.
    constexpr f32 k_coordinates[] = {1.25f, 1.75f, -0.25f, -0.75f, -1.75f};

    Opal::DynamicArray<ImageAddressMode> modes;
    modes.PushBack(ImageAddressMode::Clamp);
    modes.PushBack(ImageAddressMode::Border);
    modes.PushBack(ImageAddressMode::Repeat);
    modes.PushBack(ImageAddressMode::MirrorRepeat);
    if (has_mirror_once)
    {
        modes.PushBack(ImageAddressMode::MirrorOnce);
    }

    SECTION("Each address mode lands on the texel the wrapping table says")
    {
        for (const ImageAddressMode mode : modes)
        {
            for (const f32 u : k_coordinates)
            {
                // The index Vulkan computes before wrapping, which is what the table is written against.
                const i32 unwrapped = static_cast<i32>(Opal::Floor(u * static_cast<f32>(k_row_width)));
                const i32 texel = WrapTexelIndex(mode, unwrapped, k_row_width);
                INFO(ImageAddressModeName(mode) << " at u " << u << " expects texel " << texel);
                require_same_color(sample_at(mode, BorderColor::OpaqueBlack, u),
                                   texel_color(texel, BorderColor::OpaqueBlack));
            }
        }
    }
    SECTION("Each border colour is the one the sampler named")
    {
        // Only Border reads it at all, and only outside the texture, so this is the whole of what
        // ToVkBorderColor can be asked. The three values differ in every channel that matters, so none of
        // them can be read as another.
        constexpr BorderColor k_border_colors[] = {BorderColor::TransparentBlack, BorderColor::OpaqueBlack,
                                                   BorderColor::OpaqueWhite};
        static_assert(sizeof(k_border_colors) / sizeof(k_border_colors[0]) == static_cast<i32>(BorderColor::EnumCount),
                      "Every BorderColor has to be in this table.");
        for (const BorderColor border_color : k_border_colors)
        {
            INFO("border colour " << BorderColorName(border_color));
            require_same_color(sample_at(ImageAddressMode::Border, border_color, 1.25f),
                               BorderColorValue(border_color));
            // And inside the texture the border colour is not consulted, whichever one it is.
            require_same_color(sample_at(ImageAddressMode::Border, border_color, 0.75f), {0.0f, 1.0f, 0.0f, 1.0f});
        }
    }
    SECTION("No two address modes answer all five coordinates the same way")
    {
        // What the first section rests on. Two entries of the table exchanged is only visible where the modes
        // disagree, and the coordinates were picked for exactly that - this is where the picking is checked.
        for (const ImageAddressMode left : modes)
        {
            for (const ImageAddressMode right : modes)
            {
                if (left == right)
                {
                    continue;
                }
                INFO(ImageAddressModeName(left) << " against " << ImageAddressModeName(right));
                bool differ_somewhere = false;
                for (const f32 u : k_coordinates)
                {
                    const i32 unwrapped = static_cast<i32>(Opal::Floor(u * static_cast<f32>(k_row_width)));
                    differ_somewhere = differ_somewhere || WrapTexelIndex(left, unwrapped, k_row_width) !=
                                                               WrapTexelIndex(right, unwrapped, k_row_width);
                }
                REQUIRE(differ_somewhere);
            }
        }
    }
    if (!has_mirror_once)
    {
        WARN("This device has no samplerMirrorClampToEdge, so ImageAddressMode::MirrorOnce went unchecked.");
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge MirrorOnce on a device without the feature is refused rather than reaching the driver", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // MIRROR_CLAMP_TO_EDGE is core in Vulkan 1.2 but still a feature, and a sampler naming it on a device that
    // did not enable it is undefined. Forge refused nothing here until the mode had a test.
    ForgeFixture fixture;
    REQUIRE_FALSE(fixture.device.GetFeatures().sampler_mirror_clamp_to_edge);
    REQUIRE(Forge::Sampler::Create(fixture.device, {.address_mode_u = ImageAddressMode::MirrorOnce}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    REQUIRE(Forge::Sampler::Create(fixture.device, {.address_mode_v = ImageAddressMode::MirrorOnce}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    REQUIRE(Forge::Sampler::Create(fixture.device, {.address_mode_w = ImageAddressMode::MirrorOnce}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    // The other four modes are what every device does, so none of them is refused on the same device.
    const Forge::Sampler unaffected =
        ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.address_mode_u = ImageAddressMode::MirrorRepeat}));
    REQUIRE(unaffected.IsValid());
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * Stencil state that differs between the two faces, which is the one mistake the existing per draw case
 * cannot see. It calls CmdSetStencilCompareMask, CmdSetStencilWriteMask and CmdSetStencilReference for
 * StencilFaceBits::Front and for ::Back, but with the same values on both - so the two faces being exchanged
 * on the way to the driver would change nothing it asserts on.
 *
 * Giving the faces different values needs the test to know which face the quad actually presents, and that
 * cannot come from the same three calls without assuming the answer. It comes from culling instead: a draw
 * with Face::Back culled either survives or does not, which says what the quad is, and Face is a different
 * enum reaching a different field. Two tables would have to be wrong in the same direction for this to pass
 * while the driver is being lied to.
 */
TEST_CASE("Forge stencil state that differs between the faces", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer full_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.5f));
    const Vector4f unused_color = ByteColor(0, 0, 0, 255);
    const Vector4f paint_color = ByteColor(0, 255, 0, 255);

    const bool quad_is_front_facing = IsQuadFrontFacing(fixture, vertex_shader, fragment_shader, full_quad);
    INFO("the quad presents its " << (quad_is_front_facing ? "front" : "back") << " face");

    /**
     * Which of a pair of values the quad's own face is due, so every expectation below is spelled once. A
     * template, since the sections that read the buffer want a stencil value out of it and the one that
     * reads the colour target wants whether the paint landed.
     */
    auto for_the_quad = [&]<typename T>(T front_value, T back_value) { return quad_is_front_facing ? front_value : back_value; };

    constexpr Forge::DynamicStateBits k_dynamic_stencil = Forge::DynamicStateBits::StencilCompareMask |
                                                          Forge::DynamicStateBits::StencilWriteMask |
                                                          Forge::DynamicStateBits::StencilReference;

    /** Stamps the reference wherever it draws, taking all three values from the command buffer. */
    const Forge::Pipeline stamp_pipeline =
        MakeStencilWritePipeline(fixture.device, vertex_shader, fragment_shader, StencilOperation::Replace, k_dynamic_stencil);

    /** Paints green where the stencil test passes, leaving the buffer alone. */
    const Forge::Pipeline probe_pipeline = [&]
    {
        Forge::GraphicsPipelineDesc pipeline_desc =
            MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
        pipeline_desc.depth_stencil.stencil_test_enabled = true;
        pipeline_desc.depth_stencil.front_stencil_comparator = Comparator::Equal;
        pipeline_desc.depth_stencil.back_stencil_comparator = Comparator::Equal;
        pipeline_desc.depth_attachment_format = k_table_depth_stencil_format;
        pipeline_desc.stencil_attachment_format = k_table_depth_stencil_format;
        pipeline_desc.dynamic_state = k_dynamic_stencil;
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    }();

    /** The three values one face is given, so a call site names them together and cannot pair them wrongly. */
    struct FaceStencil
    {
        u32 compare_mask = 0xFF;
        u32 write_mask = 0xFF;
        u32 reference = 0;
    };

    auto set_faces = [](Forge::CommandBuffer& command_buffer, const FaceStencil& front, const FaceStencil& back)
    {
        REQUIRE(command_buffer.CmdSetStencilCompareMask(front.compare_mask, Forge::StencilFaceBits::Front) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetStencilWriteMask(front.write_mask, Forge::StencilFaceBits::Front) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetStencilReference(front.reference, Forge::StencilFaceBits::Front) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetStencilCompareMask(back.compare_mask, Forge::StencilFaceBits::Back) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetStencilWriteMask(back.write_mask, Forge::StencilFaceBits::Back) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetStencilReference(back.reference, Forge::StencilFaceBits::Back) == ErrorCode::Success);
    };

    /** Stamp the quad with a different set of values per face, and hand back what the buffer holds. */
    auto stamp = [&](const FaceStencil& front, const FaceStencil& back)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        Forge::Texture depth_stencil = MakeStencilTarget(fixture.device);
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   BeginTableRendering(command_buffer, color, depth_stencil, full_quad, 0);
                                   REQUIRE(command_buffer.CmdBindPipeline(stamp_pipeline) == ErrorCode::Success);
                                   set_faces(command_buffer, front, back);
                                   REQUIRE(command_buffer.CmdPushConstants(stamp_pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(unused_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                               }) == ErrorCode::Success);
        return ReadStencilValue(fixture, depth_stencil);
    };

    /**
     * Seed the buffer with `seed` through both faces, then test it with a different set of values per face,
     * and say whether the paint landed.
     */
    auto probe = [&](u8 seed, const FaceStencil& front, const FaceStencil& back)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        Forge::Texture depth_stencil = MakeStencilTarget(fixture.device);
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   BeginTableRendering(command_buffer, color, depth_stencil, full_quad, 0);

                                   REQUIRE(command_buffer.CmdBindPipeline(stamp_pipeline) == ErrorCode::Success);
                                   const FaceStencil seeding{.compare_mask = 0xFF, .write_mask = 0xFF, .reference = seed};
                                   set_faces(command_buffer, seeding, seeding);
                                   REQUIRE(command_buffer.CmdPushConstants(stamp_pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(unused_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);

                                   REQUIRE(command_buffer.CmdBindPipeline(probe_pipeline) == ErrorCode::Success);
                                   set_faces(command_buffer, front, back);
                                   REQUIRE(command_buffer.CmdPushConstants(probe_pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(paint_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                               }) == ErrorCode::Success);
        Opal::DynamicArray<u8> pixels(k_table_side * k_table_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        return CountCovered(pixels, k_table_side) == k_table_side * k_table_side;
    };

    SECTION("A reference is written by the face it was set for")
    {
        // Two different references, one per face. Whichever face the quad presents is the one whose value
        // lands, and the two faces exchanged on the way to the driver puts the other number there.
        REQUIRE(stamp({.reference = 200}, {.reference = 100}) == for_the_quad(200, 100));
        // The same pair the other way round, so the answer cannot be whichever value happened to be set last.
        REQUIRE(stamp({.reference = 100}, {.reference = 200}) == for_the_quad(100, 200));
    }
    SECTION("A write mask stops the face it was set for and no other")
    {
        // A write mask of zero writes nothing, so the buffer keeps the zero it was cleared to. Whether the
        // stamp survived says which of the two faces the mask was applied to.
        REQUIRE(stamp({.write_mask = 0xFF, .reference = 200}, {.write_mask = 0x00, .reference = 200}) ==
                for_the_quad(200, 0));
        REQUIRE(stamp({.write_mask = 0x00, .reference = 200}, {.write_mask = 0xFF, .reference = 200}) ==
                for_the_quad(0, 200));
    }
    SECTION("A compare mask is read by the face it was set for")
    {
        // A compare mask of zero makes the test read no bits, so a reference of zero matches a stored five.
        // The full mask against the same pair does not. Which of the two the quad gets is the answer.
        constexpr u8 k_seed = 5;
        REQUIRE(probe(k_seed, {.compare_mask = 0x00, .write_mask = 0, .reference = 0},
                      {.compare_mask = 0xFF, .write_mask = 0, .reference = 0}) == for_the_quad(true, false));
        REQUIRE(probe(k_seed, {.compare_mask = 0xFF, .write_mask = 0, .reference = 0},
                      {.compare_mask = 0x00, .write_mask = 0, .reference = 0}) == for_the_quad(false, true));
    }
    SECTION("Naming both faces at once is the same as naming each of them")
    {
        // FrontAndBack is the default of all three calls and what the rest of the suite uses, so it is worth
        // one assertion that it does not mean something else entirely.
        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        Forge::Texture depth_stencil = MakeStencilTarget(fixture.device);
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   BeginTableRendering(command_buffer, color, depth_stencil, full_quad, 0);
                                   REQUIRE(command_buffer.CmdBindPipeline(stamp_pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdSetStencilCompareMask(0xFF, Forge::StencilFaceBits::FrontAndBack) ==
                                           ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdSetStencilWriteMask(0xFF, Forge::StencilFaceBits::FrontAndBack) ==
                                           ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdSetStencilReference(77, Forge::StencilFaceBits::FrontAndBack) ==
                                           ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(stamp_pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(unused_color)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                               }) == ErrorCode::Success);
        REQUIRE(ReadStencilValue(fixture, depth_stencil) == 77);
        // And it reaches the face the quad does not present as well, which is the half of FrontAndBack that
        // naming one face at a time cannot show.
        REQUIRE(stamp({.reference = 77}, {.reference = 77}) == 77);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * The stencil fail and depth fail operations, which every other stencil case leaves at Keep while it drives
 * the operation table through the pass slot alone. VkStencilOpState has three operation slots per face, and
 * a fail operation written into the pass slot, or the two fail slots exchanged, is a pipeline the layer is
 * happy with and a stencil buffer that is quietly wrong.
 *
 * All six slots are given operations that leave six different values behind, and each draw is arranged to
 * reach exactly one outcome - the stencil test fails, the stencil test passes and the depth test fails, or
 * both pass - through the comparators alone. Whatever comes back names the slot the driver actually ran.
 * The quad is drawn under both windings, so each face gets a draw it presents, and the two tables are
 * then exchanged between the faces so the one slot holding Keep - the default a dropped field would fall
 * back to - holds something else the second time round.
 */
TEST_CASE("Forge the stencil fail and depth fail operations", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer full_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.5f));
    const Vector4f unused_color = ByteColor(0, 0, 0, 255);

    const bool quad_is_front_facing = IsQuadFrontFacing(fixture, vertex_shader, fragment_shader, full_quad);
    INFO("the quad presents its " << (quad_is_front_facing ? "front" : "back") << " face under CCW");

    /** The three operation slots of one face. */
    struct FaceOperations
    {
        StencilOperation stencil_fail = StencilOperation::Keep;
        StencilOperation depth_fail = StencilOperation::Keep;
        StencilOperation pass = StencilOperation::Keep;
    };

    /** Which of the three slots a draw reaches, decided by the stencil and depth comparators. */
    enum class Outcome : u8
    {
        StencilFails,
        DepthFails,
        BothPass
    };

    constexpr u8 k_seed = 5;
    constexpr u8 k_reference = 200;
    // From the seed and reference above: 250, 6 and 200 from the first table, 0, 4 and 5 from the second.
    // Six values, so no slot can stand in for another without the readback changing.
    constexpr FaceOperations k_first_table{.stencil_fail = StencilOperation::Invert,
                                           .depth_fail = StencilOperation::IncrementWrap,
                                           .pass = StencilOperation::Replace};
    constexpr FaceOperations k_second_table{.stencil_fail = StencilOperation::Zero,
                                            .depth_fail = StencilOperation::DecrementWrap,
                                            .pass = StencilOperation::Keep};
    constexpr Outcome k_outcomes[] = {Outcome::StencilFails, Outcome::DepthFails, Outcome::BothPass};

    auto slot = [](const FaceOperations& operations, Outcome outcome)
    {
        switch (outcome)
        {
            case Outcome::StencilFails:
                return operations.stencil_fail;
            case Outcome::DepthFails:
                return operations.depth_fail;
            default:
                return operations.pass;
        }
    };
    auto outcome_name = [](Outcome outcome)
    {
        switch (outcome)
        {
            case Outcome::StencilFails:
                return "stencil fails";
            case Outcome::DepthFails:
                return "depth fails";
            default:
                return "both pass";
        }
    };

    const Forge::Pipeline seed_pipeline =
        MakeStencilWritePipeline(fixture.device, vertex_shader, fragment_shader, StencilOperation::Replace);

    /** Seed the buffer, draw once under `outcome` with the given tables and winding, and read what is left. */
    auto run = [&](const FaceOperations& front, const FaceOperations& back, Outcome outcome, WindingOrder winding)
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
        pipeline_desc.rasterizer.front_face = winding;
        pipeline_desc.color_blend_attachments[0].color_write_mask = Forge::ColorWriteMaskBits::None;
        pipeline_desc.depth_attachment_format = k_table_depth_stencil_format;
        pipeline_desc.stencil_attachment_format = k_table_depth_stencil_format;

        Forge::DepthStencilDesc& depth_stencil_desc = pipeline_desc.depth_stencil;
        // The depth test is on for every outcome, so the only thing that differs between the three
        // pipelines is the comparator deciding which slot runs. Never writing depth keeps the cleared 1.0
        // from mattering to anything but the Never comparator.
        depth_stencil_desc.depth_test_enabled = true;
        depth_stencil_desc.depth_write_enabled = false;
        depth_stencil_desc.depth_comparator = outcome == Outcome::DepthFails ? Comparator::Never : Comparator::Always;
        depth_stencil_desc.stencil_test_enabled = true;
        const Comparator stencil_comparator = outcome == Outcome::StencilFails ? Comparator::Never : Comparator::Always;
        depth_stencil_desc.front_stencil_comparator = stencil_comparator;
        depth_stencil_desc.back_stencil_comparator = stencil_comparator;
        depth_stencil_desc.front_stencil_fail = front.stencil_fail;
        depth_stencil_desc.front_depth_fail = front.depth_fail;
        depth_stencil_desc.front_pass = front.pass;
        depth_stencil_desc.back_stencil_fail = back.stencil_fail;
        depth_stencil_desc.back_depth_fail = back.depth_fail;
        depth_stencil_desc.back_pass = back.pass;
        depth_stencil_desc.front_reference = k_reference;
        depth_stencil_desc.back_reference = k_reference;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        Forge::Texture depth_stencil = MakeStencilTarget(fixture.device);
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                       [&](Forge::CommandBuffer& command_buffer)
                                       {
                                           BeginTableRendering(command_buffer, color, depth_stencil, full_quad, 0);

                                           REQUIRE(command_buffer.CmdBindPipeline(seed_pipeline) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdSetStencilReference(k_seed) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdPushConstants(seed_pipeline, ShaderTypeBits::Fragment,
                                                                                   Opal::AsBytes(unused_color)) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);

                                           REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment,
                                                                                   Opal::AsBytes(unused_color)) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                                           REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                                       }) == ErrorCode::Success);
        return ReadStencilValue(fixture, depth_stencil);
    };

    SECTION("No two slots leave the same value, so a slot wired to another has somewhere to show")
    {
        // What the section below rests on, asserted rather than assumed.
        const FaceOperations tables[] = {k_first_table, k_second_table};
        u8 values[6] = {};
        i32 count = 0;
        for (const FaceOperations& table : tables)
        {
            for (const Outcome outcome : k_outcomes)
            {
                values[count++] = ApplyStencilOperation(slot(table, outcome), k_seed, k_reference);
            }
        }
        for (i32 i = 0; i < count; ++i)
        {
            for (i32 j = i + 1; j < count; ++j)
            {
                INFO("slots " << i << " and " << j);
                REQUIRE(values[i] != values[j]);
            }
        }
    }
    SECTION("Each outcome runs the operation in its own slot of the face the quad presents")
    {
        struct Assignment
        {
            FaceOperations front;
            FaceOperations back;
        };
        const Assignment assignments[] = {{k_first_table, k_second_table}, {k_second_table, k_first_table}};
        for (const Assignment& assignment : assignments)
        {
            for (const WindingOrder winding : {WindingOrder::CCW, WindingOrder::CW})
            {
                // Flipping which winding counts as front flips which face the same quad presents.
                const bool presents_front = (winding == WindingOrder::CCW) == quad_is_front_facing;
                const FaceOperations& presented = presents_front ? assignment.front : assignment.back;
                for (const Outcome outcome : k_outcomes)
                {
                    const StencilOperation expected_operation = slot(presented, outcome);
                    INFO("the " << (presents_front ? "front" : "back") << " face, " << outcome_name(outcome)
                                << ", expecting " << StencilOperationName(expected_operation));
                    REQUIRE(static_cast<i32>(run(assignment.front, assignment.back, outcome, winding)) ==
                            static_cast<i32>(ApplyStencilOperation(expected_operation, k_seed, k_reference)));
                }
            }
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/**
 * A directory beside the shader cache for the handful of cases that need a file on disk. Created once and
 * left there: the files in it are small, named after the case that wrote them, and rewritten every run.
 */
const Opal::StringUtf8& TestScratchDirectory()
{
    static const Opal::StringUtf8 directory = []
    {
        Opal::StringUtf8 path = ForgeTest::GetTestDataPath("forge-test-scratch");
        // Already there is the ordinary case and not a failure, so the code is dropped rather than checked.
        (void)Opal::CreateDirectory(path);
        return path;
    }();
    return directory;
}

Opal::StringUtf8 TestScratchPath(const char* file_name)
{
    return Opal::Paths::Combine(*TestScratchDirectory(), file_name).GetValue();
}

/**
 * The SPIR-V Slang produces for one entry point, which is what a caller of FromSpirv* would have on hand. A build
 * without the compiler (Android) takes it from the suite's shader cache instead, which is the one a desktop run
 * filled and the device was given; an entry missing from it fails here rather than as a shader that is not there.
 */
Opal::DynamicArray<u8> CompileToSpirv(const char* source, const char* entry_point)
{
#if RNDR_SHADER_COMPILER
    ShaderCompiler compiler;
    REQUIRE(compiler.LoadModule(Opal::StringUtf8(source), ShaderOutputFormat::SpirV) == ErrorCode::Success);
    CompileResult result = ForgeTest::Unwrap(compiler.CompileEntryPoint(Opal::StringUtf8(entry_point)));
    return std::move(result.code);
#else
    Opal::DynamicArray<u8> code =
        GetShaderCache().Find(GetShaderCache().MakeKey(Opal::StringUtf8(source), Opal::StringUtf8(entry_point), ShaderOutputFormat::SpirV));
    INFO("No compiler in this build, and the shader cache has no " << entry_point);
    REQUIRE(!code.IsEmpty());
    return code;
#endif
}

}  // namespace

/**
 * Shader::FromSpirvInMemory and Shader::FromSpirvFile, which had no caller anywhere in the suite. Every other
 * shader in this file comes from Slang source, so the two entry points that take SPIR-V someone else compiled
 * were reached only by way of FromSourceInMemory calling the constructor underneath them - and never with a
 * blob this file chose.
 *
 * The SPIR-V is Slang's, compiled here through ShaderCompiler rather than read from a checked-in file, so
 * nothing has to be regenerated when the compiler moves. What that costs is that a hand-written module is
 * still unreached; what it buys is that the bytes are a real module rather than a fixture that rots.
 */
TEST_CASE("Forge shaders built from SPIR-V rather than from source", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Opal::DynamicArray<u8> spirv = CompileToSpirv(k_compute_source, "main_compute");
    REQUIRE_FALSE(spirv.IsEmpty());
    const Opal::ArrayView<const u8> spirv_view(spirv.GetData(), spirv.GetSize());

    SECTION("A module from memory carries the stage and the reflection of the entry point named")
    {
        const Forge::Shader shader =
            ForgeTest::Unwrap(Forge::Shader::FromSpirvInMemory(fixture.device, spirv_view, {.entry_point = "main_compute"}));
        REQUIRE(shader.IsValid());
        REQUIRE(shader.GetShaderStage() == ShaderTypeBits::Compute);
        REQUIRE(shader.GetNativeShaderStage() == VK_SHADER_STAGE_COMPUTE_BIT);
        REQUIRE(shader.GetEntryPoint() == Opal::StringUtf8("main_compute"));
        // The stage is read out of the SPIR-V rather than passed in, so this is the reflection running on a
        // blob the caller supplied - which is the whole difference between these two entry points and the
        // source ones.
        REQUIRE(shader.GetPushConstants().GetSize() == 1);
        REQUIRE(shader.GetInputs().IsEmpty());
    }
    SECTION("A module from memory is the same shader the source path would have built")
    {
        // The two paths meet in one constructor, so what this rules out is the SPIR-V arriving mangled -
        // truncated, byte-swapped, or handed over as a size in the wrong unit.
        const Forge::Shader from_spirv =
            ForgeTest::Unwrap(Forge::Shader::FromSpirvInMemory(fixture.device, spirv_view, {.entry_point = "main_compute"}));
        const Forge::Shader from_source = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
        REQUIRE(from_spirv.GetShaderStage() == from_source.GetShaderStage());
        REQUIRE(from_spirv.GetPushConstants().GetSize() == from_source.GetPushConstants().GetSize());
        REQUIRE(from_spirv.GetBindings().GetSize() == from_source.GetBindings().GetSize());
    }
    SECTION("A module from memory dispatches and writes what the shader says")
    {
        // A shader that was created is not a shader that runs. This is the same dispatch the rest of the file
        // uses, driven by a module that came in as bytes.
        constexpr i32 k_element_count = 256;
        constexpr i32 k_group_size = 64;
        const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random,
                                                    .use_device_address = true}));
        const Forge::Shader shader =
            ForgeTest::Unwrap(Forge::Shader::FromSpirvInMemory(fixture.device, spirv_view, {.entry_point = "main_compute"}));
        const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, shader);
        const VkDeviceAddress output_address = output.GetNativeDeviceAddress();
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute,
                                                                   Opal::AsBytes(output_address)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        RequireComputeWrote(output, k_element_count);
    }
    SECTION("An entry point the module does not hold is refused")
    {
        // Reflection finds no entry point of that name and there is nothing to take a stage from, so this
        // cannot be let through: the shader would be created with a zeroed stage and refused much later by a
        // pipeline, naming the wrong thing.
        REQUIRE(Forge::Shader::FromSpirvInMemory(fixture.device, spirv_view, {.entry_point = "no_such_entry"}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        // And the default entry point of "main", which Slang did not emit under that name here.
        REQUIRE(Forge::Shader::FromSpirvInMemory(fixture.device, spirv_view).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A blob that is not SPIR-V is refused")
    {
        const Opal::DynamicArray<u8> junk = MakeBytes(256, 17);
        REQUIRE(Forge::Shader::FromSpirvInMemory(fixture.device, {junk.GetData(), junk.GetSize()},
                                                           {.entry_point = "main_compute"}).GetErrorOr(ErrorCode::Success) == ErrorCode::CorruptData);
        // A real module cut short is the other way this arrives: the magic number is right and nothing past
        // it is.
        const Opal::ArrayView<const u8> half_a_module(spirv.GetData(), spirv.GetSize() / 2);
        REQUIRE(Forge::Shader::FromSpirvInMemory(fixture.device, half_a_module, {.entry_point = "main_compute"}).GetErrorOr(ErrorCode::Success) == ErrorCode::CorruptData);
        // And an empty one, which is what a file that was there and held nothing looks like from in here.
        REQUIRE(Forge::Shader::FromSpirvInMemory(fixture.device, {}, {.entry_point = "main_compute"}).GetErrorOr(ErrorCode::Success) == ErrorCode::CorruptData);
    }
    SECTION("A module read from a file is the same as one handed over in memory")
    {
        const Opal::StringUtf8 path = TestScratchPath("from-spirv-file.spv");
        REQUIRE(Opal::WriteBytesToFile(path, spirv_view) == Opal::ErrorCode::Success);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSpirvFile(fixture.device, path, {.entry_point = "main_compute"}));
        REQUIRE(shader.IsValid());
        REQUIRE(shader.GetShaderStage() == ShaderTypeBits::Compute);
        REQUIRE(shader.GetEntryPoint() == Opal::StringUtf8("main_compute"));
    }
    SECTION("A file that is not there, and one whose contents are not SPIR-V, are both refused")
    {
        REQUIRE(Forge::Shader::FromSpirvFile(fixture.device, TestScratchPath("no-such-file.spv"),
                                                       {.entry_point = "main_compute"}).GetErrorOr(ErrorCode::Success) == ErrorCode::FileNotFound);

        const Opal::StringUtf8 junk_path = TestScratchPath("not-spirv.spv");
        const Opal::DynamicArray<u8> junk = MakeBytes(256, 23);
        REQUIRE(Opal::WriteBytesToFile(junk_path, {junk.GetData(), junk.GetSize()}) == Opal::ErrorCode::Success);
        REQUIRE(Forge::Shader::FromSpirvFile(fixture.device, junk_path, {.entry_point = "main_compute"}).GetErrorOr(ErrorCode::Success) == ErrorCode::CorruptData);

        // A file that exists and is empty takes the same path out as one that is missing, which is the whole
        // of what ReadEntireFile can tell the two apart by.
        const Opal::StringUtf8 empty_path = TestScratchPath("empty.spv");
        REQUIRE(Opal::WriteBytesToFile(empty_path, {}) == Opal::ErrorCode::Success);
        REQUIRE(Forge::Shader::FromSpirvFile(fixture.device, empty_path, {.entry_point = "main_compute"}).GetErrorOr(ErrorCode::Success) == ErrorCode::FileNotFound);
    }
    SECTION("A file with the right bytes and the wrong entry point is refused")
    {
        const Opal::StringUtf8 path = TestScratchPath("from-spirv-file.spv");
        REQUIRE(Opal::WriteBytesToFile(path, spirv_view) == Opal::ErrorCode::Success);
        REQUIRE(Forge::Shader::FromSpirvFile(fixture.device, path, {.entry_point = "no_such_entry"}).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

/**
 * Android has no Slang, so its shaders are compiled on the host by slangc at build time (rndr_compile_shader in
 * cmake/shaders.cmake) and loaded as SPIR-V. That is the shader the desktop compiles from the same source only as
 * long as slangc is given what ShaderCompiler sets on its session, and nothing else says it is: this compiles one
 * entry point both ways, slangc with the very option list the build uses, and wants the same bytes.
 *
 * Needs no device, only the two compilers.
 */
/**
 * A matrix in a buffer, for the comparison below. slangc lays matrices out column-major unless told otherwise and
 * the session defaults to row-major, and a shader with no matrix in it compiles to the same bytes either way.
 */
constexpr const char* k_matrix_compute_source = R"(
struct Transforms
{
    float4x4 matrix;
};
RWStructuredBuffer<float4> output_buffer;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_compute(uint3 thread_id : SV_DispatchThreadID, uniform Transforms* transforms)
{
    output_buffer[thread_id.x] = mul(transforms->matrix, float4(1.0, 2.0, 3.0, 1.0));
}
)";

TEST_CASE("Forge slangc and ShaderCompiler produce the same module", "[forge]")
{
#if !defined(RNDR_TEST_SLANGC)
    SKIP("This build has no host slangc.");
#else
    const Opal::StringUtf8 slangc(RNDR_TEST_SLANGC);
    if (!Opal::Exists(slangc))
    {
        SKIP("slangc is not where the build said it would be.");
    }
    const char* source = GENERATE(k_compute_source, k_matrix_compute_source);
    const Opal::StringUtf8 source_path = TestScratchPath("slangc-equivalence.slang");
    const Opal::StringUtf8 output_path = TestScratchPath("slangc-equivalence.spv");
    REQUIRE(Opal::WriteBytesToFile(source_path, {reinterpret_cast<const u8*>(source), strlen(source)}) == Opal::ErrorCode::Success);
    // Left over from an earlier run, it would be compared in place of what slangc failed to write.
    if (Opal::Exists(output_path))
    {
        REQUIRE(Opal::DeleteFile(output_path) == Opal::ErrorCode::Success);
    }

    // cmd.exe drops the first and last quote of a line that starts with one, so the whole line is quoted once more.
#if RNDR_WINDOWS
    constexpr const char* k_command_format = "\"\"%s\" \"%s\" %s -entry main_compute -stage compute -o \"%s\"\"";
#else
    constexpr const char* k_command_format = "\"%s\" \"%s\" %s -entry main_compute -stage compute -o \"%s\"";
#endif
    char command[4096] = {};
    snprintf(command, sizeof(command), k_command_format, slangc.GetData(), source_path.GetData(), RNDR_TEST_SLANGC_OPTIONS,
             output_path.GetData());
    REQUIRE(std::system(command) == 0);

    Opal::Expected<Opal::DynamicArray<u8>, Opal::ErrorCode> read = Opal::ReadFileAsBytes(output_path);
    REQUIRE(read.HasValue());
    const Opal::DynamicArray<u8> from_slangc = std::move(read.GetValue());
    const Opal::DynamicArray<u8> from_compiler = CompileToSpirv(source, "main_compute");
    REQUIRE(!from_slangc.IsEmpty());
    REQUIRE(from_slangc == from_compiler);
#endif
}

/**
 * Shader::FromSource, which had no caller: every shader elsewhere in this file comes from
 * FromSourceInMemory, with the Slang text already in a `constexpr const char*` beside the case. FromSource
 * is the same compiler behind a file read, so what is unique to it is that read - a missing or empty file,
 * and a file that reads fine but is not valid Slang.
 */
TEST_CASE("Forge shader compiled from a source file on disk", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    SECTION("A module read from a source file is the same shader FromSourceInMemory builds")
    {
        const Opal::StringUtf8 path = TestScratchPath("from-source.slang");
        REQUIRE(Opal::WriteBytesToFile(path, {reinterpret_cast<const u8*>(k_compute_source), std::strlen(k_compute_source)}) ==
               Opal::ErrorCode::Success);
        const Forge::Shader from_file =
            ForgeTest::Unwrap(Forge::Shader::FromSource(fixture.device, path, {.entry_point = "main_compute"}));
        REQUIRE(from_file.IsValid());
        REQUIRE(from_file.GetShaderStage() == ShaderTypeBits::Compute);
        REQUIRE(from_file.GetEntryPoint() == Opal::StringUtf8("main_compute"));
        const Forge::Shader from_memory = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
        REQUIRE(from_file.GetPushConstants().GetSize() == from_memory.GetPushConstants().GetSize());
        REQUIRE(from_file.GetBindings().GetSize() == from_memory.GetBindings().GetSize());
    }
    SECTION("A file that is not there, and one that is there but empty, are both FileNotFound")
    {
        Opal::Expected<Forge::Shader, ErrorCode> missing =
            Forge::Shader::FromSource(fixture.device, TestScratchPath("no-such-source.slang"), {.entry_point = "main_compute"});
        REQUIRE_FALSE(missing.HasValue());
        REQUIRE(missing.GetError() == ErrorCode::FileNotFound);

        const Opal::StringUtf8 empty_path = TestScratchPath("empty.slang");
        REQUIRE(Opal::WriteBytesToFile(empty_path, {}) == Opal::ErrorCode::Success);
        Opal::Expected<Forge::Shader, ErrorCode> empty =
            Forge::Shader::FromSource(fixture.device, empty_path, {.entry_point = "main_compute"});
        REQUIRE_FALSE(empty.HasValue());
        REQUIRE(empty.GetError() == ErrorCode::FileNotFound);
    }
    SECTION("A file that reads fine but is not valid Slang is a ShaderCompilationError")
    {
        const Opal::StringUtf8 path = TestScratchPath("not-slang.slang");
        constexpr const char* k_not_slang = "this is not { valid Slang at all";
        REQUIRE(Opal::WriteBytesToFile(path, {reinterpret_cast<const u8*>(k_not_slang), std::strlen(k_not_slang)}) ==
               Opal::ErrorCode::Success);
        Opal::Expected<Forge::Shader, ErrorCode> result = Forge::Shader::FromSource(fixture.device, path, {.entry_point = "main"});
        REQUIRE_FALSE(result.HasValue());
        REQUIRE(result.GetError() == ErrorCode::ShaderCompilationError);
    }
    SECTION("Empty source handed to FromSourceInMemory directly is InvalidArgument")
    {
        // What a caller reading a file itself and handing the text over would get - FromSource never takes
        // this path, since it turns a missing or empty file into FileNotFound before FromSourceInMemory sees it.
        Opal::Expected<Forge::Shader, ErrorCode> result =
            Forge::Shader::FromSourceInMemory(fixture.device, Opal::StringUtf8(""), {.entry_point = "main"});
        REQUIRE_FALSE(result.HasValue());
        REQUIRE(result.GetError() == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR_AT_TEARDOWN(fixture);
}

/**
 * TimestampQueryPool::TryGetResults and ResolveQueryRange, neither of which had a caller. The elapsed helpers
 * are what a frame loop uses and what the timestamp case exercises; these two are what a caller reaches for
 * when it wants the raw ticks, or wants to make the range check itself before recording anything.
 */
TEST_CASE("Forge reading timestamp ticks without blocking", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 256;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 61);
    const Forge::Buffer source = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written));
    const Forge::Buffer destination =
        ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferDestination}));

    SECTION("A pool that was reset and not written has nothing to hand back")
    {
        const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(), [&](Forge::CommandBuffer& command_buffer)
                                       { REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success); }) == ErrorCode::Success);
        Opal::InPlaceArray<u64, 2> ticks;
        ticks[0] = 0xDEAD;
        ticks[1] = 0xBEEF;
        REQUIRE_FALSE(ForgeTest::Unwrap(pool.TryGetResults({ticks.GetData(), 2})));
        // The contract says the contents are unspecified on a false, since the driver may write into the
        // range either way, so nothing is asserted about what is in there now - only that it said no.
    }
    SECTION("A pool the device has finished with hands back the same ticks the blocking read does")
    {
        const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineStart) ==
                                           ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdWriteTimestamp(pool, 1, Forge::PipelineStageBits::PipelineEnd) ==
                                           ErrorCode::Success);
                               }) == ErrorCode::Success);
        // ImmediateSubmit has already waited, so the non-blocking read is the one that has to succeed.
        Opal::InPlaceArray<u64, 2> tried;
        REQUIRE(ForgeTest::Unwrap(pool.TryGetResults({tried.GetData(), 2})));
        Opal::InPlaceArray<u64, 2> blocked;
        REQUIRE(pool.GetResults({blocked.GetData(), 2}) == ErrorCode::Success);
        REQUIRE(tried[0] == blocked[0]);
        REQUIRE(tried[1] == blocked[1]);
        REQUIRE(tried[1] >= tried[0]);

        // One query at a time, from an offset, which is the other half of what first_query is for.
        Opal::InPlaceArray<u64, 1> second;
        REQUIRE(ForgeTest::Unwrap(pool.TryGetResults({second.GetData(), 1}, 1)));
        REQUIRE(second[0] == tried[1]);
    }
    SECTION("A range that does not fit in the pool is refused before anything is read")
    {
        const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 4}));
        Opal::InPlaceArray<u64, 8> ticks;
        // A first_query at or past the end, and a count that runs off it. Both are the caller's mistake and
        // neither is something the driver would report.
        REQUIRE(pool.TryGetResults({ticks.GetData(), 1}, 4).GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
        REQUIRE(pool.TryGetResults({ticks.GetData(), 5}).GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
        REQUIRE(pool.TryGetResults({ticks.GetData(), 3}, 2).GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
        REQUIRE(pool.GetResults({ticks.GetData(), 5}) == ErrorCode::OutOfBounds);
    }
    SECTION("ResolveQueryRange turns a count that may be every query into a concrete one")
    {
        // Public precisely so a caller can make this check itself, and nothing did. The pool is never
        // recorded into here - the whole of what this does is arithmetic against the pool size.
        const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 4}));
        REQUIRE(ForgeTest::Unwrap(pool.ResolveQueryRange(0, Forge::k_all_queries, "Reading")) == 4);
        REQUIRE(ForgeTest::Unwrap(pool.ResolveQueryRange(2, Forge::k_all_queries, "Reading")) == 2);
        REQUIRE(ForgeTest::Unwrap(pool.ResolveQueryRange(3, Forge::k_all_queries, "Reading")) == 1);
        // A concrete count comes back as itself as long as it fits.
        REQUIRE(ForgeTest::Unwrap(pool.ResolveQueryRange(0, 4, "Reading")) == 4);
        REQUIRE(ForgeTest::Unwrap(pool.ResolveQueryRange(1, 2, "Reading")) == 2);
        REQUIRE(ForgeTest::Unwrap(pool.ResolveQueryRange(3, 1, "Reading")) == 1);
    }
    SECTION("ResolveQueryRange refuses every range that does not fit")
    {
        const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 4}));
        // First query at the end and past it.
        REQUIRE(pool.ResolveQueryRange(4, 1, "Reading").GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
        REQUIRE(pool.ResolveQueryRange(5, 1, "Reading").GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
        REQUIRE(pool.ResolveQueryRange(4, Forge::k_all_queries, "Reading").GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
        // A count of zero, which resolves to nothing and is never what a caller meant.
        REQUIRE(pool.ResolveQueryRange(0, 0, "Reading").GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
        // And counts that run off the end from a first query that is itself fine.
        REQUIRE(pool.ResolveQueryRange(0, 5, "Reading").GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
        REQUIRE(pool.ResolveQueryRange(3, 2, "Reading").GetErrorOr(ErrorCode::Success) == ErrorCode::OutOfBounds);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * PhysicalDevice::FindMemoryTypeIndex, which is reachable headlessly and had no caller. Forge allocates
 * through VMA everywhere, so nothing inside it asks this - it is here for a caller reaching past Forge to
 * Vulkan, and it was going unexercised.
 *
 * It used to answer index zero when nothing matched. That is a real memory type with real properties, so a
 * caller could not tell it from a match and would allocate from the wrong heap; it reports
 * FeatureNotSupported now, which is what the error handling section of docs/forge.md asks of everything else.
 */
TEST_CASE("Forge memory type selection", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Forge::PhysicalDevice& physical_device = fixture.device.GetPhysicalDevice();
    const VkPhysicalDeviceMemoryProperties& memory = physical_device.GetMemoryProperties();
    REQUIRE(memory.memoryTypeCount > 0);
    // Every type this device has, which is the filter a caller with no constraint of its own passes.
    const u32 every_type = memory.memoryTypeCount >= 32 ? 0xFFFFFFFF : (1u << memory.memoryTypeCount) - 1;

    SECTION("A type with the properties asked for is one that actually has them")
    {
        // Every device is required to have a device local type and a host visible coherent one, so both of
        // these have an answer on any machine this runs on.
        constexpr VkMemoryPropertyFlags k_wanted[] = {
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        for (const VkMemoryPropertyFlags wanted : k_wanted)
        {
            INFO("wanted properties " << wanted);
            const u32 index = ForgeTest::Unwrap(physical_device.FindMemoryTypeIndex(every_type, wanted));
            REQUIRE(index < memory.memoryTypeCount);
            REQUIRE((memory.memoryTypes[index].propertyFlags & wanted) == wanted);
        }
    }
    SECTION("The filter decides which types are eligible at all")
    {
        // Asked for one type by name, it is the one that comes back - which is what the filter is for and
        // what an allocation driven by a VkMemoryRequirements would pass.
        const u32 wanted = memory.memoryTypes[0].propertyFlags;
        REQUIRE(ForgeTest::Unwrap(physical_device.FindMemoryTypeIndex(1u << 0, wanted)) == 0);
        if (memory.memoryTypeCount > 1)
        {
            const u32 second_wanted = memory.memoryTypes[1].propertyFlags;
            REQUIRE(ForgeTest::Unwrap(physical_device.FindMemoryTypeIndex(1u << 1, second_wanted)) == 1);
        }
    }
    SECTION("No property is asked for, so the first type the filter allows is the answer")
    {
        REQUIRE(ForgeTest::Unwrap(physical_device.FindMemoryTypeIndex(every_type, 0)) == 0);
        if (memory.memoryTypeCount > 1)
        {
            // The lowest set bit of the filter, not the lowest index of the device.
            REQUIRE(ForgeTest::Unwrap(physical_device.FindMemoryTypeIndex(every_type & ~1u, 0)) == 1);
        }
    }
    SECTION("Nothing matches, and it reports rather than naming a type that does not")
    {
        // No device has every property at once - DEVICE_LOCAL and the host visible bits coexist, but
        // LAZILY_ALLOCATED and PROTECTED do not sit with them - so this is an ask that cannot be met.
        constexpr VkMemoryPropertyFlags k_impossible = 0xFFFFFFFF;
        constexpr ErrorCode k_none = ErrorCode::FeatureNotSupported;
        REQUIRE(physical_device.FindMemoryTypeIndex(every_type, k_impossible).GetErrorOr(ErrorCode::Success) == k_none);
        // A filter that allows no type at all, which is the other way to match nothing: the loop never gets
        // as far as comparing properties.
        REQUIRE(physical_device.FindMemoryTypeIndex(0, 0).GetErrorOr(ErrorCode::Success) == k_none);
        REQUIRE(physical_device.FindMemoryTypeIndex(0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).GetErrorOr(ErrorCode::Success) == k_none);
        // And a filter naming only types that lack the property, which is the case a caller actually hits:
        // the answer index zero used to give was a type the filter had already ruled out.
        const u32 host_visible = ForgeTest::Unwrap(physical_device.FindMemoryTypeIndex(
            every_type, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        REQUIRE(physical_device.FindMemoryTypeIndex(1u << host_visible, k_impossible).GetErrorOr(ErrorCode::Success) == k_none);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** Writes text into the scratch directory and hands back the path, for the mesh cases below. */
Opal::StringUtf8 WriteScratchTextFile(const char* file_name, const char* contents)
{
    Opal::StringUtf8 path = TestScratchPath(file_name);
    const Opal::ArrayView<const u8> bytes(reinterpret_cast<const u8*>(contents), static_cast<i64>(strlen(contents)));
    REQUIRE(Opal::WriteBytesToFile(path, bytes) == Opal::ErrorCode::Success);
    return path;
}

/** One triangle with every attribute LoadMesh insists on: positions, normals and UVs. */
constexpr const char* k_complete_obj = R"(
v -1.0 -1.0 0.0
v 1.0 -1.0 0.0
v 0.0 1.0 0.0
vn 0.0 0.0 1.0
vt 0.0 0.0
vt 1.0 0.0
vt 0.5 1.0
f 1/1/1 2/2/1 3/3/1
)";

/** The same triangle with no texture coordinates, which is the attribute assimp will not invent. */
constexpr const char* k_obj_without_uvs = R"(
v -1.0 -1.0 0.0
v 1.0 -1.0 0.0
v 0.0 1.0 0.0
vn 0.0 0.0 1.0
f 1//1 2//1 3//1
)";

/** And with no normals but with a face, which is the shape assimp does generate normals for. */
constexpr const char* k_obj_without_normals = R"(
v -1.0 -1.0 0.0
v 1.0 -1.0 0.0
v 0.0 1.0 0.0
vt 0.0 0.0
vt 1.0 0.0
vt 0.5 1.0
f 1/1 2/2 3/3
)";

/**
 * Three shapes that reach the null normal check with nothing to check, because assimp generates normals from
 * faces and none of these has one left by the time it runs.
 *
 * A point cloud and a line mesh never had a face. The third has one written down, but every vertex of it sits
 * at the origin, so aiProcess_FindDegenerates removes it and the mesh arrives as bare as the first two.
 */
constexpr const char* k_obj_points_only = R"(
v -1.0 -1.0 0.0
v 1.0 -1.0 0.0
v 0.0 1.0 0.0
)";

constexpr const char* k_obj_lines_only = R"(
v -1.0 -1.0 0.0
v 1.0 -1.0 0.0
l 1 2
)";

constexpr const char* k_obj_degenerate_face = R"(
v 0.0 0.0 0.0
v 0.0 0.0 0.0
v 0.0 0.0 0.0
vt 0.0 0.0
vt 1.0 0.0
vt 0.5 1.0
f 1/1 2/2 3/3
)";

}  // namespace

/**
 * Forge::LoadMesh, which had no test of its own - test/mesh-test.cpp covers the Canvas mesh, which is a
 * different type through a different reader.
 *
 * The meshes are written here rather than checked in: three lines of OBJ apiece, and a fixture on disk that
 * nothing else reads is a fixture nobody notices has gone stale.
 */
TEST_CASE("Forge loading a mesh from a file", "[forge]")
{
    // RNDR_ASSIMP is private to the library, so this cannot be decided at compile time from out here. A build
    // without it refuses every call, which is what this asks about before asserting anything.
    const Opal::StringUtf8 complete_path = WriteScratchTextFile("triangle.obj", k_complete_obj);
    {
        Forge::Mesh probe;
        if (Forge::LoadMesh(complete_path, probe) != ErrorCode::Success)
        {
            SKIP("This build has no assimp, so Forge::LoadMesh refuses every file.");
        }
    }

    SECTION("A mesh with every attribute comes back packed the way the header says")
    {
        Forge::Mesh mesh;
        REQUIRE(Forge::LoadMesh(complete_path, mesh) == ErrorCode::Success);
        // Position, normal and UV, tightly packed: three floats, three floats, two floats.
        REQUIRE(mesh.vertex_size == 8 * sizeof(f32));
        REQUIRE(mesh.vertex_count == 3);
        REQUIRE(mesh.index_size == sizeof(u32));
        REQUIRE(mesh.index_count == 3);
        REQUIRE(mesh.vertices.GetSize() == static_cast<u64>(mesh.vertex_count) * mesh.vertex_size);
        REQUIRE(mesh.indices.GetSize() == static_cast<u64>(mesh.index_count) * mesh.index_size);
        // The name is the file, extension and all.
        REQUIRE(mesh.name == Opal::StringUtf8("triangle.obj"));

        // The positions, which is the one part of the packing a caller can check against what it wrote. Read
        // as bytes and compared as floats, since that is how the buffer is handed to a vertex binding.
        Opal::DynamicArray<f32> floats(static_cast<i64>(mesh.vertex_count) * 8);
        memcpy(floats.GetData(), mesh.vertices.GetData(), mesh.vertices.GetSize());
        f32 lowest_x = floats[0];
        f32 highest_x = floats[0];
        for (u32 vertex = 0; vertex < mesh.vertex_count; ++vertex)
        {
            lowest_x = Opal::Min(lowest_x, floats[vertex * 8]);
            highest_x = Opal::Max(highest_x, floats[vertex * 8]);
            // The normal of a flat triangle in the z plane, whichever way round assimp wound it.
            const f32 normal_z = floats[vertex * 8 + 5];
            REQUIRE(Opal::Abs(normal_z) == Catch::Approx(1.0f).margin(0.01));
        }
        REQUIRE(lowest_x == Catch::Approx(-1.0f).margin(0.01));
        REQUIRE(highest_x == Catch::Approx(1.0f).margin(0.01));

        // Every index names a vertex that is there, which is the other half of what a mesh has to hold up.
        Opal::DynamicArray<u32> indices(mesh.index_count);
        memcpy(indices.GetData(), mesh.indices.GetData(), mesh.indices.GetSize());
        for (const u32 index : indices)
        {
            REQUIRE(index < mesh.vertex_count);
        }
    }
    SECTION("A mesh with no texture coordinates is refused")
    {
        const Opal::StringUtf8 path = WriteScratchTextFile("no-uvs.obj", k_obj_without_uvs);
        Forge::Mesh mesh;
        REQUIRE(Forge::LoadMesh(path, mesh) == ErrorCode::UnsupportedFormat);
    }
    SECTION("A mesh with no normals but with a face loads, because assimp generates them from the face")
    {
        // LoadMesh asks for aiProcess_GenSmoothNormals, so a file with faces and no `vn` has normals by the
        // time the null check runs. aiProcess_GenUVCoords is not the counterpart it looks like - it converts
        // a mapping that is already there rather than inventing one - which is why the UV section above does
        // refuse the mesh where this one does not.
        const Opal::StringUtf8 path = WriteScratchTextFile("no-normals.obj", k_obj_without_normals);
        Forge::Mesh mesh;
        REQUIRE(Forge::LoadMesh(path, mesh) == ErrorCode::Success);
        REQUIRE(mesh.vertex_count == 3);
        Opal::DynamicArray<f32> floats(static_cast<i64>(mesh.vertex_count) * 8);
        memcpy(floats.GetData(), mesh.vertices.GetData(), mesh.vertices.GetSize());
        for (u32 vertex = 0; vertex < mesh.vertex_count; ++vertex)
        {
            const f32 normal_z = floats[vertex * 8 + 5];
            REQUIRE(Opal::Abs(normal_z) == Catch::Approx(1.0f).margin(0.01));
        }
    }
    SECTION("A mesh with no face to generate normals from is refused")
    {
        // What the section above does not reach, and the reason the null normal check is not dead code:
        // assimp generates normals from faces, so a mesh that has none arrives with none. Three ways to get
        // there - a point cloud, a line mesh, and a triangle so degenerate that aiProcess_FindDegenerates
        // takes it away - and all three land on the same refusal.
        const char* const names[] = {"points-only.obj", "lines-only.obj", "degenerate-face.obj"};
        const char* const contents[] = {k_obj_points_only, k_obj_lines_only, k_obj_degenerate_face};
        for (i32 i = 0; i < 3; ++i)
        {
            INFO(names[i]);
            const Opal::StringUtf8 path = WriteScratchTextFile(names[i], contents[i]);
            Forge::Mesh mesh;
            REQUIRE(Forge::LoadMesh(path, mesh) == ErrorCode::UnsupportedFormat);
        }
    }
    SECTION("A file assimp cannot read is refused")
    {
        // An extension assimp knows, holding something that is not a mesh.
        const Opal::StringUtf8 junk_path = WriteScratchTextFile("not-a-mesh.obj", "this file is not a mesh at all\n");
        Forge::Mesh mesh;
        REQUIRE(Forge::LoadMesh(junk_path, mesh) == ErrorCode::FileNotFound);
        // A file that is not there at all, and one with an extension assimp has no reader for.
        REQUIRE(Forge::LoadMesh(TestScratchPath("no-such-mesh.obj"), mesh) == ErrorCode::FileNotFound);
        const Opal::StringUtf8 unknown_path = WriteScratchTextFile("unknown-format.qqq", "nothing here either\n");
        REQUIRE(Forge::LoadMesh(unknown_path, mesh) == ErrorCode::FileNotFound);
    }
    SECTION("A mesh that failed to load leaves the one handed in as it was")
    {
        // LoadMesh writes into a mesh the caller owns, so what it does to that mesh on the way out of a
        // failure is the caller's problem. It reports before touching anything, which is what lets a caller
        // keep the mesh it already had.
        Forge::Mesh mesh;
        REQUIRE(Forge::LoadMesh(complete_path, mesh) == ErrorCode::Success);
        const u32 loaded_count = mesh.vertex_count;
        REQUIRE(Forge::LoadMesh(TestScratchPath("no-such-mesh.obj"), mesh) == ErrorCode::FileNotFound);
        REQUIRE(mesh.vertex_count == loaded_count);
        REQUIRE(mesh.name == Opal::StringUtf8("triangle.obj"));
    }
}

namespace
{

/**
 * A mesh shader that emits one triangle covering the whole target, and the fragment shader that paints it.
 * No vertex input and no vertex stage: the mesh stage produces the vertices and the indices between them,
 * which is the whole point of the pipeline this drives.
 */
constexpr const char* k_mesh_source = R"(
struct MeshVertex
{
    float4 position : SV_Position;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main_mesh(out vertices MeshVertex vertices[3], out indices uint3 triangles[1])
{
    SetMeshOutputCounts(3, 1);
    vertices[0].position = float4(-1.0, -1.0, 0.0, 1.0);
    vertices[1].position = float4(3.0, -1.0, 0.0, 1.0);
    vertices[2].position = float4(-1.0, 3.0, 0.0, 1.0);
    triangles[0] = uint3(0, 1, 2);
}

[shader("fragment")]
float4 main_mesh_fragment() : SV_Target
{
    return float4(0.0, 1.0, 0.0, 1.0);
}
)";

}  // namespace

/**
 * CmdDrawMeshTasks, which until now had only ever been watched being refused: recorded and checked against
 * a device without the extension, but never against a device that enabled it and a mesh shader that could
 * actually run. This is that draw.
 */
TEST_CASE("Forge a mesh shader draw", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    if (!CanCreateDevice({.mesh_shader = true}))
    {
        SKIP("This device has no VK_EXT_mesh_shader.");
    }
    ForgeFixture fixture({.mesh_shader = true});
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader mesh_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_mesh_source, {.entry_point = "main_mesh", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_mesh_source, {.entry_point = "main_mesh_fragment", .cache = GetShaderCache()}));

    SECTION("The stage reflection reports a mesh shader rather than a vertex one")
    {
        // The stage comes out of the SPIR-V, so this is what says the mesh path through ToShaderTypeBits is
        // reached at all - and a shader that came back as a vertex stage would build a pipeline that draws
        // nothing rather than one that fails.
        REQUIRE(mesh_shader.GetShaderStage() == ShaderTypeBits::Mesh);
        REQUIRE(mesh_shader.GetNativeShaderStage() == VK_SHADER_STAGE_MESH_BIT_EXT);
        // A mesh stage takes no vertex input, which is what makes the pipeline below legal without one.
        REQUIRE(mesh_shader.GetInputs().IsEmpty());
    }
    SECTION("A pipeline with a mesh stage draws the triangle the shader emitted")
    {
        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.mesh_shader = mesh_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               // One workgroup, the way a dispatch counts
                                                               // them - there is no vertex or index count to
                                                               // give.
                                                               REQUIRE(command_buffer.CmdDrawMeshTasks(1) == ErrorCode::Success);
                                                           });
        // The triangle covers the whole target, so anything short of every texel means the mesh stage emitted
        // something other than what it was told to.
        REQUIRE(CountCovered(pixels, k_side) == k_side * k_side);
    }
    SECTION("No workgroups at all draws nothing, which is a draw rather than a failure")
    {
        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.mesh_shader = mesh_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels =
            RenderRaster(fixture, color, k_side,
                         [&](Forge::CommandBuffer& command_buffer)
                         {
                             REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                             REQUIRE(command_buffer.CmdDrawMeshTasks(0) == ErrorCode::Success);
                         });
        REQUIRE(CountCovered(pixels, k_side) == 0);
    }
    SECTION("A pipeline with both a vertex and a mesh stage, or a task stage with neither, is refused")
    {
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
        Forge::GraphicsPipelineDesc both_desc;
        both_desc.vertex_shader = vertex_shader;
        both_desc.mesh_shader = mesh_shader;
        both_desc.fragment_shader = fragment_shader;
        both_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        both_desc.color_attachment_formats.PushBack(k_format);
        REQUIRE(Forge::Pipeline::Create(fixture.device, both_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);

        // A task stage in front of nothing, which Vulkan has no shape for.
        Forge::GraphicsPipelineDesc task_only_desc;
        task_only_desc.task_shader = mesh_shader;
        task_only_desc.fragment_shader = fragment_shader;
        task_only_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        task_only_desc.color_attachment_formats.PushBack(k_format);
        REQUIRE(Forge::Pipeline::Create(fixture.device, task_only_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * The other half of CmdDrawMeshTasks: what it does on a device that never enabled the extension. The loader
 * hands out a callable trampoline either way, so a null check does not catch this - calling through it is an
 * access violation rather than a call that fails, which is why the check is on the extension.
 */
TEST_CASE("Forge a mesh shader draw without the extension", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // The default fixture asks for no mesh shader, which is the device every other case in this file builds.
    ForgeFixture fixture;
    REQUIRE_FALSE(fixture.device.GetFeatures().mesh_shader);
    REQUIRE_FALSE(fixture.device.IsExtensionEnabled(VK_EXT_MESH_SHADER_EXTENSION_NAME));

    Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    REQUIRE(command_buffer.Begin() == ErrorCode::Success);
    REQUIRE(command_buffer.CmdDrawMeshTasks(1) == ErrorCode::InvalidArgument);
    // The two indirect forms go through the same trampoline question, with a buffer that is otherwise fine.
    const Forge::Buffer commands = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device, {.size = sizeof(Forge::DrawMeshTasksIndirectCommand), .usage = Forge::BufferUsageBits::IndirectBuffer}));
    REQUIRE(command_buffer.CmdDrawMeshTasksIndirect(commands) == ErrorCode::InvalidArgument);
    REQUIRE(command_buffer.CmdDrawMeshTasksIndirectCount(commands, 0, commands, 0, 1) == ErrorCode::InvalidArgument);
    REQUIRE(command_buffer.End() == ErrorCode::Success);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/**
 * A task stage in front of the mesh one. The task shader decides how many mesh workgroups run and hands each
 * of them a payload, and the mesh shader reads the far corner of its triangle out of that payload - so a
 * triangle that covers the target says the payload arrived, and one that covers a corner of it says the
 * mesh stage read zeros.
 */
constexpr const char* k_task_source = R"(
struct MeshVertex
{
    float4 position : SV_Position;
};

struct TrianglePayload
{
    float far_corner;
};

groupshared TrianglePayload g_payload;

[shader("amplification")]
[numthreads(1, 1, 1)]
void main_task()
{
    g_payload.far_corner = 3.0;
    DispatchMesh(1, 1, 1, g_payload);
}

[shader("amplification")]
[numthreads(1, 1, 1)]
void main_task_none()
{
    g_payload.far_corner = 3.0;
    DispatchMesh(0, 0, 0, g_payload);
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main_task_mesh(in payload TrianglePayload payload, out vertices MeshVertex vertices[3], out indices uint3 triangles[1])
{
    SetMeshOutputCounts(3, 1);
    vertices[0].position = float4(-1.0, -1.0, 0.0, 1.0);
    vertices[1].position = float4(payload.far_corner, -1.0, 0.0, 1.0);
    vertices[2].position = float4(-1.0, payload.far_corner, 0.0, 1.0);
    triangles[0] = uint3(0, 1, 2);
}

[shader("fragment")]
float4 main_task_fragment() : SV_Target
{
    return float4(0.0, 1.0, 0.0, 1.0);
}
)";

/**
 * A geometry stage that turns one point into the triangle covering the target. The vertex shader reads no
 * input and puts its one vertex at the origin, so everything the target ends up holding was emitted by the
 * geometry shader.
 */
constexpr const char* k_geometry_source = R"(
struct VertexOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput main_geometry_vertex()
{
    VertexOutput output;
    output.position = float4(0.0, 0.0, 0.0, 1.0);
    return output;
}

[shader("geometry")]
[maxvertexcount(3)]
void main_geometry(point VertexOutput input[1], inout TriangleStream<VertexOutput> stream)
{
    VertexOutput corner;
    corner.position = float4(-1.0, -1.0, 0.0, 1.0);
    stream.Append(corner);
    corner.position = float4(3.0, -1.0, 0.0, 1.0);
    stream.Append(corner);
    corner.position = float4(-1.0, 3.0, 0.0, 1.0);
    stream.Append(corner);
    stream.RestartStrip();
}

[shader("fragment")]
float4 main_geometry_fragment() : SV_Target
{
    return float4(0.0, 1.0, 0.0, 1.0);
}
)";

/**
 * The two tessellation stages over a patch of three control points, with every factor at one so the patch
 * comes out as the one triangle it went in as. A patch list cannot be rasterized without these stages, so
 * anything on the target says both of them ran.
 */
constexpr const char* k_tessellation_source = R"(
struct VertexOutput
{
    float4 position : SV_Position;
};

struct PatchConstants
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

[shader("vertex")]
VertexOutput main_tessellation_vertex(float2 position : POSITION)
{
    VertexOutput output;
    output.position = float4(position, 0.0, 1.0);
    return output;
}

PatchConstants main_tessellation_constants(InputPatch<VertexOutput, 3> patch)
{
    PatchConstants constants;
    constants.edges[0] = 1.0;
    constants.edges[1] = 1.0;
    constants.edges[2] = 1.0;
    constants.inside = 1.0;
    return constants;
}

[shader("hull")]
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("main_tessellation_constants")]
VertexOutput main_tessellation_control(InputPatch<VertexOutput, 3> patch, uint index : SV_OutputControlPointID)
{
    return patch[index];
}

[shader("domain")]
[domain("tri")]
VertexOutput main_tessellation_evaluation(PatchConstants constants, float3 weights : SV_DomainLocation,
                                          const OutputPatch<VertexOutput, 3> patch)
{
    VertexOutput output;
    output.position = patch[0].position * weights.x + patch[1].position * weights.y + patch[2].position * weights.z;
    return output;
}

[shader("fragment")]
float4 main_tessellation_fragment() : SV_Target
{
    return float4(0.0, 1.0, 0.0, 1.0);
}
)";

/** The pipeline desc every stage case starts from: no culling, one colour attachment of the given format. */
Forge::GraphicsPipelineDesc MakeStagePipelineDesc(PixelFormat format)
{
    Forge::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.rasterizer.cull_mode = Face::None;
    pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
    pipeline_desc.color_attachment_formats.PushBack(format);
    return pipeline_desc;
}

}  // namespace

/**
 * The positive half of the task stage. The mesh case above proves a mesh shader draws; this is the stage in
 * front of it deciding how many mesh workgroups run and what they are told.
 */
/**
 * One quad per mesh workgroup, each a quarter of the target wide and side by side from the left, so how many
 * columns come back covered is how many workgroups ran. And the writer of the two commands the indirect mesh
 * draws read: two workgroups, then three.
 */
constexpr const char* k_mesh_column_source = R"(
struct MeshVertex
{
    float4 position : SV_Position;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main_column_mesh(uint3 group_id : SV_GroupID, out vertices MeshVertex vertices[4], out indices uint3 triangles[2])
{
    SetMeshOutputCounts(4, 2);
    float left = -1.0 + 0.5 * float(group_id.x);
    float right = left + 0.5;
    vertices[0].position = float4(left, -1.0, 0.0, 1.0);
    vertices[1].position = float4(right, -1.0, 0.0, 1.0);
    vertices[2].position = float4(right, 1.0, 0.0, 1.0);
    vertices[3].position = float4(left, 1.0, 0.0, 1.0);
    triangles[0] = uint3(0, 1, 2);
    triangles[1] = uint3(0, 2, 3);
}

[shader("fragment")]
float4 main_column_fragment() : SV_Target
{
    return float4(0.0, 1.0, 0.0, 1.0);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main_write_mesh_tasks(uniform uint32_t *output)
{
    output[0] = 2; output[1] = 1; output[2] = 1;
    output[3] = 3; output[4] = 1; output[5] = 1;
}
)";

/**
 * CmdDrawMeshTasksIndirect and CmdDrawMeshTasksIndirectCount, the mesh draws a culling pass on the device
 * feeds. The workgroup counts come off a compute shader, so a draw that took them from anywhere else - or
 * read the wrong command, or ignored the count - covers the wrong number of columns.
 */
TEST_CASE("Forge mesh task draws read from a buffer", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    if (!CanCreateDevice({.mesh_shader = true}))
    {
        SKIP("This device has no VK_EXT_mesh_shader.");
    }
    const bool has_multi_draw = CanCreateDevice({.multi_draw_indirect = true, .mesh_shader = true});
    const bool has_count = CanCreateDevice({.draw_indirect_count = true, .mesh_shader = true});
    ForgeFixture fixture({.multi_draw_indirect = has_multi_draw, .draw_indirect_count = has_count, .mesh_shader = true});
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    auto load = [&](const char* source, const char* entry_point)
    {
        return ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, source, {.entry_point = entry_point, .cache = GetShaderCache()}));
    };
    const Forge::Shader mesh_shader = load(k_mesh_column_source, "main_column_mesh");
    const Forge::Shader fragment_shader = load(k_mesh_column_source, "main_column_fragment");
    const Forge::Pipeline write_tasks = MakeAddressPipeline(fixture.device, load(k_mesh_column_source, "main_write_mesh_tasks"));
    const Forge::Pipeline write_count_one = MakeAddressPipeline(fixture.device, load(k_indirect_count_source, "main_write_count_one"));
    const Forge::Pipeline write_count_two = MakeAddressPipeline(fixture.device, load(k_indirect_count_source, "main_write_count_two"));

    Forge::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.mesh_shader = mesh_shader;
    pipeline_desc.fragment_shader = fragment_shader;
    pipeline_desc.rasterizer.cull_mode = Face::None;
    pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
    pipeline_desc.color_attachment_formats.PushBack(k_format);
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

    auto make_device_buffer = [&](u64 size)
    {
        return ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = size,
                                                                        .usage = Forge::BufferUsageBits::IndirectBuffer |
                                                                                 Forge::BufferUsageBits::StorageBuffer,
                                                                        .host_access = Forge::HostAccess::None,
                                                                        .use_device_address = true}));
    };
    const Forge::Buffer commands = make_device_buffer(2 * sizeof(Forge::DrawMeshTasksIndirectCommand));
    const Forge::Buffer count = make_device_buffer(sizeof(u32));

    /**
     * Write the commands - and the count, when a writer for it is given - on the device, then draw with
     * whatever `record_draw` records, and hand back how many columns from the left came back covered. Every
     * column past that one has to be as the clear left it, which is what makes the answer a count and not a
     * shape.
     */
    auto columns_drawn = [&](const Forge::Pipeline* count_writer, auto&& record_draw)
    {
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        const VkDeviceAddress command_address = commands.GetNativeDeviceAddress();
                        REQUIRE(command_buffer.CmdBindPipeline(write_tasks) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(write_tasks, ShaderTypeBits::Compute, Opal::AsBytes(command_address)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                        if (count_writer != nullptr)
                        {
                            const VkDeviceAddress count_address = count.GetNativeDeviceAddress();
                            REQUIRE(command_buffer.CmdBindPipeline(*count_writer) == ErrorCode::Success);
                            REQUIRE(command_buffer.CmdPushConstants(*count_writer, ShaderTypeBits::Compute,
                                                                    Opal::AsBytes(count_address)) == ErrorCode::Success);
                            REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                        }
                        const Forge::BufferBarrier barriers[] = {
                            Forge::BufferBarrier::WriteThenRead(commands, Forge::PipelineStageBits::ComputeShader,
                                                                Forge::PipelineStageBits::IndirectDraw),
                            Forge::BufferBarrier::WriteThenRead(count, Forge::PipelineStageBits::ComputeShader,
                                                                Forge::PipelineStageBits::IndirectDraw)};
                        REQUIRE(command_buffer.CmdBufferBarriers(Opal::ArrayView<const Forge::BufferBarrier>(barriers, count_writer != nullptr ? 2 : 1)) == ErrorCode::Success);
                    }) == ErrorCode::Success);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               record_draw(command_buffer);
                                                           });
        i32 columns = 0;
        while (columns < k_side && IsCovered(pixels, k_side, columns, 0))
        {
            ++columns;
        }
        for (i32 y = 0; y < k_side; ++y)
        {
            for (i32 x = 0; x < k_side; ++x)
            {
                INFO("texel " << x << "," << y << " with " << columns << " columns covered");
                REQUIRE(IsCovered(pixels, k_side, x, y) == (x < columns));
            }
        }
        return columns;
    };

    SECTION("One command draws the workgroups it names")
    {
        const i32 columns = columns_drawn(nullptr, [&](Forge::CommandBuffer& command_buffer)
                                          { REQUIRE(command_buffer.CmdDrawMeshTasksIndirect(commands) == ErrorCode::Success); });
        REQUIRE(columns == 2);
    }
    SECTION("A second command at the stride is read as well")
    {
        INFO("multi_draw_indirect supported: " << has_multi_draw);
        if (!has_multi_draw)
        {
            SKIP("This device cannot read more than one indirect command per call.");
        }
        // Three workgroups after two: the second command covers what the first did and one column more.
        const i32 columns = columns_drawn(nullptr, [&](Forge::CommandBuffer& command_buffer)
                                          { REQUIRE(command_buffer.CmdDrawMeshTasksIndirect(commands, 0, 2) == ErrorCode::Success); });
        REQUIRE(columns == 3);
    }
    SECTION("A count the device wrote decides how many commands are read")
    {
        INFO("draw_indirect_count supported: " << has_count);
        if (!has_count)
        {
            SKIP("This device cannot read an indirect draw count from a buffer.");
        }
        auto draw_counted = [&](Forge::CommandBuffer& command_buffer)
        { REQUIRE(command_buffer.CmdDrawMeshTasksIndirectCount(commands, 0, count, 0, 2) == ErrorCode::Success); };
        const i32 counted_one = columns_drawn(&write_count_one, draw_counted);
        REQUIRE(counted_one == 2);
        const i32 counted_two = columns_drawn(&write_count_two, draw_counted);
        REQUIRE(counted_two == 3);
        // The maximum still wins over what the device wrote.
        const i32 capped = columns_drawn(&write_count_two,
                                         [&](Forge::CommandBuffer& command_buffer) {
                                             REQUIRE(command_buffer.CmdDrawMeshTasksIndirectCount(commands, 0, count, 0, 1) ==
                                                     ErrorCode::Success);
                                         });
        REQUIRE(capped == 2);
    }
    SECTION("The arguments the other indirect draws check are checked here too")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        const Forge::Buffer storage_only =
            ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = 64, .usage = Forge::BufferUsageBits::StorageBuffer}));
        REQUIRE(command_buffer.CmdDrawMeshTasksIndirect(storage_only) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDrawMeshTasksIndirect(commands, 2) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.CmdDrawMeshTasksIndirect(commands, sizeof(Forge::DrawMeshTasksIndirectCommand) + 4) == ErrorCode::OutOfBounds);
        if (has_count)
        {
            REQUIRE(command_buffer.CmdDrawMeshTasksIndirectCount(commands, 0, count, 0, 3) == ErrorCode::OutOfBounds);
            REQUIRE(command_buffer.CmdDrawMeshTasksIndirectCount(commands, 0, count, 0, 1, 8) == ErrorCode::InvalidArgument);
        }
        else
        {
            REQUIRE(command_buffer.CmdDrawMeshTasksIndirectCount(commands, 0, count, 0, 1) == ErrorCode::InvalidArgument);
        }
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a task shader in front of a mesh one", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_features{.mesh_shader = true, .task_shader = true};
    if (!CanCreateDevice(k_features))
    {
        SKIP("This device has no task shaders.");
    }
    ForgeFixture fixture(k_features);
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader task_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_task_source, {.entry_point = "main_task", .cache = GetShaderCache()}));
    const Forge::Shader mesh_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_task_source, {.entry_point = "main_task_mesh", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_task_source, {.entry_point = "main_task_fragment", .cache = GetShaderCache()}));

    SECTION("The stage reflection reports a task shader")
    {
        REQUIRE(task_shader.GetShaderStage() == ShaderTypeBits::Task);
        REQUIRE(task_shader.GetNativeShaderStage() == VK_SHADER_STAGE_TASK_BIT_EXT);
        REQUIRE(task_shader.GetInputs().IsEmpty());
    }
    SECTION("The mesh stage draws the triangle the task stage described")
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakeStagePipelineDesc(k_format);
        pipeline_desc.task_shader = task_shader;
        pipeline_desc.mesh_shader = mesh_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               // One task workgroup; how many mesh ones is the
                                                               // task shader's decision.
                                                               REQUIRE(command_buffer.CmdDrawMeshTasks(1) == ErrorCode::Success);
                                                           });
        // The far corner of the triangle comes out of the payload. Zeros there would leave a triangle over
        // the bottom left quarter of the target, which is what this tells apart from the whole of it.
        REQUIRE(CountCovered(pixels, k_side) == k_side * k_side);
    }
    SECTION("A task stage that dispatches no mesh workgroups draws nothing")
    {
        const Forge::Shader none_shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_task_source, {.entry_point = "main_task_none", .cache = GetShaderCache()}));
        Forge::GraphicsPipelineDesc pipeline_desc = MakeStagePipelineDesc(k_format);
        pipeline_desc.task_shader = none_shader;
        pipeline_desc.mesh_shader = mesh_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDrawMeshTasks(1) == ErrorCode::Success);
                                                           });
        REQUIRE(CountCovered(pixels, k_side) == 0);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge a geometry shader draw", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_features{.geometry_shader = true};
    if (!CanCreateDevice(k_features))
    {
        SKIP("This device has no geometry shaders.");
    }
    ForgeFixture fixture(k_features);
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_geometry_source, {.entry_point = "main_geometry_vertex", .cache = GetShaderCache()}));
    const Forge::Shader geometry_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_geometry_source, {.entry_point = "main_geometry", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_geometry_source, {.entry_point = "main_geometry_fragment", .cache = GetShaderCache()}));

    SECTION("The stage reflection reports a geometry shader")
    {
        REQUIRE(geometry_shader.GetShaderStage() == ShaderTypeBits::Geometry);
        REQUIRE(geometry_shader.GetNativeShaderStage() == VK_SHADER_STAGE_GEOMETRY_BIT);
        // What a geometry shader reads comes from the stage before it, not from a vertex buffer.
        REQUIRE(geometry_shader.GetInputs().IsEmpty());
    }
    SECTION("One point in becomes the triangle the geometry shader emitted")
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakeStagePipelineDesc(k_format);
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.geometry_shader = geometry_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.topology = PrimitiveTopology::Point;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               // One vertex and no vertex buffer: the vertex
                                                               // shader reads nothing.
                                                               REQUIRE(command_buffer.CmdDraw(1) == ErrorCode::Success);
                                                           });
        // A point covers at most one texel; the whole target says the geometry stage replaced it.
        REQUIRE(CountCovered(pixels, k_side) == k_side * k_side);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/**
 * Two triangles over the whole target, each carrying the layer it lands in. The vertex stage picks the
 * layer, which is what DeviceFeatures::shader_output_layer allows, and the colour says which layer the
 * triangle was meant for - passed on as a varying of its own, since reading SV_RenderTargetArrayIndex back in
 * the fragment stage is a capability of its own too.
 */
constexpr const char* k_layered_source = R"(
struct LayeredOutput
{
    float4 position : SV_Position;
    uint layer : SV_RenderTargetArrayIndex;
    nointerpolation uint color_layer : COLOR_LAYER;
};

[shader("vertex")]
LayeredOutput main_layered_vertex(float2 position : POSITION, uint layer : LAYER)
{
    LayeredOutput output;
    output.position = float4(position, 0.0, 1.0);
    output.layer = layer;
    output.color_layer = layer;
    return output;
}

[shader("fragment")]
float4 main_layered_fragment(nointerpolation uint color_layer : COLOR_LAYER) : SV_Target
{
    return color_layer == 0 ? float4(0.0, 1.0, 0.0, 1.0) : float4(0.0, 0.0, 1.0, 1.0);
}
)";

/** One fullscreen triangle drawn once and rendered for every view, coloured by the view it ran for. */
constexpr const char* k_multiview_source = R"(
[shader("vertex")]
float4 main_multiview_vertex(float2 position : POSITION) : SV_Position
{
    return float4(position, 0.0, 1.0);
}

[shader("fragment")]
float4 main_multiview_fragment(uint view : SV_ViewID) : SV_Target
{
    return view == 0 ? float4(0.0, 1.0, 0.0, 1.0) : view == 2 ? float4(0.0, 0.0, 1.0, 1.0) : float4(1.0, 1.0, 1.0, 1.0);
}
)";

/** A vertex of k_layered_source: a position and the layer its triangle goes to. */
struct LayeredVertex
{
    f32 x = 0.0f;
    f32 y = 0.0f;
    u32 layer = 0;
};

}  // namespace

/**
 * RenderingDesc::layer_count and view_mask, GraphicsPipelineDesc::view_mask, and the two device features
 * behind them. Each pass renders into an array texture filled with red and reads every layer back: a layer
 * the pass never reached stays red, and one that got another layer's work shows that layer's colour.
 */
TEST_CASE("Forge layered rendering", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr u8 k_red[] = {255, 0, 0, 255};
    constexpr u8 k_green[] = {0, 255, 0, 255};
    constexpr u8 k_blue[] = {0, 0, 255, 255};

    auto make_target = [&](const Forge::Device& device, u32 layer_count)
    {
        return ForgeTest::Unwrap(Forge::Texture::Create(
            device, {.format = k_format,
                     .width = k_side,
                     .height = k_side,
                     .array_layer_count = layer_count,
                     .usage = Forge::TextureUsageBits::ColorAttachment | Forge::TextureUsageBits::TransferSource |
                              Forge::TextureUsageBits::TransferDestination,
                     .view_type = Forge::TextureViewType::Texture2DArray}));
    };
    /**
     * Fill every layer with red, run one pass of the given shape over them, and read all of them back. Filled
     * by an upload and loaded rather than cleared by the pass: a load operation reaches only the layers the
     * pass renders, and the ones it does not are the point.
     */
    auto render = [&](ForgeFixture& fixture, Forge::Texture& target, u32 layer_count, u32 view_mask, const Forge::Pipeline& pipeline,
                      const Forge::Buffer& vertices, u32 vertex_count)
    {
        const u32 target_layer_count = target.GetDesc().array_layer_count;
        Opal::DynamicArray<u8> red(k_side * k_side * target_layer_count * 4);
        for (u64 texel = 0; texel < red.GetSize() / 4; ++texel)
        {
            memcpy(red.GetData() + texel * 4, k_red, 4);
        }
        UploadMip(fixture.device, fixture.GetQueue(), target, {red.GetData(), red.GetSize()}, 0, Forge::PipelineStageBits::None);
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(target)) == ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = target,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Load,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store}},
                            .layer_count = layer_count,
                            .view_mask = view_mask};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(vertex_count) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);
        Opal::DynamicArray<u8> pixels(k_side * k_side * 4 * target_layer_count);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), target, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        return pixels;
    };
    /** Every texel of one layer of the readback is the given colour. */
    auto require_layer = [&](const Opal::DynamicArray<u8>& pixels, u32 layer, const u8 (&color)[4])
    {
        const i32 layer_base = static_cast<i32>(layer) * k_side * k_side * 4;
        for (i32 texel = 0; texel < k_side * k_side; ++texel)
        {
            const i32 base = layer_base + texel * 4;
            INFO("layer " << layer << " texel " << texel << " rgba " << +pixels[base] << " " << +pixels[base + 1] << " "
                          << +pixels[base + 2] << " " << +pixels[base + 3]);
            REQUIRE((pixels[base] == color[0] && pixels[base + 1] == color[1] && pixels[base + 2] == color[2] &&
                     pixels[base + 3] == color[3]));
        }
    };
    auto make_multiview_pipeline = [&](const Forge::Device& device, u32 view_mask)
    {
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            device, k_multiview_source,
            {.entry_point = "main_multiview_vertex", .cache = GetShaderCache()}));
        const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            device, k_multiview_source,
            {.entry_point = "main_multiview_fragment", .cache = GetShaderCache()}));
        Forge::GraphicsPipelineDesc pipeline_desc = MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, k_format);
        pipeline_desc.view_mask = view_mask;
        return Forge::Pipeline::Create(device, pipeline_desc);
    };

    SECTION("A layered pass lands each triangle in the layer its vertex stage picked")
    {
        constexpr Forge::DeviceFeatures k_features{.shader_output_layer = true};
        if (!CanCreateDevice(k_features))
        {
            SKIP("This device cannot write the layer from the vertex stage.");
        }
        ForgeFixture fixture(k_features);
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_layered_source,
            {.entry_point = "main_layered_vertex", .cache = GetShaderCache()}));
        const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_layered_source,
            {.entry_point = "main_layered_fragment", .cache = GetShaderCache()}));
        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.vertex_input.AddBinding(0, sizeof(LayeredVertex), DataRepetition::PerVertex);
        REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 1, PixelFormat::R32_UINT, 2 * sizeof(f32)) == ErrorCode::Success);
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        // The second layer's triangle goes first, so a pass that ignored the layer would end up green.
        const LayeredVertex triangles[] = {{-1.0f, -1.0f, 1}, {3.0f, -1.0f, 1}, {-1.0f, 3.0f, 1},
                                           {-1.0f, -1.0f, 0}, {3.0f, -1.0f, 0}, {-1.0f, 3.0f, 0}};
        const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device, {.size = sizeof(triangles), .usage = Forge::BufferUsageBits::VertexBuffer},
            {reinterpret_cast<const u8*>(triangles), sizeof(triangles)}));

        // Three layers and a pass over two of them: the third is past the pass and keeps its clear.
        Forge::Texture target = make_target(fixture.device, 3);
        const Opal::DynamicArray<u8> pixels = render(fixture, target, 2, 0, pipeline, vertices, 6);
        require_layer(pixels, 0, k_green);
        require_layer(pixels, 1, k_blue);
        require_layer(pixels, 2, k_red);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("A multiview pass renders one draw into the layer of every bit of its mask")
    {
        constexpr Forge::DeviceFeatures k_features{.multiview = true};
        if (!CanCreateDevice(k_features))
        {
            SKIP("This device has no multiview.");
        }
        ForgeFixture fixture(k_features);
        // Views zero and two, so the layer between them is one no view lands in.
        constexpr u32 k_view_mask = 0b101;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(make_multiview_pipeline(fixture.device, k_view_mask));
        const Forge::Buffer vertices = ForgeTest::Unwrap(Forge::Buffer::Create(
            fixture.device, {.size = sizeof(k_fullscreen_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
            {reinterpret_cast<const u8*>(k_fullscreen_vertices), sizeof(k_fullscreen_vertices)}));

        Forge::Texture target = make_target(fixture.device, 3);
        const Opal::DynamicArray<u8> pixels = render(fixture, target, 1, k_view_mask, pipeline, vertices, 3);
        require_layer(pixels, 0, k_green);
        require_layer(pixels, 1, k_red);
        require_layer(pixels, 2, k_blue);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("A pass whose layers or views the attachments or the device cannot take is refused")
    {
        ForgeFixture fixture;
        Forge::Texture target = make_target(fixture.device, 2);
        const Forge::TextureView first_layer = ForgeTest::Unwrap(Forge::TextureView::Create(
            fixture.device, target,
            {.view_type = Forge::TextureViewType::Texture2DArray, .subresource_range = {.first_array_layer = 0, .array_layer_count = 1}}));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(target)) == ErrorCode::Success);
        /** Begin a pass over the whole target, or over the view of its first layer. */
        auto begin = [&](bool through_view, u32 layer_count, u32 view_mask)
        {
            Forge::RenderingDesc rendering_desc{.render_area_extent = {k_side, k_side}, .layer_count = layer_count, .view_mask = view_mask};
            rendering_desc.color_attachments.PushBack(through_view ? Forge::RenderingAttachmentDesc{.view = first_layer}
                                                                   : Forge::RenderingAttachmentDesc{.texture = target});
            return command_buffer.CmdBeginRendering(rendering_desc);
        };
        REQUIRE(begin(false, 0, 0) == ErrorCode::InvalidArgument);
        REQUIRE(begin(false, 3, 0) == ErrorCode::InvalidArgument);
        REQUIRE(begin(false, fixture.device.GetPhysicalDevice().GetProperties().limits.maxFramebufferLayers + 1, 0) ==
                ErrorCode::InvalidArgument);
        // The view's range is what the pass reaches into, not the texture's.
        REQUIRE(begin(true, 2, 0) == ErrorCode::InvalidArgument);
        // A device made without multiview takes neither a pass nor a pipeline with a mask.
        REQUIRE_FALSE(fixture.device.GetFeatures().multiview);
        REQUIRE(begin(false, 1, 0b1) == ErrorCode::InvalidArgument);
        // Over shaders that never read the view, since a module that does is refused by the layer on this device.
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
        const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));
        Forge::GraphicsPipelineDesc pipeline_desc = MakeFullscreenPipelineDesc(vertex_shader, fragment_shader, k_format);
        pipeline_desc.view_mask = 0b1;
        REQUIRE(Forge::Pipeline::Create(fixture.device, pipeline_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("A multiview pass the attachments or its own layer count contradict is refused")
    {
        constexpr Forge::DeviceFeatures k_features{.geometry_shader = true, .multiview = true};
        if (!CanCreateDevice(k_features))
        {
            SKIP("This device has no multiview, or no geometry stage.");
        }
        ForgeFixture fixture(k_features);
        Forge::Texture target = make_target(fixture.device, 2);
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(target)) == ErrorCode::Success);
        auto begin = [&](u32 layer_count, u32 view_mask)
        {
            return command_buffer.CmdBeginRendering({.render_area_extent = {k_side, k_side},
                                                     .color_attachments = {Forge::RenderingAttachmentDesc{.texture = target}},
                                                     .layer_count = layer_count,
                                                     .view_mask = view_mask});
        };
        // A mask reaching the third layer of a two layer target, and a mask beside a layer count.
        REQUIRE(begin(1, 0b100) == ErrorCode::InvalidArgument);
        REQUIRE(begin(2, 0b11) == ErrorCode::InvalidArgument);
        REQUIRE(command_buffer.End() == ErrorCode::Success);

        // Multiview over a geometry stage is a feature Forge does not ask for.
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_geometry_source,
            {.entry_point = "main_geometry_vertex", .cache = GetShaderCache()}));
        const Forge::Shader geometry_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_geometry_source,
            {.entry_point = "main_geometry", .cache = GetShaderCache()}));
        const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_geometry_source,
            {.entry_point = "main_geometry_fragment", .cache = GetShaderCache()}));
        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.geometry_shader = geometry_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.topology = PrimitiveTopology::Point;
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
        pipeline_desc.view_mask = 0b1;
        REQUIRE(Forge::Pipeline::Create(fixture.device, pipeline_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("Without shader_output_layer on, the layer refuses a vertex stage that writes the layer")
    {
        // Nothing is built past the shader - a pipeline over a module the layer rejected is undefined.
        ForgeFixture fixture;
        REQUIRE(fixture.status == ErrorCode::Success);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_layered_source,
            {.entry_point = "main_layered_vertex", .cache = GetShaderCache()}));
        INFO(*fixture.GetValidationErrors());
        REQUIRE(fixture.GetValidationErrorCount() > 0);
    }
}

TEST_CASE("Forge a tessellation draw", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_features{.tessellation_shader = true};
    if (!CanCreateDevice(k_features))
    {
        SKIP("This device has no tessellation shaders.");
    }
    ForgeFixture fixture(k_features);
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_tessellation_source, {.entry_point = "main_tessellation_vertex", .cache = GetShaderCache()}));
    const Forge::Shader control_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_tessellation_source, {.entry_point = "main_tessellation_control", .cache = GetShaderCache()}));
    const Forge::Shader evaluation_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_tessellation_source, {.entry_point = "main_tessellation_evaluation", .cache = GetShaderCache()}));
    const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_tessellation_source, {.entry_point = "main_tessellation_fragment", .cache = GetShaderCache()}));
    const Forge::Buffer vertices = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = sizeof(k_fullscreen_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                              Opal::AsBytes(k_fullscreen_vertices)));

    /** The desc of a pipeline with both stages over one patch of three, which the refusals each break one way. */
    auto make_tessellation_desc = [&]
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakeStagePipelineDesc(k_format);
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.tessellation_control_shader = control_shader;
        pipeline_desc.tessellation_evaluation_shader = evaluation_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.topology = PrimitiveTopology::Patch;
        pipeline_desc.patch_control_points = 3;
        pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        return pipeline_desc;
    };

    SECTION("The stage reflection reports the two tessellation stages")
    {
        REQUIRE(control_shader.GetShaderStage() == ShaderTypeBits::TessellationControl);
        REQUIRE(control_shader.GetNativeShaderStage() == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        REQUIRE(control_shader.GetInputs().IsEmpty());
        REQUIRE(evaluation_shader.GetShaderStage() == ShaderTypeBits::TessellationEvaluation);
        REQUIRE(evaluation_shader.GetNativeShaderStage() == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        REQUIRE(evaluation_shader.GetInputs().IsEmpty());
    }
    SECTION("A patch of three goes through both stages and comes out as its triangle")
    {
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, make_tessellation_desc()));
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
        const Opal::DynamicArray<u8> pixels =
            RenderRaster(fixture, color, k_side,
                         [&](Forge::CommandBuffer& command_buffer)
                         {
                             REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                             REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                             REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                         });
        REQUIRE(CountCovered(pixels, k_side) == k_side * k_side);
    }
    SECTION("A control shader without an evaluation shader is refused")
    {
        Forge::GraphicsPipelineDesc pipeline_desc = make_tessellation_desc();
        pipeline_desc.tessellation_evaluation_shader = {};
        REQUIRE(Forge::Pipeline::Create(fixture.device, pipeline_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("The tessellation stages with a topology other than Patch are refused")
    {
        Forge::GraphicsPipelineDesc pipeline_desc = make_tessellation_desc();
        pipeline_desc.topology = PrimitiveTopology::Triangle;
        REQUIRE(Forge::Pipeline::Create(fixture.device, pipeline_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("The Patch topology without the tessellation stages is refused")
    {
        Forge::GraphicsPipelineDesc pipeline_desc = make_tessellation_desc();
        pipeline_desc.tessellation_control_shader = {};
        pipeline_desc.tessellation_evaluation_shader = {};
        REQUIRE(Forge::Pipeline::Create(fixture.device, pipeline_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    SECTION("A patch of no control points, or of more than the device tessellates, is refused")
    {
        Forge::GraphicsPipelineDesc pipeline_desc = make_tessellation_desc();
        pipeline_desc.patch_control_points = 0;
        REQUIRE(Forge::Pipeline::Create(fixture.device, pipeline_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        pipeline_desc.patch_control_points = fixture.device.GetPhysicalDevice().GetProperties().limits.maxTessellationPatchSize + 1;
        REQUIRE(Forge::Pipeline::Create(fixture.device, pipeline_desc).GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * Every stage behind a device feature, asked of a device that enabled none of them. The SPIR-V of each
 * declares the capability, and the check is Forge's rather than the layer's, so this passes with no
 * validation message at all.
 */
TEST_CASE("Forge a shader of a stage the device did not enable is refused", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    REQUIRE_FALSE(fixture.device.GetFeatures().geometry_shader);
    REQUIRE_FALSE(fixture.device.GetFeatures().tessellation_shader);
    REQUIRE_FALSE(fixture.device.GetFeatures().mesh_shader);
    REQUIRE_FALSE(fixture.device.GetFeatures().task_shader);

    struct Case
    {
        const char* source;
        const char* entry_point;
    };
    const Case cases[] = {{k_geometry_source, "main_geometry"},
                          {k_tessellation_source, "main_tessellation_control"},
                          {k_tessellation_source, "main_tessellation_evaluation"},
                          {k_task_source, "main_task_mesh"},
                          {k_task_source, "main_task"}};
    for (const Case& test_case : cases)
    {
        INFO("entry point " << test_case.entry_point);
        const Opal::Expected<Forge::Shader, ErrorCode> shader = Forge::Shader::FromSourceInMemory(
            fixture.device, test_case.source, {.entry_point = test_case.entry_point, .cache = GetShaderCache()});
        REQUIRE_FALSE(shader.HasValue());
        REQUIRE(shader.GetError() == ErrorCode::InvalidArgument);
    }
    // The stages every device has are still accepted by the same device.
    const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_geometry_source, {.entry_point = "main_geometry_vertex", .cache = GetShaderCache()}));
    REQUIRE(vertex_shader.GetShaderStage() == ShaderTypeBits::Vertex);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

namespace
{

/** A uniform texel buffer, which is a descriptor kind DescriptorType has no entry for. */
constexpr const char* k_texel_buffer_source = R"(
[[vk::binding(0, 0)]] Buffer<float4> texels;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> output;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_texel_buffer()
{
    output[0] = texels.Load(0);
}
)";

/** A storage texel buffer, the other kind of texel buffer and just as absent. */
constexpr const char* k_storage_texel_buffer_source = R"(
[[vk::image_format("rgba32f")]]
[[vk::binding(0, 0)]] RWBuffer<float4> texels;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_storage_texel_buffer()
{
    texels[0] = float4(1.0, 2.0, 3.0, 4.0);
}
)";

/** A ray generation entry point, a stage ShaderTypeBits has no bit for. */
constexpr const char* k_ray_generation_source = R"(
[[vk::binding(0, 0)]] RWStructuredBuffer<uint> output;

[shader("raygeneration")]
void main_ray_generation()
{
    output[DispatchRaysIndex().x] = 1;
}
)";

/** A sixteen bit float specialization constant, a width SpecializationType has no entry for. */
constexpr const char* k_half_constant_source = R"(
[SpecializationConstant]
const half SCALE = 0.5;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_half_constant(uniform float *output)
{
    output[0] = float(SCALE);
}
)";

}  // namespace

/**
 * What reflection finds and Forge does not model: a descriptor kind, a shader stage and a specialization
 * constant type. All three are refused while the SPIR-V is read, before vkCreateShaderModule, so none needs
 * the device feature the shader would otherwise want - and each answers UnsupportedFormat, which is how
 * Shader::FromSpirvInMemory says the module is fine and Forge is what falls short.
 */
TEST_CASE("Forge a shader reflection finds something Forge does not model in is refused", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    struct Case
    {
        const char* source;
        const char* entry_point;
    };
    const Case cases[] = {{k_texel_buffer_source, "main_texel_buffer"},
                          {k_storage_texel_buffer_source, "main_storage_texel_buffer"},
                          {k_ray_generation_source, "main_ray_generation"},
                          {k_half_constant_source, "main_half_constant"}};
    for (const Case& test_case : cases)
    {
        INFO("entry point " << test_case.entry_point);
        const Opal::Expected<Forge::Shader, ErrorCode> shader = Forge::Shader::FromSourceInMemory(
            fixture.device, test_case.source, {.entry_point = test_case.entry_point, .cache = GetShaderCache()});
        REQUIRE(shader.GetErrorOr(ErrorCode::Success) == ErrorCode::UnsupportedFormat);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/*
 * The device features that map onto a Vulkan feature bit in device.cpp and that nothing else in this file
 * asks for or relies on. A field wired to the wrong bit is silent until a shader needs it, so each case below
 * uses the feature the way a caller would, and where the validation layer checks that bit, also turns the
 * feature off and makes sure the layer objects - which is what shows the field reaches the bit, rather than
 * the bit being on for some other reason. shader_float64 is the fifth of them and is covered by the double
 * specialization constant case, which asks for it and does arithmetic in double.
 */
namespace
{

/**
 * A struct whose second member only lays out under scalar block layout: a float3 at offset 8 crosses a
 * sixteen byte boundary, and an array of them has a stride of twenty. Both are refused by the relaxed rules
 * a device without the feature validates against.
 */
constexpr const char* k_scalar_layout_source = R"(
struct Packed
{
    float2 first;
    float3 second;
};
[[vk::binding(0, 0)]] RWStructuredBuffer<Packed, ScalarDataLayout> packed;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_scalar_layout()
{
    packed[0].first = float2(1.0, 2.0);
    packed[0].second = float3(3.0, 4.0, 5.0);
    packed[1].first = float2(6.0, 7.0);
    packed[1].second = float3(8.0, 9.0, 10.0);
}
)";

/** An array of storage buffers whose length the shader does not state, which is the RuntimeDescriptorArray capability. */
constexpr const char* k_runtime_array_source = R"(
[[vk::binding(0, 0)]] RWStructuredBuffer<uint> buffers[];

[shader("compute")]
[numthreads(1, 1, 1)]
void main_runtime_array()
{
    buffers[0][0] = 4321;
}
)";

/** A compute pipeline over one shader and one layout, with no push constants. */
Forge::Pipeline MakeSetPipeline(const Forge::Device& device, const Forge::Shader& shader, const Forge::DescriptorSetLayout& layout)
{
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    return ForgeTest::Unwrap(Forge::Pipeline::Create(device, pipeline_desc));
}

/**
 * Bind `output` at binding zero of a set allocated from `layout`, dispatch the pipeline once, and hand back
 * the buffer's first `word_count` words.
 *
 * @param variable_count Passed through to DescriptorSet::Create, for a layout whose binding has a variable count.
 */
Opal::DynamicArray<u32> DispatchIntoStorageBuffer(ForgeFixture& fixture, const Forge::Pipeline& pipeline,
                                                  const Forge::DescriptorSetLayout& layout, i32 word_count, u32 variable_count = 0)
{
    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 4) == ErrorCode::Success);
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
    Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout, variable_count));
    const Forge::Buffer output = MakeWipedOutput(fixture.device, word_count);
    REQUIRE(set.Update(0, output) == ErrorCode::Success);
    DispatchWithSet(fixture.device, fixture.GetQueue(), pipeline, set);
    Opal::DynamicArray<u32> words(word_count);
    REQUIRE(output.Read({reinterpret_cast<u8*>(words.GetData()), words.GetSize() * sizeof(u32)}) == ErrorCode::Success);
    return words;
}

/** The desc of a layout of one storage buffer binding visible to compute, of `count` descriptors and the given flags. */
Forge::DescriptorSetLayoutDesc MakeStorageLayoutDesc(u32 count = 1, Forge::DescriptorBindingFlagBits flags = Forge::DescriptorBindingFlagBits::None)
{
    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, count, ShaderTypeBits::Compute, {}, flags) ==
            ErrorCode::Success);
    return layout_desc;
}

}  // namespace

TEST_CASE("Forge scalar block layout", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_scalar{.scalar_block_layout = true};
    if (!CanCreateDevice(k_scalar))
    {
        SKIP("This device cannot lay out blocks the scalar way.");
    }

    SECTION("With the feature, a block that only lays out under it is written where scalar layout puts it")
    {
        ForgeFixture fixture(k_scalar);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_scalar_layout_source, {.entry_point = "main_scalar_layout", .cache = GetShaderCache()}));
        const Forge::DescriptorSetLayout layout =
            ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, MakeStorageLayoutDesc()));
        const Forge::Pipeline pipeline = MakeSetPipeline(fixture.device, shader, layout);
        // Twelve words rather than ten, so two elements packed any looser would spill into the last two.
        const Opal::DynamicArray<u32> words = DispatchIntoStorageBuffer(fixture, pipeline, layout, 12);
        for (i32 i = 0; i < 12; ++i)
        {
            f32 value = 0.0f;
            memcpy(&value, &words[i], sizeof(value));
            INFO("float " << i << " is " << value);
            // Five floats an element with nothing between them, and nothing past the second element.
            REQUIRE(value == (i < 10 ? static_cast<f32>(i + 1) : 0.0f));
        }
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("Without it, the layer refuses the same shader")
    {
        // What shows the field reaches scalarBlockLayout: the only thing different from the section above
        // is the field, and the layer's answer changes with it. Nothing is built past the shader - a
        // pipeline over a module the layer rejected is undefined behaviour.
        ForgeFixture fixture;
        REQUIRE_FALSE(fixture.device.GetFeatures().scalar_block_layout);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_scalar_layout_source, {.entry_point = "main_scalar_layout", .cache = GetShaderCache()}));
        INFO(*fixture.GetValidationErrors());
        REQUIRE(fixture.GetValidationErrorCount() > 0);
    }
}

namespace
{

/**
 * Every invocation packs a key into the high word and its own index into the low one and takes the maximum
 * of the lot into one u64, the way a visibility buffer resolves depth and triangle id together. The key
 * repeats, so the winner is decided by both words and a maximum taken over 32 bits of either would differ.
 */
constexpr const char* k_int64_atomics_source = R"(
[[vk::binding(0, 0)]] RWStructuredBuffer<uint64_t> output;

uint64_t Packed(uint index)
{
    return (uint64_t((index * 37u) % 64u) << 32) | uint64_t(index);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main_buffer_max(uint3 thread_id : SV_DispatchThreadID)
{
    InterlockedMax(output[0], Packed(thread_id.x));
}

groupshared uint64_t g_group_max;

[shader("compute")]
[numthreads(64, 1, 1)]
void main_shared_max(uint3 thread_id : SV_DispatchThreadID, uint3 local_id : SV_GroupThreadID, uint3 group_id : SV_GroupID)
{
    if (local_id.x == 0)
    {
        g_group_max = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    InterlockedMax(g_group_max, Packed(thread_id.x));
    GroupMemoryBarrierWithGroupSync();
    if (local_id.x == 0)
    {
        output[group_id.x] = g_group_max;
    }
}
)";

/** The same packing on the CPU. */
u64 PackedKey(u32 index)
{
    return (static_cast<u64>((index * 37u) % 64u) << 32) | index;
}

}  // namespace

/**
 * DeviceFeatures::shader_buffer_int64_atomics and shader_shared_int64_atomics. Each is used and read back,
 * and each is shown to reach its own Vulkan bit by building a device with only the other one on: the layer
 * then refuses the shader that needs the missing one, so the two fields exchanged on the way to the driver
 * would make both of those refusals disappear.
 */
TEST_CASE("Forge 64-bit atomics", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_both{.shader_int64 = true, .shader_buffer_int64_atomics = true, .shader_shared_int64_atomics = true};
    if (!CanCreateDevice(k_both))
    {
        SKIP("This device has no 64-bit atomics on both storage buffers and shared memory.");
    }
    constexpr i32 k_group_size = 64;
    constexpr i32 k_group_count = 4;
    constexpr u32 k_invocation_count = k_group_size * k_group_count;

    /** Dispatch one entry point over the buffer of u64 and hand back what it holds. */
    auto run = [&](ForgeFixture& fixture, const char* entry_point, i32 word_count)
    {
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_int64_atomics_source, {.entry_point = entry_point, .cache = GetShaderCache()}));
        const Forge::DescriptorSetLayout layout =
            ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, MakeStorageLayoutDesc()));
        const Forge::Pipeline pipeline = MakeSetPipeline(fixture.device, shader, layout);
        Forge::DescriptorPoolDesc pool_desc;
        REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1) == ErrorCode::Success);
        const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        const Forge::Buffer output = MakeWipedOutput(fixture.device, 2 * word_count);
        REQUIRE(set.Update(0, output) == ErrorCode::Success);
        DispatchWithSet(fixture.device, fixture.GetQueue(), pipeline, set, k_group_count);
        Opal::DynamicArray<u64> words(word_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(words.GetData()), words.GetSize() * sizeof(u64)}) == ErrorCode::Success);
        return words;
    };

    SECTION("A maximum taken over a storage buffer is the largest of every invocation's key")
    {
        ForgeFixture fixture(k_both);
        u64 expected = 0;
        for (u32 index = 0; index < k_invocation_count; ++index)
        {
            expected = Opal::Max(expected, PackedKey(index));
        }
        const Opal::DynamicArray<u64> words = run(fixture, "main_buffer_max", 1);
        INFO("expected " << expected << ", got " << words[0]);
        REQUIRE(words[0] == expected);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("A maximum taken in shared memory is each workgroup's own largest key")
    {
        ForgeFixture fixture(k_both);
        const Opal::DynamicArray<u64> words = run(fixture, "main_shared_max", k_group_count);
        for (u32 group = 0; group < k_group_count; ++group)
        {
            u64 expected = 0;
            for (u32 index = group * k_group_size; index < (group + 1) * k_group_size; ++index)
            {
                expected = Opal::Max(expected, PackedKey(index));
            }
            INFO("group " << group << " expected " << expected << ", got " << words[group]);
            REQUIRE(words[group] == expected);
        }
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("With only the shared field on, the layer refuses the buffer atomics")
    {
        // Nothing is built past the shader - a pipeline over a module the layer rejected is undefined.
        ForgeFixture fixture({.shader_int64 = true, .shader_shared_int64_atomics = true});
        REQUIRE(fixture.status == ErrorCode::Success);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_int64_atomics_source, {.entry_point = "main_buffer_max", .cache = GetShaderCache()}));
        INFO(*fixture.GetValidationErrors());
        REQUIRE(fixture.GetValidationErrorCount() > 0);
    }
    SECTION("With only the buffer field on, the layer refuses the shared atomics")
    {
        ForgeFixture fixture({.shader_int64 = true, .shader_buffer_int64_atomics = true});
        REQUIRE(fixture.status == ErrorCode::Success);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_int64_atomics_source, {.entry_point = "main_shared_max", .cache = GetShaderCache()}));
        INFO(*fixture.GetValidationErrors());
        REQUIRE(fixture.GetValidationErrorCount() > 0);
    }
}

TEST_CASE("Forge runtime descriptor arrays", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }

    /** Build the runtime array shader on a device with these features and dispatch through element zero of it. */
    auto run_on = [](const Forge::DeviceFeatures& features)
    {
        ForgeFixture fixture(features);
        REQUIRE(fixture.status == ErrorCode::Success);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_runtime_array_source, {.entry_point = "main_runtime_array", .cache = GetShaderCache()}));
        const Forge::DescriptorSetLayout layout =
            ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, MakeStorageLayoutDesc()));
        const Forge::Pipeline pipeline = MakeSetPipeline(fixture.device, shader, layout);
        const Opal::DynamicArray<u32> words = DispatchIntoStorageBuffer(fixture, pipeline, layout, 1);
        REQUIRE(words[0] == 4321u);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    };

    SECTION("On by default, a shader with an unsized descriptor array builds and runs")
    {
        REQUIRE(Forge::DeviceFeatures{}.runtime_descriptor_array);
        run_on({});
    }
    SECTION("Asked for alone, without the rest of descriptor indexing, it is still enough")
    {
        // The question the defaults hide: descriptor_indexing and variable_descriptor_count are on beside it
        // everywhere else, so nothing had shown this field carries the capability by itself.
        constexpr Forge::DeviceFeatures k_alone{.descriptor_indexing = false, .variable_descriptor_count = false};
        if (!CanCreateDevice(k_alone))
        {
            SKIP("This device cannot be created with descriptor indexing turned off.");
        }
        run_on(k_alone);
    }
    SECTION("Turned off on its own, the layer refuses the same shader")
    {
        ForgeFixture fixture(Forge::DeviceFeatures{.runtime_descriptor_array = false});
        REQUIRE(fixture.status == ErrorCode::Success);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_runtime_array_source, {.entry_point = "main_runtime_array", .cache = GetShaderCache()}));
        INFO(*fixture.GetValidationErrors());
        REQUIRE(fixture.GetValidationErrorCount() > 0);
    }
}

TEST_CASE("Forge variable descriptor count", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }

    SECTION("On by default, a variable count binding with no other descriptor indexing flag passes the layer")
    {
        // The bindless cases only ever use it beside partially bound and update after bind, which want
        // features of their own; this is the flag alone, on the device every other case gets. One of the
        // four descriptors the layout allows is allocated, and it is the one written and read.
        ForgeFixture fixture;
        REQUIRE(fixture.device.GetFeatures().variable_descriptor_count);
        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_runtime_array_source, {.entry_point = "main_runtime_array", .cache = GetShaderCache()}));
        const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(
            fixture.device, MakeStorageLayoutDesc(4, Forge::DescriptorBindingFlagBits::VariableDescriptorCount)));
        const Forge::Pipeline pipeline = MakeSetPipeline(fixture.device, shader, layout);
        const Opal::DynamicArray<u32> words = DispatchIntoStorageBuffer(fixture, pipeline, layout, 1, 1);
        REQUIRE(words[0] == 4321u);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("On a device without it, a variable count binding is refused")
    {
        ForgeFixture fixture(Forge::DeviceFeatures{.variable_descriptor_count = false});
        REQUIRE(fixture.status == ErrorCode::Success);
        REQUIRE(
            Forge::DescriptorSetLayout::Create(fixture.device, MakeStorageLayoutDesc(4, Forge::DescriptorBindingFlagBits::VariableDescriptorCount))
                .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
}

TEST_CASE("Forge a BC texture on a device without the feature is refused", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    // Vulkan would take the texture on a device that reports the format, and the layer would say nothing,
    // so the refusal is Forge's own and nothing else stands behind it.
    ForgeFixture fixture;
    REQUIRE_FALSE(fixture.device.GetFeatures().texture_compression_bc);
    constexpr Forge::TextureUsageBits k_usage = Forge::TextureUsageBits::Sampled | Forge::TextureUsageBits::TransferDestination;
    // The first and the last of the BC range, so a check that stops short at either end shows.
    REQUIRE(Forge::Texture::Create(fixture.device, {.format = PixelFormat::BC1_RGB_UNORM_BLOCK, .width = 4, .height = 4, .usage = k_usage})
                      .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    REQUIRE(Forge::Texture::Create(fixture.device, {.format = PixelFormat::BC7_SRGB_BLOCK, .width = 4, .height = 4, .usage = k_usage})
                      .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
    // An uncompressed format on the same device is not refused. Not the formats either side of the range:
    // ETC2 is a feature of its own that DeviceFeatures does not model, and D32_SFLOAT_S8_UINT is not a format
    // every device offers, so neither could be asked for here without a probe of its own.
    const Forge::Texture unaffected = ForgeTest::Unwrap(
        Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM, .width = 4, .height = 4, .usage = k_usage}));
    REQUIRE(unaffected.IsValid());
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * A BC1 texture uploaded and sampled. One four by four block, encoded by hand: red and blue as the two end
 * colours, and a two bit index per texel choosing between them. Only the two end colours are used, never
 * the two BC1 interpolates between them, so every texel decodes to an exact value.
 *
 * The validation layer does not tie BC formats to textureCompressionBC - a device that reports the formats
 * lets them be used either way - so unlike the cases above, this one cannot show the field reaches its bit;
 * the case above shows Forge refuses the format without it. What this one covers is the path a compressed
 * texture takes through Forge, which no case had taken: an upload sized in blocks rather than texels, and a
 * sample that decodes it.
 */
TEST_CASE("Forge a BC compressed texture uploaded and sampled", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    constexpr Forge::DeviceFeatures k_bc{.texture_compression_bc = true};
    if (!CanCreateDevice(k_bc))
    {
        SKIP("This device has no BC compressed formats.");
    }
    ForgeFixture fixture(k_bc);
    constexpr i32 k_side = 4;

    // Colour 0 is pure red and colour 1 pure blue in RGB565. Colour 0 is the larger of the two, which is what
    // selects the four colour mode, in which index 0 is colour 0, index 1 is colour 1 and alpha is opaque.
    constexpr u16 k_red_565 = 0xF800;
    constexpr u16 k_blue_565 = 0x001F;
    // Two bits a texel, texel (x, y) at bit 2 * (4 * y + x). Row 0 alternates red and blue, row 1 starts with
    // two blues, the rest is red - a pattern with no symmetry to hide a transposed or reversed index.
    constexpr u32 k_indices = (1u << 2) | (1u << 6) | (1u << 8) | (1u << 10);
    u8 block[8] = {};
    memcpy(block + 0, &k_red_565, sizeof(k_red_565));
    memcpy(block + 2, &k_blue_565, sizeof(k_blue_565));
    memcpy(block + 4, &k_indices, sizeof(k_indices));

    Forge::Texture texture = ForgeTest::Unwrap(
        Forge::Texture::Create(fixture.device, {.format = PixelFormat::BC1_RGBA_UNORM_BLOCK,
                                                .width = k_side,
                                                .height = k_side,
                                                .usage = Forge::TextureUsageBits::Sampled | Forge::TextureUsageBits::TransferDestination}));
    UploadMip(fixture.device, fixture.GetQueue(), texture, {block, sizeof(block)}, 0);

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_combined_sample_source, {.entry_point = "main_sample_combined", .cache = GetShaderCache()}));
    SampleHarness harness = MakeSampleHarness(fixture.device, k_side * k_side);
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(harness.layout));
    pipeline_desc.push_constant_ranges.PushBack({.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(SampleParams)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    const Forge::Sampler nearest =
        ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest, .mag_filter = ImageFilter::Nearest}));

    for (i32 y = 0; y < k_side; ++y)
    {
        for (i32 x = 0; x < k_side; ++x)
        {
            // The centre of the texel, so a nearest filter has no neighbour to pick instead.
            const SampleParams params{.uv = {(static_cast<f32>(x) + 0.5f) / k_side, (static_cast<f32>(y) + 0.5f) / k_side}};
            const Vector4f result = SampleOnce(fixture, harness, pipeline, texture, nearest, params);

            const bool is_blue = ((k_indices >> (2 * (y * k_side + x))) & 0x3u) == 1u;
            INFO("texel " << x << "," << y << " rgba " << result.x << " " << result.y << " " << result.z << " " << result.w
                          << ", expected " << (is_blue ? "blue" : "red"));
            REQUIRE(result.x == Catch::Approx(is_blue ? 0.0f : 1.0f).margin(0.01));
            REQUIRE(result.y == Catch::Approx(0.0f).margin(0.01));
            REQUIRE(result.z == Catch::Approx(is_blue ? 1.0f : 0.0f).margin(0.01));
            REQUIRE(result.w == Catch::Approx(1.0f).margin(0.01));
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

/**
 * TextureUsageBits::TransientAttachment, which no case had used. A transient attachment is one whose contents
 * never outlive the pass that renders into it - cleared on the way in, thrown away on the way out - which is
 * what lets a tiled device keep it in tile memory and never back it at all. A depth buffer is the everyday
 * one, so that is what is drawn with: the colour target is the only thing read back, and whether the depth
 * test used the transient buffer is decided by which of two quads ends up on top.
 *
 * A multisampled transient colour attachment is the other textbook use, and it cannot be written here:
 * resolving it needs either a resolve at the end of the rendering pass, which Forge does not have yet, or
 * CmdResolveTexture, which reads its source as a transfer source - a usage Vulkan does not allow beside
 * the transient one.
 */
TEST_CASE("Forge a transient attachment", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_depth_format = PixelFormat::D32_SFLOAT;
    constexpr Forge::TextureUsageBits k_transient_depth =
        Forge::TextureUsageBits::DepthStencilAttachment | Forge::TextureUsageBits::TransientAttachment;

    SECTION("A transient depth buffer, cleared in and discarded out, still decides what is drawn on top")
    {
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
        const Forge::Shader fragment_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_pushed_color_source, {.entry_point = "main_color_fragment", .cache = GetShaderCache()}));
        const Forge::Buffer near_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.25f));
        const Forge::Buffer far_quad = MakeQuadBuffer(fixture.device, MakeFullTargetQuad(0.75f));
        const Vector4f near_color = ByteColor(0, 255, 0, 255);
        const Vector4f far_color = ByteColor(255, 0, 0, 255);

        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, PixelFormat::R8G8B8A8_UNORM);
        pipeline_desc.depth_stencil.depth_test_enabled = true;
        pipeline_desc.depth_stencil.depth_write_enabled = true;
        pipeline_desc.depth_stencil.depth_comparator = Comparator::Less;
        pipeline_desc.depth_attachment_format = k_depth_format;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_side);
        Forge::Texture depth = ForgeTest::Unwrap(
            Forge::Texture::Create(fixture.device, {.format = k_depth_format, .width = k_side, .height = k_side, .usage = k_transient_depth}));

        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth)) ==
                                ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                                 .clear_value = Vector4f{0.0f, 0.0f, 1.0f, 1.0f}}},
                            // Cleared in and never stored: the whole of the depth buffer's life is this pass.
                            .depth_attachment = Forge::RenderingAttachmentDesc{.texture = depth,
                                                                               .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                               .store_operation = Forge::AttachmentStoreOperation::DontCare,
                                                                               .clear_value = Forge::DepthStencilClearValue{1.0f, 0}}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                        // Near first, then far. Without a working depth buffer the later draw wins and the target
                        // is red; with one, the far quad fails the test against what the near one wrote.
                        REQUIRE(command_buffer.CmdBindVertexBuffer(near_quad, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(near_color)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(far_quad, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(far_color)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);

        const Opal::DynamicArray<u8> pixels = ReadColorPixels(fixture, color, k_side);
        REQUIRE(CountCovered(pixels, k_side) == k_side * k_side);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("Transient beside a usage that is not an attachment is refused")
    {
        // Vulkan allows a transient image only the attachment usages, since memory that is never backed has
        // nothing to sample, store to or copy out of. A texture asking for more is refused before it reaches
        // the driver rather than left for the layer.
        constexpr Forge::TextureUsageBits k_not_attachments[] = {Forge::TextureUsageBits::Sampled, Forge::TextureUsageBits::Storage,
                                                                 Forge::TextureUsageBits::TransferSource,
                                                                 Forge::TextureUsageBits::TransferDestination};
        for (const Forge::TextureUsageBits extra : k_not_attachments)
        {
            INFO("extra usage " << static_cast<u32>(extra));
            REQUIRE(Forge::Texture::Create(fixture.device,
                                                 {.format = k_depth_format, .width = k_side, .height = k_side, .usage = k_transient_depth | extra})
                              .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        }
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("A transient attachment is lazily allocated exactly when the device has lazy memory it can use")
    {
        // Whether the device offers such memory is worked out here from Vulkan directly, not from anything
        // Forge reports: the memory types the image is allowed in, against the properties of each. Desktop
        // devices have none, and there this checks the fallback; a tiled one takes the other branch.
        const Forge::Texture transient = ForgeTest::Unwrap(
            Forge::Texture::Create(fixture.device, {.format = k_depth_format, .width = k_side, .height = k_side, .usage = k_transient_depth}));
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(fixture.device.GetNativeDevice(), transient.GetNativeImage(), &requirements);
        const VkPhysicalDeviceMemoryProperties& memory = fixture.device.GetPhysicalDevice().GetMemoryProperties();
        bool device_has_lazy_memory = false;
        for (u32 i = 0; i < memory.memoryTypeCount; ++i)
        {
            const bool allowed = (requirements.memoryTypeBits & (1u << i)) != 0;
            device_has_lazy_memory = device_has_lazy_memory || (allowed && !!(memory.memoryTypes[i].propertyFlags &
                                                                               VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT));
        }
        INFO("the device " << (device_has_lazy_memory ? "has" : "has no") << " lazily allocated memory this image can use");
        const VkMemoryPropertyFlags transient_properties = transient.GetMemoryProperties();
        REQUIRE(transient_properties != 0);
        REQUIRE(!!(transient_properties & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) == device_has_lazy_memory);

        // An ordinary texture is never put there, whatever the device has, and is in device local memory.
        const Forge::Texture ordinary = ForgeTest::Unwrap(Forge::Texture::Create(
            fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM, .width = k_side, .height = k_side, .usage = Forge::TextureUsageBits::Sampled}));
        REQUIRE_FALSE(!!(ordinary.GetMemoryProperties() & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT));
        REQUIRE(!!(ordinary.GetMemoryProperties() & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        // And a texture with no image has no memory to describe.
        REQUIRE(Forge::Texture{}.GetMemoryProperties() == 0);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
    SECTION("Transient with no attachment usage at all is refused")
    {
        // The other half of the same rule: transient says how an attachment is backed, so on its own it names
        // nothing the texture is for.
        REQUIRE(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                              .width = k_side,
                                                              .height = k_side,
                                                              .usage = Forge::TextureUsageBits::TransientAttachment})
                          .GetErrorOr(ErrorCode::Success) == ErrorCode::InvalidArgument);
        REQUIRE_NO_VALIDATION_ERROR(fixture);
    }
}
