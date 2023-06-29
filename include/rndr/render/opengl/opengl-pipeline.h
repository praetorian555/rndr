#pragma once

#include "rndr/core/base.h"

#if defined RNDR_OPENGL

namespace rndr
{

struct InputLayout
{
    RNDR_DEFAULT_BODY(InputLayout);
};

struct BlendState
{
    RNDR_DEFAULT_BODY(BlendState);
};

struct DepthStencilState
{
    RNDR_DEFAULT_BODY(DepthStencilState);
};

struct RasterizerState
{
    RNDR_DEFAULT_BODY(RasterizerState);
};

struct Pipeline
{
    RNDR_DEFAULT_BODY(Pipeline);
};

}  // namespace rndr

#endif  // RNDR_OPENGL
