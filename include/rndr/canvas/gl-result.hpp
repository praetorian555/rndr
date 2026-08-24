#pragma once

#include "glad/glad.h"

#include "rndr/error-codes.hpp"
#include "rndr/log.hpp"
#include "rndr/types.hpp"

namespace Rndr::Canvas
{

inline const char* GlErrorToString(GLenum error)
{
    switch (error)
    {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM: An enum argument is not a legal value for that function.";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE: A numeric argument is out of range.";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION: The operation is not allowed in the current state.";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION: The framebuffer object is not complete.";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY: There is not enough memory left to execute the function.";
        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW: An operation would cause an internal stack to underflow.";
        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW: An operation would cause an internal stack to overflow.";
        default:
            return "Unknown OpenGL error";
    }
}

/**
 * Map an OpenGL error to the Rndr::ErrorCode it reports as. GL_OUT_OF_MEMORY is the one error GL
 * attributes to a cause; everything else collapses to ErrorCode::GraphicsAPIError, and the log line
 * carries the specific error.
 */
inline ErrorCode GlErrorToErrorCode(GLenum error)
{
    switch (error)
    {
        case GL_NO_ERROR:
            return ErrorCode::Success;
        case GL_OUT_OF_MEMORY:
            return ErrorCode::OutOfMemory;
        default:
            return ErrorCode::GraphicsAPIError;
    }
}

}  // namespace Rndr::Canvas

/**
 * Read glGetError after a GL call, and on failure log the error together with the name of the function it
 * came from and return the code it maps to. For a function that returns Rndr::ErrorCode.
 *
 * @param function_name Name of the GL function, for the log line.
 */
#define RNDR_CANVAS_GL_CHECK(function_name)                                                                 \
    do                                                                                                      \
    {                                                                                                       \
        const GLenum gl_error_ = glGetError();                                                              \
        if (gl_error_ != GL_NO_ERROR)                                                                       \
        {                                                                                                   \
            RNDR_LOG_ERROR("Canvas: {} failed: {}", function_name, Rndr::Canvas::GlErrorToString(gl_error_)); \
            return Rndr::Canvas::GlErrorToErrorCode(gl_error_);                                             \
        }                                                                                                   \
    } while (0)

/**
 * The same, for a function that returns an Opal::Expected. The error is wrapped in @p ResultType, which is
 * the Expected the enclosing function returns.
 */
#define RNDR_CANVAS_GL_CHECK_EXPECTED(function_name, ResultType)                                            \
    do                                                                                                      \
    {                                                                                                       \
        const GLenum gl_error_ = glGetError();                                                              \
        if (gl_error_ != GL_NO_ERROR)                                                                       \
        {                                                                                                   \
            RNDR_LOG_ERROR("Canvas: {} failed: {}", function_name, Rndr::Canvas::GlErrorToString(gl_error_)); \
            return ResultType(Rndr::Canvas::GlErrorToErrorCode(gl_error_));                                 \
        }                                                                                                   \
    } while (0)

/**
 * Give up on the enclosing call when a check or a step reported something. For a function that returns
 * Rndr::ErrorCode.
 */
#define RNDR_CANVAS_CHECK(expr)                      \
    do                                               \
    {                                                \
        const Rndr::ErrorCode error_code_ = (expr);  \
        if (error_code_ != Rndr::ErrorCode::Success) \
        {                                            \
            return error_code_;                      \
        }                                            \
    } while (0)

/** The same, for a function returning the Opal::Expected named by @p ResultType. */
#define RNDR_CANVAS_CHECK_EXPECTED(expr, ResultType) \
    do                                               \
    {                                                \
        const Rndr::ErrorCode error_code_ = (expr);  \
        if (error_code_ != Rndr::ErrorCode::Success) \
        {                                            \
            return ResultType(error_code_);          \
        }                                            \
    } while (0)
