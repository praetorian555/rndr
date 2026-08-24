#include <cstdlib>

#include <catch2/catch2.hpp>

#include "opal/file-system.h"
#include "opal/paths.h"

#include "rndr/core/shader-cache.hpp"
#include "rndr/core/shader-compiler.hpp"

#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/exceptions.h"

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
    static Rndr::ShaderCache cache{Opal::StringUtf8(RNDR_CORE_ASSETS_DIR "/../build/shader-cache")};
    return cache;
}

/**
 * Which of the optional queue families a fixture's device asks for. Both off by default: Device throws when
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
 * dedicated transfer queue on by default, and Device throws when a family it was asked for is not there - so
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

    explicit ForgeFixture(const Forge::DeviceFeatures& features = {}, const ForgeQueues& queues = {})
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
        Opal::Expected<Forge::Device, ErrorCode> device_result =
            Forge::Device::Create(std::move(physical_devices.GetValue()[0]), context,
                                  {.features = features,
                                   .use_async_compute_queue = queues.async_compute,
                                   .use_dedicated_transfer_queue = queues.dedicated_transfer});
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
 * Whether this machine has a Vulkan device at all, so a machine without one skips rather than fails.
 *
 * This rests on EnumeratePhysicalDevices reporting NoGraphicsDevice when it finds none. While it handed back
 * an empty list, the probe below reached `physical_devices[0]` on it and read off the end of the array - so
 * the one machine this function exists for is the one machine it did not work on.
 */
bool IsForgeAvailable()
{
    static const bool available = []
    {
        const ForgeFixture probe;
        return probe.status == ErrorCode::Success;
    }();
    return available;
}

/**
 * Whether this machine offers the optional queue families, since one family that does everything is a legal
 * device and Forge reports rather than falling back to the graphics queue when asked for a family it has not
 * got. Tests about those families skip on such a machine the way the whole file skips on one with no device.
 */
bool AreQueuesAvailable(const ForgeQueues& queues)
{
    const ForgeFixture probe({}, queues);
    return probe.status == ErrorCode::Success;
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
        const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
        const Opal::DynamicArray<Forge::PhysicalDevice> devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
        return devices[0].GetProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
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

}  // namespace

/**
 * The one case here that refuses to skip. Every other case steps aside on a machine with no Vulkan device,
 * which is what makes a run that found none look exactly like a run that passed. The same goes for a build
 * without RNDR_FORGE_VALIDATION, where the context collects no debug messages and every
 * REQUIRE_NO_VALIDATION_ERROR below asserts on nothing. A machine that is meant to have both says so through
 * RNDR_TEST_REQUIRE_VULKAN, and then this fails instead of the file going quiet.
 */
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
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation).GetValue() == 0);
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

    SECTION("A write that does not fit throws")
    {
        REQUIRE(buffer.Update(written, k_size - 1) != ErrorCode::Success);
    }
    SECTION("A read of write-combined memory throws")
    {
        const Forge::Buffer write_only =
            ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}));
        Opal::DynamicArray<u8> out(k_size);
        REQUIRE(write_only.Read(out) != ErrorCode::Success);
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

    Forge::Buffer source = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size,
                                          .usage = Forge::BufferUsageBits::TransferSource,
                                          .host_access = Forge::HostAccess::Random,
                                          .keep_memory_mapped = true},
                         written));

    Forge::Buffer moved_to(std::move(source));
    REQUIRE_FALSE(source.IsValid());
    REQUIRE(moved_to.IsValid());

    Forge::Buffer assigned_to;
    assigned_to = std::move(moved_to);
    REQUIRE_FALSE(moved_to.IsValid());
    REQUIRE(assigned_to.IsValid());

    // The mapped pointer has to have come along, or this writes through a pointer the source unmapped.
    Opal::DynamicArray<u8> read_back(k_size);
    REQUIRE(assigned_to.Read(read_back) == ErrorCode::Success);
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

    Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                          .usage = Forge::BufferUsageBits::StorageBuffer,
                                          .host_access = Forge::HostAccess::Random,
                                          .use_device_address = true}));
    const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
    // Wiped first, so nothing a previous run left behind can pass for a successful dispatch.
    REQUIRE(output.Update(zeros) == ErrorCode::Success);

    const Forge::Shader compute_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
    REQUIRE(compute_shader.IsValid());
    REQUIRE(compute_shader.GetShaderStage() == ShaderTypeBits::Compute);

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = compute_shader;
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    REQUIRE(pipeline.IsValid());

    const VkDeviceAddress output_address = output.GetNativeDeviceAddress();
    REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(output_address)) ==
                                       ErrorCode::Success);
                               REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
                           }) == ErrorCode::Success);

    Opal::DynamicArray<u32> values(k_element_count);
    REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
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

    SECTION("Reading back into a view of the wrong size throws")
    {
        Opal::DynamicArray<u8> too_small(4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, too_small, 0,
                                       Forge::ImageLayout::TransferDestination) != ErrorCode::Success);
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

    SECTION("Update and Read both throw on it")
    {
        Opal::DynamicArray<u8> out(k_size);
        REQUIRE(buffer.Update(written) != ErrorCode::Success);
        REQUIRE(buffer.Read(out) != ErrorCode::Success);
    }
    SECTION("Initial data throws rather than leaking the allocation")
    {
        REQUIRE_FALSE(Forge::Buffer::Create(fixture.device,
                                        {.size = k_size,
                                         .usage = Forge::BufferUsageBits::TransferDestination,
                                         .host_access = Forge::HostAccess::None},
                                        written).HasValue());
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
    Forge::Texture source = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                          {.format = PixelFormat::R8G8B8A8_UNORM, .width = 4, .height = 4, .usage = k_transfer_usage}));
    Forge::Texture destination = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device,
                               {.format = PixelFormat::R16G16B16A16_SFLOAT, .width = 4, .height = 4, .usage = k_transfer_usage}));
    Forge::SetDebugName(fixture.device, source, "probe-source-texture");
    Forge::SetDebugName(fixture.device, destination, "probe-destination-texture");

    // Copying between two formats of different texel size breaks a rule the guards do not check and the
    // validation layer does, which is what makes it a way to read back what the layer calls these two images.
    // The layer checks this while the command is recorded, so the command buffer is thrown away rather than
    // submitted: handing the driver work that breaks the specification is undefined behaviour, and it took
    // the next test down with it when this did.
    const Forge::TextureCopyRegion region;
    Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    REQUIRE(command_buffer.Begin() == ErrorCode::Success);
    REQUIRE(command_buffer.CmdTransition(source, Forge::ImageLayout::TransferSource) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdTransition(destination, Forge::ImageLayout::TransferDestination) == ErrorCode::Success);
    REQUIRE(command_buffer.CmdCopyTexture(source, destination, {&region, 1}) == ErrorCode::Success);
    REQUIRE(command_buffer.End() == ErrorCode::Success);

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
    const Forge::Buffer source_a = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, first_half));
    const Forge::Buffer source_b = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, second_half));
    const Forge::Buffer destination = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size * 2, .usage = k_both_ways}));
    REQUIRE(destination.Update(zeros) == ErrorCode::Success);

    // One command buffer per half, so a batch that dropped either one would show as half the buffer missing.
    Forge::CommandBuffer first = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    Forge::CommandBuffer second = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    const Forge::BufferCopyRegion first_region{.source_offset = 0, .destination_offset = 0, .size = k_size};
    const Forge::BufferCopyRegion second_region{.source_offset = 0, .destination_offset = k_size, .size = k_size};
    REQUIRE(first.Begin() == ErrorCode::Success);
    REQUIRE(first.CmdCopyBuffer(source_a, destination, {&first_region, 1}) == ErrorCode::Success);
    REQUIRE(first.End() == ErrorCode::Success);
    REQUIRE(second.Begin() == ErrorCode::Success);
    REQUIRE(second.CmdCopyBuffer(source_b, destination, {&second_region, 1}) == ErrorCode::Success);
    REQUIRE(second.End() == ErrorCode::Success);

    SECTION("Two command buffers in one batch, with a fence")
    {
        const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        const Opal::Ref<const Forge::CommandBuffer> batch[2] = {Opal::Ref<const Forge::CommandBuffer>(first),
                                                                Opal::Ref<const Forge::CommandBuffer>(second)};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {batch, 2}, .fence = fence}) == ErrorCode::Success);
        REQUIRE(fence.Wait() == ErrorCode::Success);
    }
    SECTION("The same batch without a fence, waited on through the queue")
    {
        const Opal::Ref<const Forge::CommandBuffer> batch[2] = {Opal::Ref<const Forge::CommandBuffer>(first),
                                                                Opal::Ref<const Forge::CommandBuffer>(second)};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {batch, 2}}) == ErrorCode::Success);
        REQUIRE(fixture.GetQueue().WaitIdle() == ErrorCode::Success);
    }
    SECTION("One batch per half, the second waiting on a semaphore the first signals")
    {
        const Forge::Semaphore semaphore = ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device));
        const Forge::Fence fence = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        const Opal::Ref<const Forge::CommandBuffer> first_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(first)};
        const Opal::Ref<const Forge::CommandBuffer> second_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(second)};
        const Forge::SemaphoreSubmit signal{.semaphore = semaphore, .stages = Forge::PipelineStageBits::Transfer};
        const Forge::SemaphoreSubmit wait{.semaphore = semaphore, .stages = Forge::PipelineStageBits::Transfer};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&signal, 1}}) == ErrorCode::Success);
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .wait_semaphores = {&wait, 1}, .fence = fence}) ==
                ErrorCode::Success);
        REQUIRE(fence.Wait() == ErrorCode::Success);
    }

    Opal::DynamicArray<u8> read_back(k_size * 2);
    REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), destination, read_back) == ErrorCode::Success);
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
    const Forge::Buffer source_a = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, first_half));
    const Forge::Buffer source_b = ForgeTest::Unwrap(
        Forge::Buffer::Create(fixture.device, {.size = k_size, .usage = Forge::BufferUsageBits::TransferSource}, second_half));
    const Forge::Buffer destination = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_size * 2, .usage = k_both_ways}));
    REQUIRE(destination.Update(zeros) == ErrorCode::Success);

    // The same split copy the batched submit case uses: one command buffer per half, so a batch that dropped
    // either one would show up as half the buffer missing rather than as nothing at all.
    Forge::CommandBuffer first = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    Forge::CommandBuffer second = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
    const Forge::BufferCopyRegion first_region{.source_offset = 0, .destination_offset = 0, .size = k_size};
    const Forge::BufferCopyRegion second_region{.source_offset = 0, .destination_offset = k_size, .size = k_size};
    REQUIRE(first.Begin() == ErrorCode::Success);
    REQUIRE(first.CmdCopyBuffer(source_a, destination, {&first_region, 1}) == ErrorCode::Success);
    REQUIRE(first.End() == ErrorCode::Success);
    REQUIRE(second.Begin() == ErrorCode::Success);
    REQUIRE(second.CmdCopyBuffer(source_b, destination, {&second_region, 1}) == ErrorCode::Success);
    REQUIRE(second.End() == ErrorCode::Success);
    const Opal::Ref<const Forge::CommandBuffer> first_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(first)};
    const Opal::Ref<const Forge::CommandBuffer> second_batch[1] = {Opal::Ref<const Forge::CommandBuffer>(second)};

    // Both halves in the destination, which is what every section that submits has to end up with.
    auto require_whole_buffer_copied = [&]()
    {
        Opal::DynamicArray<u8> read_back(k_size * 2);
        REQUIRE(Forge::ReadBackBuffer(fixture.device, fixture.GetQueue(), destination, read_back) == ErrorCode::Success);
        for (i32 i = 0; i < k_size; ++i)
        {
            REQUIRE(read_back[i] == first_half[i]);
            REQUIRE(read_back[k_size + i] == second_half[i]);
        }
    };

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
    SECTION("A wait that runs out of time answers false rather than throwing")
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
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&signal, 1}}) == ErrorCode::Success);
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .wait_semaphores = {&wait, 1}, .fence = fence}) ==
                ErrorCode::Success);
        REQUIRE(fence.Wait() == ErrorCode::Success);
        require_whole_buffer_copied();
    }
    SECTION("The host waits on a value the device signals, with no fence anywhere")
    {
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        const Forge::SemaphoreSubmit first_signal{.semaphore = timeline, .value = 1};
        const Forge::SemaphoreSubmit second_signal{.semaphore = timeline, .value = 2};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&first_signal, 1}}) ==
                ErrorCode::Success);
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .signal_semaphores = {&second_signal, 1}}) ==
                ErrorCode::Success);
        REQUIRE(timeline.Wait(2) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(timeline.GetValue()) == 2);
        require_whole_buffer_copied();
    }
    SECTION("WaitForAll over two timelines returns once both have been signalled")
    {
        const Forge::Semaphore first_timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        const Forge::Semaphore second_timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        const Forge::SemaphoreSubmit first_signal{.semaphore = first_timeline, .value = 1};
        const Forge::SemaphoreSubmit second_signal{.semaphore = second_timeline, .value = 1};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&first_signal, 1}}) ==
                ErrorCode::Success);
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {second_batch, 1}, .signal_semaphores = {&second_signal, 1}}) ==
                ErrorCode::Success);
        const Forge::SemaphoreWait waits[2] = {{.semaphore = first_timeline, .value = 1}, {.semaphore = second_timeline, .value = 1}};
        REQUIRE(Forge::Semaphore::WaitForAll({waits, 2}) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(first_timeline.GetValue()) == 1);
        REQUIRE(ForgeTest::Unwrap(second_timeline.GetValue()) == 1);
        require_whole_buffer_copied();
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
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&signal, 1}}) ==
                ErrorCode::InvalidArgument);
    }
    SECTION("A timeline signalled with zero is refused, since no signal can reach it")
    {
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline}));
        const Forge::SemaphoreSubmit signal{.semaphore = timeline, .value = 0};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .signal_semaphores = {&signal, 1}}) ==
                ErrorCode::InvalidArgument);
        // A wait for zero is legal and trivially satisfied, so only the signal side is turned away.
        const Forge::SemaphoreSubmit wait{.semaphore = timeline, .value = 0};
        REQUIRE(fixture.GetQueue().Submit({.command_buffers = {first_batch, 1}, .wait_semaphores = {&wait, 1}}) == ErrorCode::Success);
        REQUIRE(fixture.GetQueue().WaitIdle() == ErrorCode::Success);
    }
    SECTION("A signal that does not raise the count throws")
    {
        const Forge::Semaphore timeline =
            ForgeTest::Unwrap(Forge::Semaphore::Create(fixture.device, {.type = Forge::SemaphoreType::Timeline, .initial_value = 4}));
        REQUIRE(timeline.Signal(4) != ErrorCode::Success);
        REQUIRE(timeline.Signal(3) != ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(timeline.GetValue()) == 4);
        REQUIRE(timeline.Signal(5) == ErrorCode::Success);
        REQUIRE(ForgeTest::Unwrap(timeline.GetValue()) == 5);
    }
    SECTION("WaitForAll over two devices throws rather than naming one of them")
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
        REQUIRE(Forge::Semaphore::WaitForAll({waits, 2}) != ErrorCode::Success);
    }
    SECTION("An empty entry in WaitForAll throws, either way it is empty")
    {
        const Forge::Semaphore empty;
        const Forge::SemaphoreWait empty_reference[1] = {{}};
        REQUIRE(Forge::Semaphore::WaitForAll({empty_reference, 1}) != ErrorCode::Success);
        const Forge::SemaphoreWait empty_semaphore[1] = {{.semaphore = empty, .value = 1}};
        REQUIRE(Forge::Semaphore::WaitForAll({empty_semaphore, 1}) != ErrorCode::Success);
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
    const Opal::Ref<const Forge::CommandBuffer> empty_command_buffer{};
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

    SECTION("A fence that is not signalled in time answers false rather than throwing")
    {
        constexpr u64 k_short_timeout = 1000 * 1000;
        // Both of the fences above have been waited on, so they are signalled and answer at once.
        REQUIRE(ForgeTest::Unwrap(fences[0].TryWait(k_short_timeout)));
        REQUIRE(ForgeTest::Unwrap(Forge::Fence::TryWaitForAll(fences, k_short_timeout)));
        // Nothing is submitted against this one, so it stays unsignalled and the timeout is the only way out.
        const Forge::Fence never_signalled = ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, false));
        REQUIRE_FALSE(ForgeTest::Unwrap(never_signalled.TryWait(k_short_timeout)));
    }
    SECTION("Fences from two devices in one wait throw")
    {
        // A second logical device on the same physical one, for the reason the timeline case above gives.
        Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(fixture.context.EnumeratePhysicalDevices());
        const Forge::Device other =
            ForgeTest::Unwrap(Forge::Device::Create(std::move(physical_devices[0]), fixture.context, MakeHeadlessDeviceDesc()));
        Opal::DynamicArray<Forge::Fence> across_devices;
        across_devices.EmplaceBack(ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, true)));
        across_devices.EmplaceBack(ForgeTest::Unwrap(Forge::Fence::Create(other, true)));
        REQUIRE(Forge::Fence::WaitForAll(across_devices) != ErrorCode::Success);
    }
    SECTION("An empty fence in the list throws")
    {
        Opal::DynamicArray<Forge::Fence> with_empty;
        with_empty.EmplaceBack(ForgeTest::Unwrap(Forge::Fence::Create(fixture.device, true)));
        with_empty.EmplaceBack();
        REQUIRE(Forge::Fence::WaitForAll(with_empty) != ErrorCode::Success);
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
        REQUIRE_FALSE(Forge::Buffer::Create(device, {.size = 64,
                                                 .usage = Forge::BufferUsageBits::StorageBuffer,
                                                 .use_device_address = true}).HasValue());
    }
    SECTION("An anisotropic sampler needs the feature")
    {
        const Forge::Device device = ForgeTest::Unwrap(make_device({.sampler_anisotropy = false}));
        REQUIRE_FALSE(Forge::Sampler::Create(device, {.max_anisotropy = 8.0f}).HasValue());
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
        REQUIRE(command_buffer.CmdDrawIndirect(commands, 0, 2) != ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }

    Opal::StringUtf8 report;
    for (const Forge::DebugMessage& message : context.GetDebugMessages())
    {
        report += message.text;
        report += Opal::StringUtf8("\n");
    }
    INFO(*report);
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation).GetValue() == 0);
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

    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation).GetValue() == 0);
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
    const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
    Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());

    constexpr Forge::DeviceFeatures k_bindless_features{.partially_bound_descriptors = true,
                                                        .update_after_bind_descriptors = true,
                                                        .non_uniform_descriptor_indexing = true};
    const Forge::DeviceDesc bindless_desc = MakeHeadlessDeviceDesc(k_bindless_features);
    if (!Forge::FindPhysicalDevice(physical_devices, bindless_desc).HasValue())
    {
        SKIP("This device does not support the descriptor indexing features bindless needs.");
    }
    Forge::Device device = ForgeTest::Unwrap(Forge::Device::Create(
        ForgeTest::Unwrap(Forge::SelectPhysicalDevice(physical_devices, bindless_desc)), context, bindless_desc));
    Forge::DeviceQueue& queue = ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics));

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

        Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = k_element_count * sizeof(u32),
                                      .usage = Forge::BufferUsageBits::StorageBuffer,
                                      .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);

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

        REQUIRE(Forge::ImmediateSubmit(device, queue,
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, descriptor_set) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
                               }) == ErrorCode::Success);

        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            REQUIRE(values[i] == static_cast<u32>(i) + 2000);
        }
    }
    SECTION("A variable count above the binding's descriptor count throws")
    {
        REQUIRE_FALSE(Forge::DescriptorSet::Create(pool, layout, k_max_descriptors + 1).HasValue());
    }
    SECTION("A variable count without a binding that allows it throws")
    {
        Forge::DescriptorSetLayoutDesc plain_desc;
        REQUIRE(plain_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        const Forge::DescriptorSetLayout plain_layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(device, plain_desc));
        REQUIRE_FALSE(Forge::DescriptorSet::Create(pool, plain_layout, 1).HasValue());
    }
    SECTION("A variable count on anything but the highest binding throws")
    {
        Forge::DescriptorSetLayoutDesc bad_desc;
        REQUIRE(bad_desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 4, ShaderTypeBits::Compute, {},
                            Forge::DescriptorBindingFlagBits::VariableDescriptorCount) == ErrorCode::Success);
        REQUIRE(bad_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
        REQUIRE_FALSE(Forge::DescriptorSetLayout::Create(device, bad_desc).HasValue());
    }
    SECTION("An update after bind layout needs a pool that expects one")
    {
        Forge::DescriptorPoolDesc plain_pool_desc;
        REQUIRE(plain_pool_desc.Add(Forge::DescriptorType::StorageBuffer, k_max_descriptors) == ErrorCode::Success);
        plain_pool_desc.use_update_after_bind = false;
        const Forge::DescriptorPool plain_pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(device, plain_pool_desc));
        REQUIRE_FALSE(Forge::DescriptorSet::Create(plain_pool, layout, k_used_descriptors).HasValue());
    }

    Opal::StringUtf8 report;
    for (const Forge::DebugMessage& message : context.GetDebugMessages())
    {
        report += message.text;
        report += Opal::StringUtf8("\n");
    }
    INFO(*report);
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation).GetValue() == 0);
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
    const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
    Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());

    constexpr Forge::DeviceFeatures k_bindless_features{.partially_bound_descriptors = true,
                                                        .update_after_bind_descriptors = true,
                                                        .non_uniform_descriptor_indexing = true};
    const Forge::DeviceDesc bindless_desc = MakeHeadlessDeviceDesc(k_bindless_features);
    if (!Forge::FindPhysicalDevice(physical_devices, bindless_desc).HasValue())
    {
        SKIP("This device does not support the descriptor indexing features bindless needs.");
    }
    Forge::Device device = ForgeTest::Unwrap(Forge::Device::Create(
        ForgeTest::Unwrap(Forge::SelectPhysicalDevice(physical_devices, bindless_desc)), context, bindless_desc));
    Forge::DeviceQueue& queue = ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics));

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

        Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = k_element_count * sizeof(u32),
                                      .usage = Forge::BufferUsageBits::StorageBuffer,
                                      .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);

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

        REQUIRE(Forge::ImmediateSubmit(device, queue,
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, descriptor_set) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
                               }) == ErrorCode::Success);

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
    SECTION("An array element past the end of the binding throws")
    {
        // Three of four allocated, so elements zero through two exist and element three does not, even though
        // the layout declares four. Vulkan writes such an element without reporting anything.
        Forge::DescriptorSet descriptor_set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout, k_used_descriptors));
        const Forge::Texture texture = make_texture(k_red_at_one);
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(device, {.max_anisotropy = 1.0f}));

        REQUIRE_NOTHROW(descriptor_set.Update(1, texture, sampler, Forge::ImageLayout::ShaderReadOnly, k_used_descriptors - 1));
        REQUIRE(descriptor_set.Update(1, texture, sampler, Forge::ImageLayout::ShaderReadOnly, k_used_descriptors) != ErrorCode::Success);
        REQUIRE(descriptor_set.Update(1, texture, sampler, Forge::ImageLayout::ShaderReadOnly, k_max_descriptors) != ErrorCode::Success);
        // A binding of one descriptor has only element zero, variable count or not.
        Forge::Buffer output =
            ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = 4, .usage = Forge::BufferUsageBits::StorageBuffer}));
        REQUIRE(descriptor_set.Update(0, output, 0, Forge::k_whole_buffer, 1) != ErrorCode::Success);
    }
    SECTION("A variable count above the texture binding descriptor count throws")
    {
        REQUIRE_FALSE(Forge::DescriptorSet::Create(pool, layout, k_max_descriptors + 1).HasValue());
    }
    SECTION("An update after bind texture layout needs a pool that expects one")
    {
        Forge::DescriptorPoolDesc plain_pool_desc;
        REQUIRE(plain_pool_desc.Add(Forge::DescriptorType::StorageBuffer, 1) == ErrorCode::Success);
        REQUIRE(plain_pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, k_max_descriptors) == ErrorCode::Success);
        plain_pool_desc.use_update_after_bind = false;
        const Forge::DescriptorPool plain_pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(device, plain_pool_desc));
        REQUIRE_FALSE(Forge::DescriptorSet::Create(plain_pool, layout, k_used_descriptors).HasValue());
    }

    Opal::StringUtf8 report;
    for (const Forge::DebugMessage& message : context.GetDebugMessages())
    {
        report += message.text;
        report += Opal::StringUtf8("\n");
    }
    INFO(*report);
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation).GetValue() == 0);
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
    const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
    Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());

    constexpr Forge::DeviceFeatures k_bindless_features{.partially_bound_descriptors = true,
                                                        .update_after_bind_descriptors = true,
                                                        .non_uniform_descriptor_indexing = true};
    const Forge::DeviceDesc bindless_desc = MakeHeadlessDeviceDesc(k_bindless_features);
    if (!Forge::FindPhysicalDevice(physical_devices, bindless_desc).HasValue())
    {
        SKIP("This device does not support the descriptor indexing features bindless needs.");
    }
    if (IsSoftwareDevice())
    {
        SKIP("A software driver reports the non-uniform indexing feature and then reads element zero anyway.");
    }
    Forge::Device device = ForgeTest::Unwrap(Forge::Device::Create(
        ForgeTest::Unwrap(Forge::SelectPhysicalDevice(physical_devices, bindless_desc)), context, bindless_desc));
    Forge::DeviceQueue& queue = ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics));

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

    Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = k_element_count * sizeof(u32),
                                  .usage = Forge::BufferUsageBits::StorageBuffer,
                                  .host_access = Forge::HostAccess::Random}));
    const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
    REQUIRE(output.Update(zeros) == ErrorCode::Success);

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

    REQUIRE(Forge::ImmediateSubmit(device, queue,
                           [&](Forge::CommandBuffer& command_buffer)
                           {
                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, descriptor_set) == ErrorCode::Success);
                               REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
                           }) == ErrorCode::Success);

    Opal::DynamicArray<u32> values(k_element_count);
    REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
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
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation).GetValue() == 0);
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
    SECTION("A barrier naming the mesh stage without the extension throws")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        const Forge::MemoryBarrier barrier{.stages_must_finish = Forge::PipelineStageBits::MeshShader,
                                           .stages_must_finish_access = Forge::PipelineStageAccessBits::Write,
                                           .before_stages_start = Forge::PipelineStageBits::FragmentShader,
                                           .before_stages_start_access = Forge::PipelineStageAccessBits::Read};
        REQUIRE(command_buffer.CmdMemoryBarrier(barrier) != ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("An ownership transfer naming one family on both sides is recorded")
    {
        // Both halves of a transfer, release and acquire, on the one queue this test has. Naming the same
        // family on both sides is a no-op transfer, which is what makes it safe to record here.
        const u32 family = fixture.GetQueue().GetQueueFamilyIndex();
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdCopyBuffer(source, destination) == ErrorCode::Success);
                                   const Forge::BufferBarrier release{
                                       .stages_must_finish = Forge::PipelineStageBits::Copy,
                                       .stages_must_finish_access = Forge::PipelineStageAccessBits::TransferWrite,
                                       .before_stages_start = Forge::PipelineStageBits::None,
                                       .before_stages_start_access = Forge::PipelineStageAccessBits::None,
                                       .source_queue_family = family,
                                       .destination_queue_family = family,
                                       .buffer = destination};
                                   REQUIRE(command_buffer.CmdBufferBarrier(release) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        Opal::DynamicArray<u8> read_back(k_size);
        REQUIRE(destination.Read(read_back) == ErrorCode::Success);
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
        REQUIRE_FALSE(texture.GetCurrentLayout().HasValue());
        // A transition of the whole texture cannot say what it is coming from either.
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly) != ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A subresource the texture does not have throws")
    {
        REQUIRE_FALSE(texture.GetCurrentLayout(k_mip_count).HasValue());
        REQUIRE_FALSE(texture.GetCurrentLayout(0, 1).HasValue());
    }
    SECTION("A transfer out of a layout the role does not allow throws")
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
        REQUIRE(command_buffer.CmdBlitTexture(texture, texture, {&region, 1}) != ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A move carries the layouts across")
    {
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(), [&](Forge::CommandBuffer& command_buffer)
                    { REQUIRE(command_buffer.CmdTransition(texture, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success); }) ==
                ErrorCode::Success);
        Forge::Texture moved(std::move(texture));
        REQUIRE(ForgeTest::Unwrap(moved.GetCurrentLayout()) == Forge::ImageLayout::ShaderReadOnly);
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
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = compute_shader;
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
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
        REQUIRE(pool.Reset() != ErrorCode::Success);
    }
    SECTION("A pool that asks for no queries throws")
    {
        REQUIRE_FALSE(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 0}).HasValue());
    }
    SECTION("A query past the end of the pool throws")
    {
        Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdWriteTimestamp(pool, 2) != ErrorCode::Success);
        REQUIRE(command_buffer.CmdResetQueryPool(pool, 1, 2) != ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A timestamp naming more than one stage throws")
    {
        Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2}));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdResetQueryPool(pool) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::VertexShader |
                                                                        Forge::PipelineStageBits::FragmentShader) != ErrorCode::Success);
        REQUIRE(command_buffer.CmdWriteTimestamp(pool, 0, Forge::PipelineStageBits::None) != ErrorCode::Success);
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
        REQUIRE_FALSE(set.GetBindingDescriptorType(1).HasValue());
    }
    SECTION("A buffer written the short way reaches the shader")
    {
        const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, output) == ErrorCode::Success);

        const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(fixture.device, k_descriptor_source,
                                                                       {.entry_point = "main_descriptor", .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
                               }) == ErrorCode::Success);

        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            REQUIRE(values[i] == static_cast<u32>(i) + 7);
        }
    }
    SECTION("A texture written the short way records without complaint")
    {
        const Forge::Texture texture = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = PixelFormat::R8G8B8A8_UNORM,
                                                      .width = 4,
                                                      .height = 4,
                                                      .usage = Forge::TextureUsageBits::Sampled}));
        const Forge::Sampler sampler = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.max_anisotropy = 1.0f}));

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(2, texture, sampler) == ErrorCode::Success);
    }
    SECTION("A range past the end of the buffer throws")
    {
        const Forge::Buffer small = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                  {.size = 256, .usage = Forge::BufferUsageBits::StorageBuffer}));
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, small, 128, 256) != ErrorCode::Success);
        REQUIRE(set.Update(0, small, 0, 0) != ErrorCode::Success);
    }
    SECTION("A moved set carries what its layout declared")
    {
        // The pattern every per-frame resource in the sample uses: declare empty, assign over it. A set
        // whose binding types stayed behind would reject the very bindings its layout has.
        Forge::DescriptorSet set;
        set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.IsValid());
        REQUIRE(ForgeTest::Unwrap(set.GetBindingDescriptorType(0)) == Forge::DescriptorType::StorageBuffer);

        const Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                   {.size = 256, .usage = Forge::BufferUsageBits::StorageBuffer}));
        REQUIRE(set.Update(0, buffer) == ErrorCode::Success);

        const Forge::DescriptorSet moved(std::move(set));
        REQUIRE(ForgeTest::Unwrap(moved.GetBindingDescriptorType(0)) == Forge::DescriptorType::StorageBuffer);
        // The source is empty afterwards, so it knows about no binding at all.
        REQUIRE_FALSE(set.IsValid());
        REQUIRE_FALSE(set.GetBindingDescriptorType(0).HasValue());
    }
    SECTION("Writing a binding the layout does not have throws")
    {
        const Forge::Buffer buffer = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device,
                                   {.size = 256, .usage = Forge::BufferUsageBits::StorageBuffer}));
        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(3, buffer) != ErrorCode::Success);
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
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                                 .clear_value = Vector4f{1.0f, 0.0f, 1.0f, 1.0f}}}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        // Left in TransferSource rather than the ShaderReadOnly this defaults to: that layout needs the
        // Sampled usage, and this texture is an attachment nothing ever samples.
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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
    SECTION("A depth attachment that names no texture throws")
    {
        // What the old convention expressed as "no depth". Now that absent says it, a present attachment
        // pointing at nothing is a filled-in desc somebody forgot to finish. The colour attachment is
        // transitioned first so that the throw is the depth one and not the layout check on the colour.
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{
            .render_area_extent = {k_side, k_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}},
            .depth_attachment = Forge::RenderingAttachmentDesc{}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) != ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A colour clear value on a depth attachment throws")
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
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) != ErrorCode::Success);

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
    SECTION("A depth clear value on a colour attachment throws")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{
            .render_area_extent = {k_side, k_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{
                .texture = color, .clear_value = Forge::DepthStencilClearValue{.depth = 1.0f, .stencil = 0}}}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) != ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("An attachment whose texture was never transitioned throws")
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
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) != ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
    }
    SECTION("A colour attachment in a layout meant for something else throws")
    {
        // TransferSource is a layout this texture legitimately reaches - ReadBackTexture leaves it there -
        // so this is the plausible-but-wrong case rather than the unconfigured one above.
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE(command_buffer.CmdTransition(color, Forge::ImageLayout::TransferSource) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{.render_area_extent = {k_side, k_side},
                                                  .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) != ErrorCode::Success);
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

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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
        Forge::Texture color = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::ColorAttachment |
                                                       Forge::TextureUsageBits::TransferSource}));

        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{.color_write_mask = mask});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                                 .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}}}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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
        const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
        const Opal::DynamicArray<Forge::PhysicalDevice> devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
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
        const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
        const Opal::DynamicArray<Forge::PhysicalDevice> devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
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

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        // Left in TransferSource:
        REQUIRE(Forge::ReadBackTexture(forge.device, GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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
    SECTION("An 8-bit index buffer on a device without the feature throws")
    {
        // The index type is a plain enum value in a core call, so nothing but this check stands between a
        // device that never enabled the extension and an index type it does not accept.
        ForgeFixture plain;
        const Opal::DynamicArray<u8> index_bytes =
            ToIndexBytes(k_half_indices, static_cast<i32>(std::size(k_half_indices)), IndexSize::uint8);
        const Forge::Buffer indices = ForgeTest::Unwrap(Forge::Buffer::Create(
            plain.device, {.size = index_bytes.GetSize(), .usage = Forge::BufferUsageBits::IndexBuffer}, index_bytes));
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(plain.device, plain.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        REQUIRE_FALSE(plain.device.GetFeatures().index_type_uint8);
        REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint8) != ErrorCode::Success);
        // The two widths that need no extension still bind on the same command buffer.
        REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint16) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdBindIndexBuffer(indices, 0, IndexSize::uint32) == ErrorCode::Success);
        REQUIRE(command_buffer.End() == ErrorCode::Success);
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

    auto make_write_pipeline = [&](const Forge::Shader& writer)
    {
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = writer;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        return ForgeTest::Unwrap(Forge::Pipeline::Create(halves.forge.device, pipeline_desc));
    };

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
        const Forge::Pipeline write_pipeline = make_write_pipeline(write_draws);
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
        const Forge::Pipeline write_pipeline = make_write_pipeline(write_draws);
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
    SECTION("An indirect indexed draw follows the indices and the vertex offset it was given")
    {
        const Opal::DynamicArray<u8> index_bytes = ToIndexBytes(k_half_indices + 6, 6, IndexSize::uint32);
        const Forge::Buffer indices = ForgeTest::Unwrap(Forge::Buffer::Create(halves.forge.device,
                                    {.size = index_bytes.GetSize(), .usage = Forge::BufferUsageBits::IndexBuffer}, index_bytes));
        const Forge::Pipeline write_pipeline = make_write_pipeline(write_indexed_draw);
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

    auto make_pipeline = [&](const Forge::Shader& shader)
    {
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    };
    const Forge::Pipeline write_pipeline = make_pipeline(write_dispatch);
    const Forge::Pipeline compute_pipeline = make_pipeline(compute_shader);

    // Device-only, so the group counts cannot have come from the host.
    const Forge::Buffer group_counts = ForgeTest::Unwrap(Forge::Buffer::Create(
        fixture.device,
        {.size = sizeof(Forge::DispatchIndirectCommand),
         .usage = Forge::BufferUsageBits::IndirectBuffer | Forge::BufferUsageBits::StorageBuffer | Forge::BufferUsageBits::TransferSource,
         .host_access = Forge::HostAccess::None,
         .use_device_address = true}));

    auto make_output = [&]
    {
        Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                              .usage = Forge::BufferUsageBits::StorageBuffer,
                                              .host_access = Forge::HostAccess::Random,
                                              .use_device_address = true}));
        // Wiped first, so nothing left behind can pass for a dispatch that ran.
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);
        return output;
    };
    const Forge::Buffer indirect_output = make_output();
    const Forge::Buffer direct_output = make_output();

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

    auto make_desc = [&]()
    {
        Forge::GraphicsPipelineDesc desc;
        desc.vertex_shader = vertex_shader;
        desc.fragment_shader = fragment_shader;
        desc.rasterizer.cull_mode = Face::None;
        desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        REQUIRE(desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
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
        REQUIRE_FALSE(Forge::Pipeline::Create(fixture.device, desc).HasValue());
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
    SECTION("Dynamic state a feature gates throws without the feature")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        // The fixture device asks for neither, so both of these are the guard rather than the driver.
        REQUIRE_FALSE(fixture.device.GetFeatures().wide_lines);
        REQUIRE_FALSE(fixture.device.GetFeatures().depth_bias_clamp);
        REQUIRE(command_buffer.CmdSetLineWidth(4.0f) != ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetDepthBias(1.0f, 0.5f) != ErrorCode::Success);
        // The value every device draws, and a bias with no clamp, need no feature.
        REQUIRE(command_buffer.CmdSetLineWidth(1.0f) == ErrorCode::Success);
        REQUIRE(command_buffer.CmdSetDepthBias(1.0f) == ErrorCode::Success);
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
 * A constructor that throws leaves no object behind for a destructor to clean up, so anything it created
 * before the throw is the constructor's own to release. The pipeline layout is created first and the checks
 * that reject a description come after it, which made every rejected pipeline a leaked layout. Nothing
 * noticed: the layer only names an object that outlived its device, and that report arrives at
 * vkDestroyDevice, after the assertion at the end of a case has already passed.
 */
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
        REQUIRE_FALSE(Forge::Pipeline::Create(fixture.device, desc).HasValue());
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
        REQUIRE_FALSE(Forge::Pipeline::Create(fixture.device, desc).HasValue());
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

        Forge::Texture color = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_format,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::ColorAttachment |
                                                       Forge::TextureUsageBits::TransferSource}));
        Forge::GraphicsPipelineDesc pipeline_desc;
        pipeline_desc.vertex_shader = vertex_shader;
        pipeline_desc.fragment_shader = fragment_shader;
        pipeline_desc.rasterizer.cull_mode = Face::None;
        pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
        REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        pipeline_desc.color_attachment_formats.PushBack(k_format);
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

        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                                 .clear_value = Vector4f{0.0f, 0.0f, 0.0f, 1.0f}}}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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
    SECTION("A name no stage declares throws")
    {
        // The case Vulkan ignores in silence when the value is keyed by number, which is why it is keyed
        // by name here.
        const Forge::SpecializationConstant wrong[] = {{.name = "RED_LEVELL", .value = 1}};
        REQUIRE_FALSE(draw_specialized({wrong, 1}).HasValue());
    }
    SECTION("A value of the wrong type throws")
    {
        const Forge::SpecializationConstant wrong[] = {{.name = "RED_LEVEL", .value = 1.0f}};
        REQUIRE_FALSE(draw_specialized({wrong, 1}).HasValue());
    }
    SECTION("One constant given a value twice throws")
    {
        // Two map entries with the same constantID, which the specification does not allow within one
        // VkSpecializationInfo - and which reads as nothing worse than a repeated name from out here.
        const Forge::SpecializationConstant twice[] = {{.name = "RED_LEVEL", .value = 32},
                                                       {.name = "RED_LEVEL", .value = 64}};
        REQUIRE_FALSE(draw_specialized({twice, 2}).HasValue());
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

        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(k_element_count / 64) == ErrorCode::Success);
                               }) == ErrorCode::Success);
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
        REQUIRE_FALSE(Forge::VertexInputDesc::FromShader(fragment_shader).HasValue());
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
    SECTION("A location the shader reads that nothing feeds throws")
    {
        Forge::VertexInputDesc incomplete;
        incomplete.AddBinding(0, 28);
        REQUIRE(incomplete.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        REQUIRE(incomplete.AddAttribute(0, 1, PixelFormat::R32G32B32_SFLOAT, 8) == ErrorCode::Success);
        REQUIRE_FALSE(build_pipeline(incomplete, good_ranges).HasValue());
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
    SECTION("An attribute of the wrong numeric class throws")
    {
        // Location 2 is a uint2 in the shader; a float attribute of the same width is not the same thing.
        Forge::VertexInputDesc wrong_class;
        wrong_class.AddBinding(0, 28);
        REQUIRE(wrong_class.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
        REQUIRE(wrong_class.AddAttribute(0, 1, PixelFormat::R32G32B32_SFLOAT, 8) == ErrorCode::Success);
        REQUIRE(wrong_class.AddAttribute(0, 2, PixelFormat::R32G32_SFLOAT, 20) == ErrorCode::Success);
        REQUIRE_FALSE(build_pipeline(wrong_class, good_ranges).HasValue());
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
    SECTION("A push constant range that stops short of what the shader reads throws")
    {
        const Forge::VertexInputDesc derived = ForgeTest::Unwrap(Forge::VertexInputDesc::FromShader(vertex_shader));
        const Forge::PushConstantRange too_small{.shader_stages = ShaderTypeBits::Vertex, .offset = 0, .size = 4};
        REQUIRE_FALSE(build_pipeline(derived, {&too_small, 1}).HasValue());
    }
    SECTION("No push constant range at all, for a shader that reads one, throws")
    {
        // The likeliest way to get this wrong, and the reason the check does not wait for a range to exist.
        const Forge::VertexInputDesc derived = ForgeTest::Unwrap(Forge::VertexInputDesc::FromShader(vertex_shader));
        REQUIRE_FALSE(build_pipeline(derived, {}).HasValue());
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
    SECTION("A binding declared as the wrong kind throws")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE(desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE_FALSE(Forge::DescriptorSetLayout::Create(fixture.device, desc).HasValue());
    }
    SECTION("A binding whose stages leave out the one that reads it throws")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Vertex) == ErrorCode::Success);
        REQUIRE(desc.AddBinding(1, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE_FALSE(Forge::DescriptorSetLayout::Create(fixture.device, desc).HasValue());
    }
    SECTION("A binding the shaders read that the layout omits throws")
    {
        Forge::DescriptorSetLayoutDesc desc = make_desc();
        REQUIRE(desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Fragment) == ErrorCode::Success);
        REQUIRE_FALSE(Forge::DescriptorSetLayout::Create(fixture.device, desc).HasValue());
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
        REQUIRE_FALSE(set.GetBindingIndex("third_texture").HasValue());
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
        REQUIRE_FALSE(set.GetBindingIndex("first_texture").HasValue());
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
    return ForgeTest::Unwrap(Forge::Pipeline::Create(device, pipeline_desc));
}

/** A wiped buffer of k_lifetime_elements, so nothing left in it can pass for a dispatch that ran. */
constexpr i32 k_lifetime_elements = 64;

Forge::Buffer MakeWipedOutput(const Forge::Device& device)
{
    Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(device, {.size = k_lifetime_elements * sizeof(u32),
                                  .usage = Forge::BufferUsageBits::StorageBuffer,
                                  .host_access = Forge::HostAccess::Random,
                                  .use_device_address = true}));
    const Opal::DynamicArray<u8> zeros(k_lifetime_elements * sizeof(u32));
    REQUIRE(output.Update(zeros) == ErrorCode::Success);
    return output;
}

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
    Opal::DynamicArray<u32> values(k_lifetime_elements);
    REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
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
                              const Forge::Buffer buffer = MakeWipedOutput(device);
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
    Opal::StringUtf8 report;
    for (const Forge::DebugMessage& message : context.GetDebugMessages())
    {
        report += message.text;
        report += Opal::StringUtf8("\n");
    }
    INFO(*report);
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation).GetValue() == 0);
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
                              // Read back through the mapped pointer, which is the member 1.4 found the move
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
                              Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
                              REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0,
                                                     Forge::ImageLayout::TransferSource) == ErrorCode::Success);
                              REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferSource);
                          });

    // A sampler holds nothing but its device and its handle, so writing it into a descriptor is the cheapest
    // thing that uses both. Sampling through one is 3.18.
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
                              const Forge::Buffer output = MakeWipedOutput(fixture.device);
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
                              const Forge::Buffer output = MakeWipedOutput(fixture.device);
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
    // which is the half of 1.7 nothing runs otherwise.
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
                              const Forge::Buffer output = MakeWipedOutput(fixture.device);
                              REQUIRE(set.Update(0, output) == ErrorCode::Success);
                              REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                                                     [&](Forge::CommandBuffer& command_buffer)
                                                     {
                                                         REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                         REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                                                         REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                                                     }) == ErrorCode::Success);
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
                              // The type is a member of its own, and every host side call throws on a binary
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

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), destination, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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
    SECTION("A block compressed format throws rather than answering as if it were packed texels")
    {
        // The size of a compressed level is a count of blocks, not of texels, and answering with the texel
        // arithmetic would hand a readback a buffer of the wrong size and no reason to notice.
        const Forge::TextureDesc desc{.format = PixelFormat::BC1_RGBA_UNORM_BLOCK, .width = 8, .height = 8, .mip_level_count = 2};
        REQUIRE_FALSE(Forge::GetMipLevelSize(desc, 0).HasValue());
    }
    SECTION("A level the texture does not have throws")
    {
        const Forge::TextureDesc desc{.format = PixelFormat::R8G8B8A8_UNORM, .width = 8, .height = 8, .mip_level_count = 2};
        REQUIRE_FALSE(Forge::GetMipLevelSize(desc, 2).HasValue());
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
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = compute_shader;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

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

        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(copied.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            INFO("element " << i);
            REQUIRE(values[i] == static_cast<u32>(i) + 1000);
        }
        // The texture barrier in the same batch moved the texture, which is what makes this readback legal.
        REQUIRE(ForgeTest::Unwrap(texture.GetCurrentLayout()) == Forge::ImageLayout::TransferSource);
        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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
            Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
            REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), textures[i], pixels, 0,
                                   Forge::ImageLayout::TransferSource) == ErrorCode::Success);
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
        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), texture, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        REQUIRE(CountMismatches(expected, pixels) == 0);
    };

    SECTION("ToShaderRead moves a sampled texture without losing it")
    {
        run_preset(
            Forge::TextureUsageBits::Sampled, [](Forge::Texture& texture)
            { return ForgeTest::Unwrap(Forge::TextureBarrier::ToShaderRead(texture)); }, Forge::ImageLayout::ShaderReadOnly);
    }
    SECTION("ToTransferSource moves a texture into the layout a copy reads from")
    {
        run_preset(Forge::TextureUsageBits::Sampled,
                   [](Forge::Texture& texture) { return ForgeTest::Unwrap(Forge::TextureBarrier::ToTransferSource(texture)); },
                   Forge::ImageLayout::TransferSource);
    }
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
        // checking: the dispatch throws rather than picking whichever preset is closest. General used to be
        // the example here and stopped being one when 3.18 gave it a preset of its own.
        REQUIRE_FALSE(
            Forge::TextureBarrier::To(texture, Forge::ImageLayout::Undefined, Forge::ImageLayout::DepthStencilReadOnly).HasValue());
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
        // Rendering with one is 3.16; what this says is that the preset picks the depth aspect off the
        // format rather than the colour aspect a colour texture would have given it.
        REQUIRE(ForgeTest::Unwrap(depth.GetCurrentLayout()) == Forge::ImageLayout::DepthStencilAttachment);
    }
    SECTION("BufferBarrier::ReadThenWrite orders a read before the write that follows it")
    {
        // The write is a transfer over the same range a compute shader just read, so without the barrier the
        // copy would be free to land before the dispatch finished reading.
        const Forge::Shader compute_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = compute_shader;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

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
    const Forge::GraphicsContext context = ForgeTest::Unwrap(Forge::GraphicsContext::Create(ForgeTest::TestContextDesc()));
    Opal::DynamicArray<Forge::PhysicalDevice> physical_devices = ForgeTest::Unwrap(context.EnumeratePhysicalDevices());
    if (!physical_devices[0].IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
    {
        SKIP("This device has no swap chain extension.");
    }
    Forge::DeviceDesc device_desc = MakeHeadlessDeviceDesc();
    device_desc.extensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    Forge::Device device = ForgeTest::Unwrap(Forge::Device::Create(std::move(physical_devices[0]), context, device_desc));
    Forge::DeviceQueue& queue = ForgeTest::Unwrap(device.GetQueue(Forge::QueueFamily::Graphics));

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
    REQUIRE(context.GetDebugMessageCount(Forge::DebugMessageSeverity::Error, Forge::DebugMessageTypeBits::Validation).GetValue() == 0);
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

    auto make_wiped_output = [&]
    {
        Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                              .usage = Forge::BufferUsageBits::StorageBuffer,
                                              .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);
        return output;
    };

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
        const Forge::Buffer output = make_wiped_output();
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
        const Forge::Buffer output = make_wiped_output();
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

/** A colour target these cases render into and read straight back out of. */
Forge::Texture MakeColorTarget(const Forge::Device& device, i32 side, PixelFormat format = PixelFormat::R8G8B8A8_UNORM)
{
    return ForgeTest::Unwrap(Forge::Texture::Create(device, {.format = format,
                                   .width = static_cast<u32>(side),
                                   .height = static_cast<u32>(side),
                                   .usage = Forge::TextureUsageBits::ColorAttachment |
                                            Forge::TextureUsageBits::TransferSource}));
}

/**
 * Clear the target to opaque red, run one recorded draw over it and hand back the pixels. The viewport and
 * the scissor are set to the whole target first, so a case that wants something else says so by setting it
 * again inside the draw.
 */
template <typename Record>
Opal::DynamicArray<u8> RenderRaster(ForgeFixture& fixture, Forge::Texture& color, i32 side, Record&& record)
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
                                                                             .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}}}};
                    REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {static_cast<f32>(side), static_cast<f32>(side)}) ==
                            ErrorCode::Success);
                    REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {side, side}) == ErrorCode::Success);
                    record(command_buffer);
                    REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                }) == ErrorCode::Success);
    Opal::DynamicArray<u8> pixels(side * side * 4);
    REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
            ErrorCode::Success);
    return pixels;
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
                                                              PrimitiveTopology topology = PrimitiveTopology::Triangle)
{
    Forge::GraphicsPipelineDesc pipeline_desc;
    pipeline_desc.vertex_shader = vertex_shader;
    pipeline_desc.fragment_shader = fragment_shader;
    pipeline_desc.rasterizer = rasterizer;
    pipeline_desc.topology = topology;
    pipeline_desc.vertex_input.AddBinding(0, 2 * sizeof(f32), DataRepetition::PerVertex);
    REQUIRE(pipeline_desc.vertex_input.AddAttribute(0, 0, PixelFormat::R32G32_SFLOAT, 0) == ErrorCode::Success);
    pipeline_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
    pipeline_desc.color_attachment_formats.PushBack(format);
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
    const VkPhysicalDeviceFeatures device_features = GetFirstPhysicalDeviceFeatures();
    const bool has_wireframe = device_features.fillModeNonSolid == VK_TRUE;
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
    SECTION("A wireframe on a device without the feature throws")
    {
        // The polygon mode is a plain enum in the create info, so without this the device is handed a mode
        // it never agreed to and the validation layer is the only thing that notices.
        ForgeFixture plain({.fill_mode_non_solid = false});
        const Forge::Shader plain_vertex = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            plain.device, k_fullscreen_source, {.entry_point = "main_vertex", .cache = GetShaderCache()}));
        const Forge::Shader plain_fragment = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            plain.device, k_fullscreen_source, {.entry_point = "main_fragment", .cache = GetShaderCache()}));
        REQUIRE_FALSE(MakeRasterPipeline(plain.device, plain_vertex, plain_fragment, k_format,
                                             {.fill_mode = FillMode::Wireframe, .cull_mode = Face::None}).HasValue());
        // Solid on the same device is fine, so what threw was the fill mode and not the pipeline.
        const Forge::Pipeline solid_pipeline = ForgeTest::Unwrap(MakeRasterPipeline(plain.device, plain_vertex, plain_fragment, k_format,
                                                                  {.cull_mode = Face::None}));
        REQUIRE(solid_pipeline.IsValid());
        REQUIRE_NO_VALIDATION_ERROR(plain);
    }
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

    Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_format);
    const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_side,
                                                       [&](Forge::CommandBuffer& command_buffer)
                                                       {
                                                           REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                           REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                                                           REQUIRE(command_buffer.CmdBindVertexBuffer(instance_buffer, 1) ==
                                                                   ErrorCode::Success);
                                                           REQUIRE(command_buffer.CmdDraw(6, 4) == ErrorCode::Success);
                                                       });

    // Each quarter carries the bits of its own instance value, so an instance that read the wrong entry of
    // the second binding shows up as the wrong quarter rather than as a missing one.
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
        Forge::Texture depth = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_depth_format,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::DepthStencilAttachment |
                                                       Forge::TextureUsageBits::TransferSource}));

        REQUIRE(Forge::ImmediateSubmit(
            fixture.device, fixture.GetQueue(),
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth)) == ErrorCode::Success);
                const Forge::RenderingDesc rendering_desc{
                    .render_area_extent = {k_side, k_side},
                    .color_attachments = {Forge::RenderingAttachmentDesc{
                        .texture = color,
                        .load_operation = Forge::AttachmentLoadOperation::Clear,
                        .store_operation = Forge::AttachmentStoreOperation::Store,
                        .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}}},
                    .depth_attachment = Forge::RenderingAttachmentDesc{
                        .texture = depth,
                        .load_operation = Forge::AttachmentLoadOperation::Clear,
                        .store_operation = Forge::AttachmentStoreOperation::Store,
                        .clear_value = Forge::DepthStencilClearValue{1.0f, 0}}};
                REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}, min_depth, max_depth) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindVertexBuffer(vertices, 0) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdDraw(3) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
            }) == ErrorCode::Success);

        Opal::DynamicArray<u8> bytes(k_side * k_side * sizeof(f32));
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), depth, bytes, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        Opal::DynamicArray<f32> values(k_side * k_side);
        memcpy(values.GetData(), bytes.GetData(), bytes.GetSize());
        return values;
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
    const VkPhysicalDeviceFeatures device_features = GetFirstPhysicalDeviceFeatures();
    const bool has_depth_clamp = device_features.depthClamp == VK_TRUE;
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
    SECTION("Clamping on a device without the feature throws")
    {
        ForgeFixture plain({.depth_clamp = false});
        const Forge::Shader plain_vertex = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            plain.device, k_depth_position_source, {.entry_point = "main_depth_vertex", .cache = GetShaderCache()}));
        const Forge::Shader plain_fragment = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            plain.device, k_depth_position_source, {.entry_point = "main_depth_fragment", .cache = GetShaderCache()}));
        REQUIRE_FALSE(MakeDepthPipeline(plain.device, plain_vertex, plain_fragment, k_format, PixelFormat::Undefined, true).HasValue());
        REQUIRE_NO_VALIDATION_ERROR(plain);
    }
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

    /** What one pass leaves behind: the colour that survived and the depth beside it. */
    struct PassResult
    {
        Opal::DynamicArray<u8> pixels;
        Opal::DynamicArray<f32> depths;
    };

    /** Draw the near quad then the far one and hand back both the colour and the depth that survived. */
    auto draw_both = [&](Comparator comparator, bool depth_write, f32 clear_depth)
    {
        const Forge::Pipeline pipeline = make_pipeline(comparator, depth_write);
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        Forge::Texture depth = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_depth_format,
                                              .width = k_side,
                                              .height = k_side,
                                              .usage = Forge::TextureUsageBits::DepthStencilAttachment |
                                                       Forge::TextureUsageBits::TransferSource}));

        REQUIRE(Forge::ImmediateSubmit(
            fixture.device, fixture.GetQueue(),
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth)) == ErrorCode::Success);
                const Forge::RenderingDesc rendering_desc{
                    .render_area_extent = {k_side, k_side},
                    .color_attachments = {Forge::RenderingAttachmentDesc{
                        .texture = color,
                        .load_operation = Forge::AttachmentLoadOperation::Clear,
                        .store_operation = Forge::AttachmentStoreOperation::Store,
                        .clear_value = Vector4f{0.0f, 0.0f, 1.0f, 1.0f}}},
                    .depth_attachment = Forge::RenderingAttachmentDesc{
                        .texture = depth,
                        .load_operation = Forge::AttachmentLoadOperation::Clear,
                        .store_operation = Forge::AttachmentStoreOperation::Store,
                        // Cleared to whichever end of the range the comparator counts as furthest, so the
                        // first draw passes either way. Clearing to one under Greater would reject both.
                        .clear_value = Forge::DepthStencilClearValue{clear_depth, 0}}};
                REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
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

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        Opal::DynamicArray<u8> depth_bytes(k_side * k_side * sizeof(f32));
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), depth, depth_bytes, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        Opal::DynamicArray<f32> depths(k_side * k_side);
        memcpy(depths.GetData(), depth_bytes.GetData(), depth_bytes.GetSize());
        return PassResult{std::move(pixels), std::move(depths)};
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

    /** The pipeline that stamps a one into the stencil buffer wherever it draws, writing no colour at all. */
    const Forge::Pipeline mask_pipeline = [&]
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_color_format);
        pipeline_desc.depth_stencil.stencil_test_enabled = true;
        pipeline_desc.depth_stencil.front_stencil_comparator = Comparator::Always;
        pipeline_desc.depth_stencil.front_pass = StencilOperation::Replace;
        pipeline_desc.depth_stencil.front_reference = 1;
        pipeline_desc.depth_stencil.back_stencil_comparator = Comparator::Always;
        pipeline_desc.depth_stencil.back_pass = StencilOperation::Replace;
        pipeline_desc.depth_stencil.back_reference = 1;
        // The colour is masked off, so what the second draw finds is entirely the stencil buffer.
        pipeline_desc.color_blend_attachments[0].color_write_mask = Forge::ColorWriteMaskBits::None;
        pipeline_desc.depth_attachment_format = k_depth_stencil_format;
        pipeline_desc.stencil_attachment_format = k_depth_stencil_format;
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    }();

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
        Forge::Texture depth_stencil = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_depth_stencil_format,
                                                      .width = k_side,
                                                      .height = k_side,
                                                      .usage = Forge::TextureUsageBits::DepthStencilAttachment}));

        REQUIRE(Forge::ImmediateSubmit(
            fixture.device, fixture.GetQueue(),
            [&](Forge::CommandBuffer& command_buffer)
            {
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth_stencil)) ==
                        ErrorCode::Success);
                // One texture carries both, so the same view is named twice - Vulkan takes the two sides
                // apart even then, and each gets its own load and store.
                const Forge::RenderingAttachmentDesc depth_stencil_attachment{
                    .texture = depth_stencil,
                    .load_operation = Forge::AttachmentLoadOperation::Clear,
                    .store_operation = Forge::AttachmentStoreOperation::Store,
                    .clear_value = Forge::DepthStencilClearValue{1.0f, 0}};
                const Forge::RenderingDesc rendering_desc{
                    .render_area_extent = {k_side, k_side},
                    .color_attachments = {Forge::RenderingAttachmentDesc{
                        .texture = color,
                        .load_operation = Forge::AttachmentLoadOperation::Clear,
                        .store_operation = Forge::AttachmentStoreOperation::Store,
                        .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}}},
                    .depth_attachment = depth_stencil_attachment.Clone(),
                    .stencil_attachment = depth_stencil_attachment.Clone()};
                REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);

                REQUIRE(command_buffer.CmdBindPipeline(mask_pipeline) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdBindVertexBuffer(left_quad, 0) == ErrorCode::Success);
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

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        return pixels;
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
    SECTION("The same draw with the test satisfied everywhere covers the whole target")
    {
        // Identical geometry and identical stencil buffer; only the comparator moved. Without this the case
        // above would also pass on a device that simply dropped the second draw's right half.
        const Opal::DynamicArray<u8> pixels = run_pass(make_paint_pipeline(Comparator::Always));
        for (i32 i = 0; i < k_side * k_side; ++i)
        {
            INFO("texel " << i);
            REQUIRE(static_cast<i32>(pixels[i * 4 + 0]) == 0);
            REQUIRE(static_cast<i32>(pixels[i * 4 + 1]) == 255);
        }
    }
    SECTION("A stencil attachment that names no texture throws")
    {
        Forge::CommandBuffer command_buffer = ForgeTest::Unwrap(Forge::CommandBuffer::Create(fixture.device, fixture.GetQueue()));
        Forge::Texture color = MakeColorTarget(fixture.device, k_side, k_color_format);
        REQUIRE(command_buffer.Begin() == ErrorCode::Success);
        // Transitioned first so that the throw is the stencil one and not the layout check on the colour.
        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
        const Forge::RenderingDesc rendering_desc{
            .render_area_extent = {k_side, k_side},
            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color}},
            .stencil_attachment = Forge::RenderingAttachmentDesc{}};
        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) != ErrorCode::Success);
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
    // number below comes from a CmdSet call. The static half of the same three is 3.16's business.
    constexpr Forge::DynamicStateBits k_dynamic_stencil = Forge::DynamicStateBits::StencilCompareMask |
                                                          Forge::DynamicStateBits::StencilWriteMask |
                                                          Forge::DynamicStateBits::StencilReference;

    /** Stamps into the stencil buffer wherever it draws, writing no colour. */
    const Forge::Pipeline mask_pipeline = [&]
    {
        Forge::GraphicsPipelineDesc pipeline_desc = MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_color_format);
        pipeline_desc.depth_stencil.stencil_test_enabled = true;
        pipeline_desc.depth_stencil.front_stencil_comparator = Comparator::Always;
        pipeline_desc.depth_stencil.front_pass = StencilOperation::Replace;
        pipeline_desc.depth_stencil.back_stencil_comparator = Comparator::Always;
        pipeline_desc.depth_stencil.back_pass = StencilOperation::Replace;
        pipeline_desc.color_blend_attachments[0].color_write_mask = Forge::ColorWriteMaskBits::None;
        pipeline_desc.depth_attachment_format = k_depth_stencil_format;
        pipeline_desc.stencil_attachment_format = k_depth_stencil_format;
        pipeline_desc.dynamic_state = k_dynamic_stencil;
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    }();

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
        Forge::Texture depth_stencil = ForgeTest::Unwrap(Forge::Texture::Create(fixture.device, {.format = k_depth_stencil_format,
                                                      .width = k_side,
                                                      .height = k_side,
                                                      .usage = Forge::TextureUsageBits::DepthStencilAttachment}));

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
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToDepthStencilAttachment(depth_stencil)) ==
                        ErrorCode::Success);
                const Forge::RenderingAttachmentDesc depth_stencil_attachment{
                    .texture = depth_stencil,
                    .load_operation = Forge::AttachmentLoadOperation::Clear,
                    .store_operation = Forge::AttachmentStoreOperation::Store,
                    .clear_value = Forge::DepthStencilClearValue{1.0f, 0}};
                const Forge::RenderingDesc rendering_desc{
                    .render_area_extent = {k_side, k_side},
                    .color_attachments = {Forge::RenderingAttachmentDesc{
                        .texture = color,
                        .load_operation = Forge::AttachmentLoadOperation::Clear,
                        .store_operation = Forge::AttachmentStoreOperation::Store,
                        .clear_value = Vector4f{1.0f, 0.0f, 0.0f, 1.0f}}},
                    .depth_attachment = depth_stencil_attachment.Clone(),
                    .stencil_attachment = depth_stencil_attachment.Clone()};
                REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);

                // Comparator Always, so the compare mask decides nothing here and the write mask is what
                // picks which bits of the reference land in the buffer.
                REQUIRE(command_buffer.CmdBindPipeline(mask_pipeline) == ErrorCode::Success);
                set_stencil(command_buffer, 0xFF, mask_write_mask, mask_reference);
                REQUIRE(command_buffer.CmdBindVertexBuffer(left_quad, 0) == ErrorCode::Success);
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

        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        return pixels;
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
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_side, k_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                                 .clear_value = Vector4f{0.0f, 0.0f, 0.0f, 1.0f}}}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_side, k_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(quad, 0) == ErrorCode::Success);
                        // The destination is drawn rather than cleared to, so it is exactly the
                        // bytes the shader wrote and not a float the clear had to convert.
                        REQUIRE(command_buffer.CmdBindPipeline(opaque_pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(opaque_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(destination)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(blend_pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(blend_pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(source)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);
        Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
        return pixels;
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

    /** The fourth channel, which the sections above leave alone and the two below are entirely about. */
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

    SECTION("Source alpha over one minus source alpha is the classic blend")
    {
        const Opal::DynamicArray<u8> pixels = blend_over_destination({.blend_enabled = true,
                                                                      .src_color_factor = BlendFactor::SrcAlpha,
                                                                      .dst_color_factor = BlendFactor::InvSrcAlpha,
                                                                      .color_operation = BlendOperation::Add});
        const f32 source_alpha = static_cast<f32>(k_src[3]) / 255.0f;
        i32 expected[3] = {};
        for (i32 channel = 0; channel < 3; ++channel)
        {
            expected[channel] = expected_channel(static_cast<f32>(k_src[channel]) / 255.0f,
                                                 static_cast<f32>(k_dst[channel]) / 255.0f, source_alpha,
                                                 1.0f - source_alpha, false);
        }
        require_channels(pixels, expected);
    }
    SECTION("One and one add the two together")
    {
        const Opal::DynamicArray<u8> pixels = blend_over_destination({.blend_enabled = true,
                                                                      .src_color_factor = BlendFactor::One,
                                                                      .dst_color_factor = BlendFactor::One,
                                                                      .color_operation = BlendOperation::Add});
        i32 expected[3] = {};
        for (i32 channel = 0; channel < 3; ++channel)
        {
            expected[channel] = expected_channel(static_cast<f32>(k_src[channel]) / 255.0f,
                                                 static_cast<f32>(k_dst[channel]) / 255.0f, 1.0f, 1.0f, false);
        }
        require_channels(pixels, expected);
    }
    SECTION("A zero source factor leaves the destination alone")
    {
        // The draw still runs and still covers the target; what reaches the attachment is the factors.
        const Opal::DynamicArray<u8> pixels = blend_over_destination({.blend_enabled = true,
                                                                      .src_color_factor = BlendFactor::Zero,
                                                                      .dst_color_factor = BlendFactor::One,
                                                                      .color_operation = BlendOperation::Add});
        const i32 expected[3] = {k_dst[0], k_dst[1], k_dst[2]};
        require_channels(pixels, expected);
    }
    SECTION("The operation is read as well as the factors")
    {
        // The same two factors as the additive case, so what separates the two answers is the operation.
        const Opal::DynamicArray<u8> pixels = blend_over_destination({.blend_enabled = true,
                                                                      .src_color_factor = BlendFactor::One,
                                                                      .dst_color_factor = BlendFactor::One,
                                                                      .color_operation = BlendOperation::ReverseSubtract});
        i32 expected[3] = {};
        for (i32 channel = 0; channel < 3; ++channel)
        {
            expected[channel] = expected_channel(static_cast<f32>(k_src[channel]) / 255.0f,
                                                 static_cast<f32>(k_dst[channel]) / 255.0f, 1.0f, 1.0f, true);
        }
        require_channels(pixels, expected);
    }
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

/** Uploads bytes into one mip level of a texture and leaves it where a shader can read it. */
void UploadMip(const Forge::Device& device, Forge::DeviceQueue& queue, Forge::Texture& texture, Opal::ArrayView<const u8> pixels,
               u32 mip_level)
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
                                       REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToShaderRead(
                                                   texture, Forge::PipelineStageBits::ComputeShader)) == ErrorCode::Success);
                                   }) == ErrorCode::Success);
}

}  // namespace

TEST_CASE("Forge sampler filtering and addressing", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    ForgeFixture fixture;
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_combined_sample_source, {.entry_point = "main_sample_combined", .cache = GetShaderCache()}));

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 8) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 8) == ErrorCode::Success);
    pool_desc.max_sets = 8;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
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
    {
        const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = sizeof(Vector4f),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(sizeof(Vector4f));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, texture, sampler, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
        REQUIRE(set.Update(1, output) == ErrorCode::Success);
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
        return result;
    };

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
    SECTION("Wrapping and clamping differ at a coordinate outside the texture")
    {
        // A quarter past the right edge. Repeat wraps it back onto the left texel; clamping holds it on the
        // right one. A nearest filter, so the answer is a whole texel either way.
        const SampleParams params{.uv = {1.25f, 0.5f}};
        const Forge::Sampler repeating = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest,
                                                        .mag_filter = ImageFilter::Nearest,
                                                        .address_mode_u = ImageAddressMode::Repeat,
                                                        .address_mode_v = ImageAddressMode::Repeat}));
        const Forge::Sampler clamping = ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest,
                                                       .mag_filter = ImageFilter::Nearest,
                                                       .address_mode_u = ImageAddressMode::Clamp,
                                                       .address_mode_v = ImageAddressMode::Clamp}));

        const Vector4f wrapped = sample_with(repeating, row, params);
        INFO("wrapped rgba " << wrapped.x << " " << wrapped.y);
        REQUIRE(wrapped.x == Catch::Approx(1.0f).margin(0.01));
        REQUIRE(wrapped.y == Catch::Approx(0.0f).margin(0.01));

        const Vector4f held = sample_with(clamping, row, params);
        INFO("clamped rgba " << held.x << " " << held.y);
        REQUIRE(held.x == Catch::Approx(0.0f).margin(0.01));
        REQUIRE(held.y == Catch::Approx(1.0f).margin(0.01));
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

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, immutable_layout));
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

    Opal::DynamicArray<u8> pixels(k_side * k_side * 4);
    REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), storage, pixels, 0, Forge::ImageLayout::TransferSource) ==
            ErrorCode::Success);
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

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 4) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 4) == ErrorCode::Success);
    pool_desc.max_sets = 4;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    const Forge::Sampler nearest =
        ForgeTest::Unwrap(Forge::Sampler::Create(fixture.device, {.min_filter = ImageFilter::Nearest, .mag_filter = ImageFilter::Nearest}));

    /** Sample one texture through one shader at one direction, and hand back what came out. */
    auto sample_shape = [&](const char* source, const char* entry_point, Forge::Texture& texture, const Vector4f& direction)
    {
        const Forge::Shader shader = ForgeTest::Unwrap(
            Forge::Shader::FromSourceInMemory(fixture.device, source, {.entry_point = entry_point, .cache = GetShaderCache()}));
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(Vector4f)});
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = sizeof(Vector4f),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(sizeof(Vector4f));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, texture, nearest, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
        REQUIRE(set.Update(1, output) == ErrorCode::Success);
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute, Opal::AsBytes(direction)) ==
                                           ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        Vector4f result;
        REQUIRE(output.Read({reinterpret_cast<u8*>(&result), sizeof(result)}) == ErrorCode::Success);
        return result;
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
    SECTION("A cube view over a layer count that is not a multiple of six throws")
    {
        REQUIRE_FALSE(Forge::Texture::Create(fixture.device, {.format = k_format,
                                                          .width = 1,
                                                          .height = 1,
                                                          .array_layer_count = 4,
                                                          .usage = Forge::TextureUsageBits::Sampled,
                                                          .view_type = Forge::TextureViewType::Cube}).HasValue());
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
        const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);
        REQUIRE(set.Update(0, output) == ErrorCode::Success);
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdBindDescriptorSet(pipeline, set) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(1) == ErrorCode::Success);
                               }) == ErrorCode::Success);
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
    if (!AreQueuesAvailable(k_queues))
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
    const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = k_element_count * sizeof(u32),
                                                .usage = Forge::BufferUsageBits::StorageBuffer,
                                                .host_access = Forge::HostAccess::Random,
                                                .use_device_address = true}));
    const Opal::DynamicArray<u8> zeros(k_element_count * sizeof(u32));
    REQUIRE(output.Update(zeros) == ErrorCode::Success);

    const Forge::Shader compute_shader = ForgeTest::Unwrap(
        Forge::Shader::FromSourceInMemory(fixture.device, k_compute_source, {.entry_point = "main_compute", .cache = GetShaderCache()}));
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = compute_shader;
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
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

    Opal::DynamicArray<u32> values(k_element_count);
    REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
    for (i32 i = 0; i < k_element_count; ++i)
    {
        REQUIRE(values[i] == static_cast<u32>(i) + 1000);
    }

    SECTION("Timestamps on that family are read with its own valid bits")
    {
        const u32 family_index = compute_queue.GetQueueFamilyIndex();
        const VkQueueFamilyProperties& properties = fixture.device.GetPhysicalDevice().GetQueueFamilyProperties()[family_index];
        if (properties.timestampValidBits == 0)
        {
            // A family that can time nothing is named rather than answered with zeroes.
            REQUIRE_FALSE(
                Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 2, .queue_family = Forge::QueueFamily::AsyncCompute})
                    .HasValue());
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
    if (!AreQueuesAvailable(k_queues))
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
    if (!AreQueuesAvailable(k_queues))
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
    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = compute_shader;
    pipeline_desc.push_constant_ranges.PushBack(
        {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
    const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
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

    Opal::DynamicArray<u32> values(k_element_count);
    REQUIRE(host_visible.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
    for (i32 i = 0; i < k_element_count; ++i)
    {
        REQUIRE(values[i] == static_cast<u32>(i) + 1000);
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
 * A pipeline that covers the whole target, writes no colour, and applies `pass_operation` to the stencil
 * buffer wherever it draws. Comparator::Always, so nothing about the test decides whether the operation runs.
 */
Forge::Pipeline MakeStencilWritePipeline(const Forge::Device& device, const Forge::Shader& vertex_shader,
                                         const Forge::Shader& fragment_shader, StencilOperation pass_operation)
{
    Forge::GraphicsPipelineDesc pipeline_desc =
        MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
    pipeline_desc.depth_stencil.stencil_test_enabled = true;
    pipeline_desc.depth_stencil.front_stencil_comparator = Comparator::Always;
    pipeline_desc.depth_stencil.back_stencil_comparator = Comparator::Always;
    pipeline_desc.depth_stencil.front_pass = pass_operation;
    pipeline_desc.depth_stencil.back_pass = pass_operation;
    pipeline_desc.color_blend_attachments[0].color_write_mask = Forge::ColorWriteMaskBits::None;
    pipeline_desc.depth_attachment_format = k_table_depth_stencil_format;
    pipeline_desc.stencil_attachment_format = k_table_depth_stencil_format;
    pipeline_desc.dynamic_state = Forge::DynamicStateBits::StencilReference;
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
    return ForgeTest::Unwrap(Forge::Texture::Create(device, {.format = k_table_depth_stencil_format,
                                   .width = k_table_side,
                                   .height = k_table_side,
                                   .usage = Forge::TextureUsageBits::DepthStencilAttachment |
                                            Forge::TextureUsageBits::TransferSource}));
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

}  // namespace

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
        REQUIRE(Forge::ImmediateSubmit(
                    fixture.device, fixture.GetQueue(),
                    [&](Forge::CommandBuffer& command_buffer)
                    {
                        REQUIRE(command_buffer.CmdTextureBarrier(Forge::TextureBarrier::ToColorAttachment(color)) == ErrorCode::Success);
                        const Forge::RenderingDesc rendering_desc{
                            .render_area_extent = {k_table_side, k_table_side},
                            .color_attachments = {Forge::RenderingAttachmentDesc{.texture = color,
                                                                                 .load_operation = Forge::AttachmentLoadOperation::Clear,
                                                                                 .store_operation = Forge::AttachmentStoreOperation::Store,
                                                                                 .clear_value = dst}}};
                        REQUIRE(command_buffer.CmdBeginRendering(rendering_desc) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetViewport(Vector2f::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdSetScissor(Vector2i::Zero(), {k_table_side, k_table_side}) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdBindVertexBuffer(full_quad, 0) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Fragment, Opal::AsBytes(src)) ==
                                ErrorCode::Success);
                        REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                        REQUIRE(command_buffer.CmdEndRendering() == ErrorCode::Success);
                    }) == ErrorCode::Success);

        Opal::DynamicArray<u8> pixels(k_table_side * k_table_side * 4);
        REQUIRE(Forge::ReadBackTexture(fixture.device, fixture.GetQueue(), color, pixels, 0, Forge::ImageLayout::TransferSource) ==
                ErrorCode::Success);
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

/** Whether this machine offers the feature ImageAddressMode::MirrorOnce needs. */
bool IsMirrorClampToEdgeAvailable()
{
    static const bool available = []
    {
        try
        {
            const ForgeFixture probe({.sampler_mirror_clamp_to_edge = true});
            return true;
        }
        catch (const Opal::Exception&)
        {
            return false;
        }
    }();
    return available;
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
 * value they did not know. They throw now, which is what the rest of Forge does and what keeps an enumerator
 * added later from silently meaning something else.
 */
TEST_CASE("Forge the sampler address modes and border colours", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    const bool has_mirror_once = IsMirrorClampToEdgeAvailable();
    INFO("MIRROR_CLAMP_TO_EDGE available: " << has_mirror_once);
    ForgeFixture fixture({.sampler_mirror_clamp_to_edge = has_mirror_once});
    constexpr PixelFormat k_format = PixelFormat::R8G8B8A8_UNORM;
    constexpr i32 k_row_width = 2;

    const Forge::Shader shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
        fixture.device, k_combined_sample_source, {.entry_point = "main_sample_combined", .cache = GetShaderCache()}));

    Forge::DescriptorPoolDesc pool_desc;
    REQUIRE(pool_desc.Add(Forge::DescriptorType::CombinedImageSampler, 32) == ErrorCode::Success);
    REQUIRE(pool_desc.Add(Forge::DescriptorType::StorageBuffer, 32) == ErrorCode::Success);
    pool_desc.max_sets = 32;
    const Forge::DescriptorPool pool = ForgeTest::Unwrap(Forge::DescriptorPool::Create(fixture.device, pool_desc));

    Forge::DescriptorSetLayoutDesc layout_desc;
    REQUIRE(layout_desc.AddBinding(0, Forge::DescriptorType::CombinedImageSampler, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    REQUIRE(layout_desc.AddBinding(1, Forge::DescriptorType::StorageBuffer, 1, ShaderTypeBits::Compute) == ErrorCode::Success);
    const Forge::DescriptorSetLayout layout = ForgeTest::Unwrap(Forge::DescriptorSetLayout::Create(fixture.device, layout_desc));

    Forge::ComputePipelineDesc pipeline_desc;
    pipeline_desc.shader = shader;
    pipeline_desc.descriptor_set_layouts.PushBack(Opal::Ref<const Forge::DescriptorSetLayout>(layout));
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
        const Forge::Buffer output = ForgeTest::Unwrap(Forge::Buffer::Create(fixture.device, {.size = sizeof(Vector4f),
                                                    .usage = Forge::BufferUsageBits::StorageBuffer,
                                                    .host_access = Forge::HostAccess::Random}));
        const Opal::DynamicArray<u8> zeros(sizeof(Vector4f));
        REQUIRE(output.Update(zeros) == ErrorCode::Success);

        Forge::DescriptorSet set = ForgeTest::Unwrap(Forge::DescriptorSet::Create(pool, layout));
        REQUIRE(set.Update(0, row, sampler, Forge::ImageLayout::ShaderReadOnly) == ErrorCode::Success);
        REQUIRE(set.Update(1, output) == ErrorCode::Success);
        const SampleParams params{.uv = {u, 0.5f}};
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
        return result;
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
    SECTION("MirrorOnce without the feature throws rather than reaching the driver")
    {
        // MIRROR_CLAMP_TO_EDGE is core in Vulkan 1.2 but still a feature, and a sampler naming it on a device
        // that did not enable it is undefined. Forge refused nothing here until the mode had a test.
        ForgeFixture plain_fixture;
        REQUIRE_FALSE(Forge::Sampler::Create(plain_fixture.device, {.address_mode_u = ImageAddressMode::MirrorOnce}).HasValue());
        REQUIRE_FALSE(Forge::Sampler::Create(plain_fixture.device, {.address_mode_v = ImageAddressMode::MirrorOnce}).HasValue());
        REQUIRE_FALSE(Forge::Sampler::Create(plain_fixture.device, {.address_mode_w = ImageAddressMode::MirrorOnce}).HasValue());
        // The other four modes are what every device does, so none of them is refused on the same device.
        const Forge::Sampler unaffected =
            ForgeTest::Unwrap(Forge::Sampler::Create(plain_fixture.device, {.address_mode_u = ImageAddressMode::MirrorRepeat}));
        REQUIRE(unaffected.IsValid());
        REQUIRE_NO_VALIDATION_ERROR(plain_fixture);
    }
    if (!has_mirror_once)
    {
        WARN("This device has no samplerMirrorClampToEdge, so ImageAddressMode::MirrorOnce went unchecked.");
    }
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

    /**
     * Which face this quad presents, decided by whether culling the back of it leaves anything behind. The
     * winding of a quad in clip space depends on the viewport transform as much as on the order of its
     * vertices, so it is measured rather than reasoned about - the same reason the culling case never asserts
     * which way its triangle is wound.
     */
    const bool quad_is_front_facing = [&]
    {
        Forge::GraphicsPipelineDesc pipeline_desc =
            MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
        pipeline_desc.rasterizer.cull_mode = Face::Back;
        pipeline_desc.rasterizer.front_face = WindingOrder::CCW;
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));

        Forge::Texture color = MakeColorTarget(fixture.device, k_table_side, k_table_color_format);
        const Opal::DynamicArray<u8> pixels = RenderRaster(fixture, color, k_table_side,
                                                           [&](Forge::CommandBuffer& command_buffer)
                                                           {
                                                               REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdBindVertexBuffer(full_quad, 0) ==
                                                                       ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdPushConstants(
                                                                   pipeline, ShaderTypeBits::Fragment,
                                                                   Opal::AsBytes(paint_color)) == ErrorCode::Success);
                                                               REQUIRE(command_buffer.CmdDraw(6) == ErrorCode::Success);
                                                           });
        return CountCovered(pixels, k_table_side) == k_table_side * k_table_side;
    }();
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
    const Forge::Pipeline stamp_pipeline = [&]
    {
        Forge::GraphicsPipelineDesc pipeline_desc =
            MakePushedColorPipelineDesc(vertex_shader, fragment_shader, k_table_color_format);
        pipeline_desc.depth_stencil.stencil_test_enabled = true;
        pipeline_desc.depth_stencil.front_stencil_comparator = Comparator::Always;
        pipeline_desc.depth_stencil.back_stencil_comparator = Comparator::Always;
        pipeline_desc.depth_stencil.front_pass = StencilOperation::Replace;
        pipeline_desc.depth_stencil.back_pass = StencilOperation::Replace;
        pipeline_desc.color_blend_attachments[0].color_write_mask = Forge::ColorWriteMaskBits::None;
        pipeline_desc.depth_attachment_format = k_table_depth_stencil_format;
        pipeline_desc.stencil_attachment_format = k_table_depth_stencil_format;
        pipeline_desc.dynamic_state = k_dynamic_stencil;
        return ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
    }();

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
        Opal::StringUtf8 path(RNDR_CORE_ASSETS_DIR "/../build/forge-test-scratch");
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

/** The SPIR-V Slang produces for one entry point, which is what a caller of FromSpirv* would have on hand. */
Opal::DynamicArray<u8> CompileToSpirv(const char* source, const char* entry_point)
{
    ShaderCompiler compiler;
    compiler.LoadModule(Opal::StringUtf8(source), ShaderOutputFormat::SpirV);
    CompileResult result = compiler.CompileEntryPoint(Opal::StringUtf8(entry_point));
    return std::move(result.code);
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
        Forge::ComputePipelineDesc pipeline_desc;
        pipeline_desc.shader = shader;
        pipeline_desc.push_constant_ranges.PushBack(
            {.shader_stages = ShaderTypeBits::Compute, .offset = 0, .size = sizeof(VkDeviceAddress)});
        const Forge::Pipeline pipeline = ForgeTest::Unwrap(Forge::Pipeline::Create(fixture.device, pipeline_desc));
        const VkDeviceAddress output_address = output.GetNativeDeviceAddress();
        REQUIRE(Forge::ImmediateSubmit(fixture.device, fixture.GetQueue(),
                               [&](Forge::CommandBuffer& command_buffer)
                               {
                                   REQUIRE(command_buffer.CmdBindPipeline(pipeline) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdPushConstants(pipeline, ShaderTypeBits::Compute,
                                                                   Opal::AsBytes(output_address)) == ErrorCode::Success);
                                   REQUIRE(command_buffer.CmdDispatch(k_element_count / k_group_size) == ErrorCode::Success);
                               }) == ErrorCode::Success);
        Opal::DynamicArray<u32> values(k_element_count);
        REQUIRE(output.Read({reinterpret_cast<u8*>(values.GetData()), values.GetSize() * sizeof(u32)}) == ErrorCode::Success);
        for (i32 i = 0; i < k_element_count; ++i)
        {
            REQUIRE(values[i] == static_cast<u32>(i) + 1000);
        }
    }
    SECTION("An entry point the module does not hold throws")
    {
        // Reflection finds no entry point of that name and there is nothing to take a stage from, so this
        // cannot be let through: the shader would be created with a zeroed stage and refused much later by a
        // pipeline, naming the wrong thing.
        REQUIRE_FALSE(Forge::Shader::FromSpirvInMemory(fixture.device, spirv_view, {.entry_point = "no_such_entry"}).HasValue());
        // And the default entry point of "main", which Slang did not emit under that name here.
        REQUIRE_FALSE(Forge::Shader::FromSpirvInMemory(fixture.device, spirv_view).HasValue());
    }
    SECTION("A blob that is not SPIR-V throws")
    {
        const Opal::DynamicArray<u8> junk = MakeBytes(256, 17);
        REQUIRE_FALSE(Forge::Shader::FromSpirvInMemory(fixture.device, {junk.GetData(), junk.GetSize()},
                                                           {.entry_point = "main_compute"}).HasValue());
        // A real module cut short is the other way this arrives: the magic number is right and nothing past
        // it is.
        const Opal::ArrayView<const u8> half_a_module(spirv.GetData(), spirv.GetSize() / 2);
        REQUIRE_FALSE(Forge::Shader::FromSpirvInMemory(fixture.device, half_a_module, {.entry_point = "main_compute"}).HasValue());
        // And an empty one, which is what a file that was there and held nothing looks like from in here.
        REQUIRE_FALSE(Forge::Shader::FromSpirvInMemory(fixture.device, {}, {.entry_point = "main_compute"}).HasValue());
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
    SECTION("A file that is not there, and one whose contents are not SPIR-V, both throw")
    {
        REQUIRE_FALSE(Forge::Shader::FromSpirvFile(fixture.device, TestScratchPath("no-such-file.spv"),
                                                       {.entry_point = "main_compute"}).HasValue());

        const Opal::StringUtf8 junk_path = TestScratchPath("not-spirv.spv");
        const Opal::DynamicArray<u8> junk = MakeBytes(256, 23);
        REQUIRE(Opal::WriteBytesToFile(junk_path, {junk.GetData(), junk.GetSize()}) == Opal::ErrorCode::Success);
        REQUIRE_FALSE(Forge::Shader::FromSpirvFile(fixture.device, junk_path, {.entry_point = "main_compute"}).HasValue());

        // A file that exists and is empty takes the same path out as one that is missing, which is the whole
        // of what ReadEntireFile can tell the two apart by.
        const Opal::StringUtf8 empty_path = TestScratchPath("empty.spv");
        REQUIRE(Opal::WriteBytesToFile(empty_path, {}) == Opal::ErrorCode::Success);
        REQUIRE_FALSE(Forge::Shader::FromSpirvFile(fixture.device, empty_path, {.entry_point = "main_compute"}).HasValue());
    }
    SECTION("A file with the right bytes and the wrong entry point throws")
    {
        const Opal::StringUtf8 path = TestScratchPath("from-spirv-file.spv");
        REQUIRE(Opal::WriteBytesToFile(path, spirv_view) == Opal::ErrorCode::Success);
        REQUIRE_FALSE(Forge::Shader::FromSpirvFile(fixture.device, path, {.entry_point = "no_such_entry"}).HasValue());
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
    SECTION("A range that does not fit in the pool throws before anything is read")
    {
        const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 4}));
        Opal::InPlaceArray<u64, 8> ticks;
        // A first_query at or past the end, and a count that runs off it. Both are the caller's mistake and
        // neither is something the driver would report.
        REQUIRE_FALSE(pool.TryGetResults({ticks.GetData(), 1}, 4).HasValue());
        REQUIRE_FALSE(pool.TryGetResults({ticks.GetData(), 5}).HasValue());
        REQUIRE_FALSE(pool.TryGetResults({ticks.GetData(), 3}, 2).HasValue());
        REQUIRE(pool.GetResults({ticks.GetData(), 5}) != ErrorCode::Success);
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
    SECTION("ResolveQueryRange throws on every range that does not fit")
    {
        const Forge::TimestampQueryPool pool = ForgeTest::Unwrap(Forge::TimestampQueryPool::Create(fixture.device, {.query_count = 4}));
        // First query at the end and past it.
        REQUIRE_FALSE(pool.ResolveQueryRange(4, 1, "Reading").HasValue());
        REQUIRE_FALSE(pool.ResolveQueryRange(5, 1, "Reading").HasValue());
        REQUIRE_FALSE(pool.ResolveQueryRange(4, Forge::k_all_queries, "Reading").HasValue());
        // A count of zero, which resolves to nothing and is never what a caller meant.
        REQUIRE_FALSE(pool.ResolveQueryRange(0, 0, "Reading").HasValue());
        // And counts that run off the end from a first query that is itself fine.
        REQUIRE_FALSE(pool.ResolveQueryRange(0, 5, "Reading").HasValue());
        REQUIRE_FALSE(pool.ResolveQueryRange(3, 2, "Reading").HasValue());
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
    // without it throws out of every call, which is what this asks about before asserting anything.
    const Opal::StringUtf8 complete_path = WriteScratchTextFile("triangle.obj", k_complete_obj);
    {
        Forge::Mesh probe;
        try
        {
            REQUIRE(Forge::LoadMesh(complete_path, probe) == ErrorCode::Success);
        }
        catch (const Opal::Exception&)
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
    SECTION("A mesh with no texture coordinates throws")
    {
        const Opal::StringUtf8 path = WriteScratchTextFile("no-uvs.obj", k_obj_without_uvs);
        Forge::Mesh mesh;
        REQUIRE(Forge::LoadMesh(path, mesh) != ErrorCode::Success);
    }
    SECTION("A mesh with no normals but with a face loads, because assimp generates them from the face")
    {
        // LoadMesh asks for aiProcess_GenSmoothNormals, so a file with faces and no `vn` has normals by the
        // time the null check runs. aiProcess_GenUVCoords is not the counterpart it looks like - it converts
        // a mapping that is already there rather than inventing one - which is why the UV section above does
        // throw where this one does not.
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
    SECTION("A mesh with no face to generate normals from throws")
    {
        // What the section above does not reach, and the reason the null normal check is not dead code:
        // assimp generates normals from faces, so a mesh that has none arrives with none. Three ways to get
        // there - a point cloud, a line mesh, and a triangle so degenerate that aiProcess_FindDegenerates
        // takes it away - and all three land on the same throw.
        const char* const names[] = {"points-only.obj", "lines-only.obj", "degenerate-face.obj"};
        const char* const contents[] = {k_obj_points_only, k_obj_lines_only, k_obj_degenerate_face};
        for (i32 i = 0; i < 3; ++i)
        {
            INFO(names[i]);
            const Opal::StringUtf8 path = WriteScratchTextFile(names[i], contents[i]);
            Forge::Mesh mesh;
            REQUIRE(Forge::LoadMesh(path, mesh) != ErrorCode::Success);
        }
    }
    SECTION("A file assimp cannot read throws")
    {
        // An extension assimp knows, holding something that is not a mesh.
        const Opal::StringUtf8 junk_path = WriteScratchTextFile("not-a-mesh.obj", "this file is not a mesh at all\n");
        Forge::Mesh mesh;
        REQUIRE(Forge::LoadMesh(junk_path, mesh) != ErrorCode::Success);
        // A file that is not there at all, and one with an extension assimp has no reader for.
        REQUIRE(Forge::LoadMesh(TestScratchPath("no-such-mesh.obj"), mesh) != ErrorCode::Success);
        const Opal::StringUtf8 unknown_path = WriteScratchTextFile("unknown-format.qqq", "nothing here either\n");
        REQUIRE(Forge::LoadMesh(unknown_path, mesh) != ErrorCode::Success);
    }
    SECTION("A mesh that failed to load leaves the one handed in as it was")
    {
        // LoadMesh writes into a mesh the caller owns, so what it does to that mesh on the way out of a
        // failure is the caller's problem. It throws before touching anything, which is what lets a caller
        // keep the mesh it already had.
        Forge::Mesh mesh;
        REQUIRE(Forge::LoadMesh(complete_path, mesh) == ErrorCode::Success);
        const u32 loaded_count = mesh.vertex_count;
        REQUIRE(Forge::LoadMesh(TestScratchPath("no-such-mesh.obj"), mesh) != ErrorCode::Success);
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

/** Whether this machine can create a device with the mesh shader stage on it. */
bool IsMeshShaderAvailable()
{
    static const bool available = []
    {
        try
        {
            const ForgeFixture probe({.mesh_shader = true});
            return true;
        }
        catch (const Opal::Exception&)
        {
            return false;
        }
    }();
    return available;
}

}  // namespace

/**
 * CmdDrawMeshTasks, which until now had only ever been watched throwing. 3.3 wrote it and could not do more
 * than that, since nothing enabled VK_EXT_mesh_shader; 3.6 enabled it and nothing came back to draw through
 * it. This is that draw.
 */
TEST_CASE("Forge a mesh shader draw", "[forge]")
{
    if (!IsForgeAvailable())
    {
        SKIP("No Vulkan device on this machine.");
    }
    if (!IsMeshShaderAvailable())
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
    SECTION("A pipeline with both a vertex and a mesh stage, or a task stage with neither, throws")
    {
        const Forge::Shader vertex_shader = ForgeTest::Unwrap(Forge::Shader::FromSourceInMemory(
            fixture.device, k_pushed_color_source, {.entry_point = "main_color_vertex", .cache = GetShaderCache()}));
        Forge::GraphicsPipelineDesc both_desc;
        both_desc.vertex_shader = vertex_shader;
        both_desc.mesh_shader = mesh_shader;
        both_desc.fragment_shader = fragment_shader;
        both_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        both_desc.color_attachment_formats.PushBack(k_format);
        REQUIRE_FALSE(Forge::Pipeline::Create(fixture.device, both_desc).HasValue());

        // A task stage in front of nothing, which Vulkan has no shape for.
        Forge::GraphicsPipelineDesc task_only_desc;
        task_only_desc.task_shader = mesh_shader;
        task_only_desc.fragment_shader = fragment_shader;
        task_only_desc.color_blend_attachments.PushBack(Forge::ColorBlendDesc{});
        task_only_desc.color_attachment_formats.PushBack(k_format);
        REQUIRE_FALSE(Forge::Pipeline::Create(fixture.device, task_only_desc).HasValue());
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
    REQUIRE(command_buffer.CmdDrawMeshTasks(1) != ErrorCode::Success);
    REQUIRE(command_buffer.End() == ErrorCode::Success);
    REQUIRE_NO_VALIDATION_ERROR(fixture);
}
