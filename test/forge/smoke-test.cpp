#include <catch2/catch2.hpp>

#include "opal/container/dynamic-array.h"
#include "opal/exceptions.h"

#include "rndr/forge/buffer.hpp"
#include "rndr/forge/command-buffer.hpp"
#include "rndr/forge/debug.hpp"
#include "rndr/forge/device.hpp"
#include "rndr/forge/graphics-context.hpp"
#include "rndr/forge/physical-device.hpp"
#include "rndr/forge/pipeline.hpp"
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

    ForgeFixture() : context({.collect_debug_messages = true})
    {
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = context.EnumeratePhysicalDevices();
        device = Forge::Device(std::move(physical_devices[0]), context);
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
