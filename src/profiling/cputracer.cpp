#include "rndr/profiling/cputracer.h"

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

#include "rndr/utility/stackarray.h"

static uint32_t GetThreadId()
{
#ifdef PLATFORM_WINDOWS
    return GetCurrentThreadId();
#else
    return 0;
#endif
}

void rndr::CpuTracer::Init()
{
    auto Now = std::chrono::system_clock::now();
    auto NowTime = std::chrono::system_clock::to_time_t(Now);

    std::stringstream Buffer;
    tm NowTm;
    localtime_s(&NowTm, &NowTime);
    Buffer << std::put_time(&NowTm, "%d-%m-%Y-%Hh%Mm%Ss");
    const std::string OutputName = "cputrace/cputrace-" + Buffer.str() + ".log";

#ifdef RNDR_SPDLOG
    m_Logger = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>("async_cputrace_logger",
                                                                       OutputName);
    m_Logger->set_pattern("%v");
    m_Logger->info("[");
#endif  // RNDR_SPDLOG
}

void rndr::CpuTracer::ShutDown()
{
    constexpr int kStackStringSize = 4 * 1024;
    StackArray<char, kStackStringSize> Trace;
    const auto Timestamp = std::chrono::high_resolution_clock::now();
    const int64_t StartUS =
        std::chrono::duration_cast<std::chrono::microseconds>(Timestamp.time_since_epoch()).count();
    const uint32_t ThreadId = GetThreadId();
    const int64_t DurationUS = 0;
    sprintf_s(Trace.data(), kStackStringSize,
              "{\"name\":\"%s\", \"cat\":\"\", \"ph\":\"X\", \"ts\": %I64d, \"dur\": %I64d, "
              "\"pid\": "
              "0, \"tid\": %u}",
              "", StartUS, DurationUS, ThreadId);

#ifdef RNDR_SPDLOG
    m_Logger->info("{}", Trace.data());
    m_Logger->info("]");
#endif  // RNDR_SPDLOG
}

void rndr::CpuTracer::AddTrace(const std::string& Name,
                               int64_t StartMicroSeconds,
                               int64_t EndMicroSeconds)
{
    const int64_t Duration = StartMicroSeconds - EndMicroSeconds;
    const uint32_t ThreadId = GetThreadId();
    constexpr int kTraceSize = 4 * 1024;
    StackArray<char, kTraceSize> Trace;
    sprintf_s(Trace.data(), kTraceSize,
              "{\"name\":\"%s\", \"cat\":\"\", \"ph\":\"X\", \"ts\": %I64d, \"dur\": %I64d, "
              "\"pid\": "
              "0, \"tid\": %u},",
              Name.c_str(), StartMicroSeconds, Duration, ThreadId);

#ifdef RNDR_SPDLOG
    m_Logger->info("{}", Trace.data());
#endif  // RNDR_SPDLOG
}

rndr::CpuTrace::CpuTrace(CpuTracer* Tracer, std::string Name)
    : m_Name(std::move(Name)), m_Tracer(Tracer)
{
    const auto Timestamp = std::chrono::high_resolution_clock::now();

    m_StartUS =
        std::chrono::duration_cast<std::chrono::microseconds>(Timestamp.time_since_epoch()).count();
}

rndr::CpuTrace::~CpuTrace()
{
    const auto Timestamp = std::chrono::high_resolution_clock::now();
    const int64_t EndUS =
        std::chrono::duration_cast<std::chrono::microseconds>(Timestamp.time_since_epoch()).count();

    m_Tracer->AddTrace(m_Name, m_StartUS, EndUS);
}
