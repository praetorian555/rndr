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

    Sampler() = default;
    ~Sampler();

    Sampler(const Sampler& Other) = delete;
    Sampler& operator=(const Sampler& Other) = delete;

    Sampler(Sampler&& Other) = delete;
    Sampler& operator=(Sampler&& Other) = delete;

    bool Init(rndr::GraphicsContext* Context, const SamplerProperties& InProps = SamplerProperties{});
};

}  // namespace rndr

#endif  // RNDR_DX11
