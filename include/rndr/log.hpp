#pragma once

#include "opal/logging.h"

// The format string travels inside __VA_ARGS__ rather than as a named parameter, so there is always at
// least one argument and no trailing comma for the GNU `, ##__VA_ARGS__` extension to swallow.
#define RNDR_LOG_ERROR(...)                                                            \
    do                                                                                 \
    {                                                                                  \
        Opal::Logger& logger_ = Opal::GetLogger();                                    \
        if (logger_.IsCategoryRegistered("Rndr"))                                      \
            logger_.Error("Rndr", __VA_ARGS__);                                        \
    } while (0)
#define RNDR_LOG_WARNING(...)                                                          \
    do                                                                                 \
    {                                                                                  \
        Opal::Logger& logger_ = Opal::GetLogger();                                    \
        if (logger_.IsCategoryRegistered("Rndr"))                                      \
            logger_.Warning("Rndr", __VA_ARGS__);                                      \
    } while (0)
#define RNDR_LOG_DEBUG(...)                                                            \
    do                                                                                 \
    {                                                                                  \
        Opal::Logger& logger_ = Opal::GetLogger();                                    \
        if (logger_.IsCategoryRegistered("Rndr"))                                      \
            logger_.Verbose("Rndr", __VA_ARGS__);                                      \
    } while (0)
#define RNDR_LOG_INFO(...)                                                             \
    do                                                                                 \
    {                                                                                  \
        Opal::Logger& logger_ = Opal::GetLogger();                                    \
        if (logger_.IsCategoryRegistered("Rndr"))                                      \
            logger_.Info("Rndr", __VA_ARGS__);                                         \
    } while (0)
