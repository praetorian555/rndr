#pragma once

#include "opal/logging.h"

#define RNDR_LOG_ERROR(fmt, ...)                                                       \
    do                                                                                 \
    {                                                                                  \
        Opal::Logger& logger_ = Opal::GetLogger();                                    \
        if (logger_.IsCategoryRegistered("Rndr"))                                      \
            logger_.Error("Rndr", fmt, ##__VA_ARGS__);                                 \
    } while (0)
#define RNDR_LOG_WARNING(fmt, ...)                                                     \
    do                                                                                 \
    {                                                                                  \
        Opal::Logger& logger_ = Opal::GetLogger();                                    \
        if (logger_.IsCategoryRegistered("Rndr"))                                      \
            logger_.Warning("Rndr", fmt, ##__VA_ARGS__);                               \
    } while (0)
#define RNDR_LOG_DEBUG(fmt, ...)                                                       \
    do                                                                                 \
    {                                                                                  \
        Opal::Logger& logger_ = Opal::GetLogger();                                    \
        if (logger_.IsCategoryRegistered("Rndr"))                                      \
            logger_.Verbose("Rndr", fmt, ##__VA_ARGS__);                               \
    } while (0)
#define RNDR_LOG_INFO(fmt, ...)                                                        \
    do                                                                                 \
    {                                                                                  \
        Opal::Logger& logger_ = Opal::GetLogger();                                    \
        if (logger_.IsCategoryRegistered("Rndr"))                                      \
            logger_.Info("Rndr", fmt, ##__VA_ARGS__);                                  \
    } while (0)
