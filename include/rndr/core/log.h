#pragma once

#include "rndr/core/base.h"

#ifdef RNDR_SPDLOG
namespace spdlog
{
class logger;
}
#endif  // RNDR_SPDLOG

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

class Logger
{
public:
    Logger() = default;
    virtual ~Logger() = default;

    Logger(const Logger& Other) = delete;
    Logger& operator=(const Logger& Other) = delete;

    Logger(Logger&& Other) = delete;
    Logger& operator=(Logger&& Other) = delete;

    virtual void Log(const char* File,
                     int Line,
                     const char* Function,
                     rndr::LogLevel LogLevel,
                     const char* Message) = 0;
};

class StdAsyncLogger : public Logger
{
public:
    StdAsyncLogger();
    ~StdAsyncLogger() final;

    StdAsyncLogger(const StdAsyncLogger& Other) = delete;
    StdAsyncLogger& operator=(const StdAsyncLogger& Other) = delete;

    StdAsyncLogger(StdAsyncLogger&& Other) = delete;
    StdAsyncLogger& operator=(StdAsyncLogger&& Other) = delete;

    void Log(const char* File,
             int Line,
             const char* Function,
             rndr::LogLevel LogLevel,
             const char* Message) override;

private:
#ifdef RNDR_SPDLOG
    std::shared_ptr<spdlog::logger> m_ImplLogger = nullptr;
#endif  // RNDR_SPDLOG
};

void Log(const char* File,
         int Line,
         const char* Function,
         rndr::LogLevel LogLevel,
         const char* Format,
         ...);

}  // namespace rndr

#define RNDR_LOG_ERROR(format, ...) \
    rndr::Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Error, format, __VA_ARGS__)
#define RNDR_LOG_WARNING(format, ...) \
    rndr::Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Warning, format, __VA_ARGS__)
#define RNDR_LOG_DEBUG(format, ...) \
    rndr::Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Debug, format, __VA_ARGS__)
#define RNDR_LOG_INFO(format, ...) \
    rndr::Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Info, format, __VA_ARGS__)
#define RNDR_LOG_TRACE(format, ...) \
    rndr::Log(__FILE__, __LINE__, __func__, rndr::LogLevel::Trace, format, __VA_ARGS__)

#if defined RNDR_DEBUG
#define RNDR_LOG_ERROR_OR_ASSERT(format, ...) \
    RNDR_LOG_ERROR(format, __VA_ARGS__);      \
    assert(false)
#else
#define RNDR_LOG_ERROR_OR_ASSERT(format, ...) RNDR_LOG_ERROR(format, __VA_ARGS__)
#endif
