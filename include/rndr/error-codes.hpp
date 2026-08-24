#pragma once

#include "opal/error-codes.h"

#include "rndr/types.hpp"

namespace Rndr
{

enum class ErrorCode : u8
{
    Success = static_cast<u8>(Opal::ErrorCode::Success),
    OutOfBounds = static_cast<u8>(Opal::ErrorCode::OutOfBounds),
    OutOfMemory = static_cast<u8>(Opal::ErrorCode::OutOfMemory),
    InvalidArgument = static_cast<u8>(Opal::ErrorCode::InvalidArgument),
    GraphicsAPIError,
    /** The graphics device was lost, or the surface it drew to went away. Nothing made from it is usable. */
    DeviceLost,
    /** No graphics device to render with: no Vulkan capable device on the machine. */
    NoGraphicsDevice,
    ShaderCompilationError,
    ShaderLinkingError,
    WindowAlreadyClosed,
    PlatformError,
    FeatureNotSupported,
    /** The path does not name a file that exists. */
    FileNotFound,
    /** A real file of a kind this code does not handle: six channels, a compressed WAV, an extension nobody reads. */
    UnsupportedFormat,
    /** The bytes claim to be a format and are not, or the file ends in the middle of something. */
    CorruptData,
    /** No output device to play through. */
    NoAudioDevice,
    /** A fixed pool - clip slots, voices - has nothing left to hand out. */
    OutOfResources,
};

}  // namespace Rndr
