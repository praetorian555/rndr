#pragma once

#include "rndr/core/definitions.h"

namespace rndr
{

using OpaquePtr = void*;

/**
 * Allocator interface.
 */
struct Allocator
{
    OpaquePtr allocator_data = nullptr;

    bool (*init)(OpaquePtr init_data, OpaquePtr* allocator_data) = nullptr;
    bool (*destroy)(OpaquePtr allocator_data) = nullptr;
    OpaquePtr (*allocate)(OpaquePtr allocator_data, uint64_t size, const char* tag) = nullptr;
    void (*free)(OpaquePtr allocator_data, OpaquePtr ptr) = nullptr;
};

enum class LogLevel
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
    OpaquePtr logger_data = nullptr;

    bool (*init)(OpaquePtr init_data, OpaquePtr* logger_data) = nullptr;
    bool (*destroy)(OpaquePtr logger_data) = nullptr;
    void (*log)(OpaquePtr logger_data,
                const char* file,
                int line,
                const char* function,
                rndr::LogLevel log_level,
                const char* message) = nullptr;
};

/**
 * Default allocator. Uses malloc and free to allocate and free memory.
 */
namespace DefaultAllocator
{
rndr::OpaquePtr Allocate(OpaquePtr allocator_data, uint64_t size, const char* tag);
void Free(OpaquePtr allocator_data, OpaquePtr ptr);
}  // namespace DefaultAllocator

/**
 * Default logger. Uses spdlog to log messages. Log is sent to standard output using the background
 * thread.
 */
namespace DefaultLogger
{
bool Init(OpaquePtr init_data, OpaquePtr* logger_data);
bool Destroy(OpaquePtr logger_data);
void Log(OpaquePtr logger_data,
         const char* file,
         int line,
         const char* function,
         rndr::LogLevel log_level,
         const char* message);
}  // namespace DefaultLogger

struct RndrDesc
{
    // User specified allocator. If no allocator is provided, the default allocator is used.
    Allocator user_allocator;
    // User specified logger. If no logger is provided, the default logger is used.
    Logger user_logger;
};

/**
 * Creates the Rndr library instance. There can be only one.
 * @param desc Configuration for the library.
 * @return True if the library was initialized successfully.
 */
bool Create(const RndrDesc& desc = RndrDesc{});

/**
 * Destroys the Rndr library instance.
 * @return True if the library was destroyed successfully.
 */
bool Destroy();

/**
 * Checks if the allocator is valid.
 * @param allocator Allocator to check.
 * @return True if the allocator is valid.
 */
bool IsValid(const Allocator& allocator);

/**
 * Checks if the logger is valid.
 * @param logger Logger to check.
 * @return True if the logger is valid.
 */
bool IsValid(const Logger& logger);

/**
 * Gets the library's allocator.
 * @return Library's allocator.
 */
const Allocator& GetAllocator();

/**
 * Sets the library's allocator.
 * @param allocator Allocator to set.
 * @return True if the allocator was set successfully.
 */
bool SetAllocator(const Allocator& allocator);

/**
 * Gets the library's logger.
 * @return Library's logger.
 */
const Logger& GetLogger();

/**
 * Sets the library's logger.
 * @param logger Logger to set.
 * @return True if the logger was set successfully.
 */
bool SetLogger(const Logger& logger);

/**
 * Allocates memory using the user specified allocator. If no allocator is provided, the default
 * allocator is used.
 * @param size Size of the memory to allocate.
 * @param tag Tag for the memory.
 * @return Pointer to the allocated memory.
 */
OpaquePtr Allocate(uint64_t size, const char* tag);

/**
 * Frees memory using the user specified allocator. If no allocator is provided, the default
 * allocator is used.
 * @param ptr Pointer to the memory to free.
 */
void Free(OpaquePtr ptr);

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
void Log(const char* file,
         int line,
         const char* function_name,
         rndr::LogLevel log_level,
         const char* format,
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
