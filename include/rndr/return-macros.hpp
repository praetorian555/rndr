#pragma once

#include "rndr/definitions.h"
#include "rndr/error-codes.h"
#include "rndr/log.h"
#include "rndr/types.h"

#if RNDR_DEBUG
#define RNDR_RETURN_ON_FAIL(cond, err, msg, do_if_fails) \
    if (!(cond))                                         \
    {                                                    \
        RNDR_DEBUG_BREAK;                                \
        RNDR_LOG_ERROR(msg);                             \
        do_if_fails;                                     \
        return err;                                      \
    }
#else
#define RNDR_RETURN_ON_FAIL(cond, err, msg, do_if_fails) \
    if (!(cond))                                         \
    {                                                    \
        RNDR_LOG_ERROR(msg);                             \
        do_if_fails;                                     \
        return err;                                      \
    }
#endif