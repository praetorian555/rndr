#include "rndr/profiling/cputracer.h"

#include <iomanip>
#include <sstream>
#include <array>

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

#include "Windows.h"

std::unique_ptr<rndr::CpuTracer> rndr::CpuTracer::s_Tracer;
static std::shared_ptr<spdlog::logger> s_SpdLogger;

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
    ss << std::put_time(std::localtime(&in_time_t), "%d-%m-%Y-%Hh%Mm%Ss");
    const std::string OutputName = "cputrace/cputrace-" + ss.str() + ".log";

    s_SpdLogger = spdlog::create_async<spdlog::sinks::basic_file_sink_mt>("async_cputrace_logger",
                                                                          OutputName);

    s_SpdLogger->set_pattern("%v");

    s_SpdLogger->info("[");
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
    sprintf(Trace.data(),
            "{\"name\":\"%s\", \"cat\":\"\", \"ph\":\"X\", \"ts\": %I64d, \"dur\": %I64d, "
            "\"pid\": "
            "0, \"tid\": %u}",
            "", StartUS, DurationUS, ThreadId);

    s_SpdLogger->info("{}", Trace);
    s_SpdLogger->info("]");
}

void rndr::CpuTracer::AddTrace(const std::string& Name,
                               int64_t StartMicroSeconds,
                               int64_t EndMicroSeconds)
{
    const int64_t Duration = StartMicroSeconds - EndMicroSeconds;
    const uint32_t ThreadId =
        GetCurrentThreadId();  // TODO(mkostic): Hide this behind platform-agnostic API
    char Trace[4196] = {};
    sprintf(Trace,
            "{\"name\":\"%s\", \"cat\":\"\", \"ph\":\"X\", \"ts\": %I64d, \"dur\": %I64d, "
            "\"pid\": "
            "0, \"tid\": %u},",
            Name.c_str(), StartMicroSeconds, Duration, ThreadId);

    s_SpdLogger->info("{}", Trace);
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
