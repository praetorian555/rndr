#include "rndr/canvas/timestamp-query.hpp"

#include "glad/glad.h"

#include "rndr/exception.hpp"
#include "rndr/log.hpp"
#include "rndr/trace.hpp"

Rndr::Canvas::TimestampQuery::TimestampQuery(Opal::StringUtf8 name) : m_name(std::move(name))
{
    RNDR_CPU_EVENT_SCOPED("Canvas::TimestampQuery::TimestampQuery");

    glCreateQueries(GL_TIMESTAMP, 1, &m_handle);
    if (m_handle == 0)
    {
        throw GraphicsAPIException(0, "Failed to create GL timestamp query!");
    }

    if (!m_name.IsEmpty())
    {
        glObjectLabel(GL_QUERY, m_handle, static_cast<GLsizei>(m_name.GetSize()), m_name.GetData());
        RNDR_LOG_INFO("Created timestamp query '{}' with native id {}", *m_name, m_handle);
    }
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

void Rndr::Canvas::TimestampQuery::Record()
{
    if (m_handle == 0)
    {
        throw GraphicsAPIException(0, "Cannot record into an invalid timestamp query!");
    }

    glQueryCounter(m_handle, GL_TIMESTAMP);
    m_recorded = true;
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

Rndr::u64 Rndr::Canvas::TimestampQuery::GetResult() const
{
    RNDR_CPU_EVENT_SCOPED("Canvas::TimestampQuery::GetResult");

    if (m_handle == 0)
    {
        throw GraphicsAPIException(0, "Cannot read an invalid timestamp query!");
    }
    if (!m_recorded)
    {
        throw GraphicsAPIException(0, "Cannot read a timestamp query that was never recorded!");
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
    return static_cast<u64>(result);
}

bool Rndr::Canvas::TimestampQuery::TryGetResult(u64& out_result) const
{
    if (!IsResultAvailable())
    {
        return false;
    }

    GLuint64 result = 0;
    glGetQueryObjectui64v(m_handle, GL_QUERY_RESULT, &result);
    out_result = static_cast<u64>(result);
    return true;
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

Rndr::u64 Rndr::Canvas::GetElapsedNanoseconds(const TimestampQuery& start, const TimestampQuery& end)
{
    const u64 start_ns = start.GetResult();
    const u64 end_ns = end.GetResult();
    return end_ns > start_ns ? end_ns - start_ns : 0;
}

Rndr::f64 Rndr::Canvas::GetElapsedMilliseconds(const TimestampQuery& start, const TimestampQuery& end)
{
    constexpr f64 k_nanoseconds_per_millisecond = 1'000'000.0;
    return static_cast<f64>(GetElapsedNanoseconds(start, end)) / k_nanoseconds_per_millisecond;
}
