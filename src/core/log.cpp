#include "rndr/core/log.h"

#include <cstdarg>

#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "rndr/core/rndrcontext.h"

rndr::StdAsyncLogger::StdAsyncLogger()
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%H:%M:%S:%e][%P][%t][%^%l%$][%@] %v");

    spdlog::init_thread_pool(8192, 1);  // queue with 8k items and 1 backing thread.

    std::shared_ptr<spdlog::logger> spl = spdlog::create<spdlog::sinks::stdout_color_sink_st>("stdout_logger");
    m_ImplLogger = spl.get();
}

rndr::StdAsyncLogger::~StdAsyncLogger()
{
    spdlog::drop(m_ImplLogger->name());
}

void rndr::StdAsyncLogger::Log(const char* File, int Line, const char* Function, rndr::LogLevel LogLevel, const char* Message)
{
    spdlog::source_loc SourceInfo(File, Line, Function);

    switch (LogLevel)
    {
        case rndr::LogLevel::Error:
        {
            m_ImplLogger->log(SourceInfo, spdlog::level::level_enum::err, Message);
            break;
        }
        case rndr::LogLevel::Warning:
        {
            m_ImplLogger->log(SourceInfo, spdlog::level::level_enum::warn, Message);
            break;
        }
        case rndr::LogLevel::Debug:
        {
            m_ImplLogger->log(SourceInfo, spdlog::level::level_enum::debug, Message);
            break;
        }
        case rndr::LogLevel::Info:
        {
            m_ImplLogger->log(SourceInfo, spdlog::level::level_enum::info, Message);
            break;
        }
        case rndr::LogLevel::Trace:
        {
            m_ImplLogger->log(SourceInfo, spdlog::level::level_enum::trace, Message);
            break;
        }
    }
}

void rndr::Log(const char* File, int Line, const char* Function, rndr::LogLevel LogLevel, const char* Format, ...)
{
    constexpr int MESSAGE_SIZE = 4096;
    char Message[MESSAGE_SIZE] = {};

    va_list Args;
    va_start(Args, Format);
    vsprintf_s(Message, MESSAGE_SIZE, Format, Args);
    va_end(Args);

    Logger* L = GRndrContext->GetLogger();
    if (L)
    {
        L->Log(File, Line, Function, LogLevel, Message);
    }
}
