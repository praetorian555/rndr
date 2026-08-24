#include "rndr/canvas/timestamp-query.hpp"

#include "glad/glad.h"

#include "rndr/log.hpp"
#include "rndr/trace.hpp"

Opal::Expected<Rndr::Canvas::TimestampQuery, Rndr::ErrorCode> Rndr::Canvas::TimestampQuery::Create(Opal::StringUtf8 name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::TimestampQuery::Create");
    using ResultType = Opal::Expected<TimestampQuery, ErrorCode>;

    TimestampQuery query;
    query.m_name = std::move(name);

    glCreateQueries(GL_TIMESTAMP, 1, &query.m_handle);
    if (query.m_handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL timestamp query");
        return ResultType(ErrorCode::GraphicsAPIError);
    }

    if (!query.m_name.IsEmpty())
    {
        glObjectLabel(GL_QUERY, query.m_handle, static_cast<GLsizei>(query.m_name.GetSize()), query.m_name.GetData());
        RNDR_LOG_INFO("Created timestamp query '{}' with native id {}", *query.m_name, query.m_handle);
    }
    return ResultType(std::move(query));
}

Rndr::Canvas::TimestampQuery::~TimestampQuery()
{
    Destroy();
}

Rndr::Canvas::TimestampQuery::TimestampQuery(TimestampQuery&& other) noexcept
    : m_handle(other.m_handle), m_recorded(other.m_recorded), m_name(std::move(other.m_name))
{
    other.m_handle = 0;
    other.m_recorded = false;
}

Rndr::Canvas::TimestampQuery& Rndr::Canvas::TimestampQuery::operator=(TimestampQuery&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_handle = other.m_handle;
        m_recorded = other.m_recorded;
        m_name = std::move(other.m_name);
        other.m_handle = 0;
        other.m_recorded = false;
    }
    return *this;
}

void Rndr::Canvas::TimestampQuery::Destroy()
{
    if (m_handle != 0)
    {
        glDeleteQueries(1, &m_handle);
        m_handle = 0;
        m_recorded = false;
    }
}

Rndr::ErrorCode Rndr::Canvas::TimestampQuery::Record()
{
    if (m_handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Cannot record into an invalid timestamp query");
        return ErrorCode::InvalidArgument;
    }

    glQueryCounter(m_handle, GL_TIMESTAMP);
    m_recorded = true;
    return ErrorCode::Success;
}

bool Rndr::Canvas::TimestampQuery::IsResultAvailable() const
{
    if (m_handle == 0 || !m_recorded)
    {
        return false;
    }

    GLint available = GL_FALSE;
    glGetQueryObjectiv(m_handle, GL_QUERY_RESULT_AVAILABLE, &available);
    return available == GL_TRUE;
}

Opal::Expected<Rndr::u64, Rndr::ErrorCode> Rndr::Canvas::TimestampQuery::GetResult() const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::TimestampQuery::GetResult");
    using ResultType = Opal::Expected<u64, ErrorCode>;

    if (m_handle == 0)
    {
        RNDR_LOG_ERROR("Canvas: Cannot read an invalid timestamp query");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (!m_recorded)
    {
        RNDR_LOG_ERROR("Canvas: Cannot read a timestamp query that was never recorded");
        return ResultType(ErrorCode::InvalidArgument);
    }

    // The query may still be in the command stream, in which case the driver has no reason to have
    // started on it yet. Flush so the blocking read below is waiting on work the GPU is doing rather
    // than on a submission that never happens.
    if (!IsResultAvailable())
    {
        glFlush();
    }

    GLuint64 result = 0;
    glGetQueryObjectui64v(m_handle, GL_QUERY_RESULT, &result);
    return ResultType(static_cast<u64>(result));
}

Opal::Expected<bool, Rndr::ErrorCode> Rndr::Canvas::TimestampQuery::TryGetResult(u64& out_result) const
{
    using ResultType = Opal::Expected<bool, ErrorCode>;
    if (m_handle == 0 || !m_recorded)
    {
        RNDR_LOG_ERROR("Canvas: Cannot read an invalid or never recorded timestamp query");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (!IsResultAvailable())
    {
        return ResultType(false);
    }

    GLuint64 result = 0;
    glGetQueryObjectui64v(m_handle, GL_QUERY_RESULT, &result);
    out_result = static_cast<u64>(result);
    return ResultType(true);
}

bool Rndr::Canvas::TimestampQuery::IsRecorded() const
{
    return m_recorded;
}

const Opal::StringUtf8& Rndr::Canvas::TimestampQuery::GetName() const
{
    return m_name;
}

Rndr::u32 Rndr::Canvas::TimestampQuery::GetNativeHandle() const
{
    return m_handle;
}

bool Rndr::Canvas::TimestampQuery::IsValid() const
{
    return m_handle != 0;
}

Opal::Expected<Rndr::u64, Rndr::ErrorCode> Rndr::Canvas::GetElapsedNanoseconds(const TimestampQuery& start, const TimestampQuery& end)
{
    using ResultType = Opal::Expected<u64, ErrorCode>;
    ResultType start_result = start.GetResult();
    if (!start_result.HasValue())
    {
        return start_result;
    }
    ResultType end_result = end.GetResult();
    if (!end_result.HasValue())
    {
        return end_result;
    }
    const u64 start_ns = start_result.GetValue();
    const u64 end_ns = end_result.GetValue();
    return ResultType(end_ns > start_ns ? end_ns - start_ns : 0);
}

Opal::Expected<Rndr::f64, Rndr::ErrorCode> Rndr::Canvas::GetElapsedMilliseconds(const TimestampQuery& start, const TimestampQuery& end)
{
    using ResultType = Opal::Expected<f64, ErrorCode>;
    const auto elapsed_result = GetElapsedNanoseconds(start, end);
    if (!elapsed_result.HasValue())
    {
        return ResultType(elapsed_result.GetError());
    }
    constexpr f64 k_nanoseconds_per_millisecond = 1'000'000.0;
    return ResultType(static_cast<f64>(elapsed_result.GetValue()) / k_nanoseconds_per_millisecond);
}
