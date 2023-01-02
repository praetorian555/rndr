#include "rndr/profiling/cputracer.h"

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdio>

#ifdef RNDR_SPDLOG
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#endif  // RNDR_SPDLOG

#include "Windows.h"

std::unique_ptr<rndr::CpuTracer> rndr::CpuTracer::s_Tracer;

#ifdef RNDR_SPDLOG
static std::shared_ptr<spdlog::logger> s_SpdLogger;
#endif  // RNDR_SPDLOG

rndr::CpuTracer* rndr::CpuTracer::Get()
{
    if (!s_Tracer)
    {
        s_Tracer.reset(new CpuTracer{});
    }

    return s_Tracer.get();
}

void rndr::CpuTracer::Init()
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    tm time;
    localtime_s(&time, &in_time_t);
    ss << std::put_time(&time, "%d-%m-%Y-%Hh%Mm%Ss");
    const std::string OutputName = "cputrace/cputrace-" + ss.str() + ".log";

#ifdef RNDR_SPDLOG
    s_SpdLogger = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>("async_cputrace_logger",
                                                                          OutputName);
    s_SpdLogger->set_pattern("%v");
    s_SpdLogger->info("[");
#endif  // RNDR_SPDLOG
}

void rndr::CpuTracer::ShutDown()
{
    constexpr int StackStringSize = 4 * 1024;
    std::array<char, StackStringSize> Trace;
    const auto Timestamp = std::chrono::high_resolution_clock::now();
    const int64_t StartUS =
        std::chrono::duration_cast<std::chrono::microseconds>(Timestamp.time_since_epoch()).count();
    const uint32_t ThreadId =
        GetCurrentThreadId();  // TODO(mkostic): Hide this behind platform-agnostic API
    const int64_t DurationUS = 0;
    sprintf_s(Trace.data(), StackStringSize,
            "{\"name\":\"%s\", \"cat\":\"\", \"ph\":\"X\", \"ts\": %I64d, \"dur\": %I64d, "
            "\"pid\": "
            "0, \"tid\": %u}",
            "", StartUS, DurationUS, ThreadId);

#ifdef RNDR_SPDLOG
    s_SpdLogger->info("{}", Trace.data());
    s_SpdLogger->info("]");
#endif  // RNDR_SPDLOG
}

void rndr::CpuTracer::AddTrace(const std::string& Name,
                               int64_t StartMicroSeconds,
                               int64_t EndMicroSeconds)
{
    const int64_t Duration = StartMicroSeconds - EndMicroSeconds;
    const uint32_t ThreadId =
        GetCurrentThreadId();  // TODO(mkostic): Hide this behind platform-agnostic API
    char Trace[4196] = {};
    sprintf_s(Trace,
            "{\"name\":\"%s\", \"cat\":\"\", \"ph\":\"X\", \"ts\": %I64d, \"dur\": %I64d, "
            "\"pid\": "
            "0, \"tid\": %u},",
            Name.c_str(), StartMicroSeconds, Duration, ThreadId);

#ifdef RNDR_SPDLOG
    s_SpdLogger->info("{}", Trace);
#endif  // RNDR_SPDLOG
}

rndr::CpuTrace::CpuTrace(const std::string& Name) : m_Name(Name)
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

    CpuTracer::Get()->AddTrace(m_Name, m_StartUS, EndUS);
}
