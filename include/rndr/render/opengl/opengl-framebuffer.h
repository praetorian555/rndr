#pragma once

#include "rndr/core/base.h"
#include "rndr/core/memory.h"
#include "rndr/utility/array.h"

#if defined RNDR_OPENGL

namespace rndr
{

struct Image;

struct FrameBuffer
{
    Array<ScopePtr<Image>> color_buffers;

    RNDR_DEFAULT_BODY(FrameBuffer);

    bool Resize(rndr::GraphicsContext* context, int width, int height)
    {
        RNDR_UNUSED(context);
        RNDR_UNUSED(width);
        RNDR_UNUSED(height);
        return false;
    }
};

}  // namespace rndr

#endif  // RNDR_OPENGL
