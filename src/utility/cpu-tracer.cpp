#include "rndr/utility/cpu-tracer.h"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>

#ifdef PLATFORM_WINDOWS
#include "Windows.h"
#endif

#ifdef RNDR_SPDLOG
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#endif  // RNDR_SPDLOG

#include "rndr/core/stack-array.h"
#include "rndr/core/time.h"

namespace
{
uint32_t GetThreadId()
{
#ifdef PLATFORM_WINDOWS
    return GetCurrentThreadId();
#else
    return 0;
#endif
}
}  // namespace

namespace Rndr
{
struct CpuTracerData
{
#ifdef RNDR_SPDLOG
    std::shared_ptr<spdlog::logger> logger;
#endif  // RNDR_SPDLOG
};
}  // namespace Rndr

Rndr::ScopePtr<Rndr::CpuTracerData> Rndr::CpuTracer::g_data;

bool Rndr::CpuTracer::Init()
{
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);

    std::stringstream buffer;
    tm now_tm;
    localtime_s(&now_tm, &now_time);
    buffer << std::put_time(&now_tm, "%d-%m-%Y-%Hh%Mm%Ss");
    const String output_name = "cputrace/cputrace-" + buffer.str() + ".log";

    g_data = RNDR_MAKE_SCOPED(CpuTracerData);
#ifdef RNDR_SPDLOG
    g_data->logger =
        spdlog::create_async<spdlog::sinks::basic_file_sink_mt>("async_cputrace_logger",
                                                                output_name);
    g_data->logger->set_pattern("%v");
    g_data->logger->info("[");
#endif  // Rndr_SPDLOG

    return true;
}

bool Rndr::CpuTracer::Destroy()
{
    using namespace std::chrono;
    constexpr int k_stack_string_size = 4 * 1024;
    StackArray<char, k_stack_string_size> trace;
    const Timestamp start_us = GetTimestamp();
    const uint32_t thread_id = GetThreadId();
    const int64_t duration_us = 0;
    sprintf_s(trace.data(),
              k_stack_string_size,
              "{\"name\":\"%s\", \"cat\":\"\", \"ph\":\"X\", \"ts\": %I64d, \"dur\": %I64d, "
              "\"pid\": "
              "0, \"tid\": %u}",
              "",
              start_us,
              duration_us,
              thread_id);

#ifdef RNDR_SPDLOG
    g_data->logger->info("{}", trace.data());
    g_data->logger->info("]");
#endif  // Rndr_SPDLOG
    return true;
}

void Rndr::CpuTracer::AddTrace(const String& name, int64_t start_us, int64_t end_us)
{
    const int64_t duration = end_us - start_us;
    const uint32_t thread_id = GetThreadId();
    constexpr int k_trace_size = 4 * 1024;
    StackArray<char, k_trace_size> trace;
    sprintf_s(trace.data(),
              k_trace_size,
              "{\"name\":\"%s\", \"cat\":\"\", \"ph\":\"X\", \"ts\": %I64d, \"dur\": %I64d, "
              "\"pid\": "
              "0, \"tid\": %u},",
              name.c_str(),
              start_us,
              duration,
              thread_id);

#ifdef RNDR_SPDLOG
    g_data->logger->info("{}", trace.data());
#endif  // Rndr_SPDLOG
}

Rndr::CpuTraceScoped::CpuTraceScoped(String name)
    : m_name(std::move(name)), m_start_us(GetTimestamp())
{
}

Rndr::CpuTraceScoped::~CpuTraceScoped()
{
    const Timestamp end_us = GetTimestamp();
    CpuTracer::AddTrace(m_name, m_start_us, end_us);
}

Rndr::CpuTrace::CpuTrace(String name) : m_name(std::move(name)), m_start_us(GetTimestamp()) {}

void Rndr::CpuTrace::End() const
{
    const Timestamp end_us = GetTimestamp();
    CpuTracer::AddTrace(m_name, m_start_us, end_us);
}
