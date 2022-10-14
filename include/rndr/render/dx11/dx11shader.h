#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <Windows.h>
#include <d3d.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "rndr/render/graphicstypes.h"

namespace rndr
{

class GraphicsContext;

struct Shader
{
    ShaderProperties Props;
    ID3DBlob* DX11ShaderBuffer;
    union
    {
        ID3D11VertexShader* DX11VertexShader;
        ID3D11PixelShader* DX11FragmentShader;
    };

    Shader(GraphicsContext* Context, const ShaderProperties& Props = ShaderProperties{});
    ~Shader();
};

}  // namespace rndr

#endif  // RNDR_DX11
