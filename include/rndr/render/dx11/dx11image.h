#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;

namespace math
{
class Point2;
class Vector2;
}  // namespace math

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

    bool Update(GraphicsContext* Context, int ArrayIndex, const math::Point2& Start, const math::Vector2& Size, ByteSpan Contents) const;
    bool Read(GraphicsContext* Context, int ArrayIndex, const math::Point2& Start, const math::Vector2& Size, ByteSpan OutContents) const;
    
    static bool Copy(GraphicsContext* Context, Image* Src, Image* Dest);

private:
    bool InitInternal(GraphicsContext* Context, Span<ByteSpan> InitData, bool bCubeMap = false);
};

}  // namespace rndr

#endif  // RNDR_DX11
