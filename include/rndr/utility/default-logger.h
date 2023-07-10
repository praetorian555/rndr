#pragma once

#if RNDR_DEFAULT_LOGGER

#include <memory>

#include "rndr/core/base.h"

namespace spdlog
{
class logger;
}

namespace Rndr
{
class DefaultLogger : public Logger
{
public:
    DefaultLogger();
    ~DefaultLogger();

    void Log(const std::source_location& source_location, LogLevel log_level, const char* message) override;

private:
    std::shared_ptr<class spdlog::logger> m_logger;
};
}  // namespace Rndr

#endif  // RNDR_DEFAULT_LOGGER