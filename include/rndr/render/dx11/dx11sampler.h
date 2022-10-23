#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct ID3D11SamplerState;

namespace rndr
{

class GraphicsContext;

struct Sampler
{
    SamplerProperties Props;

    ID3D11SamplerState* DX11State;

    ~Sampler();

    bool Init(rndr::GraphicsContext* Context, const SamplerProperties& Props = SamplerProperties{});
};

}  // namespace rndr

#endif  // RNDR_DX11
