#pragma once

#include "rndr/core/base.h"

namespace rndr
{

enum class LogLevel
{
    Error,
    Warning,
    Debug,
    Info,
    Trace
};

class StdAsyncLogger
{
public:
    static StdAsyncLogger* Get();

    void Init();
    void ShutDown();

    void Log(rndr::LogLevel LogLevel, const char* Format, ...);

private:
    static std::unique_ptr<StdAsyncLogger> s_Logger;
};

}  // namespace rndr

#define RNDR_LOG_ERROR(format, ...) \
    rndr::StdAsyncLogger::Get()->Log(rndr::LogLevel::Error, format, __VA_ARGS__)
#define RNDR_LOG_WARNING(format, ...) \
    rndr::StdAsyncLogger::Get()->Log(rndr::LogLevel::Warning, format, __VA_ARGS__)
#define RNDR_LOG_DEBUG(format, ...) \
    rndr::StdAsyncLogger::Get()->Log(rndr::LogLevel::Debug, format, __VA_ARGS__)
#define RNDR_LOG_INFO(format, ...) \
    rndr::StdAsyncLogger::Get()->Log(rndr::LogLevel::Info, format, __VA_ARGS__)
#define RNDR_LOG_TRACE(format, ...) \
    rndr::StdAsyncLogger::Get()->Log(rndr::LogLevel::Trace, format, __VA_ARGS__)