#include "rndr/log.hpp"

Opal::Logger& Rndr::GetLogger()
{
    Opal::Logger& logger = Opal::GetLogger();
    if (!logger.IsCategoryRegistered("Rndr"))
    {
        logger.RegisterCategory("Rndr", Opal::LogLevel::Verbose);
    }
    return logger;
}
