#pragma once

#include <source_location>

#include "rndr/core/definitions.h"

namespace Rndr
{

// Types ///////////////////////////////////////////////////////////////////////////////////////////

enum class LogLevel : uint8_t
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

    virtual void Log(const std::source_location& source_location,
                     LogLevel log_level,
                     const char* message) = 0;
};

struct RndrDesc
{
    /** User specified logger. If no logger is provided, the default logger is used. */
    Logger* user_logger = nullptr;

    /** If we should enable the input system. Defaults to no. */
    bool enable_input_system = false;

    /** If we should enable the CPU tracer. Defaults to no. */
    bool enable_cpu_tracer = false;
};

// API /////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the Rndr library instance. There can be only one.
 * @param desc Configuration for the library.
 * @return True if the library was initialized successfully.
 */
bool Init(const RndrDesc& desc = RndrDesc{});

/**
 * Destroys the Rndr library instance.
 * @return True if the library was destroyed successfully.
 */
bool Destroy();

/**
 * Checks if the logger is valid.
 * @param logger Logger to check.
 * @return True if the logger is valid.
 */
bool IsValid(const Logger& logger);

/**
 * Gets the library's logger.
 * @return Library's logger.
 */
const Logger& GetLogger();

/**
 * Logs a message using the user specified logger. If no logger is provided, the default logger is
 * used.
 * @param file File where the log was called.
 * @param line Line where the log was called.
 * @param function_name Function where the log was called.
 * @param log_level Log level of the message.
 * @param format Format string for the message.
 * @param ... Arguments for the format string.
 */
void Log(const std::source_location& source_location,
         Rndr::LogLevel log_level,
         const char* format,
         ...);

void WaitForDebuggerToAttach();

}  // namespace Rndr

// Helper macros ///////////////////////////////////////////////////////////////////////////////////

/**
 * Helper macro to allocate and create a new object.
 * @param type Type of the object to create.
 * @param ... Arguments for the object's constructor.
 */
#define RNDR_NEW(type, ...) \
    new type                \
    {                       \
        __VA_ARGS__         \
    }

/**
 * Helper macro to invoke destructor and to deallocate memory.
 * @param type Type of the object to delete.
 * @param ptr Pointer to the object to delete.
 */
#define RNDR_DELETE(type, ptr) delete ptr

/**
 * Helper macro to invoke destructor and to deallocate memory for an array.
 * @param type Type of the elements of the array to delete.
 * @param ptr Pointer to the start of the array to delete.
 */
#define RNDR_DELETE_ARRAY(type, ptr) delete[] ptr

/**
 * Helper macros to log messages.
 */
#define RNDR_LOG_ERROR(format, ...) \
    Rndr::Log(std::source_location::current(), Rndr::LogLevel::Error, format, __VA_ARGS__)
#define RNDR_LOG_WARNING(format, ...) \
    Rndr::Log(std::source_location::current(), Rndr::LogLevel::Warning, format, __VA_ARGS__)
#define RNDR_LOG_DEBUG(format, ...) \
    Rndr::Log(std::source_location::current(), Rndr::LogLevel::Debug, format, __VA_ARGS__)
#define RNDR_LOG_INFO(format, ...) \
    Rndr::Log(std::source_location::current(), Rndr::LogLevel::Info, format, __VA_ARGS__)
#define RNDR_LOG_TRACE(format, ...) \
    Rndr::Log(std::source_location::current(), Rndr::LogLevel::Trace, format, __VA_ARGS__)
