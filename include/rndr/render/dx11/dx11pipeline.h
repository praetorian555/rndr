#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11BlendState;
struct ID3D11InputLayout;

namespace rndr
{

class GraphicsContext;
struct Shader;

struct InputLayout
{
    Span<InputLayoutProperties> Props;
    ID3D11InputLayout* DX11InputLayout;

    InputLayout(GraphicsContext* Context, Span<InputLayoutProperties> Props, rndr::Shader* Shader);
    ~InputLayout();
};

struct RasterizerState
{
    RasterizerProperties Props;
    ID3D11RasterizerState* DX11RasterizerState;

    RasterizerState(GraphicsContext* Context, const RasterizerProperties& P);
    ~RasterizerState();
};

struct DepthStencilState
{
    DepthStencilProperties Props;
    ID3D11DepthStencilState* DX11DepthStencilState;

    DepthStencilState(GraphicsContext* Context, const DepthStencilProperties& P);
    ~DepthStencilState();
};

struct BlendState
{
    BlendProperties Props;
    ID3D11BlendState* DX11BlendState;

    BlendState(GraphicsContext* Context, const BlendProperties& P);
    ~BlendState();
};

}  // namespace rndr

#endif  // RNDR_DX11
