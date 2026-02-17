#pragma once

#include "opal/source-location.h"

#include "rndr/application.hpp"
#include "rndr/types.hpp"

namespace Rndr
{

enum class LogLevel : u8
{
    Error,
    Warning,
    Debug,
    Info,
    Trace
};

/**
 * Logger interface.
 */
struct Logger
{
    virtual ~Logger() = default;

    virtual void Log(const Opal::SourceLocation& source_location, LogLevel log_level, const char* message) = 0;
};

/**
 * Default logger implementation.
 */
struct StdLogger : public Logger
{
    void Log(const Opal::SourceLocation& source_location, LogLevel log_level, const char* message) override;
};

/**
 * Logs a message using the user-specified logger. If no logger is provided, the default logger is
 * used.
 * @param source_location Contains information about the source location of the message.
 * @param log_level Log level of the message.
 * @param format Format string for the message.
 * @param ... Arguments for the format string.
 */
void Log(Logger& logger, const Opal::SourceLocation& source_location, Rndr::LogLevel log_level, const char* format, ...);

}  // namespace Rndr

/**
 * Helper macros to log messages.
 */
#define RNDR_LOG_ERROR(format, ...)                                                                                              \
    do                                                                                                                            \
    {                                                                                                                             \
        if (Rndr::Application* app_ = Rndr::Application::Get())                                                                  \
            Rndr::Log(app_->GetLoggerChecked(), Opal::CurrentSourceLocation(), Rndr::LogLevel::Error, format, __VA_ARGS__);       \
    } while (0)
#define RNDR_LOG_WARNING(format, ...)                                                                                             \
    do                                                                                                                            \
    {                                                                                                                             \
        if (Rndr::Application* app_ = Rndr::Application::Get())                                                                  \
            Rndr::Log(app_->GetLoggerChecked(), Opal::CurrentSourceLocation(), Rndr::LogLevel::Warning, format, __VA_ARGS__);     \
    } while (0)
#define RNDR_LOG_DEBUG(format, ...)                                                                                               \
    do                                                                                                                            \
    {                                                                                                                             \
        if (Rndr::Application* app_ = Rndr::Application::Get())                                                                  \
            Rndr::Log(app_->GetLoggerChecked(), Opal::CurrentSourceLocation(), Rndr::LogLevel::Debug, format, __VA_ARGS__);       \
    } while (0)
#define RNDR_LOG_INFO(format, ...)                                                                                                \
    do                                                                                                                            \
    {                                                                                                                             \
        if (Rndr::Application* app_ = Rndr::Application::Get())                                                                  \
            Rndr::Log(app_->GetLoggerChecked(), Opal::CurrentSourceLocation(), Rndr::LogLevel::Info, format, __VA_ARGS__);        \
    } while (0)
#define RNDR_LOG_TRACE(format, ...)                                                                                               \
    do                                                                                                                            \
    {                                                                                                                             \
        if (Rndr::Application* app_ = Rndr::Application::Get())                                                                  \
            Rndr::Log(app_->GetLoggerChecked(), Opal::CurrentSourceLocation(), Rndr::LogLevel::Trace, format, __VA_ARGS__);       \
    } while (0)

#define RNDR_CUSTOM_LOG_ERROR(logger_ptr, format, ...) \
    Rndr::Log(*logger_ptr, Opal::CurrentSourceLocation(), Rndr::LogLevel::Error, format, __VA_ARGS__)
#define RNDR_CUSTOM_LOG_WARNING(logger_ptr, format, ...) \
    Rndr::Log(*logger_ptr, Opal::CurrentSourceLocation(), Rndr::LogLevel::Warning, format, __VA_ARGS__)
#define RNDR_CUSTOM_LOG_DEBUG(logger_ptr, format, ...) \
    Rndr::Log(*logger_ptr, Opal::CurrentSourceLocation(), Rndr::LogLevel::Debug, format, __VA_ARGS__)
#define RNDR_CUSTOM_LOG_INFO(logger_ptr, format, ...) \
    Rndr::Log(*logger_ptr, Opal::CurrentSourceLocation(), Rndr::LogLevel::Info, format, __VA_ARGS__)
#define RNDR_CUSTOM_LOG_TRACE(logger_ptr, format, ...) \
    Rndr::Log(*logger_ptr, Opal::CurrentSourceLocation(), Rndr::LogLevel::Trace, format, __VA_ARGS__)
