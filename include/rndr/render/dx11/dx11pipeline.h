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

    InputLayoutBuilder(const InputLayoutBuilder& Other) = default;
    InputLayoutBuilder& operator=(const InputLayoutBuilder& Other) = default;
    
    InputLayoutBuilder(InputLayoutBuilder&& Other) = default;
    InputLayoutBuilder& operator=(InputLayoutBuilder&& Other) = default;

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

    InputLayout() = default;
    ~InputLayout();

    InputLayout(const InputLayout& Other) = delete;
    InputLayout& operator=(const InputLayout& Other) = delete;

    InputLayout(InputLayout&& Other) = delete;
    InputLayout& operator=(InputLayout&& Other) = delete;

    bool Init(GraphicsContext* Context, Span<InputLayoutProperties> InProps, rndr::Shader* Shader);
};

struct RasterizerState
{
    RasterizerProperties Props;

    ID3D11RasterizerState* DX11RasterizerState;

    RasterizerState() = default;
    ~RasterizerState();

    RasterizerState(const RasterizerState& Other) = delete;
    RasterizerState& operator=(const RasterizerState& Other) = delete;

    RasterizerState(RasterizerState&& Other) = delete;
    RasterizerState& operator=(RasterizerState&& Other) = delete;

    bool Init(GraphicsContext* Context, const RasterizerProperties& InProps);
};

struct DepthStencilState
{
    DepthStencilProperties Props;

    ID3D11DepthStencilState* DX11DepthStencilState;
    
    DepthStencilState() = default;
    ~DepthStencilState();

    DepthStencilState(const DepthStencilState& Other) = delete;
    DepthStencilState& operator=(const DepthStencilState& Other) = delete;

    DepthStencilState(DepthStencilState&& Other) = delete;
    DepthStencilState& operator=(DepthStencilState&& Other) = delete;

    bool Init(GraphicsContext* Context, const DepthStencilProperties& InProps);
};

struct BlendState
{
    BlendProperties Props;

    ID3D11BlendState* DX11BlendState;

    BlendState() = default;
    ~BlendState();

    BlendState(const BlendState& Other) = delete;
    BlendState& operator=(const BlendState& Other) = delete;

    BlendState(BlendState&& Other) = delete;
    BlendState& operator=(BlendState&& Other) = delete;

    bool Init(GraphicsContext* Context, const BlendProperties& InProps);
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

    Pipeline(const Pipeline& Other) = delete;
    Pipeline& operator=(const Pipeline& Other) = delete;

    Pipeline(Pipeline&& Other) = delete;
    Pipeline& operator=(Pipeline&& Other) = delete;

    bool Init(GraphicsContext* Context, const PipelineProperties& Props);
};

}  // namespace rndr

#endif  // RNDR_DX11
