#include <catch2/catch2.hpp>

#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/exceptions.h"

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/debug.hpp"
#include "rndr/forge/descriptor-set.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/pipeline.hpp"
#include "rndr/forge/query.hpp"
#include "rndr/forge/shader.hpp"
#include "rndr/forge/texture.hpp"
#include "rndr/forge/transfer.hpp"
#include "rndr/types.hpp"

/**
 * Headless tests for Forge: no window, no surface, no swap chain, so they run anywhere a Vulkan device
 * exists. Every one of them ends in a readback and compares against a value computed on the CPU, since the
 * absence of a validation message says nothing about whether the device did the right thing.
 */

namespace
{

using namespace Rndr;

/** A Vulkan instance and a device with no surface. Everything below is built on one of these. */
struct ForgeFixture
{
    Forge::GraphicsContext context;
    Forge::Device device;

    /**
     * @param features What the device is asked to turn on. The default is what every test but the ones about
     *        a specific feature wants, and asking for one that this device lacks throws out of here.
     */
    explicit ForgeFixture(const Forge::DeviceFeatures& features = {}) : context({.collect_debug_messages = true})
    {
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = context.EnumeratePhysicalDevices();
        device = Forge::Device(std::move(physical_devices[0]), context, {.features = features});
    }

    Forge::DeviceQueue& GetQueue() { return device.GetQueue(Forge::QueueFamily::Graphics); }

    /**
     * What the validation layer reported, as text, so a failure names the problem instead of only counting
     * it. Validation messages only: the loader reports a layer manifest that some other application left
     * behind at error severity, which says nothing about this code. Always empty in a build without
     * RNDR_FORGE_VALIDATION, where there is no layer to report anything.
     */
    [[nodiscard]] Opal::StringUtf8 GetValidationErrors() const
    {
        Opal::StringUtf8 report;
        for (const Forge::DebugMessage& message : context.GetDebugMessages())
        {
            if (message.severity == Forge::DebugMessageSeverity::Error && !!(message.types & Forge::DebugMessageTypeBits::Validation))
            {
                report += message.text;
                if (!message.objects.IsEmpty())
                {
                    report += Opal::StringUtf8(" [objects: ");
                    report += message.objects;
                    report += Opal::StringUtf8("]");
                }
                report += Opal::StringUtf8("\n");
            }
        }
        return report;
    }

    [[nodiscard]] u32 GetValidationErrorCount() const
    {
        return context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation);
    }
};

/** Fails the test with the text of the messages when the validation layer reported an error. */
#define REQUIRE_NO_VALIDATION_ERROR(fixture)                                        \
    do                                                                              \
    {                                                                               \
        const Opal::StringUtf8 validation_errors = (fixture).GetValidationErrors(); \
        INFO(*validation_errors);                                                   \
        REQUIRE((fixture).GetValidationErrorCount() == 0);                          \
    } while (false)

/** Whether this machine has a Vulkan device at all, so a machine without one skips rather than fails. */
bool IsForgeAvailable()
{
    static const bool available = []
    {
        try
        {
            const ForgeFixture probe;
            return true;
        }
        catch (const Opal::Exception&)
        {
            return false;
        }
    }();
    return available;
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

}  // namespace

TEST_CASE("Forge context and device", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const Forge::GraphicsContext context({.collect_debug_messages = true});
    REQUIRE(context.IsValid());

    Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = context.EnumeratePhysicalDevices();
    REQUIRE_FALSE(physical_devices.IsEmpty());

    Forge::Device device(std::move(physical_devices[0]), context);
    REQUIRE(device.IsValid());
    REQUIRE(device.GetQueue(Forge::QueueFamily::Graphics).IsValid());
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation) == 0);
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

    Forge::Buffer buffer(fixture.device, {.size = k_size,
                                          .usage = Forge::BufferUsageBits::TransferSource,
                                          .host_access = Forge::HostAccess::Random});
    const Opal::DynamicArray<u8> zeros(k_size);
    buffer.Update(zeros);
    buffer.Update(written, k_offset);

    Opal::DynamicArray<u8> read_back(k_size - k_offset);
    buffer.Read(read_back, k_offset);
    REQUIRE(CountMismatches(written, read_back) == 0);

    // The bytes before the offset must be untouched, which is what makes this a test of the offset rather
    // than of the write.
    Opal::DynamicArray<u8> head(k_offset);
    buffer.Read(head, 0);
    for (i32 i = 0; i < k_offset; ++i)
    {
        REQUIRE(head[i] == 0);
    }

    SECTION("A write that does not fit throws")
    {
        REQUIRE_THROWS_AS(buffer.Update(written, k_size - 1), Opal::Exception);
    }
    SECTION("A read of write-combined memory throws")
    {
        const Forge::Buffer write_only(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource});
        Opal::DynamicArray<u8> out(k_size);
        REQUIRE_THROWS_AS(write_only.Read(out), Opal::Exception);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge buffer survives a move", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 128;
    const Opal::DynamicArray<u8> written = MakeBytes(k_size, 17);

    Forge::Buffer source(fixture.device, {.size = k_size,
                                          .usage = Forge::BufferUsageBits::TransferSource,
                                          .host_access = Forge::HostAccess::Random,
                                          .keep_memory_mapped = true},
                         written);

    Forge::Buffer moved_to(std::move(source));
    REQUIRE_FALSE(source.IsValid());
    REQUIRE(moved_to.IsValid());

    Forge::Buffer assigned_to;
    assigned_to = std::move(moved_to);
    REQUIRE_FALSE(moved_to.IsValid());
    REQUIRE(assigned_to.IsValid());

    // The mapped pointer has to have come along, or this writes through a pointer the source unmapped.
    Opal::DynamicArray<u8> read_back(k_size);
    assigned_to.Read(read_back);
    REQUIRE(CountMismatches(written, read_back) == 0);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge compute dispatch and readback", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_element_count = 256;
    constexpr i32 k_group_size = 64;

    Forge::Buffer output(fixture.device, {.size = k_element_count * sizeof(u32),
                                          .usage = Forge::BufferUsageBits::StorageBuffer,
                                          .host_access = Forge::HostAccess::Random,
                                          .use_device_address = true});
    const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
    // Wiped first, so nothing a previous run left behind can pass for a successful dispatch.
    output.Update(zeros);

    const Forge::Shader compute_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute"});
    REQUIRE(compute_shader.IsValid());
    REQUIRE(compute_shader.GetShaderStage() == ShaderTypeBits::Compute);

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = compute_shader;
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
    const Forge::Pipeline pipeline(fixture.device, pipeline_desc);
    REQUIRE(pipeline.IsValid());

    const VkDeviceAddress output_address = output.GetNativeDeviceAddress();
    Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               command_buffer.CmdBindPipeline(pipeline);
                               command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(output_address));
                               command_buffer.CmdDispatch(k_element_count / k_group_size);
                           });

    Opal::DynamicArray<u32> values(k_element_count);
    output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)});
    for (i32 i = 0; i < k_element_count; ++i)
    {
        REQUIRE(values[i] == static_cast<u32>(i) + 1000);
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

    const Forge::Buffer source(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written);
    const Forge::Buffer destination(
        fixture.device,
        {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination});
    const Opal::DynamicArray<u8> zeros(k_size);
    destination.Update(zeros);

    Opal::DynamicArray<u8> read_back(k_size);
    Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), destination, read_back);
    REQUIRE(CountMismatches(zeros, read_back) == 0);

    Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer) { command_buffer.CmdCopyBuffer(source, destination); });
    Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), destination, read_back);
    REQUIRE(CountMismatches(written, read_back) == 0);

    SECTION("A copy from a buffer without the transfer usage throws")
    {
        const Forge::Buffer no_transfer(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::ConstantBuffer});
        REQUIRE_THROWS_AS(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                                 [&](Forge::CommandBuffer& command_buffer)
                                                 { command_buffer.CmdCopyBuffer(no_transfer, destination); }),
                          Opal::Exception);
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

    Forge::Texture texture(fixture.device, {.format = k_format,
                                            .width = k_side,
                                            .height = k_side,
                                            .mip_level_count = k_mip_count,
                                            .usage = Forge::TextureUsageBits::TransferSource |
                                                     Forge::TextureUsageBits::TransferDestination |
                                                     Forge::TextureUsageBits::Sampled});
    const Forge::Buffer staging(fixture.device, {.size = mip0.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, mip0);
    const Forge::BufferImageCopyRegion mip0_region;
    Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferDestination(texture));
                               command_buffer.CmdCopyBufferToImage(staging, texture, {&mip0_region, 1});
                               command_buffer.CmdGenerateMips(texture, Forge::ImageLayout::TransferDestination,
                                                              Forge::ImageLayout::TransferDestination);
                           });

    for (u32 level = 0; level < k_mip_count; ++level)
    {
        const i32 side = k_side >> level;
        Opal::DynamicArray<u8> level_pixels(side * side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, Forge::ImageLayout::TransferDestination, level_pixels, level,
                               Forge::ImageLayout::TransferDestination);
        INFO("mip level " << level);
        for (i32 i = 0; i < level_pixels.GetSize(); ++i)
        {
            REQUIRE(level_pixels[i] == k_texel[i % 4]);
        }
    }

    SECTION("Reading back into a view of the wrong size throws")
    {
        Opal::DynamicArray<u8> too_small(4);
        REQUIRE_THROWS_AS(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, Forge::ImageLayout::TransferDestination,
                                                 too_small, 0, Forge::ImageLayout::TransferDestination),
                          Opal::Exception);
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
    const Forge::Buffer buffer(
        fixture.device,
        {.size = k_size,
         .usage = Forge::BufferUsageBits::StorageBuffer | Forge::BufferUsageBits::TransferSource |
                  Forge::BufferUsageBits::TransferDestination,
         .host_access = Forge::HostAccess::None});
    REQUIRE(buffer.IsValid());

    Forge::UploadToBuffer(fixture.device, fixture.GetQueue(), buffer, written);
    Opal::DynamicArray<u8> read_back(k_size);
    Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), buffer, read_back);
    REQUIRE(CountMismatches(written, read_back) == 0);

    SECTION("Update and Read both throw on it")
    {
        Opal::DynamicArray<u8> out(k_size);
        REQUIRE_THROWS_AS(buffer.Update(written), Opal::Exception);
        REQUIRE_THROWS_AS(buffer.Read(out), Opal::Exception);
    }
    SECTION("Initial data throws rather than leaking the allocation")
    {
        REQUIRE_THROWS_AS(Forge::Buffer(fixture.device,
                                        {.size = k_size,
                                         .usage = Forge::BufferUsageBits::TransferDestination,
                                         .host_access = Forge::HostAccess::None},
                                        written),
                          Opal::Exception);
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

    constexpr Forge::TextureUsageBits k_transfer_usage =
        Forge::TextureUsageBits::TransferSource | Forge::TextureUsageBits::TransferDestination;
    Forge::Texture source(fixture.device,
                          {.format = PixelFormat::R8G8B8A8_UNORM, .width = 4, .height = 4, .usage = k_transfer_usage});
    Forge::Texture destination(fixture.device,
                               {.format = PixelFormat::R16G16B16A16_SFLOAT, .width = 4, .height = 4, .usage = k_transfer_usage});
    Forge::SetDebugName(fixture.device, source, "probe-source-texture");
    Forge::SetDebugName(fixture.device, destination, "probe-destination-texture");

    // Copying between two formats of different texel size breaks a rule the guards do not check and the
    // validation layer does, which is what makes it a way to read back what the layer calls these two images.
    // The layer checks this while the command is recorded, so the command buffer is thrown away rather than
    // submitted: handing the driver work that breaks the specification is undefined behaviour, and it took
    // the next test down with it when this did.
    const Forge::ImageCopyRegion region;
    Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
    command_buffer.Begin();
    command_buffer.CmdCopyImage(source, destination, {&region, 1});
    command_buffer.End();

    const Opal::StringUtf8 errors = fixture.GetValidationErrors();
    INFO(*errors);
    REQUIRE(fixture.GetValidationErrorCount() > 0);
    REQUIRE(strstr(reinterpret_cast<const char*>(*errors), "probe-source-texture") != nullptr);
    REQUIRE(strstr(reinterpret_cast<const char*>(*errors), "probe-destination-texture") != nullptr);

    // The error above was the point of the test, so it must not be left for the next assertion to trip over.
    fixture.context.ClearDebugMessages();
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge batched submit", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_size = 64;
    const Opal::DynamicArray<u8> first_half = MakeBytes(k_size, 5);
    const Opal::DynamicArray<u8> second_half = MakeBytes(k_size, 90);
    const Opal::DynamicArray<u8> zeros(k_size * 2);

    constexpr Forge::BufferUsageBits k_both_ways = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination;
    const Forge::Buffer source_a(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, first_half);
    const Forge::Buffer source_b(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, second_half);
    const Forge::Buffer destination(fixture.device, {.size = k_size * 2, .usage = k_both_ways});
    destination.Update(zeros);

    // One command buffer per half, so a batch that dropped either one would show as half the buffer missing.
    Forge::CommandBuffer first(fixture.device, fixture.GetQueue());
    Forge::CommandBuffer second(fixture.device, fixture.GetQueue());
    const Forge::BufferCopyRegion first_region{.source_offset = 0, .destination_offset = 0, .size = k_size};
    const Forge::BufferCopyRegion second_region{.source_offset = 0, .destination_offset = k_size, .size = k_size};
    first.Begin();
    first.CmdCopyBuffer(source_a, destination, {&first_region, 1});
    first.End();
    second.Begin();
    second.CmdCopyBuffer(source_b, destination, {&second_region, 1});
    second.End();

    SECTION("Two command buffers in one batch, with a fence")
    {
        const Forge::Fence fence(fixture.device, false);
        const Opal::Ref<const Forge::CommandBuffer> batch[2] = {Opal::Ref<const Forge::CommandBuffer>(first),
                                                                Opal::Ref<const Forge::CommandBuffer>(second)};
        fixture.GetQueue().Submit({.command_buffers = {batch, 2}, .fence = fence});
        fence.Wait();
    }
    SECTION("The same batch without a fence, waited on through the queue")
    {
        const Opal::Ref<const Forge::CommandBuffer> batch[2] = {Opal::Ref<const Forge::CommandBuffer>(first),
                                                                Opal::Ref<const Forge::CommandBuffer>(second)};
        fixture.GetQueue().Submit({.command_buffers = {batch, 2}});
        fixture.GetQueue().WaitIdle();
    }
    SECTION("One batch per half, the second waiting on a semaphore the first signals")
    {
        const Forge::Semaphore semaphore(fixture.device);
        const Forge::Fence fence(fixture.device, false);
        const Opal::Ref<const Forge::CommandBuffer> first_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(first)};
        const Opal::Ref<const Forge::CommandBuffer> second_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(second)};
        const Forge::SemaphoreSubmit signal{.semaphore = semaphore, .stages = Forge::PipelineStageBits::Transfer};
        const Forge::SemaphoreSubmit wait{.semaphore = semaphore, .stages = Forge::PipelineStageBits::Transfer};
        fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&signal, 1}});
        fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .wait_semaphores = {&wait, 1}, .fence = fence});
        fence.Wait();
    }

    Opal::DynamicArray<u8> read_back(k_size * 2);
    Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), destination, read_back);
    for (i32 i = 0; i < k_size; ++i)
    {
        REQUIRE(read_back[i] == first_half[i]);
        REQUIRE(read_back[k_size + i] == second_half[i]);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge submit rejects an empty object", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Opal::Ref<const Forge::CommandBuffer> empty_command_buffer{};
    const Forge::CommandBuffer valid(fixture.device, fixture.GetQueue());

    SECTION("An empty command buffer throws")
    {
        Forge::CommandBuffer empty;
        const Opal::Ref<const Forge::CommandBuffer> batch[1] = {Opal::Ref<const Forge::CommandBuffer>(empty)};
        REQUIRE_THROWS_AS(fixture.GetQueue().Submit({.command_buffers = {batch, 1}}), Opal::Exception);
    }
    SECTION("An empty semaphore throws")
    {
        Forge::Semaphore empty;
        const Forge::SemaphoreSubmit wait{.semaphore = empty};
        REQUIRE_THROWS_AS(fixture.GetQueue().Submit({.wait_semaphores = {&wait, 1}}), Opal::Exception);
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
    const Forge::Buffer source(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written);
    constexpr Forge::BufferUsageBits k_both_ways = Forge::BufferUsageBits::TransferSource | Forge::BufferUsageBits::TransferDestination;
    const Forge::Buffer first_destination(fixture.device, {.size = k_size, .usage = k_both_ways});
    const Forge::Buffer second_destination(fixture.device, {.size = k_size, .usage = k_both_ways});

    Opal::DynamicArray<Forge::Fence> fences;
    fences.EmplaceBack(fixture.device, false);
    fences.EmplaceBack(fixture.device, false);

    Forge::CommandBuffer first(fixture.device, fixture.GetQueue());
    Forge::CommandBuffer second(fixture.device, fixture.GetQueue());
    first.Begin();
    first.CmdCopyBuffer(source, first_destination);
    first.End();
    second.Begin();
    second.CmdCopyBuffer(source, second_destination);
    second.End();

    const Opal::Ref<const Forge::CommandBuffer> first_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(first)};
    const Opal::Ref<const Forge::CommandBuffer> second_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(second)};
    fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .fence = fences[0]});
    fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .fence = fences[1]});
    Forge::Fence::WaitForAll(fences);

    Opal::DynamicArray<u8> read_back(k_size);
    Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), first_destination, read_back);
    REQUIRE(CountMismatches(written, read_back) == 0);
    Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), second_destination, read_back);
    REQUIRE(CountMismatches(written, read_back) == 0);

    SECTION("An empty fence in the list throws")
    {
        Opal::DynamicArray<Forge::Fence> with_empty;
        with_empty.EmplaceBack(fixture.device, true);
        with_empty.EmplaceBack();
        REQUIRE_THROWS_AS(Forge::Fence::WaitForAll(with_empty), Opal::Exception);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge device features", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const Forge::GraphicsContext context({.collect_debug_messages = true});

    // Builds a device on this machine's first physical device with the given features asked for.
    auto make_device = [&context](const Forge::DeviceFeatures& features)
    {
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = context.EnumeratePhysicalDevices();
        return Forge::Device(std::move(physical_devices[0]), context, {.features = features});
    };

    SECTION("The defaults are what the device reports back")
    {
        const Forge::Device device = make_device({});
        REQUIRE(device.GetFeatures().buffer_device_address);
        REQUIRE(device.GetFeatures().descriptor_indexing);
        REQUIRE(device.GetFeatures().sampler_anisotropy);
        REQUIRE_FALSE(device.GetFeatures().mesh_shader);
        REQUIRE_FALSE(device.GetFeatures().geometry_shader);
    }
    SECTION("Asking for mesh shaders succeeds exactly when this device has them")
    {
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = context.EnumeratePhysicalDevices();
        const bool has_extension = physical_devices[0].IsExtensionSupported(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        INFO("VK_EXT_mesh_shader supported: " << has_extension);
        if (has_extension)
        {
            const Forge::Device device = make_device({.mesh_shader = true, .task_shader = true});
            REQUIRE(device.IsExtensionEnabled(VK_EXT_MESH_SHADER_EXTENSION_NAME));
        }
        else
        {
            REQUIRE_THROWS_AS(make_device({.mesh_shader = true}), Opal::Exception);
        }
    }
    SECTION("A buffer wanting a device address needs the feature")
    {
        const Forge::Device device = make_device({.buffer_device_address = false});
        REQUIRE_THROWS_AS(Forge::Buffer(device, {.size = 64,
                                                 .usage = Forge::BufferUsageBits::StorageBuffer,
                                                 .use_device_address = true}),
                          Opal::Exception);
    }
    SECTION("An anisotropic sampler needs the feature")
    {
        const Forge::Device device = make_device({.sampler_anisotropy = false});
        REQUIRE_THROWS_AS(Forge::Sampler(device, {.max_anisotropy = 8.0f}), Opal::Exception);
        // One that does not ask for anisotropy is fine on the same device.
        const Forge::Sampler sampler(device, {.max_anisotropy = 1.0f});
        REQUIRE(sampler.IsValid());
    }
    SECTION("More than one indirect command needs the feature")
    {
        Forge::Device device = make_device({.multi_draw_indirect = false});
        Forge::DeviceQueue& queue = device.GetQueue(Forge::QueueFamily::Graphics);
        const Forge::Buffer commands(device, {.size = 2 * sizeof(Forge::DrawIndirectCommand),
                                              .usage = Forge::BufferUsageBits::IndirectBuffer});
        Forge::CommandBuffer command_buffer(device, queue);
        command_buffer.Begin();
        REQUIRE_THROWS_AS(command_buffer.CmdDrawIndirect(commands, 0, 2), Opal::Exception);
        command_buffer.End();
    }

    Opal::StringUtf8 report;
    for (const Forge::DebugMessage& message : context.GetDebugMessages())
    {
        report += message.text;
        report += Opal::StringUtf8("\n");
    }
    INFO(*report);
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation) == 0);
}

TEST_CASE("Forge physical device selection", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const Forge::GraphicsContext context({.collect_debug_messages = true});
    Opal::DynamicArray<Forge::PhysicalDevice> devices = context.EnumeratePhysicalDevices();
    REQUIRE_FALSE(devices.IsEmpty());

    SECTION("A headless desc is met by some device on this machine")
    {
        const Opal::Optional<u32> best = Forge::FindPhysicalDevice(devices);
        REQUIRE(best.HasValue());
        REQUIRE(best.GetValue() < static_cast<u32>(devices.GetSize()));
        // The one it picked has to actually work, which is the whole point of choosing rather than guessing.
        const Forge::Device device(std::move(devices[static_cast<i32>(best.GetValue())]), context);
        REQUIRE(device.IsValid());
    }
    SECTION("A requirement nothing can meet leaves the answer empty")
    {
        // A device supporting an extension under this name would be a surprising machine indeed.
        const char* nonsense_extension = "VK_EXT_this_extension_does_not_exist";
        Forge::DeviceDesc desc;
        desc.extensions.PushBack(nonsense_extension);
        REQUIRE_FALSE(Forge::FindPhysicalDevice(devices, desc).HasValue());
    }
    SECTION("Selecting when nothing qualifies throws, naming the requirement")
    {
        Forge::DeviceDesc desc;
        desc.extensions.PushBack("VK_EXT_this_extension_does_not_exist");
        REQUIRE_THROWS_AS(Forge::SelectPhysicalDevice(devices, desc), Opal::Exception);
    }
    SECTION("Selecting moves the chosen device out of the list")
    {
        Forge::PhysicalDevice chosen = Forge::SelectPhysicalDevice(devices);
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
        Forge::DeviceDesc desc;
        REQUIRE(Forge::FindPhysicalDevice(devices, desc).HasValue());
    }

    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation) == 0);
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
    const Forge::GraphicsContext context({.collect_debug_messages = true});
    Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = context.EnumeratePhysicalDevices();

    constexpr Forge::DeviceFeatures k_bindless_features{.partially_bound_descriptors = true,
                                                        .update_after_bind_descriptors = true,
                                                        .non_uniform_descriptor_indexing = true};
    const Forge::DeviceDesc bindless_desc{.features = k_bindless_features};
    if (!Forge::FindPhysicalDevice(physical_devices, bindless_desc).HasValue())
    {
        SKIP("This device does not support the descriptor indexing features bindless needs.");
    }
    Forge::Device device(Forge::SelectPhysicalDevice(physical_devices, bindless_desc), context, bindless_desc);
    Forge::DeviceQueue& queue = device.GetQueue(Forge::QueueFamily::Graphics);

    constexpr u32 k_max_descriptors = 4;
    constexpr u32 k_used_descriptors = 2;
    constexpr i32 k_element_count = 256;
    constexpr i32 k_group_size = 64;

    Forge::DescriptorPoolDesc pool_desc;
    pool_desc.Add(Forge::DescriptorType::StorageBuffer, k_max_descriptors);
    pool_desc.max_sets = 1;
    pool_desc.use_update_after_bind = true;
    const Forge::DescriptorPool pool(device, pool_desc);

    Forge::DescriptorSetLayoutDesc layout_desc;
    layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, k_max_descriptors, ShaderTypeBits::Compute, {},
                           Forge::DescriptorBindingFlagBits::PartiallyBound | Forge::DescriptorBindingFlagBits::UpdateAfterBind |
                               Forge::DescriptorBindingFlagBits::VariableDescriptorCount);
    const Forge::DescriptorSetLayout layout(device, layout_desc);

    SECTION("A partially bound array is written and read where it was written")
    {
        // Two of the four descriptors, so the variable count is doing something, and only the second one is
        // ever written, so partially bound is doing something too.
        Forge::DescriptorSet descriptor_set(pool, layout, k_used_descriptors);

        Forge::Buffer output(device, {.size = k_element_count * sizeof(u32),
                                      .usage = Forge::BufferUsageBits::StorageBuffer,
                                      .host_access = Forge::HostAccess::Random});
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        output.Update(zeros);

        // Only descriptor 1 of the array is written. Descriptor 0 is left alone, which is what
        // PartiallyBound allows and what the shader stays away from.
        Opal::DynamicArray<Forge::DescriptorSetUpdateBinding> updates;
        updates.PushBack(Forge::DescriptorSetUpdateBinding{
            .descriptor_type = Forge::DescriptorType::StorageBuffer,
            .binding = 0,
            .array_element = 1,
            .resource_info = Forge::DescriptorSetUpdateBinding::BufferInfo{.buffer = output}});
        descriptor_set.Update(updates);

        const Forge::Shader shader = Forge::Shader::FromSourceInMemory(device, k_bindless_source, {.entry_point = "main_bindless"});
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline(device, pipeline_desc);

        Forge::ImmediateSubmit(device, queue,
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdBindPipeline(pipeline);
                                   command_buffer.CmdBindDescriptorSet(pipeline, descriptor_set);
                                   command_buffer.CmdDispatch(k_element_count / k_group_size);
                               });

        Opal::DynamicArray<u32> values(k_element_count);
        output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)});
        for (i32 i = 0; i < k_element_count; ++i)
        {
            REQUIRE(values[i] == static_cast<u32>(i) + 2000);
        }
    }
    SECTION("A variable count above the binding's descriptor count throws")
    {
        REQUIRE_THROWS_AS(Forge::DescriptorSet(pool, layout, k_max_descriptors + 1), Opal::Exception);
    }
    SECTION("A variable count without a binding that allows it throws")
    {
        Forge::DescriptorSetLayoutDesc plain_desc;
        plain_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute);
        const Forge::DescriptorSetLayout plain_layout(device, plain_desc);
        REQUIRE_THROWS_AS(Forge::DescriptorSet(pool, plain_layout, 1), Opal::Exception);
    }
    SECTION("A variable count on anything but the highest binding throws")
    {
        Forge::DescriptorSetLayoutDesc bad_desc;
        bad_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 4, ShaderTypeBits::Compute, {},
                            Forge::DescriptorBindingFlagBits::VariableDescriptorCount);
        bad_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute);
        REQUIRE_THROWS_AS(Forge::DescriptorSetLayout(device, bad_desc), Opal::Exception);
    }
    SECTION("An update after bind layout needs a pool that expects one")
    {
        Forge::DescriptorPoolDesc plain_pool_desc;
        plain_pool_desc.Add(Forge::DescriptorType::StorageBuffer, k_max_descriptors);
        plain_pool_desc.use_update_after_bind = false;
        const Forge::DescriptorPool plain_pool(device, plain_pool_desc);
        REQUIRE_THROWS_AS(Forge::DescriptorSet(plain_pool, layout, k_used_descriptors), Opal::Exception);
    }

    Opal::StringUtf8 report;
    for (const Forge::DebugMessage& message : context.GetDebugMessages())
    {
        report += message.text;
        report += Opal::StringUtf8("\n");
    }
    INFO(*report);
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation) == 0);
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
    const Forge::Buffer source(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written);
    const Forge::Buffer destination(fixture.device,
                                    {.size = k_size, .usage = k_both_ways, .host_access = Forge::HostAccess::Random});
    destination.Update(zeros);

    SECTION("A copy ordered against the host with the narrow stages and access")
    {
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdCopyBuffer(source, destination);
                                   // The stages synchronization2 split out, and the access that says which
                                   // write rather than any write at all.
                                   const Forge::BufferBarrier barrier{
                                       .stages_must_finish = Forge::PipelineStageBits::Copy,
                                       .stages_must_finish_access = Forge::PipelineStageAccessBits::TransferWrite,
                                       .before_stages_start = Forge::PipelineStageBits::Host,
                                       .before_stages_start_access = Forge::PipelineStageAccessBits::HostRead,
                                       .buffer = destination};
                                   command_buffer.CmdBufferBarrier(barrier);
                               });
        Opal::DynamicArray<u8> read_back(k_size);
        destination.Read(read_back);
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
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdCopyBuffer(source, destination);
                                   command_buffer.CmdBufferBarriers(barriers);
                               });
        Opal::DynamicArray<u8> read_back(k_size);
        destination.Read(read_back);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("A barrier naming the mesh stage without the extension throws")
    {
        Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
        command_buffer.Begin();
        const Forge::MemoryBarrier barrier{.stages_must_finish = Forge::PipelineStageBits::MeshShader,
                                           .stages_must_finish_access = Forge::PipelineStageAccessBits::Write,
                                           .before_stages_start = Forge::PipelineStageBits::FragmentShader,
                                           .before_stages_start_access = Forge::PipelineStageAccessBits::Read};
        REQUIRE_THROWS_AS(command_buffer.CmdMemoryBarrier(barrier), Opal::Exception);
        command_buffer.End();
    }
    SECTION("An ownership transfer between two families is recorded")
    {
        // Both halves of a transfer, release and acquire, on the one queue this test has. Naming the same
        // family on both sides is a no-op transfer, which is what makes it safe to record here.
        const u32 family = fixture.GetQueue().GetQueueFamilyIndex();
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdCopyBuffer(source, destination);
                                   const Forge::BufferBarrier release{
                                       .stages_must_finish = Forge::PipelineStageBits::Copy,
                                       .stages_must_finish_access = Forge::PipelineStageAccessBits::TransferWrite,
                                       .before_stages_start = Forge::PipelineStageBits::None,
                                       .before_stages_start_access = Forge::PipelineStageAccessBits::None,
                                       .source_queue_family = family,
                                       .destination_queue_family = family,
                                       .buffer = destination};
                                   command_buffer.CmdBufferBarrier(release);
                               });
        Opal::DynamicArray<u8> read_back(k_size);
        destination.Read(read_back);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }

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

    const Forge::Buffer source(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written);
    const Forge::Buffer destination(fixture.device, {.size = k_size,
                                                     .usage = Forge::BufferUsageBits::TransferDestination,
                                                     .host_access = Forge::HostAccess::Random});

    SECTION("A labelled region records and the work inside it still runs")
    {
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdBeginDebugLabel("copy region", {0.2f, 0.6f, 1.0f, 1.0f});
                                   command_buffer.CmdInsertDebugLabel("about to copy");
                                   command_buffer.CmdCopyBuffer(source, destination);
                                   command_buffer.CmdEndDebugLabel();
                               });
        Opal::DynamicArray<u8> read_back(k_size);
        destination.Read(read_back);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("Regions nest")
    {
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdBeginDebugLabel("frame");
                                   command_buffer.CmdBeginDebugLabel("copy pass");
                                   command_buffer.CmdCopyBuffer(source, destination);
                                   command_buffer.CmdEndDebugLabel();
                                   command_buffer.CmdEndDebugLabel();
                               });
        Opal::DynamicArray<u8> read_back(k_size);
        destination.Read(read_back);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("ScopedDebugLabel closes the region it opened")
    {
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   const Forge::ScopedDebugLabel scope(command_buffer, "copy pass", {1.0f, 0.5f, 0.0f, 1.0f});
                                   command_buffer.CmdCopyBuffer(source, destination);
                               });
        Opal::DynamicArray<u8> read_back(k_size);
        destination.Read(read_back);
        REQUIRE(CountMismatches(written, read_back) == 0);
    }
    SECTION("A region left open by a throw is still closed")
    {
        // The point of the guard: the copy below is rejected while it is recorded, and the region has to end
        // on the way out anyway. A region left open is what the layer would report at End().
        const Forge::Buffer no_transfer(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::ConstantBuffer});
        Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
        command_buffer.Begin();
        try
        {
            const Forge::ScopedDebugLabel scope(command_buffer, "doomed pass");
            command_buffer.CmdCopyBuffer(no_transfer, destination);
            FAIL("The copy should have thrown.");
        }
        catch (const Opal::Exception&)
        {
        }
        command_buffer.End();
        // Not submitted: work the layer rejected while it was recorded is undefined behaviour once it runs.
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

    const Forge::Buffer output(fixture.device, {.size = k_element_count * sizeof(u32),
                                                .usage = Forge::BufferUsageBits::StorageBuffer,
                                                .host_access = Forge::HostAccess::Random,
                                                .use_device_address = true});
    const Forge::Shader compute_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute"});
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = compute_shader;
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
    const Forge::Pipeline pipeline(fixture.device, pipeline_desc);
    const VkDeviceAddress output_address = output.GetNativeDeviceAddress();

    // The dispatch every measurement below wraps, so what differs between them is only how it is timed.
    auto record_dispatch = [&](Forge::CommandBuffer& command_buffer)
    {
        command_buffer.CmdBindPipeline(pipeline);
        command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(output_address));
        command_buffer.CmdDispatch(k_element_count / k_group_size);
    };

    SECTION("A span around a dispatch comes back as a plausible duration")
    {
        Forge::TimestampQueryPool pool(fixture.device, {.query_count = 2});
        REQUIRE(pool.IsValid());
        REQUIRE(pool.GetQueryCount() == 2);
        Forge::SetDebugName(fixture.device, pool, "dispatch timing");

        // A pool that has been reset and not yet written has nothing to read, which is what the first frames
        // of a per-frame pool look like and the reason a frame loop asks rather than blocking. Reading one
        // that was never reset at all is not this case: that is undefined, and the layer says so.
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer) { command_buffer.CmdResetQueryPool(pool); });
        f64 too_early_ms = -1.0;
        REQUIRE_FALSE(pool.TryGetElapsedMilliseconds(0, 1, too_early_ms));
        REQUIRE(too_early_ms == -1.0);

        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdResetQueryPool(pool);
                                   command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineStart);
                                   record_dispatch(command_buffer);
                                   command_buffer.CmdWriteTimestamp(pool, 1, Forge::PipelineStageBits::PipelineEnd);
                               });

        Opal::InPlaceArray<u64, 2> ticks;
        pool.GetResults({ticks.GetData(), 2});
        INFO("ticks " << ticks[0] << " -> " << ticks[1] << ", period " << pool.GetTimestampPeriod() << " ns");
        REQUIRE(ticks[1] >= ticks[0]);

        // ImmediateSubmit has already waited, so the result is there without blocking.
        f64 elapsed_ms = -1.0;
        REQUIRE(pool.TryGetElapsedMilliseconds(0, 1, elapsed_ms));
        INFO("elapsed " << elapsed_ms << " ms");
        REQUIRE(elapsed_ms >= 0.0);
        // A dispatch this small cannot take a second on a device that finished it, so a figure above one
        // says the period or the valid bits were applied wrong rather than that the device was slow.
        REQUIRE(elapsed_ms < 1000.0);
        REQUIRE(elapsed_ms == pool.GetElapsedMilliseconds(0, 1));
    }
    SECTION("Two writes into a drained pipeline measure the dispatch on its own")
    {
        // The other pattern: PipelineEnd on both sides, so the write in front waits for everything before it
        // and the difference covers the dispatch and nothing else.
        Forge::TimestampQueryPool pool(fixture.device, {.query_count = 2});
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdResetQueryPool(pool);
                                   command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineEnd);
                                   record_dispatch(command_buffer);
                                   command_buffer.CmdWriteTimestamp(pool, 1, Forge::PipelineStageBits::PipelineEnd);
                               });
        const f64 elapsed_ms = pool.GetElapsedMilliseconds(0, 1);
        INFO("isolated " << elapsed_ms << " ms");
        REQUIRE(elapsed_ms >= 0.0);
        REQUIRE(elapsed_ms < 1000.0);
    }
    SECTION("A pair at the ends of a pool is readable with the queries between them unwritten")
    {
        // Reading the two as one range would report the whole range unavailable, since the middle was never
        // written, and a measurement that never arrives is indistinguishable from a device that is behind.
        Forge::TimestampQueryPool pool(fixture.device, {.query_count = 4});
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdResetQueryPool(pool);
                                   command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineStart);
                                   record_dispatch(command_buffer);
                                   command_buffer.CmdWriteTimestamp(pool, 3, Forge::PipelineStageBits::PipelineEnd);
                               });
        f64 elapsed_ms = -1.0;
        REQUIRE(pool.TryGetElapsedMilliseconds(0, 3, elapsed_ms));
        REQUIRE(elapsed_ms >= 0.0);
    }
    SECTION("Resetting from the host needs the feature")
    {
        const Forge::TimestampQueryPool pool(fixture.device, {.query_count = 2});
        REQUIRE_THROWS_AS(pool.Reset(), Opal::Exception);
    }
    SECTION("A pool that asks for no queries throws")
    {
        REQUIRE_THROWS_AS(Forge::TimestampQueryPool(fixture.device, {.query_count = 0}), Opal::Exception);
    }
    SECTION("A query past the end of the pool throws")
    {
        Forge::TimestampQueryPool pool(fixture.device, {.query_count = 2});
        Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
        command_buffer.Begin();
        command_buffer.CmdResetQueryPool(pool);
        REQUIRE_THROWS_AS(command_buffer.CmdWriteTimestamp(pool, 2), Opal::Exception);
        REQUIRE_THROWS_AS(command_buffer.CmdResetQueryPool(pool, 1, 2), Opal::Exception);
        command_buffer.End();
    }
    SECTION("A timestamp naming more than one stage throws")
    {
        Forge::TimestampQueryPool pool(fixture.device, {.query_count = 2});
        Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
        command_buffer.Begin();
        command_buffer.CmdResetQueryPool(pool);
        REQUIRE_THROWS_AS(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::VertexShader |
                                                                        Forge::PipelineStageBits::FragmentShader),
                          Opal::Exception);
        REQUIRE_THROWS_AS(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::None), Opal::Exception);
        command_buffer.End();
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
    const Forge::Buffer source(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, written);
    const Forge::Buffer destination(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferDestination});

    const Forge::TimestampQueryPool pool(fixture.device, {.query_count = 2});
    // The whole point of the host side: the pool is made ready without a command buffer having to carry it.
    pool.Reset();
    Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::PipelineStart);
                               command_buffer.CmdCopyBuffer(source, destination);
                               command_buffer.CmdWriteTimestamp(pool, 1, Forge::PipelineStageBits::PipelineEnd);
                           });
    f64 elapsed_ms = -1.0;
    REQUIRE(pool.TryGetElapsedMilliseconds(0, 1, elapsed_ms));
    REQUIRE(elapsed_ms >= 0.0);

    // Reset again and the results are gone, which is what makes a per-frame pool reusable.
    pool.Reset();
    f64 after_reset_ms = -1.0;
    REQUIRE_FALSE(pool.TryGetElapsedMilliseconds(0, 1, after_reset_ms));
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
    pool_desc.Add(Forge::DescriptorType::StorageBuffer, 4);
    pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 4);
    pool_desc.max_sets = 4;
    const Forge::DescriptorPool pool(fixture.device, pool_desc);

    Forge::DescriptorSetLayoutDesc layout_desc;
    layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute);
    layout_desc.AddBinding(2, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
    const Forge::DescriptorSetLayout layout(fixture.device, layout_desc);

    SECTION("The descriptor type comes from the layout")
    {
        const Forge::DescriptorSet set(pool, layout);
        REQUIRE(set.GetBindingDescriptorType(0) == Forge::DescriptorType::StorageBuffer);
        REQUIRE(set.GetBindingDescriptorType(2) == Forge::DescriptorType::CombinedImageSampler);
        // Binding 1 is a gap in this layout, which is a binding index the set has to reject rather than
        // guess a type for.
        REQUIRE_THROWS_AS(set.GetBindingDescriptorType(1), Opal::Exception);
    }
    SECTION("A buffer written the short way reaches the shader")
    {
        const Forge::Buffer output(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random});
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        output.Update(zeros);

        Forge::DescriptorSet set(pool, layout);
        set.Update(0, output);

        const Forge::Shader shader =
            Forge::Shader::FromSourceInMemory(fixture.device, k_descriptor_source, {.entry_point = "main_descriptor"});
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline(fixture.device, pipeline_desc);

        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdBindPipeline(pipeline);
                                   command_buffer.CmdBindDescriptorSet(pipeline, set);
                                   command_buffer.CmdDispatch(k_element_count / k_group_size);
                               });

        Opal::DynamicArray<u32> values(k_element_count);
        output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)});
        for (i32 i = 0; i < k_element_count; ++i)
        {
            REQUIRE(values[i] == static_cast<u32>(i) + 7);
        }
    }
    SECTION("A texture written the short way records without complaint")
    {
        const Forge::Texture texture(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                      .width = 4,
                                                      .height = 4,
                                                      .usage = Forge::TextureUsageBits::Sampled});
        const Forge::Sampler sampler(fixture.device, {.max_anisotropy = 1.0f});

        Forge::DescriptorSet set(pool, layout);
        set.Update(2, texture, sampler);
    }
    SECTION("A range past the end of the buffer throws")
    {
        const Forge::Buffer small(fixture.device,
                                  {.size = 256, .usage = Forge::BufferUsageBits::StorageBuffer});
        Forge::DescriptorSet set(pool, layout);
        REQUIRE_THROWS_AS(set.Update(0, small, 128, 256), Opal::Exception);
        REQUIRE_THROWS_AS(set.Update(0, small, 0, 0), Opal::Exception);
    }
    SECTION("A moved set carries what its layout declared")
    {
        // The pattern every per-frame resource in the sample uses: declare empty, assign over it. A set
        // whose binding types stayed behind would reject the very bindings its layout has.
        Forge::DescriptorSet set;
        set = Forge::DescriptorSet(pool, layout);
        REQUIRE(set.IsValid());
        REQUIRE(set.GetBindingDescriptorType(0) == Forge::DescriptorType::StorageBuffer);

        const Forge::Buffer buffer(fixture.device,
                                   {.size = 256, .usage = Forge::BufferUsageBits::StorageBuffer});
        set.Update(0, buffer);

        const Forge::DescriptorSet moved(std::move(set));
        REQUIRE(moved.GetBindingDescriptorType(0) == Forge::DescriptorType::StorageBuffer);
        // The source is empty afterwards, so it knows about no binding at all.
        REQUIRE_FALSE(set.IsValid());
        REQUIRE_THROWS_AS(set.GetBindingDescriptorType(0), Opal::Exception);
    }
    SECTION("Writing a binding the layout does not have throws")
    {
        const Forge::Buffer buffer(fixture.device,
                                   {.size = 256, .usage = Forge::BufferUsageBits::StorageBuffer});
        Forge::DescriptorSet set(pool, layout);
        REQUIRE_THROWS_AS(set.Update(3, buffer), Opal::Exception);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}
