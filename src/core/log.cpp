#include "rndr/core/log.h"

#include <cstdarg>

#include "spdlog/async.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

std::unique_ptr<rndr::StdAsyncLogger> rndr::StdAsyncLogger::s_Logger;

static std::shared_ptr<spdlog::logger> s_SpdLogger;

rndr::StdAsyncLogger* rndr::StdAsyncLogger::Get()
{
    if (!s_Logger)
    {
        s_Logger = std::make_unique<StdAsyncLogger>();
    }

    return s_Logger.get();
}

void rndr::StdAsyncLogger::Init()
{
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_level(spdlog::level::trace);
    spdlog::set_pattern("[%H:%M:%S:%e][%P][%t][%^%l%$][%@] %v");

    spdlog::init_thread_pool(8192, 1);  // queue with 8k items and 1 backing thread.
    s_SpdLogger = spdlog::create_async<spdlog::sinks::stdout_color_sink_mt>("async_stdout_logger");
}

void rndr::StdAsyncLogger::ShutDown() {}

void rndr::StdAsyncLogger::Log(const char* File, int Line, const char* Function, rndr::LogLevel LogLevel, const char* format, ...)
{
    const int MESSAGE_SIZE = 4096;
    char message[MESSAGE_SIZE] = {};

    spdlog::source_loc SourceInfo(File, Line, Function);

    va_list args;
    va_start(args, format);
    vsprintf_s(message, MESSAGE_SIZE, format, args);
    va_end(args);

    switch (LogLevel)
    {
        case rndr::LogLevel::Error:
        {
            s_SpdLogger->log(SourceInfo, spdlog::level::level_enum::err, message);
            break;
        }
        case rndr::LogLevel::Warning:
        {
            s_SpdLogger->log(SourceInfo, spdlog::level::level_enum::warn, message);            
            break;
        }
        case rndr::LogLevel::Debug:
        {
            s_SpdLogger->log(SourceInfo, spdlog::level::level_enum::debug, message);
            break;
        }
        case rndr::LogLevel::Info:
        {
            s_SpdLogger->log(SourceInfo, spdlog::level::level_enum::info, message);
            break;
        }
        case rndr::LogLevel::Trace:
        {
            s_SpdLogger->log(SourceInfo, spdlog::level::level_enum::trace, message);
            break;
        }
    }
}