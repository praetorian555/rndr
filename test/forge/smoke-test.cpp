#include <catch2/catch2.hpp>

#include "opal/file-system.h"

#include "rndr/core/shader-cache.hpp"

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
    static Rndr::ShaderCache cache{Opal::StringUtf8(RNDR_CORE_ASSETS_DIR "/../build/shader-cache")};
    return cache;
}

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
    const Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = fixture.context.EnumeratePhysicalDevices();
    REQUIRE_FALSE(physical_devices.IsEmpty());
    const Forge::Buffer buffer(fixture.device, {.size = 32, .usage = Forge::BufferUsageBits::TransferDestination});
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
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()});
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
                               command_buffer.CmdGenerateMips(texture, Forge::ImageLayout::TransferDestination);
                           });
    // Nothing above named a source layout: the mip chain reads each level off the texture as it goes, and
    // leaves every one of them in the layout it was told to finish in.
    REQUIRE(texture.GetCurrentLayout() == Forge::ImageLayout::TransferDestination);

    for (u32 level = 0; level < k_mip_count; ++level)
    {
        const i32 side = k_side >> level;
        Opal::DynamicArray<u8> level_pixels(side * side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, level_pixels, level, Forge::ImageLayout::TransferDestination);
        INFO("mip level " << level);
        for (i32 i = 0; i < level_pixels.GetSize(); ++i)
        {
            REQUIRE(level_pixels[i] == k_texel[i % 4]);
        }
    }

    SECTION("Reading back into a view of the wrong size throws")
    {
        Opal::DynamicArray<u8> too_small(4);
        REQUIRE_THROWS_AS(
            Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, too_small, 0, Forge::ImageLayout::TransferDestination),
            Opal::Exception);
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

    Forge::Texture texture(fixture.device, {.format = k_format,
                                            .width = k_side,
                                            .height = k_side,
                                            .mip_level_count = k_mip_count,
                                            .array_layer_count = k_layer_count,
                                            .usage = Forge::TextureUsageBits::TransferSource |
                                                     Forge::TextureUsageBits::TransferDestination |
                                                     Forge::TextureUsageBits::Sampled,
                                            .view_type = Forge::TextureViewType::Texture2DArray});
    const Forge::Buffer staging(fixture.device, {.size = mip0.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, mip0);
    // One region for both layers: the buffer holds them back to back, which is the order Vulkan copies them in.
    const Forge::BufferImageCopyRegion mip0_region{.image_subresource = {.array_layer_count = k_layer_count}};
    Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferDestination(texture));
                               command_buffer.CmdCopyBufferToImage(staging, texture, {&mip0_region, 1});
                               command_buffer.CmdGenerateMips(texture, Forge::ImageLayout::TransferDestination);
                           });

    // Every level of every layer, not just of layer zero: a blit region names one array layer unless told
    // otherwise, so a mip chain built without saying so leaves every layer past the first holding whatever it
    // was created with, while the barriers around it still report the whole texture as filled and transitioned.
    for (u32 level = 0; level < k_mip_count; ++level)
    {
        const i32 side = k_side >> level;
        Opal::DynamicArray<u8> level_pixels(static_cast<i32>(k_layer_count) * side * side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, level_pixels, level, Forge::ImageLayout::TransferDestination);
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
    command_buffer.CmdTransition(source, Forge::ImageLayout::TransferSource);
    command_buffer.CmdTransition(destination, Forge::ImageLayout::TransferDestination);
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

TEST_CASE("Forge timeline semaphores", "[forge]")
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

    // The same split copy the batched submit case uses: one command buffer per half, so a batch that dropped
    // either one would show up as half the buffer missing rather than as nothing at all.
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
    const Opal::Ref<const Forge::CommandBuffer> first_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(first)};
    const Opal::Ref<const Forge::CommandBuffer> second_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(second)};

    // Both halves in the destination, which is what every section that submits has to end up with.
    auto require_whole_buffer_copied = [&]()
    {
        Opal::DynamicArray<u8> read_back(k_size * 2);
        Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), destination, read_back);
        for (i32 i = 0; i < k_size; ++i)
        {
            REQUIRE(read_back[i] == first_half[i]);
            REQUIRE(read_back[k_size + i] == second_half[i]);
        }
    };

    SECTION("A fresh timeline starts at its initial value and the host can raise it")
    {
        constexpr u64 k_initial = 7;
        const Forge::Semaphore timeline(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = k_initial});
        REQUIRE(timeline.IsTimeline());
        REQUIRE(timeline.GetType() == Forge::SemaphoreType::Timeline);
        REQUIRE(timeline.GetValue() == k_initial);
        timeline.Signal(k_initial + 3);
        REQUIRE(timeline.GetValue() == k_initial + 3);
    }
    SECTION("A wait for a value already reached returns at once")
    {
        const Forge::Semaphore timeline(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = 4});
        timeline.Wait(4);
        timeline.Wait(1);
        REQUIRE(timeline.GetValue() == 4);
    }
    SECTION("A wait that runs out of time answers false rather than throwing")
    {
        // A millisecond, in the nanoseconds Vulkan counts timeouts in. Long enough that a machine under load
        // does not report a timeout for the value that was already reached, short enough not to stall the run.
        constexpr u64 k_short_timeout = 1000 * 1000;
        const Forge::Semaphore timeline(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = 4});
        REQUIRE(timeline.TryWait(4, k_short_timeout));
        // Nothing was submitted that could raise it, so the timeout is the only way out of these two.
        REQUIRE_FALSE(timeline.TryWait(5, k_short_timeout));
        const Forge::SemaphoreWait waits[1] = {{.semaphore = timeline, .value = 5}};
        REQUIRE_FALSE(Forge::Semaphore::TryWaitForAll({waits, 1}, k_short_timeout));
    }
    SECTION("One batch per half, the second waiting on the value the first signals")
    {
        const Forge::Semaphore timeline(fixture.device, {.type = Forge::SemaphoreType::Timeline});
        const Forge::Fence fence(fixture.device, false);
        const Forge::SemaphoreSubmit signal{.semaphore = timeline, .stages = Forge::PipelineStageBits::Transfer, .value = 1};
        const Forge::SemaphoreSubmit wait{.semaphore = timeline, .stages = Forge::PipelineStageBits::Transfer, .value = 1};
        fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&signal, 1}});
        fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .wait_semaphores = {&wait, 1}, .fence = fence});
        fence.Wait();
        require_whole_buffer_copied();
    }
    SECTION("The host waits on a value the device signals, with no fence anywhere")
    {
        const Forge::Semaphore timeline(fixture.device, {.type = Forge::SemaphoreType::Timeline});
        const Forge::SemaphoreSubmit first_signal{.semaphore = timeline, .value = 1};
        const Forge::SemaphoreSubmit second_signal{.semaphore = timeline, .value = 2};
        fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&first_signal, 1}});
        fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .signal_semaphores = {&second_signal, 1}});
        timeline.Wait(2);
        REQUIRE(timeline.GetValue() == 2);
        require_whole_buffer_copied();
    }
    SECTION("WaitForAll over two timelines returns once both have been signalled")
    {
        const Forge::Semaphore first_timeline(fixture.device, {.type = Forge::SemaphoreType::Timeline});
        const Forge::Semaphore second_timeline(fixture.device, {.type = Forge::SemaphoreType::Timeline});
        const Forge::SemaphoreSubmit first_signal{.semaphore = first_timeline, .value = 1};
        const Forge::SemaphoreSubmit second_signal{.semaphore = second_timeline, .value = 1};
        fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&first_signal, 1}});
        fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .signal_semaphores = {&second_signal, 1}});
        const Forge::SemaphoreWait waits[2] = {{.semaphore = first_timeline, .value = 1}, {.semaphore = second_timeline, .value = 1}};
        Forge::Semaphore::WaitForAll({waits, 2});
        REQUIRE(first_timeline.GetValue() == 1);
        REQUIRE(second_timeline.GetValue() == 1);
        require_whole_buffer_copied();
    }
    SECTION("The host side of a timeline throws on a binary semaphore")
    {
        const Forge::Semaphore binary(fixture.device);
        REQUIRE_FALSE(binary.IsTimeline());
        REQUIRE_THROWS_AS(binary.Wait(1), Opal::Exception);
        REQUIRE_THROWS_AS(binary.Signal(1), Opal::Exception);
        REQUIRE_THROWS_AS(binary.GetValue(), Opal::Exception);
    }
    SECTION("A value on a binary semaphore throws, since Vulkan would ignore it")
    {
        const Forge::Semaphore binary(fixture.device);
        const Forge::SemaphoreSubmit signal{.semaphore = binary, .value = 1};
        REQUIRE_THROWS_AS(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&signal, 1}}),
                          Opal::Exception);
    }
    SECTION("A timeline signalled with zero throws, since no signal can reach it")
    {
        const Forge::Semaphore timeline(fixture.device, {.type = Forge::SemaphoreType::Timeline});
        const Forge::SemaphoreSubmit signal{.semaphore = timeline, .value = 0};
        REQUIRE_THROWS_AS(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&signal, 1}}),
                          Opal::Exception);
        // A wait for zero is legal and trivially satisfied, so only the signal side is turned away.
        const Forge::SemaphoreSubmit wait{.semaphore = timeline, .value = 0};
        fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .wait_semaphores = {&wait, 1}});
        fixture.GetQueue().WaitIdle();
    }
    SECTION("A signal that does not raise the count throws")
    {
        const Forge::Semaphore timeline(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = 4});
        REQUIRE_THROWS_AS(timeline.Signal(4), Opal::Exception);
        REQUIRE_THROWS_AS(timeline.Signal(3), Opal::Exception);
        REQUIRE(timeline.GetValue() == 4);
        timeline.Signal(5);
        REQUIRE(timeline.GetValue() == 5);
    }
    SECTION("WaitForAll over two devices throws rather than naming one of them")
    {
        // A second logical device on the same physical one. Not a second ForgeFixture: its context would
        // call volkFinalize on the way out and unload Vulkan from under this one.
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = fixture.context.EnumeratePhysicalDevices();
        const Forge::Device other(std::move(physical_devices[0]), fixture.context, {});
        const Forge::Semaphore here(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = 1});
        const Forge::Semaphore there(other, {.type = Forge::SemaphoreType::Timeline, .initial_value = 1});
        const Forge::SemaphoreWait waits[2] = {{.semaphore = here, .value = 1}, {.semaphore = there, .value = 1}};
        REQUIRE_THROWS_AS(Forge::Semaphore::WaitForAll({waits, 2}), Opal::Exception);
    }
    SECTION("An empty entry in WaitForAll throws, either way it is empty")
    {
        const Forge::Semaphore empty;
        const Forge::SemaphoreWait empty_reference[1] = {{}};
        REQUIRE_THROWS_AS(Forge::Semaphore::WaitForAll({empty_reference, 1}), Opal::Exception);
        const Forge::SemaphoreWait empty_semaphore[1] = {{.semaphore = empty, .value = 1}};
        REQUIRE_THROWS_AS(Forge::Semaphore::WaitForAll({empty_semaphore, 1}), Opal::Exception);
    }

    fixture.GetQueue().WaitIdle();
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

    SECTION("A fence that is not signalled in time answers false rather than throwing")
    {
        constexpr u64 k_short_timeout = 1000 * 1000;
        // Both of the fences above have been waited on, so they are signalled and answer at once.
        REQUIRE(fences[0].TryWait(k_short_timeout));
        REQUIRE(Forge::Fence::TryWaitForAll(fences, k_short_timeout));
        // Nothing is submitted against this one, so it stays unsignalled and the timeout is the only way out.
        const Forge::Fence never_signalled(fixture.device, false);
        REQUIRE_FALSE(never_signalled.TryWait(k_short_timeout));
    }
    SECTION("Fences from two devices in one wait throw")
    {
        // A second logical device on the same physical one, for the reason the timeline case above gives.
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = fixture.context.EnumeratePhysicalDevices();
        const Forge::Device other(std::move(physical_devices[0]), fixture.context, {});
        Opal::DynamicArray<Forge::Fence> across_devices;
        across_devices.EmplaceBack(fixture.device, true);
        across_devices.EmplaceBack(other, true);
        REQUIRE_THROWS_AS(Forge::Fence::WaitForAll(across_devices), Opal::Exception);
    }
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

        const Forge::Shader shader = Forge::Shader::FromSourceInMemory(device, k_bindless_source, {.entry_point = "main_bindless", .cache = GetShaderCache()});
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
        Forge::Texture texture(device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                        .width = 1,
                                        .height = 1,
                                        .usage = Forge::TextureUsageBits::Sampled | Forge::TextureUsageBits::TransferDestination});
        const u8 texel[4] = {static_cast<u8>(red), 0, 0, 255};
        const Forge::Buffer staging(device, {.size = sizeof(texel), .usage = Forge::BufferUsageBits::TransferSource},
                                    {texel, sizeof(texel)});
        const Forge::BufferImageCopyRegion region;
        Forge::ImmediateSubmit(device, queue,
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferDestination(texture));
                                   command_buffer.CmdCopyBufferToImage(staging, texture, {&region, 1});
                                   command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly);
                               });
        return texture;
    };

    Forge::DescriptorPoolDesc pool_desc;
    pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1);
    pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, k_max_descriptors);
    pool_desc.max_sets = 1;
    pool_desc.use_update_after_bind = true;
    const Forge::DescriptorPool pool(device, pool_desc);

    // The texture array is the highest binding, which is where a variable count is allowed to sit.
    Forge::DescriptorSetLayoutDesc layout_desc;
    layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute);
    layout_desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, k_max_descriptors, ShaderTypeBits::Compute, {},
                           Forge::DescriptorBindingFlagBits::PartiallyBound | Forge::DescriptorBindingFlagBits::UpdateAfterBind |
                               Forge::DescriptorBindingFlagBits::VariableDescriptorCount);
    const Forge::DescriptorSetLayout layout(device, layout_desc);

    SECTION("A shader samples the element of the array it indexes")
    {
        // Three of the four, so the variable count is doing something.
        Forge::DescriptorSet descriptor_set(pool, layout, k_used_descriptors);

        Forge::Buffer output(device, {.size = k_element_count * sizeof(u32),
                                      .usage = Forge::BufferUsageBits::StorageBuffer,
                                      .host_access = Forge::HostAccess::Random});
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        output.Update(zeros);

        const Forge::Texture texture_one = make_texture(k_red_at_one);
        const Forge::Texture texture_two = make_texture(k_red_at_two);
        const Forge::Sampler sampler(device, {.max_anisotropy = 1.0f});

        // Elements one and two, never element zero: a write past the first descriptor of a binding is the
        // part of this that a binding holding one descriptor could never have exercised.
        descriptor_set.Update(0, output);
        descriptor_set.Update(1, texture_one, sampler, Forge::ImageLayout::ShaderReadOnly, 1);
        descriptor_set.Update(1, texture_two, sampler, Forge::ImageLayout::ShaderReadOnly, 2);

        const Forge::Shader shader = Forge::Shader::FromSourceInMemory(
            device, k_bindless_texture_source, {.entry_point = "main_bindless_textures", .cache = GetShaderCache()});
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
            // An even invocation reads element one and an odd one element two, so a shader that ignored the
            // index, or an update that landed on the wrong element, comes back uniform instead.
            const u32 expected = (i % 2) == 0 ? k_red_at_one : k_red_at_two;
            INFO("invocation " << i);
            REQUIRE(values[i] == expected);
        }
    }
    SECTION("An array element past the end of the binding throws")
    {
        // Three of four allocated, so elements zero through two exist and element three does not, even though
        // the layout declares four. Vulkan writes such an element without reporting anything.
        Forge::DescriptorSet descriptor_set(pool, layout, k_used_descriptors);
        const Forge::Texture texture = make_texture(k_red_at_one);
        const Forge::Sampler sampler(device, {.max_anisotropy = 1.0f});

        REQUIRE_NOTHROW(descriptor_set.Update(1, texture, sampler, Forge::ImageLayout::ShaderReadOnly, k_used_descriptors - 1));
        REQUIRE_THROWS_AS(descriptor_set.Update(1, texture, sampler, Forge::ImageLayout::ShaderReadOnly, k_used_descriptors),
                          Opal::Exception);
        REQUIRE_THROWS_AS(descriptor_set.Update(1, texture, sampler, Forge::ImageLayout::ShaderReadOnly, k_max_descriptors),
                          Opal::Exception);
        // A binding of one descriptor has only element zero, variable count or not.
        Forge::Buffer output(device, {.size = 4, .usage = Forge::BufferUsageBits::StorageBuffer});
        REQUIRE_THROWS_AS(descriptor_set.Update(0, output, 0, Forge::k_whole_buffer, 1), Opal::Exception);
    }
    SECTION("A variable count above the texture binding descriptor count throws")
    {
        REQUIRE_THROWS_AS(Forge::DescriptorSet(pool, layout, k_max_descriptors + 1), Opal::Exception);
    }
    SECTION("An update after bind texture layout needs a pool that expects one")
    {
        Forge::DescriptorPoolDesc plain_pool_desc;
        plain_pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1);
        plain_pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, k_max_descriptors);
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
    pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1);
    pool_desc.Add(Forge::DescriptorType::ConstantBuffer, k_max_descriptors);
    pool_desc.max_sets = 1;
    const Forge::DescriptorPool pool(device, pool_desc);

    Forge::DescriptorSetLayoutDesc layout_desc;
    layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute);
    layout_desc.AddBinding(1, Forge::DescriptorType::ConstantBuffer, k_max_descriptors, ShaderTypeBits::Compute);
    const Forge::DescriptorSetLayout layout(device, layout_desc);
    Forge::DescriptorSet descriptor_set(pool, layout);

    Forge::Buffer output(device, {.size = k_element_count * sizeof(u32),
                                  .usage = Forge::BufferUsageBits::StorageBuffer,
                                  .host_access = Forge::HostAccess::Random});
    const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
    output.Update(zeros);

    // One constant register each, which is the smallest a constant buffer is laid out in.
    auto make_params = [&](u32 value)
    {
        const u32 contents[4] = {value, 0, 0, 0};
        return Forge::Buffer(device, {.size = sizeof(contents), .usage = Forge::BufferUsageBits::ConstantBuffer},
                             {reinterpret_cast<const u8*>(contents), sizeof(contents)});
    };
    const Forge::Buffer params_zero = make_params(k_value_at_zero);
    const Forge::Buffer params_one = make_params(k_value_at_one);

    descriptor_set.Update(0, output);
    descriptor_set.Update(1, params_zero, 0, Forge::k_whole_buffer, 0);
    descriptor_set.Update(1, params_one, 0, Forge::k_whole_buffer, 1);

    const Forge::Shader shader = Forge::Shader::FromSourceInMemory(
        device, k_bindless_constant_source, {.entry_point = "main_bindless_constants", .cache = GetShaderCache()});
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
        const u32 expected = (i % 2) == 0 ? k_value_at_zero : k_value_at_one;
        INFO("invocation " << i);
        REQUIRE(values[i] == expected);
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

TEST_CASE("Forge texture layout tracking", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr u32 k_mip_count = 4;  // 8 -> 4 -> 2 -> 1
    Forge::Texture texture(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                            .width = 8,
                                            .height = 8,
                                            .mip_level_count = k_mip_count,
                                            .usage = Forge::TextureUsageBits::TransferSource |
                                                     Forge::TextureUsageBits::TransferDestination |
                                                     Forge::TextureUsageBits::Sampled});

    SECTION("A fresh texture is undefined and a transition moves every level of it")
    {
        REQUIRE(texture.GetCurrentLayout() == Forge::ImageLayout::Undefined);
        REQUIRE(texture.GetCurrentLayout(k_mip_count - 1) == Forge::ImageLayout::Undefined);

        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly);
                                   REQUIRE(texture.GetCurrentLayout() == Forge::ImageLayout::ShaderReadOnly);
                                   // The second one has no old layout to be told: it reads ShaderReadOnly off
                                   // the texture, which is the whole point of tracking it.
                                   command_buffer.CmdTransition(texture, Forge::ImageLayout::TransferSource);
                               });
        REQUIRE(texture.GetCurrentLayout() == Forge::ImageLayout::TransferSource);
        for (u32 level = 0; level < k_mip_count; ++level)
        {
            REQUIRE(texture.GetCurrentLayout(level) == Forge::ImageLayout::TransferSource);
        }
    }
    SECTION("A partial range splits the grid and the whole-texture answer stops existing")
    {
        Forge::ImageBarrier barrier = Forge::ImageBarrier::ToTransferDestination(texture, Forge::ImageLayout::Undefined);
        barrier.subresource_range.first_mip_level = 0;
        barrier.subresource_range.mip_level_count = 2;
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer) { command_buffer.CmdImageBarrier(barrier); });

        REQUIRE(texture.GetCurrentLayout(0) == Forge::ImageLayout::TransferDestination);
        REQUIRE(texture.GetCurrentLayout(1) == Forge::ImageLayout::TransferDestination);
        REQUIRE(texture.GetCurrentLayout(2) == Forge::ImageLayout::Undefined);
        REQUIRE(texture.GetCurrentLayout(3) == Forge::ImageLayout::Undefined);
        REQUIRE_THROWS_AS(texture.GetCurrentLayout(), Opal::Exception);
        // A transition of the whole texture cannot say what it is coming from either.
        Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
        command_buffer.Begin();
        REQUIRE_THROWS_AS(command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly), Opal::Exception);
        command_buffer.End();
    }
    SECTION("A subresource the texture does not have throws")
    {
        REQUIRE_THROWS_AS(texture.GetCurrentLayout(k_mip_count), Opal::Exception);
        REQUIRE_THROWS_AS(texture.GetCurrentLayout(0, 1), Opal::Exception);
    }
    SECTION("A transfer out of a layout the role does not allow throws")
    {
        if (!fixture.device.GetPhysicalDevice().SupportsBlit(PixelFormat::R8G8B8A8_UNORM, true) ||
            !fixture.device.GetPhysicalDevice().SupportsBlit(PixelFormat::R8G8B8A8_UNORM, false))
        {
            SKIP("This device cannot blit R8G8B8A8_UNORM either way.");
        }
        Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
        command_buffer.Begin();
        command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly);
        // Halving mip 0 into mip 1, which is what mip generation does, but with both levels left where a
        // shader reads them rather than where a transfer does.
        const Forge::ImageBlitRegion region{.source = {.mip_level = 0}, .destination = {.mip_level = 1}};
        REQUIRE_THROWS_AS(command_buffer.CmdBlitImage(texture, texture, {&region, 1}), Opal::Exception);
        command_buffer.End();
    }
    SECTION("A move carries the layouts across")
    {
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               { command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly); });
        Forge::Texture moved(std::move(texture));
        REQUIRE(moved.GetCurrentLayout() == Forge::ImageLayout::ShaderReadOnly);
        REQUIRE_FALSE(texture.IsValid());  // NOLINT(bugprone-use-after-move)
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
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()});
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
            Forge::Shader::FromSourceInMemory(fixture.device, k_descriptor_source, {.entry_point = "main_descriptor", .cache = GetShaderCache()});
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

TEST_CASE("Forge rendering without a depth attachment", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    Forge::Texture color(fixture.device, {.format = k_format,
                                          .width = k_side,
                                          .height = k_side,
                                          .usage = Forge::TextureUsageBits::ColorAttachment |
                                                   Forge::TextureUsageBits::TransferSource});

    SECTION("An absent depth attachment renders colour only")
    {
        // No pipeline and no draw: the load operation is what writes the attachment, so what comes back says
        // the pass ran with a colour attachment and nothing else.
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToColorAttachment(color));
                                   const Forge::RenderingDesc rendering_desc{
                                       .render_area_extent = {k_side, k_side},
                                       .color_attachments = {Forge::RenderingAttachmentDesc{
                                           .image_view = color.GetNativeImageView(),
                                           .image_layout = Forge::ImageLayout::ColorAttachment,
                                           .load_operation = Forge::AttachmentLoadOperation::Clear,
                                           .store_operation = Forge::AttachmentStoreOperation::Store,
                                           .clear_value = {.color = {1.0f, 0.0f, 1.0f, 1.0f}}}}};
                                   command_buffer.CmdBeginRendering(rendering_desc);
                                   command_buffer.CmdEndRendering();
                               });

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        // Left in TransferSource rather than the ShaderReadOnly this defaults to: that layout needs the
        // Sampled usage, and this texture is an attachment nothing ever samples.
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource);
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
    SECTION("A depth attachment that names no image view throws")
    {
        // What the old convention expressed as "no depth". Now that absent says it, a present attachment
        // pointing at nothing is a filled-in desc somebody forgot to finish.
        Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
        command_buffer.Begin();
        const Forge::RenderingDesc rendering_desc{
            .render_area_extent = {k_side, k_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{.image_view = color.GetNativeImageView(),
                                                                 .image_layout = Forge::ImageLayout::ColorAttachment}},
            .depth_attachment = Forge::RenderingAttachmentDesc{}};
        REQUIRE_THROWS_AS(command_buffer.CmdBeginRendering(rendering_desc), Opal::Exception);
        command_buffer.End();
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

    const Forge::Shader vertex_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()});
    const Forge::Shader fragment_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()});

    /**
     * Clear the target to opaque red, draw the triangle through a pipeline with the given mask, and hand back
     * one texel. The shader writes (0, 1, 0, 0), so every channel differs from what the clear left, and a
     * channel that comes back red is one the mask kept the draw away from.
     */
    const Forge::Buffer vertices(fixture.device,
                                 {.size = sizeof(k_fullscreen_vertices),
                                  .usage = Forge::BufferUsageBits::VertexBuffer},
                                 Opal::AsBytes(k_fullscreen_vertices));

    auto draw_through_mask = [&](Forge::ColorWriteMaskBits mask)
    {
        Forge::Texture color(fixture.device, {.format = k_format,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::ColorAttachment |
                                                       Forge::TextureUsageBits::TransferSource});

        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0);
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{.color_write_mask = mask});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
        const Forge::Pipeline pipeline(fixture.device, pipeline_desc);

        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToColorAttachment(color));
                                   const Forge::RenderingDesc rendering_desc{
                                       .render_area_extent = {k_side, k_side},
                                       .color_attachments = {Forge::RenderingAttachmentDesc{
                                           .image_view = color.GetNativeImageView(),
                                           .image_layout = Forge::ImageLayout::ColorAttachment,
                                           .load_operation = Forge::AttachmentLoadOperation::Clear,
                                           .store_operation = Forge::AttachmentStoreOperation::Store,
                                           .clear_value = {.color = {1.0f, 0.0f, 0.0f, 1.0f}}}}};
                                   command_buffer.CmdBeginRendering(rendering_desc);
                                   command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side});
                                   command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side});
                                   command_buffer.CmdBindPipeline(pipeline);
                                   command_buffer.CmdBindVertexBuffer(vertices, 0);
                                   command_buffer.CmdDraw(3);
                                   command_buffer.CmdEndRendering();
                               });

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource);
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

/**
 * Whether the first physical device has 8-bit indices, under either of the two names the extension has, so a
 * device that has neither skips rather than fails. One context for the whole binary: enumerating is cheap and
 * creating a device is what this suite spends its time on.
 */
bool IsIndexTypeUint8Supported()
{
    static const bool supported = []
    {
        const Forge::GraphicsContext context(Forge::GraphicsContextDesc{});
        const Opal::DynamicArray<Forge::PhysicalDevice> devices = context.EnumeratePhysicalDevices();
        return devices[0].IsExtensionSupported(VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME) ||
               devices[0].IsExtensionSupported(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);
    }();
    return supported;
}

/** What the first physical device supports of the core features, for a case that has to skip without one. */
VkPhysicalDeviceFeatures GetFirstPhysicalDeviceFeatures()
{
    static const VkPhysicalDeviceFeatures features = []
    {
        const Forge::GraphicsContext context(Forge::GraphicsContextDesc{});
        const Opal::DynamicArray<Forge::PhysicalDevice> devices = context.EnumeratePhysicalDevices();
        return devices[0].GetFeatures();
    }();
    return features;
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
        vertex_shader = Forge::Shader::FromSourceInMemory(forge.device, k_halves_source,
                                                          {.entry_point = "main_vertex", .cache = GetShaderCache()});
        fragment_shader = Forge::Shader::FromSourceInMemory(forge.device, k_halves_source,
                                                            {.entry_point = "main_fragment", .cache = GetShaderCache()});

        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        // Off, so that which way a triangle winds is never what a failing case is about.
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0);
        pipeline_desc.vertex_input.AddBinding(1, sizeof(u32), DataRepetition::PerInstance);
        pipeline_desc.vertex_input.AddAttribute(1, 1, PixelFormat::R32_UINT, 0);
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
        pipeline = Forge::Pipeline(forge.device, pipeline_desc);

        corners = Forge::Buffer(forge.device, {.size = sizeof(k_half_corners), .usage = Forge::BufferUsageBits::VertexBuffer},
                                Opal::AsBytes(k_half_corners));
        vertices = Forge::Buffer(forge.device, {.size = sizeof(k_half_vertices), .usage = Forge::BufferUsageBits::VertexBuffer},
                                 Opal::AsBytes(k_half_vertices));
        instances = Forge::Buffer(forge.device, {.size = sizeof(k_instance_values), .usage = Forge::BufferUsageBits::VertexBuffer},
                                  Opal::AsBytes(k_instance_values));
        color = Forge::Texture(forge.device, {.format = k_format,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::ColorAttachment |
                                                       Forge::TextureUsageBits::TransferSource});
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
        Forge::ImmediateSubmit(forge.device, GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   record_before(command_buffer);
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToColorAttachment(color));
                                   const Forge::RenderingDesc rendering_desc{
                                       .render_area_extent = {k_side, k_side},
                                       .color_attachments = {Forge::RenderingAttachmentDesc{
                                           .image_view = color.GetNativeImageView(),
                                           .image_layout = Forge::ImageLayout::ColorAttachment,
                                           .load_operation = Forge::AttachmentLoadOperation::Clear,
                                           .store_operation = Forge::AttachmentStoreOperation::Store,
                                           .clear_value = {.color = {0.0f, 0.0f, 0.0f, 1.0f}}}}};
                                   command_buffer.CmdBeginRendering(rendering_desc);
                                   command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side});
                                   command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side});
                                   command_buffer.CmdBindPipeline(pipeline);
                                   command_buffer.CmdBindVertexBuffer(instances, 1);
                                   record_draw(command_buffer);
                                   command_buffer.CmdEndRendering();
                               });

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        // Left in TransferSource:
        Forge::ReadBackTexture(forge.device, GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource);
        return pixels;
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

TEST_CASE("Forge indexed draws", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const bool has_uint8 = IsIndexTypeUint8Supported();
    HalvesFixture halves({.index_type_uint8 = has_uint8});

    // The one draw every section below makes, with the index buffer built at the given width. Both offsets are
    // non-zero: first_index skips the six padding indices and vertex_offset moves the left corners onto the
    // right ones, so the right half comes back written and nothing else does.
    auto draw_right_half_through = [&](IndexSize index_size)
    {
        const Opal::DynamicArray<u8> index_bytes =
            ToIndexBytes(k_half_indices, static_cast<i32>(std::size(k_half_indices)), index_size);
        const Forge::Buffer indices(halves.forge.device,
                                    {.size = index_bytes.GetSize(), .usage = Forge::BufferUsageBits::IndexBuffer}, index_bytes);
        return halves.Render(
            [&](Forge::CommandBuffer& command_buffer)
            {
                command_buffer.CmdBindVertexBuffer(halves.corners, 0);
                command_buffer.CmdBindIndexBuffer(indices, 0, index_size);
                command_buffer.CmdDrawIndexed(6, 1, 6, 4, 0);
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
    SECTION("An 8-bit index buffer on a device without the feature throws")
    {
        // The index type is a plain enum value in a core call, so nothing but this check stands between a
        // device that never enabled the extension and an index type it does not accept.
        ForgeFixture plain;
        const Opal::DynamicArray<u8> index_bytes =
            ToIndexBytes(k_half_indices, static_cast<i32>(std::size(k_half_indices)), IndexSize::uint8);
        const Forge::Buffer indices(plain.device, {.size = index_bytes.GetSize(), .usage = Forge::BufferUsageBits::IndexBuffer},
                                    index_bytes);
        Forge::CommandBuffer command_buffer(plain.device, plain.GetQueue());
        command_buffer.Begin();
        REQUIRE_FALSE(plain.device.GetFeatures().index_type_uint8);
        REQUIRE_THROWS_AS(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint8), Opal::Exception);
        // The two widths that need no extension still bind on the same command buffer.
        command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint16);
        command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint32);
        command_buffer.End();
        REQUIRE_NO_VALIDATION_ERROR(plain);
    }
    REQUIRE_NO_VALIDATION_ERROR(halves.forge);
}

TEST_CASE("Forge indirect draws", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const VkPhysicalDeviceFeatures device_features = GetFirstPhysicalDeviceFeatures();
    if (device_features.drawIndirectFirstInstance == VK_FALSE)
    {
        SKIP("This device cannot start an indirect draw at a non-zero instance.");
    }
    const bool has_multi_draw = device_features.multiDrawIndirect == VK_TRUE;
    HalvesFixture halves({.multi_draw_indirect = has_multi_draw, .draw_indirect_first_instance = true});

    const Forge::Shader write_draws =
        Forge::Shader::FromSourceInMemory(halves.forge.device, k_indirect_command_source,
                                          {.entry_point = "main_write_draws", .cache = GetShaderCache()});
    const Forge::Shader write_indexed_draw =
        Forge::Shader::FromSourceInMemory(halves.forge.device, k_indirect_command_source,
                                          {.entry_point = "main_write_indexed_draw", .cache = GetShaderCache()});

    // The commands live in memory the host cannot touch, so nothing but the dispatch below can have put them
    // there - which is what separates this from a direct draw with the same numbers written into a buffer.
    const Forge::Buffer commands(halves.forge.device, {.size = 2 * sizeof(Forge::DrawIndexedIndirectCommand),
                                                       .usage = Forge::BufferUsageBits::IndirectBuffer |
                                                                Forge::BufferUsageBits::StorageBuffer,
                                                       .host_access = Forge::HostAccess::None,
                                                       .use_device_address = true});

    auto make_write_pipeline = [&](const Forge::Shader& writer)
    {
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = writer;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        return Forge::Pipeline(halves.forge.device, pipeline_desc);
    };

    // Dispatches the writer over the command buffer, then orders that write against the indirect read.
    auto record_write = [&](const Forge::Pipeline& write_pipeline)
    {
        const VkDeviceAddress address = commands.GetNativeDeviceAddress();
        return [&, address](Forge::CommandBuffer& command_buffer)
        {
            command_buffer.CmdBindPipeline(write_pipeline);
            command_buffer.CmdPushConstants(write_pipeline, ShaderTypeBits::Compute, Opal::AsBytes(address));
            command_buffer.CmdDispatch(1);
            command_buffer.CmdBufferBarrier(
                Forge::BufferBarrier::WriteThenRead(commands, Forge::PipelineStageBits::ComputeShader, Forge::PipelineStageBits::IndirectDraw));
        };
    };

    SECTION("An indirect draw runs the command a compute shader wrote")
    {
        const Forge::Pipeline write_pipeline = make_write_pipeline(write_draws);
        const Opal::DynamicArray<u8> pixels =
            halves.Render(record_write(write_pipeline),
                          [&](Forge::CommandBuffer& command_buffer)
                          {
                              command_buffer.CmdBindVertexBuffer(halves.vertices, 0);
                              command_buffer.CmdDrawIndirect(commands, 0, 1);
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
        const Forge::Pipeline write_pipeline = make_write_pipeline(write_draws);
        const Opal::DynamicArray<u8> pixels =
            halves.Render(record_write(write_pipeline),
                          [&](Forge::CommandBuffer& command_buffer)
                          {
                              command_buffer.CmdBindVertexBuffer(halves.vertices, 0);
                              command_buffer.CmdDrawIndirect(commands, 0, 2);
                          });
        // Both commands ran, and each fetched the instance its own first_instance named rather than one of
        // them deciding for both.
        REQUIRE_HALF_COLOR(pixels, false, k_instance_two);
        REQUIRE_HALF_COLOR(pixels, true, k_instance_three);
    }
    SECTION("An indirect indexed draw follows the indices and the vertex offset it was given")
    {
        const Opal::DynamicArray<u8> index_bytes = ToIndexBytes(k_half_indices + 6, 6, IndexSize::uint32);
        const Forge::Buffer indices(halves.forge.device,
                                    {.size = index_bytes.GetSize(), .usage = Forge::BufferUsageBits::IndexBuffer}, index_bytes);
        const Forge::Pipeline write_pipeline = make_write_pipeline(write_indexed_draw);
        const Opal::DynamicArray<u8> pixels =
            halves.Render(record_write(write_pipeline),
                          [&](Forge::CommandBuffer& command_buffer)
                          {
                              command_buffer.CmdBindVertexBuffer(halves.corners, 0);
                              command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint32);
                              command_buffer.CmdDrawIndexedIndirect(commands, 0, 1);
                          });
        // The indices name the left corners; the vertex offset of four in the command is the only reason the
        // right half is what comes back.
        REQUIRE_HALF_COLOR(pixels, true, k_instance_three);
        REQUIRE_HALF_COLOR(pixels, false, k_untouched);
    }
    REQUIRE_NO_VALIDATION_ERROR(halves.forge);
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
        Forge::Shader::FromSourceInMemory(fixture.device, k_indirect_command_source,
                                          {.entry_point = "main_write_dispatch", .cache = GetShaderCache()});
    const Forge::Shader compute_shader = Forge::Shader::FromSourceInMemory(
        fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()});

    auto make_pipeline = [&](const Forge::Shader& shader)
    {
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        return Forge::Pipeline(fixture.device, pipeline_desc);
    };
    const Forge::Pipeline write_pipeline = make_pipeline(write_dispatch);
    const Forge::Pipeline compute_pipeline = make_pipeline(compute_shader);

    // Device-only, so the group counts cannot have come from the host.
    const Forge::Buffer group_counts(fixture.device, {.size = sizeof(Forge::DispatchIndirectCommand),
                                                      .usage = Forge::BufferUsageBits::IndirectBuffer |
                                                               Forge::BufferUsageBits::StorageBuffer |
                                                               Forge::BufferUsageBits::TransferSource,
                                                      .host_access = Forge::HostAccess::None,
                                                      .use_device_address = true});

    auto make_output = [&]
    {
        Forge::Buffer output(fixture.device, {.size = k_element_count * sizeof(u32),
                                              .usage = Forge::BufferUsageBits::StorageBuffer,
                                              .host_access = Forge::HostAccess::Random,
                                              .use_device_address = true});
        // Wiped first, so nothing left behind can pass for a dispatch that ran.
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        output.Update(zeros);
        return output;
    };
    const Forge::Buffer indirect_output = make_output();
    const Forge::Buffer direct_output = make_output();

    const VkDeviceAddress group_counts_address = group_counts.GetNativeDeviceAddress();
    const VkDeviceAddress indirect_address = indirect_output.GetNativeDeviceAddress();
    const VkDeviceAddress direct_address = direct_output.GetNativeDeviceAddress();
    Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               command_buffer.CmdBindPipeline(write_pipeline);
                               command_buffer.CmdPushConstants(write_pipeline, ShaderTypeBits::Compute,
                                                               Opal::AsBytes(group_counts_address));
                               command_buffer.CmdDispatch(1);
                               command_buffer.CmdBufferBarrier(Forge::BufferBarrier::WriteThenRead(
                                   group_counts, Forge::PipelineStageBits::ComputeShader, Forge::PipelineStageBits::IndirectDraw));

                               command_buffer.CmdBindPipeline(compute_pipeline);
                               command_buffer.CmdPushConstants(compute_pipeline, ShaderTypeBits::Compute,
                                                               Opal::AsBytes(indirect_address));
                               command_buffer.CmdDispatchIndirect(group_counts);
                               // The two dispatches write different buffers, so nothing has to order them
                               // against each other - only the push constant between them, which records in
                               // order with the commands around it.
                               command_buffer.CmdPushConstants(compute_pipeline, ShaderTypeBits::Compute,
                                                               Opal::AsBytes(direct_address));
                               command_buffer.CmdDispatch(k_group_count);
                           });

    SECTION("The group counts came off the device")
    {
        Forge::DispatchIndirectCommand written;
        Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), group_counts,
                              {reinterpret_cast<u8*>(&written), sizeof(written)});
        REQUIRE(written.group_count_x == k_group_count);
        REQUIRE(written.group_count_y == 1);
        REQUIRE(written.group_count_z == 1);
    }
    SECTION("An indirect dispatch of those counts matches a direct dispatch of the same ones")
    {
        Opal::DynamicArray<u32> from_indirect(k_element_count);
        Opal::DynamicArray<u32> from_direct(k_element_count);
        indirect_output.Read({reinterpret_cast<u8*>(from_indirect.GetData()), from_indirect.GetSize() * sizeof(u32)});
        direct_output.Read({reinterpret_cast<u8*>(from_direct.GetData()), from_direct.GetSize() * sizeof(u32)});
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

TEST_CASE("Forge pipeline sample count and dynamic state", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()});
    const Forge::Shader fragment_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()});

    auto make_desc = [&]()
    {
        Forge::GraphicsPipelineDesc desc;
        desc.vertex_shader = vertex_shader;
        desc.fragment_shader = fragment_shader;
        desc.rasterizer.cull_mode = Face::None;
        desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0);
        desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        desc.color_attachment_formats.PushBack(k_format);
        return desc;
    };

    SECTION("A sample count this device supports builds a pipeline")
    {
        const VkPhysicalDeviceLimits& limits = fixture.device.GetPhysicalDevice().GetProperties().limits;
        const VkSampleCountFlags supported = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
        INFO("supported sample counts mask " << supported);
        // One is the only count every device has to support, so it is the only one this can require.
        REQUIRE((supported & VK_SAMPLE_COUNT_1_BIT) != 0);

        Forge::GraphicsPipelineDesc desc = make_desc();
        desc.sample_count = Forge::SampleCount::Count1;
        const Forge::Pipeline pipeline(fixture.device, desc);
        REQUIRE(pipeline.IsValid());

        if ((supported & VK_SAMPLE_COUNT_4_BIT) != 0)
        {
            Forge::GraphicsPipelineDesc four = make_desc();
            four.sample_count = Forge::SampleCount::Count4;
            const Forge::Pipeline multisampled(fixture.device, four);
            REQUIRE(multisampled.IsValid());
        }
    }
    SECTION("A sample count this device does not support throws")
    {
        const VkPhysicalDeviceLimits& limits = fixture.device.GetPhysicalDevice().GetProperties().limits;
        const VkSampleCountFlags supported = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
        if ((supported & VK_SAMPLE_COUNT_64_BIT) != 0)
        {
            SKIP("This device supports 64 samples, so there is no unsupported count to test with.");
        }
        Forge::GraphicsPipelineDesc desc = make_desc();
        desc.sample_count = Forge::SampleCount::Count64;
        REQUIRE_THROWS_AS(Forge::Pipeline(fixture.device, desc), Opal::Exception);
    }
    SECTION("Dynamic state is recorded on a pipeline that asked for it")
    {
        Forge::GraphicsPipelineDesc desc = make_desc();
        desc.dynamic_state = Forge::DynamicStateBits::DepthBias | Forge::DynamicStateBits::StencilReference;
        desc.rasterizer.depth_bias_enabled = true;
        const Forge::Pipeline pipeline(fixture.device, desc);

        Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
        command_buffer.Begin();
        command_buffer.CmdBindPipeline(pipeline);
        command_buffer.CmdSetDepthBias(1.0f);
        command_buffer.CmdSetStencilReference(3);
        command_buffer.End();
    }
    SECTION("Dynamic state a feature gates throws without the feature")
    {
        Forge::CommandBuffer command_buffer(fixture.device, fixture.GetQueue());
        command_buffer.Begin();
        // The fixture device asks for neither, so both of these are the guard rather than the driver.
        REQUIRE_FALSE(fixture.device.GetFeatures().wide_lines);
        REQUIRE_FALSE(fixture.device.GetFeatures().depth_bias_clamp);
        REQUIRE_THROWS_AS(command_buffer.CmdSetLineWidth(4.0f), Opal::Exception);
        REQUIRE_THROWS_AS(command_buffer.CmdSetDepthBias(1.0f, 0.5f), Opal::Exception);
        // The value every device draws, and a bias with no clamp, need no feature.
        command_buffer.CmdSetLineWidth(1.0f);
        command_buffer.CmdSetDepthBias(1.0f);
        command_buffer.End();
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

TEST_CASE("Forge specialization constants", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr i32 k_side = 4;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader vertex_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_specialized_source, {.entry_point = "main_vertex", .cache = GetShaderCache()});
    const Forge::Shader fragment_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_specialized_source, {.entry_point = "main_fragment", .cache = GetShaderCache()});
    const Forge::Buffer vertices(fixture.device,
                                 {.size = sizeof(k_fullscreen_vertices),
                                  .usage = Forge::BufferUsageBits::VertexBuffer},
                                 Opal::AsBytes(k_fullscreen_vertices));

    /** Builds a pipeline with the given values, draws through it, and hands back one texel. */
    auto draw_specialized = [&](Opal::ArrayView<const Forge::SpecializationConstant> values)
    {
        Forge::Texture color(fixture.device, {.format = k_format,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::ColorAttachment |
                                                       Forge::TextureUsageBits::TransferSource});
        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0);
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
        for (i32 i = 0; i < values.GetSize(); ++i)
        {
            pipeline_desc.specialization.PushBack(
                Forge::SpecializationConstant{.name = values[i].name.Clone(), .value = values[i].value});
        }
        const Forge::Pipeline pipeline(fixture.device, pipeline_desc);

        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToColorAttachment(color));
                                   const Forge::RenderingDesc rendering_desc{
                                       .render_area_extent = {k_side, k_side},
                                       .color_attachments = {Forge::RenderingAttachmentDesc{
                                           .image_view = color.GetNativeImageView(),
                                           .image_layout = Forge::ImageLayout::ColorAttachment,
                                           .load_operation = Forge::AttachmentLoadOperation::Clear,
                                           .store_operation = Forge::AttachmentStoreOperation::Store,
                                           .clear_value = {.color = {0.0f, 0.0f, 0.0f, 1.0f}}}}};
                                   command_buffer.CmdBeginRendering(rendering_desc);
                                   command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side});
                                   command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side});
                                   command_buffer.CmdBindPipeline(pipeline);
                                   command_buffer.CmdBindVertexBuffer(vertices, 0);
                                   command_buffer.CmdDraw(3);
                                   command_buffer.CmdEndRendering();
                               });

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource);
        return Opal::DynamicArray<u8>{pixels[0], pixels[1], pixels[2], pixels[3]};
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
        const Opal::DynamicArray<u8> dim_texel = draw_specialized({dim, 1});
        const Opal::DynamicArray<u8> bright_texel = draw_specialized({bright, 2});
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
        const Opal::DynamicArray<u8> texel = draw_specialized({});
        REQUIRE(static_cast<i32>(texel[0]) == 64);
        REQUIRE(static_cast<i32>(texel[2]) == 0);
    }
    SECTION("A name no stage declares throws")
    {
        // The case Vulkan ignores in silence when the value is keyed by number, which is why it is keyed
        // by name here.
        const Forge::SpecializationConstant wrong[] = {{.name = "RED_LEVELL", .value = 1}};
        REQUIRE_THROWS_AS(draw_specialized({wrong, 1}), Opal::Exception);
    }
    SECTION("A value of the wrong type throws")
    {
        const Forge::SpecializationConstant wrong[] = {{.name = "RED_LEVEL", .value = 1.0f}};
        REQUIRE_THROWS_AS(draw_specialized({wrong, 1}), Opal::Exception);
    }
    SECTION("One constant given a value twice throws")
    {
        // Two map entries with the same constantID, which the specification does not allow within one
        // VkSpecializationInfo - and which reads as nothing worse than a repeated name from out here.
        const Forge::SpecializationConstant twice[] = {{.name = "RED_LEVEL", .value = 32},
                                                       {.name = "RED_LEVEL", .value = 64}};
        REQUIRE_THROWS_AS(draw_specialized({twice, 2}), Opal::Exception);
    }
    SECTION("A compute pipeline specializes the same way")
    {
        constexpr i32 k_element_count = 128;
        const Forge::Buffer output(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random,
                                                    .use_device_address = true});
        const Forge::Shader compute_shader = Forge::Shader::FromSourceInMemory(fixture.device, k_specialized_compute_source,
                                                                              {.entry_point = "main_specialized", .cache = GetShaderCache()});
        Forge::DescriptorPoolDesc pool_desc;
        pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1);
        const Forge::DescriptorPool pool(fixture.device, pool_desc);
        Forge::DescriptorSetLayoutDesc layout_desc;
        layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute);
        const Forge::DescriptorSetLayout layout(fixture.device, layout_desc);
        Forge::DescriptorSet set(pool, layout);
        set.Update(0, output);

        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = compute_shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        pipeline_desc.specialization.PushBack(Forge::SpecializationConstant{.name = "ADDEND", .value = 100u});
        const Forge::Pipeline pipeline(fixture.device, pipeline_desc);

        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdBindPipeline(pipeline);
                                   command_buffer.CmdBindDescriptorSet(pipeline, set);
                                   command_buffer.CmdDispatch(k_element_count / 64);
                               });
        Opal::DynamicArray<u32> values(k_element_count);
        output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)});
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

TEST_CASE("Forge shader reflection", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    const Forge::Shader vertex_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_reflected_source, {.entry_point = "main_vertex", .cache = GetShaderCache()});
    const Forge::Shader fragment_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_reflected_source, {.entry_point = "main_fragment", .cache = GetShaderCache()});

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
            INFO("input " << reinterpret_cast<const char*>(inputs[i].name.GetData()));
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
        const Forge::VertexInputDesc derived = Forge::VertexInputDesc::FromShader(vertex_shader);
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
        REQUIRE_THROWS_AS(Forge::VertexInputDesc::FromShader(fragment_shader), Opal::Exception);
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
        layout_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        layout_desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        const Forge::DescriptorSetLayout layout(fixture.device, layout_desc);

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
        return Forge::Pipeline(fixture.device, pipeline_desc);
    };

    SECTION("The derived vertex input and ranges build a pipeline")
    {
        const Forge::VertexInputDesc derived = Forge::VertexInputDesc::FromShader(vertex_shader);
        const Forge::Pipeline pipeline = build_pipeline(derived, good_ranges);
        REQUIRE(pipeline.IsValid());
    }
    SECTION("A location the shader reads that nothing feeds throws")
    {
        Forge::VertexInputDesc incomplete;
        incomplete.AddBinding(0, 28);
        incomplete.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0);
        incomplete.AddAttribute(0, 1, PixelFormat::R32G32B32_SFLOAT, 8);
        REQUIRE_THROWS_AS(build_pipeline(incomplete, good_ranges), Opal::Exception);
    }
    SECTION("An attribute at a location the shader declares nothing at is accepted")
    {
        // Tempting to refuse, and wrong to: an input the shader does not read is optimised out of the
        // SPIR-V, so this is indistinguishable from a vertex struct with a field only some of its pipelines
        // read. The unused input further down proves the two really are the same case from out here.
        Forge::VertexInputDesc extra = Forge::VertexInputDesc::FromShader(vertex_shader);
        extra.AddAttribute(0, 7, PixelFormat::R32_SFLOAT, 28);
        const Forge::Pipeline pipeline = build_pipeline(extra, good_ranges);
        REQUIRE(pipeline.IsValid());
    }
    SECTION("An input the shader never reads is not reported at all")
    {
        // Why the check above cannot exist. The struct has two members and reflection reports one.
        const Forge::Shader partial =
            Forge::Shader::FromSourceInMemory(fixture.device, k_unused_input_source, {.entry_point = "main_vertex", .cache = GetShaderCache()});
        REQUIRE(partial.GetInputs().GetSize() == 1);
        REQUIRE(partial.GetInputs()[0].location == 0);
    }
    SECTION("An attribute of the wrong numeric class throws")
    {
        // Location 2 is a uint2 in the shader; a float attribute of the same width is not the same thing.
        Forge::VertexInputDesc wrong_class;
        wrong_class.AddBinding(0, 28);
        wrong_class.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0);
        wrong_class.AddAttribute(0, 1, PixelFormat::R32G32B32_SFLOAT, 8);
        wrong_class.AddAttribute(0, 2, PixelFormat::R32G32_SFLOAT, 20);
        REQUIRE_THROWS_AS(build_pipeline(wrong_class, good_ranges), Opal::Exception);
    }
    SECTION("A normalised attribute feeding a float input is accepted")
    {
        // UNORM arrives in the shader as a float, so the class agrees even though the format does not.
        Forge::VertexInputDesc normalised;
        normalised.AddBinding(0, 28);
        normalised.AddAttribute(0, 0, PixelFormat::R8G8_UNORM, 0);
        normalised.AddAttribute(0, 1, PixelFormat::R32G32B32_SFLOAT, 8);
        normalised.AddAttribute(0, 2, PixelFormat::R32G32_UINT, 20);
        const Forge::Pipeline pipeline = build_pipeline(normalised, good_ranges);
        REQUIRE(pipeline.IsValid());
    }
    SECTION("A push constant range that stops short of what the shader reads throws")
    {
        const Forge::VertexInputDesc derived = Forge::VertexInputDesc::FromShader(vertex_shader);
        const Forge::PushConstantRange too_small{.shader_stages = ShaderTypeBits::Vertex, .offset = 0, .size = 4};
        REQUIRE_THROWS_AS(build_pipeline(derived, {&too_small, 1}), Opal::Exception);
    }
    SECTION("No push constant range at all, for a shader that reads one, throws")
    {
        // The likeliest way to get this wrong, and the reason the check does not wait for a range to exist.
        const Forge::VertexInputDesc derived = Forge::VertexInputDesc::FromShader(vertex_shader);
        REQUIRE_THROWS_AS(build_pipeline(derived, {}), Opal::Exception);
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
    const Forge::Shader vertex_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_reflected_source, {.entry_point = "main_vertex", .cache = GetShaderCache()});
    const Forge::Shader fragment_shader =
        Forge::Shader::FromSourceInMemory(fixture.device, k_reflected_source, {.entry_point = "main_fragment", .cache = GetShaderCache()});

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
        desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        const Forge::DescriptorSetLayout layout(fixture.device, desc);
        REQUIRE(layout.GetDesc().bindings[0].name == Opal::StringUtf8("first_texture"));
        REQUIRE(layout.GetDesc().bindings[1].name == Opal::StringUtf8("second_texture"));
        // The caller's desc is untouched - the names went onto the layout's own copy.
        REQUIRE(desc.bindings[0].name.IsEmpty());
    }
    SECTION("A binding declared as the wrong kind throws")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Fragment);
        desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        REQUIRE_THROWS_AS(Forge::DescriptorSetLayout(fixture.device, desc), Opal::Exception);
    }
    SECTION("A binding whose stages leave out the one that reads it throws")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Vertex);
        desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        REQUIRE_THROWS_AS(Forge::DescriptorSetLayout(fixture.device, desc), Opal::Exception);
    }
    SECTION("A binding the shaders read that the layout omits throws")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        REQUIRE_THROWS_AS(Forge::DescriptorSetLayout(fixture.device, desc), Opal::Exception);
    }
    SECTION("A binding no shader reads is accepted and stays nameless")
    {
        // A descriptor nothing samples is optimised out of the SPIR-V, so reflection cannot tell this apart
        // from a binding that was never declared - the sample binds a metallic roughness texture its shader
        // does not read yet. It keeps an empty name, which is the whole of what it costs.
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        desc.AddBinding(5, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        const Forge::DescriptorSetLayout layout(fixture.device, desc);
        REQUIRE(layout.GetDesc().bindings[0].name == Opal::StringUtf8("first_texture"));
        REQUIRE(layout.GetDesc().bindings[2].name.IsEmpty());
    }
    SECTION("A set writes the same descriptor by name as by index")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        const Forge::DescriptorSetLayout layout(fixture.device, desc);

        Forge::DescriptorPoolDesc pool_desc;
        pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 4);
        pool_desc.max_sets = 2;
        const Forge::DescriptorPool pool(fixture.device, pool_desc);

        const Forge::Texture texture(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                      .width = 4,
                                                      .height = 4,
                                                      .usage = Forge::TextureUsageBits::Sampled});
        const Forge::Sampler sampler(fixture.device, {.max_anisotropy = 1.0f});

        Forge::DescriptorSet set(pool, layout);
        REQUIRE(set.GetBindingIndex("first_texture") == 0);
        REQUIRE(set.GetBindingIndex("second_texture") == 1);
        set.Update("first_texture", texture, sampler);
        set.Update("second_texture", texture, sampler);
        REQUIRE_THROWS_AS(set.GetBindingIndex("third_texture"), Opal::Exception);
    }
    SECTION("A set from a layout built without shaders carries no names")
    {
        Forge::DescriptorSetLayoutDesc desc;
        desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
        const Forge::DescriptorSetLayout layout(fixture.device, desc);

        Forge::DescriptorPoolDesc pool_desc;
        pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 1);
        const Forge::DescriptorPool pool(fixture.device, pool_desc);

        Forge::DescriptorSet set(pool, layout);
        REQUIRE_THROWS_AS(set.GetBindingIndex("first_texture"), Opal::Exception);
    }
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

    const Opal::StringUtf8 directory{RNDR_CORE_ASSETS_DIR "/../build/shader-cache-test"};
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
            Forge::Shader::FromSourceInMemory(fixture.device, source, {.entry_point = entry_point, .cache = cache});
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
        const Forge::Shader shader =
            Forge::Shader::FromSourceInMemory(fixture.device, k_cache_source, {.entry_point = "main_first", .cache = reopened});
        REQUIRE(shader.IsValid());
        Rndr::ShaderCache third{directory};
        REQUIRE(third.Find(key) == written);
    }
    SECTION("A cache with no directory still answers within the process")
    {
        Rndr::ShaderCache memory_only;
        REQUIRE(memory_only.GetDirectory().IsEmpty());
        REQUIRE(memory_only.GetFilePath(key).IsEmpty());
        const Forge::Shader first =
            Forge::Shader::FromSourceInMemory(fixture.device, k_cache_source, {.entry_point = "main_first", .cache = memory_only});
        const Forge::Shader second =
            Forge::Shader::FromSourceInMemory(fixture.device, k_cache_source, {.entry_point = "main_first", .cache = memory_only});
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
 * worst at catching by accident - 3.11 found three of them in Device and DeviceQueue alone - so each type is
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
    // what DeviceQueue's assignment got wrong before 3.11. The leak itself shows up in the validation layer
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

/** The compute pipeline k_compute_source needs, which pushes the address it writes through. */
Forge::Pipeline MakeAddressPipeline(const Forge::Device& device, const Forge::Shader& shader)
{
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
    return Forge::Pipeline(device, pipeline_desc);
}

/** A wiped buffer of k_lifetime_elements, so nothing left in it can pass for a dispatch that ran. */
constexpr i32 k_lifetime_elements = 64;

Forge::Buffer MakeWipedOutput(const Forge::Device& device)
{
    Forge::Buffer output(device, {.size = k_lifetime_elements * sizeof(u32),
                                  .usage = Forge::BufferUsageBits::StorageBuffer,
                                  .host_access = Forge::HostAccess::Random,
                                  .use_device_address = true});
    const Opal::DynamicArray<u8> zeros(k_lifetime_elements * sizeof(u32));
    output.Update(zeros);
    return output;
}

/** Dispatch the address pipeline over one buffer and check every element it should have written. */
void RequireDispatchWrites(const Forge::Device& device, Forge::DeviceQueue& queue, const Forge::Pipeline& pipeline,
                           const Forge::Buffer& output)
{
    const VkDeviceAddress address = output.GetNativeDeviceAddress();
    Forge::ImmediateSubmit(device, queue,
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               command_buffer.CmdBindPipeline(pipeline);
                               command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(address));
                               command_buffer.CmdDispatch(1);
                           });
    Opal::DynamicArray<u32> values(k_lifetime_elements);
    output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)});
    for (i32 i = 0; i < k_lifetime_elements; ++i)
    {
        INFO("element " << i);
        REQUIRE(values[i] == static_cast<u32>(i) + 1000);
    }
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
        "GraphicsContext", [] { return Forge::GraphicsContext({.collect_debug_messages = true}); },
        [](const Forge::GraphicsContext& context)
        {
            REQUIRE(context.GetInstance() != VK_NULL_HANDLE);
            // Enumerating is the cheapest call that goes through the instance the move had to carry.
            REQUIRE(context.EnumeratePhysicalDevices().GetSize() > 0);
        });
}

TEST_CASE("Forge empty state and moves of the device stack", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const Forge::GraphicsContext context({.collect_debug_messages = true});

    auto make_physical_device = [&context]
    {
        Opal::DynamicArray<Forge::PhysicalDevice> devices = context.EnumeratePhysicalDevices();
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
    { return Forge::Device(make_physical_device(), context, {}); };
    CheckLifetimeContract("Device", make_device,
                          [](Forge::Device& device)
                          {
                              // Allocating is what needs the VMA allocator, which is one of the two members
                              // Device's move used to drop; the other was the enabled extension list.
                              const Forge::Buffer buffer = MakeWipedOutput(device);
                              REQUIRE(buffer.IsValid());
                              REQUIRE(device.GetNativeDevice() != VK_NULL_HANDLE);
                              REQUIRE(device.GetPhysicalDevice().IsValid());
                              // Every queue holds a reference back to the device, which a move has to
                              // re-point, so reaching one through the moved device is the check for that.
                              REQUIRE(device.GetQueue(Forge::QueueFamily::Graphics).IsValid());
                          });

    Forge::Device device = make_device();
    const u32 graphics_family = device.GetQueue(Forge::QueueFamily::Graphics).GetQueueFamilyIndex();
    // A queue of its own rather than one from GetQueue: those belong to the device, and destroying one would
    // leave the device holding a queue with no command pool. See the note on DeviceQueue::Destroy.
    CheckLifetimeContract("DeviceQueue", [&device, graphics_family] { return Forge::DeviceQueue(device, graphics_family); },
                          [&device](Forge::DeviceQueue& queue)
                          {
                              REQUIRE(queue.GetNativeQueue() != VK_NULL_HANDLE);
                              REQUIRE(queue.GetNativeCommandPool() != VK_NULL_HANDLE);
                              // Submitting needs every member at once: the device, the queue, the family
                              // index and the command pool the command buffer is allocated out of.
                              Forge::ImmediateSubmit(device, queue, [](Forge::CommandBuffer&) {});
                              queue.WaitIdle();
                          });
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
                              return Forge::Buffer(fixture.device,
                                                   {.size = written.GetSize(),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random},
                                                   written);
                          },
                          [&](const Forge::Buffer& buffer)
                          {
                              // Read back through the mapped pointer, which is the member 1.4 found the move
                              // leaving behind.
                              Opal::DynamicArray<u8> read_back(written.GetSize());
                              buffer.Read(read_back);
                              REQUIRE(CountMismatches(written, read_back) == 0);
                          });

    CheckLifetimeContract("Texture",
                          [&]
                          {
                              return Forge::Texture(fixture.device, {.format = k_format,
                                                                     .width = k_side,
                                                                     .height = k_side,
                                                                     .usage = Forge::TextureUsageBits::ColorAttachment |
                                                                              Forge::TextureUsageBits::TransferSource});
                          },
                          [&](Forge::Texture& texture)
                          {
                              REQUIRE(texture.GetNativeImageView() != VK_NULL_HANDLE);
                              REQUIRE(texture.GetDesc().width == k_side);
                              // Forge tracks the layout per subresource itself, so the move has to carry that
                              // array; a readback transitions the texture and then asks where it ended up.
                              REQUIRE(texture.GetCurrentLayout() == Forge::ImageLayout::Undefined);
                              Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
                              Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0,
                                                     Forge::ImageLayout::TransferSource);
                              REQUIRE(texture.GetCurrentLayout() == Forge::ImageLayout::TransferSource);
                          });

    // A sampler holds nothing but its device and its handle, so writing it into a descriptor is the cheapest
    // thing that uses both. Sampling through one is 3.18.
    Forge::DescriptorPoolDesc sampler_pool_desc;
    sampler_pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 4);
    sampler_pool_desc.max_sets = 4;
    const Forge::DescriptorPool sampler_pool(fixture.device, sampler_pool_desc);
    Forge::DescriptorSetLayoutDesc sampler_layout_desc;
    sampler_layout_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment);
    const Forge::DescriptorSetLayout sampler_layout(fixture.device, sampler_layout_desc);
    const Forge::Texture sampled(fixture.device, {.format = k_format,
                                                  .width = k_side,
                                                  .height = k_side,
                                                  .usage = Forge::TextureUsageBits::Sampled});

    CheckLifetimeContract("Sampler", [&] { return Forge::Sampler(fixture.device, {.max_anisotropy = 1.0f}); },
                          [&](const Forge::Sampler& sampler)
                          {
                              REQUIRE(sampler.GetNativeSampler() != VK_NULL_HANDLE);
                              Forge::DescriptorSet set(sampler_pool, sampler_layout);
                              set.Update(0, sampled, sampler);
                          });

    CheckLifetimeContract("Shader",
                          [&]
                          {
                              return Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source,
                                                                       {.entry_point = "main_compute", .cache = GetShaderCache()});
                          },
                          [&](const Forge::Shader& shader)
                          {
                              // The stage and the entry point are read out of the reflection at construction,
                              // and a pipeline built from the moved shader needs both of them plus the module.
                              REQUIRE(shader.GetShaderStage() == ShaderTypeBits::Compute);
                              REQUIRE(shader.GetEntryPoint() == Opal::StringUtf8("main_compute"));
                              const Forge::Pipeline pipeline = MakeAddressPipeline(fixture.device, shader);
                              const Forge::Buffer output = MakeWipedOutput(fixture.device);
                              RequireDispatchWrites(fixture.device, fixture.GetQueue(), pipeline, output);
                          });

    const Forge::Shader compute_shader = Forge::Shader::FromSourceInMemory(
        fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()});
    CheckLifetimeContract("Pipeline", [&] { return MakeAddressPipeline(fixture.device, compute_shader); },
                          [&](const Forge::Pipeline& pipeline)
                          {
                              // Binding needs the layout and the bind point beside the pipeline, and the push
                              // constant the dispatch depends on goes through the layout.
                              REQUIRE(pipeline.GetNativePipelineLayout() != VK_NULL_HANDLE);
                              REQUIRE(pipeline.GetBindPoint() == VK_PIPELINE_BIND_POINT_COMPUTE);
                              const Forge::Buffer output = MakeWipedOutput(fixture.device);
                              RequireDispatchWrites(fixture.device, fixture.GetQueue(), pipeline, output);
                          });

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge empty state and moves of the descriptor objects", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;

    Forge::DescriptorPoolDesc pool_desc;
    pool_desc.Add(Forge::DescriptorType::StorageBuffer, 16);
    pool_desc.max_sets = 16;
    // On, so that DescriptorSet::Destroy returns the set to its pool rather than only dropping the handle,
    // which is the half of 1.7 nothing runs otherwise.
    pool_desc.free_individual_sets = true;

    Forge::DescriptorSetLayoutDesc layout_desc;
    layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute);

    CheckLifetimeContract("DescriptorPool", [&] { return Forge::DescriptorPool(fixture.device, pool_desc); },
                          [&](const Forge::DescriptorPool& pool)
                          {
                              REQUIRE(pool.GetNativeDescriptorPool() != VK_NULL_HANDLE);
                              // Allocating out of the moved pool is what needs its device and its desc, and
                              // the set it hands back holds the pool by reference.
                              const Forge::DescriptorSetLayout layout(fixture.device, layout_desc);
                              const Forge::DescriptorSet set(pool, layout);
                              REQUIRE(set.IsValid());
                          });

    CheckLifetimeContract("DescriptorSetLayout", [&] { return Forge::DescriptorSetLayout(fixture.device, layout_desc); },
                          [&](const Forge::DescriptorSetLayout& layout)
                          {
                              REQUIRE(layout.GetNativeDescriptorSetLayout() != VK_NULL_HANDLE);
                              // The desc is what a set reads its binding types out of, so a layout that lost
                              // it allocates a set that then knows about no binding at all.
                              REQUIRE(layout.GetDesc().bindings.GetSize() == 1);
                              const Forge::DescriptorPool pool(fixture.device, pool_desc);
                              const Forge::DescriptorSet set(pool, layout);
                              REQUIRE(set.GetBindingDescriptorType(0) == Forge::DescriptorType::StorageBuffer);
                          });

    const Forge::DescriptorPool pool(fixture.device, pool_desc);
    const Forge::DescriptorSetLayout layout(fixture.device, layout_desc);
    const Forge::Shader shader = Forge::Shader::FromSourceInMemory(
        fixture.device, k_descriptor_source, {.entry_point = "main_descriptor", .cache = GetShaderCache()});
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
    const Forge::Pipeline pipeline(fixture.device, pipeline_desc);

    CheckLifetimeContract("DescriptorSet", [&] { return Forge::DescriptorSet(pool, layout); },
                          [&](Forge::DescriptorSet& set)
                          {
                              REQUIRE(set.GetBindingDescriptorType(0) == Forge::DescriptorType::StorageBuffer);
                              const Forge::Buffer output = MakeWipedOutput(fixture.device);
                              set.Update(0, output);
                              Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                                     [&](Forge::CommandBuffer& command_buffer)
                                                     {
                                                         command_buffer.CmdBindPipeline(pipeline);
                                                         command_buffer.CmdBindDescriptorSet(pipeline, set);
                                                         command_buffer.CmdDispatch(1);
                                                     });
                              Opal::DynamicArray<u32> values(k_lifetime_elements);
                              output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)});
                              for (i32 i = 0; i < k_lifetime_elements; ++i)
                              {
                                  INFO("element " << i);
                                  REQUIRE(values[i] == static_cast<u32>(i) + 7);
                              }
                          });

    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge empty state and moves of the command and synchronization objects", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    Forge::DeviceQueue& queue = fixture.GetQueue();

    CheckLifetimeContract("CommandBuffer", [&] { return Forge::CommandBuffer(fixture.device, queue); },
                          [&](Forge::CommandBuffer& command_buffer)
                          {
                              // Recording and submitting is what needs the queue the buffer was allocated on
                              // beside the handle itself.
                              command_buffer.Begin();
                              command_buffer.End();
                              const Forge::Fence fence(fixture.device, false);
                              queue.Submit(command_buffer, fence);
                              fence.Wait();
                          });

    CheckLifetimeContract("Fence", [&] { return Forge::Fence(fixture.device, false); },
                          [&](Forge::Fence& fence)
                          {
                              Forge::CommandBuffer command_buffer(fixture.device, queue);
                              command_buffer.Begin();
                              command_buffer.End();
                              queue.Submit(command_buffer, fence);
                              fence.Wait();
                              // Signalled now, and Reset has to reach the device the move carried.
                              REQUIRE(fence.TryWait(0));
                              fence.Reset();
                              REQUIRE_FALSE(fence.TryWait(0));
                          });

    CheckLifetimeContract("Semaphore",
                          [&] {
                              return Forge::Semaphore(fixture.device,
                                                      {.type = Forge::SemaphoreType::Timeline, .initial_value = 3});
                          },
                          [](const Forge::Semaphore& semaphore)
                          {
                              // The type is a member of its own, and every host side call throws on a binary
                              // semaphore, so a move that dropped it would fail here rather than answer wrong.
                              REQUIRE(semaphore.IsTimeline());
                              REQUIRE(semaphore.GetValue() == 3);
                              semaphore.Signal(7);
                              REQUIRE(semaphore.GetValue() == 7);
                          });

    // Four rather than the two TimestampQueryPoolDesc defaults to: a check that asks for the default value
    // cannot tell a desc that came through the move from one that was never assigned.
    CheckLifetimeContract("TimestampQueryPool", [&] { return Forge::TimestampQueryPool(fixture.device, {.query_count = 4}); },
                          [&](Forge::TimestampQueryPool& pool)
                          {
                              REQUIRE(pool.GetQueryCount() == 4);
                              // Read off the device once at construction and used by every elapsed helper.
                              // Compared against what the device reports rather than against zero, because the
                              // member defaults to one: a move that dropped it would otherwise keep answering
                              // a plausible number and turn every measurement into ticks.
                              REQUIRE(pool.GetTimestampPeriod() ==
                                      fixture.device.GetPhysicalDevice().GetProperties().limits.timestampPeriod);
                              Forge::ImmediateSubmit(fixture.device, queue,
                                                     [&](Forge::CommandBuffer& command_buffer)
                                                     {
                                                         command_buffer.CmdResetQueryPool(pool);
                                                         command_buffer.CmdWriteTimestamp(pool, 0,
                                                                                          Forge::PipelineStageBits::PipelineStart);
                                                         command_buffer.CmdWriteTimestamp(pool, 1,
                                                                                          Forge::PipelineStageBits::PipelineEnd);
                                                     });
                              Opal::InPlaceArray<u64, 2> ticks;
                              pool.GetResults({ticks.GetData(), 2});
                              REQUIRE(ticks[1] >= ticks[0]);
                          });

    REQUIRE_NO_VALIDATION_ERROR(fixture);
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
    const Forge::Buffer staging(device, {.size = pixels.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, pixels);
    const Forge::BufferImageCopyRegion region{
        .image_subresource = {.array_layer_count = texture.GetDesc().array_layer_count}};
    Forge::ImmediateSubmit(device, queue,
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferDestination(texture));
                               command_buffer.CmdCopyBufferToImage(staging, texture, {&region, 1});
                           });
}

/** A texture of the given size holding MakeTexelGrid, ready to be copied out of and into. */
Forge::Texture MakeGridTexture(const Forge::Device& device, Forge::DeviceQueue& queue, i32 width, i32 height, u8 seed,
                               PixelFormat format = PixelFormat::R8G8B8A8_UNORM)
{
    Forge::Texture texture(device, {.format = format,
                                    .width = static_cast<u32>(width),
                                    .height = static_cast<u32>(height),
                                    .usage = Forge::TextureUsageBits::TransferSource |
                                             Forge::TextureUsageBits::TransferDestination});
    const Opal::DynamicArray<u8> pixels = MakeTexelGrid(width, height, seed);
    UploadGrid(device, queue, texture, pixels);
    return texture;
}

/** An empty texture a blit or a copy writes into. */
Forge::Texture MakeTransferTarget(const Forge::Device& device, i32 width, i32 height,
                                  PixelFormat format = PixelFormat::R8G8B8A8_UNORM)
{
    return Forge::Texture(device, {.format = format,
                                   .width = static_cast<u32>(width),
                                   .height = static_cast<u32>(height),
                                   .usage = Forge::TextureUsageBits::TransferSource |
                                            Forge::TextureUsageBits::TransferDestination});
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

        const Forge::ImageBlitRegion region{};
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferSource(source));
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferDestination(destination));
                                   command_buffer.CmdBlitImage(source, destination, {&region, 1}, ImageFilter::Nearest);
                               });

        Opal::DynamicArray<u8> pixels(k_target_side * k_target_side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, pixels, 0, Forge::ImageLayout::TransferSource);
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
        const Forge::ImageBlitRegion region{.destination_offset = {k_side, 0, 0}, .destination_extent = {-k_side, k_side, 1}};
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferSource(source));
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferDestination(destination));
                                   command_buffer.CmdBlitImage(source, destination, {&region, 1}, ImageFilter::Nearest);
                               });

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, pixels, 0, Forge::ImageLayout::TransferSource);
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

        const Forge::ImageBlitRegion region{};
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferSource(source));
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferDestination(destination));
                                   command_buffer.CmdBlitImage(source, destination, {&region, 1}, ImageFilter::Nearest);
                               });

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, pixels, 0, Forge::ImageLayout::TransferSource);
        // Read back as the bytes of a BGRA image, so the red the source wrote is now the third byte. A blit
        // that had copied rather than converted would leave it first, which is what separates this from
        // CmdCopyImage.
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

        const Forge::ImageBlitRegion region{};
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferSource(source));
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferDestination(destination));
                                   command_buffer.CmdBlitImage(source, destination, {&region, 1}, ImageFilter::Linear);
                               });

        Opal::DynamicArray<u8> pixels(k_target_side * k_target_side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, pixels, 0, Forge::ImageLayout::TransferSource);
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
        Forge::Buffer buffer(fixture.device, {.size = k_buffer_size,
                                              .usage = Forge::BufferUsageBits::TransferDestination,
                                              .host_access = Forge::HostAccess::Random});
        Opal::DynamicArray<u8> filler(k_buffer_size);
        for (i32 i = 0; i < k_buffer_size; ++i)
        {
            filler[i] = k_sentinel;
        }
        buffer.Update(filler);
        return buffer;
    };

    // Runs one region and hands back the whole buffer, so a case can check what was written and what was not.
    auto copy_region = [&](const Forge::BufferImageCopyRegion& region)
    {
        Forge::Texture source = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, k_seed);
        const Forge::Buffer buffer = make_sentinel_buffer();
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferSource(source));
                                   command_buffer.CmdCopyImageToBuffer(source, buffer, {&region, 1});
                               });
        Opal::DynamicArray<u8> out(k_buffer_size);
        buffer.Read(out);
        return out;
    };

    SECTION("A sub-box copies the texels it names and no others")
    {
        const Forge::BufferImageCopyRegion region{.image_offset = {1, 1, 0}, .image_extent = {2, 2, 1}};
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
        const Forge::BufferImageCopyRegion region{.buffer_offset = k_offset, .image_extent = {k_side, k_side, 1}};
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
        const Forge::BufferImageCopyRegion region{.buffer_row_length = 4, .image_extent = {2, 2, 1}};
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
    SECTION("A row length and an image height space out the rows and the layers")
    {
        constexpr i32 k_layer_count = 2;
        constexpr i32 k_box = 2;
        constexpr i32 k_row_length = 4;
        constexpr i32 k_image_height = 3;
        Forge::Texture source(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                               .width = k_side,
                                               .height = k_side,
                                               .array_layer_count = k_layer_count,
                                               .usage = Forge::TextureUsageBits::TransferSource |
                                                        Forge::TextureUsageBits::TransferDestination,
                                               .view_type = Forge::TextureViewType::Texture2DArray});
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
        const Forge::BufferImageCopyRegion region{.buffer_row_length = k_row_length,
                                                  .buffer_image_height = k_image_height,
                                                  .image_subresource = {.array_layer_count = k_layer_count},
                                                  .image_extent = {k_box, k_box, 1}};
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToTransferSource(source));
                                   command_buffer.CmdCopyImageToBuffer(source, buffer, {&region, 1});
                               });
        Opal::DynamicArray<u8> out(k_buffer_size);
        buffer.Read(out);

        // One layer is row_length * image_height texels apart from the next, which is larger than the box
        // the copy actually wrote - so the gap between them has to still hold the sentinel.
        constexpr i32 k_layer_stride = k_row_length * k_image_height;
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
        // The last row of the first layer's image height is past its box and was never written.
        for (i32 byte = 0; byte < 4 * k_row_length; ++byte)
        {
            const i32 index = (2 * k_row_length) * 4 + byte;
            INFO("byte " << index << " of the gap between the layers");
            REQUIRE(out[index] == k_sentinel);
        }
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}

TEST_CASE("Forge mip level sizes", "[forge]")
{
    // No device: GetMipLevelSize reads a desc and nothing else, so this is the one case here that needs
    // nothing on the machine.
    SECTION("Every level is a quarter of the one above it")
    {
        const Forge::TextureDesc desc{.format = PixelFormat::R8G8B8A8_UNORM, .width = 8, .height = 8, .mip_level_count = 4};
        REQUIRE(Forge::GetMipLevelSize(desc, 0) == 8 * 8 * 4);
        REQUIRE(Forge::GetMipLevelSize(desc, 1) == 4 * 4 * 4);
        REQUIRE(Forge::GetMipLevelSize(desc, 2) == 2 * 2 * 4);
        REQUIRE(Forge::GetMipLevelSize(desc, 3) == 1 * 1 * 4);
    }
    SECTION("An odd extent halves down rather than rounding up, and never below one")
    {
        const Forge::TextureDesc desc{.format = PixelFormat::R8G8B8A8_UNORM, .width = 5, .height = 3, .mip_level_count = 4};
        REQUIRE(Forge::GetMipLevelSize(desc, 0) == 5 * 3 * 4);
        REQUIRE(Forge::GetMipLevelSize(desc, 1) == 2 * 1 * 4);
        // Both axes are already at one here, and a level below that is still one texel rather than none.
        REQUIRE(Forge::GetMipLevelSize(desc, 2) == 1 * 1 * 4);
        REQUIRE(Forge::GetMipLevelSize(desc, 3) == 1 * 1 * 4);
    }
    SECTION("Array layers and depth both multiply the level")
    {
        const Forge::TextureDesc layered{
            .format = PixelFormat::R8G8B8A8_UNORM, .width = 4, .height = 4, .mip_level_count = 2, .array_layer_count = 3};
        REQUIRE(Forge::GetMipLevelSize(layered, 0) == 4 * 4 * 4 * 3);
        REQUIRE(Forge::GetMipLevelSize(layered, 1) == 2 * 2 * 4 * 3);

        const Forge::TextureDesc volume{.dimension = Forge::TextureDimension::Texture3D,
                                        .format = PixelFormat::R8G8B8A8_UNORM,
                                        .width = 4,
                                        .height = 4,
                                        .depth = 4,
                                        .mip_level_count = 2};
        REQUIRE(Forge::GetMipLevelSize(volume, 0) == 4 * 4 * 4 * 4);
        // Depth halves with the other two axes.
        REQUIRE(Forge::GetMipLevelSize(volume, 1) == 2 * 2 * 2 * 4);
    }
    SECTION("A depth format is sized by its own texel, not by four bytes of colour")
    {
        const Forge::TextureDesc half{.format = PixelFormat::D16_UNORM, .width = 4, .height = 4};
        REQUIRE(Forge::GetMipLevelSize(half, 0) == 4 * 4 * 2);
        const Forge::TextureDesc full{.format = PixelFormat::D32_SFLOAT, .width = 4, .height = 4};
        REQUIRE(Forge::GetMipLevelSize(full, 0) == 4 * 4 * 4);
    }
    SECTION("A block compressed format throws rather than answering as if it were packed texels")
    {
        // The size of a compressed level is a count of blocks, not of texels, and answering with the texel
        // arithmetic would hand a readback a buffer of the wrong size and no reason to notice.
        const Forge::TextureDesc desc{.format = PixelFormat::BC1_RGBA_UNORM_BLOCK, .width = 8, .height = 8, .mip_level_count = 2};
        REQUIRE_THROWS_AS(Forge::GetMipLevelSize(desc, 0), Opal::Exception);
    }
    SECTION("A level the texture does not have throws")
    {
        const Forge::TextureDesc desc{.format = PixelFormat::R8G8B8A8_UNORM, .width = 8, .height = 8, .mip_level_count = 2};
        REQUIRE_THROWS_AS(Forge::GetMipLevelSize(desc, 2), Opal::Exception);
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
        const Forge::Shader compute_shader = Forge::Shader::FromSourceInMemory(
            fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()});
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = compute_shader;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        const Forge::Pipeline pipeline(fixture.device, pipeline_desc);

        const Forge::Buffer written(fixture.device, {.size = k_element_count * sizeof(u32),
                                                     .usage = Forge::BufferUsageBits::StorageBuffer |
                                                              Forge::BufferUsageBits::TransferSource,
                                                     .host_access = Forge::HostAccess::None,
                                                     .use_device_address = true});
        const Forge::Buffer copied(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::TransferDestination,
                                                    .host_access = Forge::HostAccess::Random});
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        copied.Update(zeros);
        Forge::Texture texture = MakeGridTexture(fixture.device, fixture.GetQueue(), k_side, k_side, 33);

        const VkDeviceAddress address = written.GetNativeDeviceAddress();
        Forge::ImmediateSubmit(
            fixture.device, fixture.GetQueue(),
            [&](Forge::CommandBuffer& command_buffer)
            {
                command_buffer.CmdBindPipeline(pipeline);
                command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(address));
                command_buffer.CmdDispatch(1);

                const Forge::MemoryBarrier memory{.stages_must_finish = Forge::PipelineStageBits::ComputeShader,
                                                  .stages_must_finish_access = Forge::PipelineStageAccessBits::Write,
                                                  .before_stages_start = Forge::PipelineStageBits::Transfer,
                                                  .before_stages_start_access = Forge::PipelineStageAccessBits::Read};
                const Forge::BufferBarrier buffer = Forge::BufferBarrier::WriteThenRead(
                    written, Forge::PipelineStageBits::ComputeShader, Forge::PipelineStageBits::Transfer);
                const Forge::ImageBarrier image = Forge::ImageBarrier::ToTransferSource(texture);
                // CmdBarriers is what every other Cmd*Barrier delegates to, and the only way to put all
                // three kinds into one dependency.
                command_buffer.CmdBarriers({.memory = {&memory, 1}, .buffer = {&buffer, 1}, .image = {&image, 1}});

                command_buffer.CmdCopyBuffer(written, copied);
            });

        Opal::DynamicArray<u32> values(k_element_count);
        copied.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)});
        for (i32 i = 0; i < k_element_count; ++i)
        {
            INFO("element " << i);
            REQUIRE(values[i] == static_cast<u32>(i) + 1000);
        }
        // The image barrier in the same batch moved the texture, which is what makes this readback legal.
        REQUIRE(texture.GetCurrentLayout() == Forge::ImageLayout::TransferSource);
        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0, Forge::ImageLayout::TransferSource);
        const Opal::DynamicArray<u8> expected = MakeTexelGrid(k_side, k_side, 33);
        REQUIRE(CountMismatches(expected, pixels) == 0);
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
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   const Forge::ImageBarrier image = Forge::ImageBarrier::ToTransferSource(texture);
                                   command_buffer.CmdBarriers(
                                       {.image = {&image, 1}, .flags = Forge::DependencyFlagBits::ByRegion});
                               });
        REQUIRE(texture.GetCurrentLayout() == Forge::ImageLayout::TransferSource);
        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0, Forge::ImageLayout::TransferSource);
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
        Forge::Texture texture(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                .width = k_side,
                                                .height = k_side,
                                                .usage = Forge::TextureUsageBits::TransferSource |
                                                         Forge::TextureUsageBits::TransferDestination | usage});
        UploadGrid(fixture.device, fixture.GetQueue(), texture, expected);
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               { command_buffer.CmdImageBarrier(make_barrier(texture)); });
        REQUIRE(texture.GetCurrentLayout() == expected_layout);

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0, Forge::ImageLayout::TransferSource);
        REQUIRE(CountMismatches(expected, pixels) == 0);
    };

    SECTION("ToShaderRead moves a sampled texture without losing it")
    {
        run_preset(Forge::TextureUsageBits::Sampled, [](Forge::Texture& texture) { return Forge::ImageBarrier::ToShaderRead(texture); },
                   Forge::ImageLayout::ShaderReadOnly);
    }
    SECTION("ToTransferSource moves a texture into the layout a copy reads from")
    {
        run_preset(Forge::TextureUsageBits::Sampled,
                   [](Forge::Texture& texture) { return Forge::ImageBarrier::ToTransferSource(texture); },
                   Forge::ImageLayout::TransferSource);
    }
    SECTION("The three argument To is told both layouts")
    {
        // No short form on purpose: with both, dropping an argument would leave a call that compiles and
        // means the opposite, since the layout in the middle is the source and the one at the end is not.
        run_preset(Forge::TextureUsageBits::Sampled,
                   [](Forge::Texture& texture)
                   {
                       return Forge::ImageBarrier::To(texture, texture.GetCurrentLayout(), Forge::ImageLayout::TransferDestination);
                   },
                   Forge::ImageLayout::TransferDestination);
    }
    SECTION("A layout with no preset throws rather than guessing")
    {
        Forge::Texture texture(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                .width = k_side,
                                                .height = k_side,
                                                .usage = Forge::TextureUsageBits::TransferSource});
        // General is a real layout with no preset behind it, which is the near miss worth checking: the
        // dispatch throws rather than picking whichever preset is closest.
        REQUIRE_THROWS_AS(Forge::ImageBarrier::To(texture, Forge::ImageLayout::Undefined, Forge::ImageLayout::General),
                          Opal::Exception);
    }
    SECTION("ToDepthStencilAttachment moves a depth texture")
    {
        Forge::Texture depth(fixture.device, {.format = PixelFormat::D32_SFLOAT,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::DepthStencilAttachment});
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               { command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToDepthStencilAttachment(depth)); });
        // Rendering with one is 3.16; what this says is that the preset picks the depth aspect off the
        // format rather than the colour aspect a colour texture would have given it.
        REQUIRE(depth.GetCurrentLayout() == Forge::ImageLayout::DepthStencilAttachment);
    }
    SECTION("BufferBarrier::ReadThenWrite orders a read before the write that follows it")
    {
        // The write is a transfer over the same range a compute shader just read, so without the barrier the
        // copy would be free to land before the dispatch finished reading.
        const Forge::Shader compute_shader = Forge::Shader::FromSourceInMemory(
            fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()});
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = compute_shader;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        const Forge::Pipeline pipeline(fixture.device, pipeline_desc);

        constexpr i32 k_element_count = 64;
        const Opal::DynamicArray<u8> replacement = MakeBytes(k_element_count * sizeof(u32), 13);
        const Forge::Buffer shared(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer |
                                                             Forge::BufferUsageBits::TransferDestination,
                                                    .host_access = Forge::HostAccess::Random,
                                                    .use_device_address = true});
        const Forge::Buffer source(fixture.device,
                                   {.size = replacement.GetSize(), .usage = Forge::BufferUsageBits::TransferSource}, replacement);

        const VkDeviceAddress address = shared.GetNativeDeviceAddress();
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdBindPipeline(pipeline);
                                   command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(address));
                                   command_buffer.CmdDispatch(1);
                                   command_buffer.CmdBufferBarrier(Forge::BufferBarrier::ReadThenWrite(
                                       shared, Forge::PipelineStageBits::ComputeShader, Forge::PipelineStageBits::Transfer));
                                   command_buffer.CmdCopyBuffer(source, shared);
                               });

        // The copy is last, so what is in the buffer is what it wrote.
        Opal::DynamicArray<u8> out(replacement.GetSize());
        shared.Read(out);
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
    const Forge::GraphicsContext context({.collect_debug_messages = true});
    Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = context.EnumeratePhysicalDevices();
    if (!physical_devices[0].IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
    {
        SKIP("This device has no swap chain extension.");
    }
    Forge::DeviceDesc device_desc;
    device_desc.extensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    Forge::Device device(std::move(physical_devices[0]), context, device_desc);
    Forge::DeviceQueue& queue = device.GetQueue(Forge::QueueFamily::Graphics);

    constexpr i32 k_side = 4;
    Forge::Texture texture(device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                    .width = k_side,
                                    .height = k_side,
                                    .usage = Forge::TextureUsageBits::ColorAttachment |
                                             Forge::TextureUsageBits::TransferSource});
    Forge::ImmediateSubmit(device, queue,
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToColorAttachment(texture));
                               command_buffer.CmdImageBarrier(Forge::ImageBarrier::ToPresent(texture));
                           });
    REQUIRE(texture.GetCurrentLayout() == Forge::ImageLayout::Present);

    Opal::StringUtf8 report;
    for (const Forge::DebugMessage& message : context.GetDebugMessages())
    {
        if (message.severity == Forge::DebugMessageSeverity::Error && !!(message.types & Forge::DebugMessageTypeBits::Validation))
        {
            report += message.text;
            report += Opal::StringUtf8("\n");
        }
    }
    INFO(*report);
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation) == 0);
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
    pool_desc.Add(Forge::DescriptorType::StorageBuffer, 8);
    pool_desc.max_sets = 8;
    const Forge::DescriptorPool pool(fixture.device, pool_desc);

    Forge::DescriptorSetLayoutDesc layout_desc;
    layout_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute);
    const Forge::DescriptorSetLayout first_layout(fixture.device, layout_desc);
    const Forge::DescriptorSetLayout second_layout(fixture.device, layout_desc);

    const Forge::Shader shader = Forge::Shader::FromSourceInMemory(fixture.device, k_two_set_source,
                                                                   {.entry_point = "main_two_sets", .cache = GetShaderCache()});
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(first_layout));
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(second_layout));
    const Forge::Pipeline pipeline(fixture.device, pipeline_desc);

    // Distinct values per element, so a shader that read the wrong set would produce a pattern rather than
    // one wrong number.
    Opal::DynamicArray<u32> input_values(k_element_count);
    for (i32 i = 0; i < k_element_count; ++i)
    {
        input_values[i] = static_cast<u32>(i) + 500;
    }
    const Forge::Buffer input(fixture.device, {.size = k_element_count * sizeof(u32),
                                               .usage = Forge::BufferUsageBits::StorageBuffer,
                                               .host_access = Forge::HostAccess::Random},
                              {reinterpret_cast<const u8*>(input_values.GetData()), input_values.GetSize() * sizeof(u32)});

    auto make_wiped_output = [&]
    {
        Forge::Buffer output(fixture.device, {.size = k_element_count * sizeof(u32),
                                              .usage = Forge::BufferUsageBits::StorageBuffer,
                                              .host_access = Forge::HostAccess::Random});
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        output.Update(zeros);
        return output;
    };

    auto require_doubled = [&](const Forge::Buffer& output)
    {
        Opal::DynamicArray<u32> values(k_element_count);
        output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)});
        for (i32 i = 0; i < k_element_count; ++i)
        {
            INFO("element " << i);
            REQUIRE(values[i] == input_values[i] * 2);
        }
    };

    SECTION("Both sets go down in one call")
    {
        Forge::DescriptorSet first(pool, first_layout);
        Forge::DescriptorSet second(pool, second_layout);
        const Forge::Buffer output = make_wiped_output();
        first.Update(0, input);
        second.Update(0, output);

        const Opal::InPlaceArray<Opal::Ref<const Forge::DescriptorSet>, 2> sets{Opal::Ref<const Forge::DescriptorSet>(first),
                                                                                Opal::Ref<const Forge::DescriptorSet>(second)};
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdBindPipeline(pipeline);
                                   command_buffer.CmdBindDescriptorSets(pipeline, {sets.GetData(), 2});
                                   command_buffer.CmdDispatch(1);
                               });
        require_doubled(output);
    }
    SECTION("A non-zero first set binds into the slot it names")
    {
        // Set zero goes down on its own and set one through the plural call at first_set one, so a call that
        // ignored first_set would overwrite set zero and the shader would read its output as its input.
        Forge::DescriptorSet first(pool, first_layout);
        Forge::DescriptorSet second(pool, second_layout);
        const Forge::Buffer output = make_wiped_output();
        first.Update(0, input);
        second.Update(0, output);

        const Opal::InPlaceArray<Opal::Ref<const Forge::DescriptorSet>, 1> sets{Opal::Ref<const Forge::DescriptorSet>(second)};
        Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   command_buffer.CmdBindPipeline(pipeline);
                                   command_buffer.CmdBindDescriptorSet(pipeline, first, 0);
                                   command_buffer.CmdBindDescriptorSets(pipeline, {sets.GetData(), 1}, 1);
                                   command_buffer.CmdDispatch(1);
                               });
        require_doubled(output);
    }
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}
