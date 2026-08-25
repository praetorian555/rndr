#pragma once

#include "volk.h"

#include "opal/container/array-view.h"
#include "opal/container/expected.h"
#include "opal/container/ref.h"

#include "rndr/forge/device.hpp"
#include "rndr/forge/forward.hpp"
#include "rndr/forge/types.hpp"
#include "rndr/types.hpp"

namespace Rndr::Forge
{

struct TimestampQueryPoolDesc
{
    /** How many timestamps fit. Two per measured range, and one pool per frame in flight. */
    u32 query_count = 2;

    /**
     * Family the timestamps will be written on. Timestamp support is per family - a transfer only queue
     * often has none - and the resolution differs between them, so the pool asks at creation rather than
     * handing back zeroes long afterwards.
     */
    QueueFamily queue_family = QueueFamily::Graphics;
};

/**
 * A pool of GPU timestamps. CommandBuffer::CmdWriteTimestamp writes the tick counter into one query, and
 * the difference between two of them is how long the device spent between those two points - which is what
 * the CPU side of a frame cannot tell you, since it measures how long submitting took rather than how long
 * the work did.
 *
 * Three things about it are easy to get wrong.
 *
 * **A pool holds undefined values until it is reset.** CommandBuffer::CmdResetQueryPool, or Reset() on the
 * host where DeviceFeatures::host_query_reset is on, has to run before the first write and before every
 * reuse. Writing into a query that was not reset is undefined behaviour, not a failure anything reports.
 *
 * **A result is not there when the command buffer is submitted**, only when the device has run that far.
 * Blocking on it in the frame that recorded it throws away the frames in flight. Keep one pool per frame in
 * flight, write into the pool of this frame, and read the pool whose fence has already been waited on - the
 * measurement lags by the number of frames in flight and costs nothing.
 *
 * **A timestamp is a marker, not a scope timer.** See CommandBuffer::CmdWriteTimestamp and the timestamp
 * section of docs/forge.md for what a pair of them actually measures; the stage each one names is what
 * decides it.
 *
 * Ticks belong to the timeline of one queue family, so a tick from a graphics queue and one from an async
 * compute queue are not comparable. The ticks this class hands out are already masked to the bits the family
 * actually writes, and the elapsed helpers already apply the resolution the device reports.
 */
class TimestampQueryPool
{
public:
    TimestampQueryPool() = default;
    ~TimestampQueryPool();

    /**
     * @param desc How many queries, and the queue family whose timeline they belong to.
     * @return The pool, ErrorCode::InvalidArgument for a pool of no queries or a family the device was not
     *         created with, ErrorCode::FeatureNotSupported when that family writes no timestamp bits, or
     *         whatever the failing creation maps to.
     */
    [[nodiscard]] static Opal::Expected<TimestampQueryPool, ErrorCode> Create(const Device& device,
                                                                              const TimestampQueryPoolDesc& desc = {});

    TimestampQueryPool(const TimestampQueryPool&) = delete;
    TimestampQueryPool& operator=(const TimestampQueryPool&) = delete;

    TimestampQueryPool(TimestampQueryPool&& other) noexcept;
    TimestampQueryPool& operator=(TimestampQueryPool&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const { return m_query_pool != VK_NULL_HANDLE; }
    [[nodiscard]] VkQueryPool GetNativeQueryPool() const { return m_query_pool; }
    [[nodiscard]] const TimestampQueryPoolDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] u32 GetQueryCount() const { return m_desc.query_count; }

    /** Nanoseconds one tick of this device stands for, which is the floor on what a measurement can resolve. */
    [[nodiscard]] f32 GetTimestampPeriod() const { return m_timestamp_period; }

    /**
     * Put a range of the pool back into the state a write needs, from the host rather than from a command
     * buffer. Needs DeviceFeatures::host_query_reset and is refused when the device was created without it;
     * CommandBuffer::CmdResetQueryPool works either way.
     *
     * Resetting from the host is only safe when nothing on the device is using the pool, which for a pool
     * written every frame means after the fence of that frame.
     *
     * @param first_query First query to reset.
     * @param query_count How many to reset. k_all_queries is the rest of the pool past first_query.
     * @return ErrorCode::Success, ErrorCode::OutOfBounds when the range does not fit, or
     *         ErrorCode::InvalidArgument without the feature.
     */
    [[nodiscard]] ErrorCode Reset(u32 first_query = 0, u32 query_count = k_all_queries) const;

    /**
     * Read raw ticks, blocking until the device has written every one of them. A query that was never
     * written blocks forever, so this is for a range the command buffer definitely filled.
     * @param out_results Filled with one tick per query. Its size decides how many are read.
     * @param first_query First query to read.
     * @return ErrorCode::Success, ErrorCode::OutOfBounds when the range does not fit, or whatever the failing
     *         read maps to.
     */
    [[nodiscard]] ErrorCode GetResults(Opal::ArrayView<u64> out_results, u32 first_query = 0) const;

    /**
     * Read raw ticks only if the device has already written all of them.
     * @param out_results Filled with one tick per query when this returns true. Its contents are unspecified
     *                    when it returns false, since the driver is free to write into the range either way.
     * @param first_query First query to read.
     * @return True when every query in the range was available, false when any was not - which is an outcome
     *         rather than a failure - or the code the range check or the read reported.
     */
    [[nodiscard]] Opal::Expected<bool, ErrorCode> TryGetResults(Opal::ArrayView<u64> out_results, u32 first_query = 0) const;

    /**
     * Milliseconds between two written timestamps, blocking until both are there.
     * @param start_query Query written before the measured work.
     * @param end_query Query written after it.
     * @return Elapsed milliseconds - zero when end_query does not hold a later tick than start_query, which
     *         is what a wrapped counter looks like - or the code the range check or the read reported.
     */
    [[nodiscard]] Opal::Expected<f64, ErrorCode> GetElapsedMilliseconds(u32 start_query, u32 end_query) const;

    /**
     * Milliseconds between two written timestamps, only if both are already there. This is what a frame loop
     * calls, since it never stalls on the device.
     * @param start_query Query written before the measured work.
     * @param end_query Query written after it.
     * @param out_milliseconds Set when this returns true, untouched otherwise.
     * @return True when both queries were available, or the code the range check or the read reported.
     */
    [[nodiscard]] Opal::Expected<bool, ErrorCode> TryGetElapsedMilliseconds(u32 start_query, u32 end_query, f64& out_milliseconds) const;

    /**
     * Turn a first_query and a count that may be k_all_queries into a concrete count. Public because
     * CommandBuffer::CmdResetQueryPool and CmdWriteTimestamp make the same check, and one range check beats
     * three.
     * @param first_query First query of the range.
     * @param query_count How many, or k_all_queries for the rest of the pool.
     * @param what What the caller is doing, so the log line names it. Reads as "Reading 2 queries from index 3...".
     * @return How many queries the range covers, or ErrorCode::OutOfBounds when it does not fit in the pool.
     */
    [[nodiscard]] Opal::Expected<u32, ErrorCode> ResolveQueryRange(u32 first_query, u32 query_count, const char* what) const;

private:
    Opal::Ref<const Device> m_device;
    VkQueryPool m_query_pool = VK_NULL_HANDLE;
    TimestampQueryPoolDesc m_desc;
    /** Nanoseconds per tick, from VkPhysicalDeviceLimits::timestampPeriod. */
    f32 m_timestamp_period = 1.0f;
    /** The low bits of a tick this family writes. The rest are undefined and have to be masked away. */
    u64 m_valid_bits_mask = 0;
};

}  // namespace Rndr::Forge
