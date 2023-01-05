#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct ID3D10Blob;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;

namespace rndr
{

class GraphicsContext;

struct Shader
{
    ShaderProperties Props;

    ID3D10Blob* DX11ShaderBuffer;
    union
    {
        ID3D11VertexShader* DX11VertexShader;
        ID3D11PixelShader* DX11FragmentShader;
        ID3D11ComputeShader* DX11ComputeShader;
    };

    ~Shader();

    bool Init(GraphicsContext* Context,
              const ByteSpan& ShaderContents,
              const ShaderProperties& InProps = ShaderProperties{});
};

}  // namespace rndr

#endif  // RNDR_DX11
