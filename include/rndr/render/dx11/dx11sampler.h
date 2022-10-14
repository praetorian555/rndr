#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct ID3D11SamplerState;

namespace rndr
{

class GraphicsContext;

class Sampler
{
public:
    Sampler(GraphicsContext* Context, const SamplerProperties& Props = SamplerProperties{});
    ~Sampler();

    ID3D11SamplerState* GetSamplerState();

private:
    friend class GraphicsContext;

private:
    SamplerProperties m_Props;
    ID3D11SamplerState* m_State;
};

}  // namespace rndr

#endif  // RNDR_DX11
