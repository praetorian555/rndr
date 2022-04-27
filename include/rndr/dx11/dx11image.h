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

class Image
{
public:
    Image(GraphicsContext* Context, int Width, int Height, const ImageProperties& Props = ImageProperties{});
    Image(GraphicsContext* Context);

    ~Image();

    ID3D11RenderTargetView* GetRenderTargetView();
    ID3D11DepthStencilView* GetStencilTargetView();
    ID3D11ShaderResourceView* GetShaderResourceView();

private:
    void Create();

private:
    GraphicsContext* m_GraphicsContext;
    int m_Width, m_Height;
    ImageProperties m_Props;

    ID3D11Texture2D* m_Texture;
    ID3D11ShaderResourceView* m_ShaderResourceView;
    union
    {
        ID3D11RenderTargetView* m_RenderTargetView;
        ID3D11DepthStencilView* m_DepthStencilView;
    };

    bool m_bSwapchainBackBuffer;
};

}  // namespace rndr

#endif  // RNDR_DX11
