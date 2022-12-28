#pragma once

#include <map>

#include "rndr/core/base.h"
#include "rndr/core/memory.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"
#include "rndr/render/shader.h"

struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11BlendState;
struct ID3D11InputLayout;

namespace rndr
{

class GraphicsContext;

struct InputLayoutBuilder
{
    InputLayoutBuilder();
    ~InputLayoutBuilder();

    InputLayoutBuilder& AddBuffer(int BufferIndex, DataRepetition Repetition, int PerInstanceRate);
    // If the same SemanticName is used twice in one builder instance this will increment underlying
    // semantic index
    InputLayoutBuilder& AppendElement(int BufferIndex,
                                      const std::string& SemanticName,
                                      PixelFormat Format);

    Span<InputLayoutProperties> Build();

private:
    struct BufferInfo
    {
        DataRepetition Repetiton;
        int PerInstanceRate;
        int EntriesCount = 0;
    };

    std::map<int, BufferInfo> m_Buffers;
    std::map<std::string, int> m_Names;
    rndr::Span<InputLayoutProperties> m_Props;
};

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

struct Pipeline
{
    ScopePtr<Shader> VertexShader;
    ScopePtr<Shader> PixelShader;
    ScopePtr<InputLayout> InputLayout;
    ScopePtr<RasterizerState> Rasterizer;
    ScopePtr<BlendState> Blend;
    ScopePtr<DepthStencilState> DepthStencil;

    Pipeline() = default;
    ~Pipeline() = default;

    bool Init(GraphicsContext* Context, const PipelineProperties& Props);
};

}  // namespace rndr

#endif  // RNDR_DX11
