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

    void Init(bool bMultithread = false);
    void ShutDown();

    void Log(const char* File, int Line, const char* Function, rndr::LogLevel LogLevel, const char* Format, ...);

private:
    static std::unique_ptr<StdAsyncLogger> s_Logger;
};

}  // namespace rndr

#define RNDR_LOG_ERROR(format, ...) \
    rndr::StdAsyncLogger::Get()->Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Error, format, __VA_ARGS__)
#define RNDR_LOG_WARNING(format, ...) \
    rndr::StdAsyncLogger::Get()->Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Warning, format, __VA_ARGS__)
#define RNDR_LOG_DEBUG(format, ...) \
    rndr::StdAsyncLogger::Get()->Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Debug, format, __VA_ARGS__)
#define RNDR_LOG_INFO(format, ...) rndr::StdAsyncLogger::Get()->Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Info, format, __VA_ARGS__)
#define RNDR_LOG_TRACE(format, ...) \
    rndr::StdAsyncLogger::Get()->Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Trace, format, __VA_ARGS__)

#if defined RNDR_DEBUG
#define RNDR_LOG_ERROR_OR_ASSERT(format, ...) \
    RNDR_LOG_ERROR(format, __VA_ARGS__);      \
    assert(false)
#else
#define RNDR_LOG_ERROR_OR_ASSERT(format, ...) RNDR_LOG_ERROR(format, __VA_ARGS__)
#endif
