#pragma once

#include "math/vector4.h"

#include "rndr/core/base.h"

#if defined RNDR_OPENGL

namespace rndr
{

struct Image;

struct CommandList
{
    RNDR_DEFAULT_BODY(CommandList);

    void Finish(GraphicsContext* context) const { RNDR_UNUSED(context); }
    [[nodiscard]] bool IsFinished() const { return true; }

    void ClearColor(Image* image, const math::Vector4& color) const
    {
        RNDR_UNUSED(image);
        RNDR_UNUSED(color);
    }
};

}  // namespace rndr

#endif  // RNDR_OPENGL
