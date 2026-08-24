#pragma once

#include "opal/container/expected.h"
#include "opal/container/string.h"

#include "rndr/error-codes.hpp"
#include "rndr/types.hpp"

namespace Rndr
{
namespace Canvas
{

/**
 * GPU timestamp query. Records the moment the GPU reaches this point in the command stream, letting
 * you measure how long GPU work actually took instead of how long it took the CPU to submit it.
 *
 * A single query is one point in time. Measuring a range takes two queries -- one recorded before
 * the work and one after -- and the elapsed time is the difference between their results.
 *
 * The result is not ready the moment the query is recorded; it becomes available only once the GPU
 * has executed that point in the command stream. Reading it with GetResult() blocks until then,
 * which stalls the pipeline. To avoid the stall, poll with IsResultAvailable() / TryGetResult() and
 * read the timings a frame or two later, keeping one pair of queries per frame in flight.
 *
 * Like all OpenGL query objects, a query belongs to the context that created it and is NOT shared
 * with other contexts.
 *
 * Typical usage:
 * @code
 *   auto start = Canvas::TimestampQuery::Create("FrameStart").GetValue();
 *   auto end = Canvas::TimestampQuery::Create("FrameEnd").GetValue();
 *
 *   list.WriteTimestamp(start);
 *   list.Draw(mesh, brush);
 *   list.WriteTimestamp(end);
 *   list.Execute();
 *
 *   // Next frame, once the GPU has caught up.
 *   if (end.IsResultAvailable())
 *   {
 *       const f64 gpu_ms = Canvas::GetElapsedMilliseconds(start, end).GetValue();
 *   }
 * @endcode
 */
class TimestampQuery
{
public:
    TimestampQuery() = default;

    /**
     * Create a GPU timestamp query.
     * @param name Debug name for GPU debugging tools.
     * @return The query, or ErrorCode::GraphicsAPIError when the query object could not be created. The
     *         reason is logged at error level.
     */
    [[nodiscard]] static Opal::Expected<TimestampQuery, ErrorCode> Create(Opal::StringUtf8 name = {});

    ~TimestampQuery();

    TimestampQuery(const TimestampQuery&) = delete;
    TimestampQuery& operator=(const TimestampQuery&) = delete;
    TimestampQuery(TimestampQuery&& other) noexcept;
    TimestampQuery& operator=(TimestampQuery&& other) noexcept;

    void Destroy();

    /**
     * Record a timestamp at the current point in the command stream. Recording again overwrites the
     * previous result, so a query is reusable across frames.
     * @return ErrorCode::Success, or ErrorCode::InvalidArgument for an invalid query.
     */
    [[nodiscard]] ErrorCode Record();

    /**
     * @return True if the GPU has reached the recorded point and the result can be read without
     *         blocking. False if nothing was recorded yet or the GPU is still behind.
     */
    [[nodiscard]] bool IsResultAvailable() const;

    /**
     * Read the recorded timestamp, blocking until the GPU has produced it.
     * @return GPU timestamp in nanoseconds - the origin is implementation defined, so only differences
     *         between timestamps are meaningful - or ErrorCode::InvalidArgument when the query is invalid
     *         or was never recorded.
     */
    [[nodiscard]] Opal::Expected<u64, ErrorCode> GetResult() const;

    /**
     * Read the recorded timestamp only if it is already available.
     * @param out_result Set to the GPU timestamp in nanoseconds when this returns true, untouched
     *                   otherwise.
     * @return True if the result was read, false if it is not ready yet, or ErrorCode::InvalidArgument
     *         when the query is invalid or was never recorded.
     */
    [[nodiscard]] Opal::Expected<bool, ErrorCode> TryGetResult(u64& out_result) const;

    /** @return True if a timestamp was recorded into this query at least once. */
    [[nodiscard]] bool IsRecorded() const;

    [[nodiscard]] const Opal::StringUtf8& GetName() const;
    [[nodiscard]] u32 GetNativeHandle() const;
    [[nodiscard]] bool IsValid() const;

private:
    u32 m_handle = 0;
    bool m_recorded = false;
    Opal::StringUtf8 m_name;
};

/**
 * Time between two recorded timestamps. Blocks until both results are available.
 * @param start Query recorded before the measured work.
 * @param end Query recorded after the measured work.
 * @return Elapsed nanoseconds - 0 if @p end was recorded before @p start - or
 *         ErrorCode::InvalidArgument when either query is invalid or was never recorded.
 */
[[nodiscard]] Opal::Expected<u64, ErrorCode> GetElapsedNanoseconds(const TimestampQuery& start, const TimestampQuery& end);

/**
 * Time between two recorded timestamps. Blocks until both results are available.
 * @param start Query recorded before the measured work.
 * @param end Query recorded after the measured work.
 * @return Elapsed milliseconds - 0 if @p end was recorded before @p start - or
 *         ErrorCode::InvalidArgument when either query is invalid or was never recorded.
 */
[[nodiscard]] Opal::Expected<f64, ErrorCode> GetElapsedMilliseconds(const TimestampQuery& start, const TimestampQuery& end);

}  // namespace Canvas
}  // namespace Rndr
