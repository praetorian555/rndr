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
        ID3D11ComputeShader* DX11ComputeShader;
    };

    ~Shader();

    bool Init(GraphicsContext* Context, const ByteSpan& ShaderContents, const ShaderProperties& Props = ShaderProperties{});
};

}  // namespace rndr

#endif  // RNDR_DX11
