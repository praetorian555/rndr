#pragma once

#include "rndr/core/base.h"

#if defined RNDR_OPENGL

namespace rndr
{

struct Buffer
{
    RNDR_DEFAULT_BODY(Buffer);

    bool Update(rndr::GraphicsContext* context, ByteSpan data, uint32_t start_offset = 0) const
    {
        RNDR_UNUSED(context);
        RNDR_UNUSED(data);
        RNDR_UNUSED(start_offset);
        return false;
    }

    bool Read(rndr::GraphicsContext* context, ByteSpan out_data, uint32_t read_offset = 0) const
    {
        RNDR_UNUSED(context);
        RNDR_UNUSED(out_data);
        RNDR_UNUSED(read_offset);
        return false;
    }
};

}  // namespace rndr

#endif  // RNDR_OPENGL
