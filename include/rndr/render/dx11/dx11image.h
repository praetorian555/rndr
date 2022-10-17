#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;

namespace rndr
{

class GraphicsContext;

struct Image
{
    int Width = 0;
    int Height = 0;
    int ArraySize = 1;
    ImageProperties Props;

    ID3D11Texture2D* DX11Texture = nullptr;
    ID3D11ShaderResourceView* DX11ShaderResourceView = nullptr;
    ID3D11RenderTargetView* DX11RenderTargetView = nullptr;
    ID3D11DepthStencilView* DX11DepthStencilView = nullptr;

    Image() = default;

    bool Init(GraphicsContext* Context, int Width, int Height, const ImageProperties& Props, ByteSpan InitData);
    bool InitArray(GraphicsContext* Context, int Width, int Height, int ArraySize, const ImageProperties& Props, Span<ByteSpan> InitData);
    bool InitCubeMap(GraphicsContext* Context, int Width, int Height, const ImageProperties& Props, Span<ByteSpan> InitData);
    bool InitSwapchainBackBuffer(GraphicsContext* Context);

    ~Image();

    void Update(GraphicsContext* Context, int ArrayIndex, ByteSpan Contents, int BoxWidth, int BoxHeight) const;

    // TODO: Method for copying this image into another, performed on the GPU side

private:
    bool InitInternal(GraphicsContext* Context, Span<ByteSpan> InitData);
};

}  // namespace rndr

#endif  // RNDR_DX11
