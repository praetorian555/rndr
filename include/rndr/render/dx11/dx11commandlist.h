#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct ID3D11DeviceContext;
struct ID3D11CommandList;

namespace rndr
{

struct Image;
struct Shader;
struct Sampler;
struct Buffer;
struct FrameBuffer;
struct InputLayout;
struct RasterizerState;
struct DepthStencilState;
struct BlendState;
struct GraphicsContext;

struct CommandList
{
public:
    ID3D11DeviceContext* DX11DeferredContext = nullptr;
    ID3D11CommandList* DX11CommandList = nullptr;

    CommandList() = default;
    ~CommandList();

    CommandList(const CommandList& Other) = delete;
    CommandList& operator=(const CommandList& Other) = delete;

    CommandList(CommandList&& Other) = delete;
    CommandList& operator=(CommandList&& Other) = delete;

    bool Init(GraphicsContext* Context);
    bool Finish(GraphicsContext* Context);

    [[nodiscard]] bool IsFinished() const;

    void ClearColor(Image* Image, math::Vector4 Color) const;
    void ClearDepth(Image* Image, real Depth) const;
    void ClearStencil(Image* Image, uint8_t Stencil) const;
    void ClearDepthStencil(Image* Image, real Depth, uint8_t Stencil) const;

    void BindShader(Shader* Shader) const;
    void BindImageAsShaderResource(Image* Image, int Slot, Shader* Shader) const;
    void BindSampler(Sampler* Sampler, int Slot, Shader* Shader) const;
    void BindBuffer(Buffer* Buffer, int Slot, Shader* Shader = nullptr) const;
    void BindFrameBuffer(FrameBuffer* FrameBuffer) const;
    void BindInputLayout(InputLayout* InputLayout) const;
    void BindRasterizerState(RasterizerState* State) const;
    void BindDepthStencilState(DepthStencilState* State) const;
    void BindBlendState(BlendState* State) const;

    void DrawIndexed(PrimitiveTopology Topology, int IndicesCount) const;
    void DrawIndexedInstanced(PrimitiveTopology Topology,
                              int IndexCount,
                              int InstanceCount,
                              int IndexOffset = 0,
                              int InstanceOffset = 0) const;

    void Dispatch(uint32_t ThreadGroupCountX,
                  uint32_t ThreadGroupCountY,
                  uint32_t ThreadGroupCountZ) const;
};

}  // namespace rndr

#endif  // RNDR_DX11
