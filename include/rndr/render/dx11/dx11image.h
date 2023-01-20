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

struct GraphicsContext;
struct SwapChain;

struct Image
{
    int Width = 0;
    int Height = 0;
    int ArraySize = 1;
    int BackBufferIndex = -1;
    ImageProperties Props;

    ID3D11Texture2D* DX11Texture = nullptr;
    ID3D11ShaderResourceView* DX11ShaderResourceView = nullptr;
    ID3D11RenderTargetView* DX11RenderTargetView = nullptr;
    ID3D11DepthStencilView* DX11DepthStencilView = nullptr;

    Image() = default;

    Image(const Image& Other) = delete;
    Image& operator=(const Image& Other) = delete;

    Image(Image&& Other) = delete;
    Image& operator=(Image&& Other) = delete;

    bool Init(GraphicsContext* Context,
              int InWidth,
              int InHeight,
              const ImageProperties& InProps,
              ByteSpan InitData);
    bool Init(GraphicsContext* Context,
              int InWidth,
              int InHeight,
              int InArraySize,
              const ImageProperties& InProps,
              Span<ByteSpan> InitData);
    bool Init(GraphicsContext* Context,
              int InWidth,
              int InHeight,
              const ImageProperties& InProps,
              Span<ByteSpan> InitData);
    bool Init(GraphicsContext* Context, rndr::SwapChain* SwapChain, int BufferIndex);

    ~Image();

    bool Update(GraphicsContext* Context,
                int ArrayIndex,
                const math::Point2& Start,
                const math::Vector2& Size,
                ByteSpan Contents) const;
    bool Read(GraphicsContext* Context,
              int ArrayIndex,
              const math::Point2& Start,
              const math::Vector2& Size,
              ByteSpan OutContents) const;

    static bool Copy(GraphicsContext* Context, Image* Src, Image* Dest);

private:
    bool InitInternal(GraphicsContext* Context,
                      Span<ByteSpan> InitData,
                      bool IsCubeMap = false,
                      rndr::SwapChain* SwapChain = nullptr);
};

}  // namespace rndr

#endif  // RNDR_DX11
