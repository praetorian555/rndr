#include "log/log.h"

#include <cstdarg>

#include "spdlog/spdlog.h"

void Log::Init(const Config& config)
{
    if (config.EnableDebug)
    {
        spdlog::set_level(spdlog::level::debug);
    }

    if (config.EnableTrace)
    {
        spdlog::set_level(spdlog::level::trace);
    }
}

void Log::ShutDown() {}

void Log::Log(Log::Level logLevel, const char* format, ...)
{
    const int MESSAGE_SIZE = 4096;
    char message[MESSAGE_SIZE];

    va_list ap;
    va_start(ap, format);
    sprintf_s(message, MESSAGE_SIZE, format, ap);
    va_end(ap);

    switch (logLevel)
    {
        case Log::Level::Error:
        {
            spdlog::error("{}", message);
            break;
        }
        case Log::Level::Warning:
        {
            spdlog::warn("{}", message);

            break;
        }
        case Log::Level::Debug:
        {
            spdlog::debug("{}", message);
            break;
        }
        case Log::Level::Info:
        {
            spdlog::info("{}", message);
            break;
        }
        case Log::Level::Trace:
        {
            spdlog::trace("{}", message);
            break;
        }
    }
}