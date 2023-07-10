#pragma once

#if RNDR_TRACER

#include <memory>

#include "rndr/core/base.h"
#include "rndr/core/scope-ptr.h"
#include "rndr/core/string.h"
#include "rndr/core/time.h"

namespace Rndr
{

/**
 * Singleton class that is used to log duration of pieces of code on CPU side.
 */
class CpuTracer
{
public:
    static bool Init();
    static bool Destroy();

    /**
     * Add a trace to the log.
     * @param name Name of the trace.
     * @param start_us Start time in microseconds.
     * @param end_us End time in microseconds.
     */
    static void AddTrace(const String& name, int64_t start_us, int64_t end_us);

private:
    CpuTracer() = default;  // Singleton

    static ScopePtr<struct CpuTracerData> g_data;
};

/**
 * RAII class that is used to log duration of pieces of code on CPU side.
 */
class CpuTraceScoped
{
public:
    explicit CpuTraceScoped(String name);
    ~CpuTraceScoped();

private:
    String m_name;
    Timestamp m_start_us = 0;
};

/**
 * Helper class that is used to add a trace to the log. The time is counted from the creation of the
 * object to the call of End().
 */
class CpuTrace
{
public:
    explicit CpuTrace(String name);
    void End() const;

private:
    String m_name;
    Timestamp m_start_us = 0;
};

}  // namespace Rndr

/**
 * Macro that creates a CpuTraceScoped object with the given name.
 */
#define RNDR_TRACE_SCOPED(name) const Rndr::CpuTraceScoped __scoped_trace_##__LINE__(#name)

/**
 * Macro that creates a CpuTrace object with the given name.
 */
#define RNDR_TRACE_START(name) const Rndr::CpuTrace __trace_##name(#name)

/**
 * Macro that ends a CpuTrace object.
 */
#define RNDR_TRACE_END(name) __trace_##name.End()

#else

#define RNDR_TRACE_SCOPED(name)
#define RNDR_TRACE_START(name)
#define RNDR_TRACE_END(name)

#endif // RNDR_TRACER
