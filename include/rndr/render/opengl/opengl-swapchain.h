#pragma once

#include "rndr/core/base.h"

#if defined RNDR_OPENGL

namespace rndr
{

struct FrameBuffer;

struct SwapChain
{
    FrameBuffer* frame_buffer = nullptr;

    RNDR_DEFAULT_BODY(SwapChain);
};

}  // namespace rndr

#endif  // RNDR_OPENGL
