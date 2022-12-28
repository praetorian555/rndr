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

public:
    CommandList() = default;
    ~CommandList();

    bool Init(GraphicsContext* Context);
    bool Finish(GraphicsContext* Context);

    bool IsFinished() const;

    void ClearColor(Image* Image, math::Vector4 Color);
    void ClearDepth(Image* Image, real Depth);
    void ClearStencil(Image* Image, uint8_t Stencil);
    void ClearDepthStencil(Image* Image, real Depth, uint8_t Stencil);

    void BindShader(Shader* Shader);
    void BindImageAsShaderResource(Image* Image, int Slot, Shader* Shader);
    void BindSampler(Sampler* Sampler, int Slot, Shader* Shader);
    void BindBuffer(Buffer* Buffer, int Slot, Shader* Shader = nullptr);
    void BindFrameBuffer(FrameBuffer* FrameBuffer);
    void BindInputLayout(InputLayout* InputLayout);
    void BindRasterizerState(RasterizerState* State);
    void BindDepthStencilState(DepthStencilState* State);
    void BindBlendState(BlendState* State);

    void DrawIndexed(PrimitiveTopology Topology, int IndicesCount);
    void DrawIndexedInstanced(PrimitiveTopology Topology,
                              int IndexCount,
                              int InstanceCount,
                              int IndexOffset = 0,
                              int InstanceOffset = 0);

    void Dispatch(const uint32_t ThreadGroupCountX,
                  const uint32_t ThreadGroupCountY,
                  const uint32_t ThreadGroupCountZ);
};

}  // namespace rndr

#endif  // RNDR_DX11
