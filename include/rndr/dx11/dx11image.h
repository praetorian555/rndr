#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/core/graphicstypes.h"

struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;

namespace rndr
{

class GraphicsContext;

struct Image
{
    int Width, Height;
    ImageProperties Props;

    ID3D11Texture2D* DX11Texture = nullptr;
    ID3D11ShaderResourceView* DX11ShaderResourceView = nullptr;
    ID3D11RenderTargetView* DX11RenderTargetView = nullptr;
    ID3D11DepthStencilView* DX11DepthStencilView = nullptr;

    Image() = default;

    bool Init(GraphicsContext* Context, int Width, int Height, const ImageProperties& Props, ByteSpan InitData);
    bool InitArray(GraphicsContext* Context, int Width, int Height, const ImageProperties& Props, Span<ByteSpan> InitData);
    bool InitSwapchainBackBuffer(GraphicsContext* Context);

    ~Image();

    void Update(GraphicsContext* Context, int ArrayIndex, ByteSpan Contents) const;
};

}  // namespace rndr

#endif  // RNDR_DX11
