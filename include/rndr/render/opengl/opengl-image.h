#pragma once

#include "rndr/core/base.h"

#if defined RNDR_OPENGL

namespace rndr
{

struct GraphicsContext;

struct Image
{
    RNDR_DEFAULT_BODY(Image);

    bool Read(GraphicsContext* context,
              int array_index,
              const math::Point2& start,
              const math::Vector2& size,
              ByteSpan out_contents) const
    {
        RNDR_UNUSED(context);
        RNDR_UNUSED(array_index);
        RNDR_UNUSED(start);
        RNDR_UNUSED(size);
        RNDR_UNUSED(out_contents);
        return false;
    }

    bool Update(GraphicsContext* context,
                int array_index,
                const math::Point2& start,
                const math::Vector2& size,
                ByteSpan contents) const
    {
        RNDR_UNUSED(context);
        RNDR_UNUSED(array_index);
        RNDR_UNUSED(start);
        RNDR_UNUSED(size);
        RNDR_UNUSED(contents);
        return false;
    }

    static bool Copy(GraphicsContext* context, Image* src, Image* dest)
    {
        RNDR_UNUSED(context);
        RNDR_UNUSED(src);
        RNDR_UNUSED(dest);
        return false;
    }
};

}  // namespace rndr

#endif  // RNDR_OPENGL
