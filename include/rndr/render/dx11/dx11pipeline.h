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

    ~InputLayout();

    bool Init(GraphicsContext* Context, Span<InputLayoutProperties> Props, rndr::Shader* Shader);
};

struct RasterizerState
{
    RasterizerProperties Props;

    ID3D11RasterizerState* DX11RasterizerState;

    ~RasterizerState();

    bool Init(GraphicsContext* Context, const RasterizerProperties& Props);
};

struct DepthStencilState
{
    DepthStencilProperties Props;

    ID3D11DepthStencilState* DX11DepthStencilState;

    ~DepthStencilState();

    bool Init(GraphicsContext* Context, const DepthStencilProperties& Props);
};

struct BlendState
{
    BlendProperties Props;

    ID3D11BlendState* DX11BlendState;

    ~BlendState();

    bool Init(GraphicsContext* Context, const BlendProperties& Props);
};

}  // namespace rndr

#endif  // RNDR_DX11
