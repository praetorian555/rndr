#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <Windows.h>
#include <d3d.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "rndr/core/graphicstypes.h"

namespace rndr
{

class Image;
class GraphicsContext;
class Sampler;

class Shader
{
public:
    Shader(GraphicsContext* Context, const ShaderProperties& Props = ShaderProperties{});
    ~Shader();

    void AddInputLayout(Span<InputLayoutProperties> InputLayout);
    void AddImage(int Slot, Image* I, Sampler* S);
    void AddConstantBuffer(int Slot, ConstantBufferProperties& Props = ConstantBufferProperties{});

    ShaderType GetShaderType() const;

    void UpdateConstantBuffer(int Slot, ByteSpan Data);

    void Render();

private:
    GraphicsContext* m_GraphicsContext;
    ShaderProperties m_Props;

    ID3DBlob* m_ShaderBuffer;
    union
    {
        ID3D11VertexShader* m_VertexShader;
        ID3D11PixelShader* m_FragmentShader;
    };

    ID3D11InputLayout* m_InputLayout = nullptr;
    std::vector<Image*> m_Images;
    std::vector<Sampler*> m_Samplers;
    std::vector<ID3D11Buffer*> m_ConstantBuffers;
};

}  // namespace rndr

#endif  // RNDR_DX11
