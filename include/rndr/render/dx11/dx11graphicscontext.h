#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <string>

#include <d3d11.h>
#include <d3dcompiler.h>

#include "rndr/render/graphicstypes.h"

namespace math
{
class Vector4;
}

namespace rndr
{

struct FrameBuffer;
struct Image;
struct Shader;
struct Sampler;
struct Buffer;
struct CommandList;
struct InputLayout;
struct RasterizerState;
struct DepthStencilState;
struct BlendState;
struct SwapChain;
struct Pipeline;

template <typename T>
class ScopePtr;

class GraphicsContext
{
public:
    ID3D11Device* DX11Device = nullptr;
    ID3D11DeviceContext* DX11DeviceContext = nullptr;
    D3D_FEATURE_LEVEL DX11FeatureLevel;

#if RNDR_DEBUG
    ID3D11InfoQueue* DX11DebugInfoQueue = nullptr;
    int DX11DebugLastMessageId = 0;
#endif  // RNDR_DEBUG

public:
    GraphicsContext() = default;
    ~GraphicsContext();

    GraphicsContext(const GraphicsContext& Other) = delete;
    GraphicsContext& operator=(const GraphicsContext& Other) = delete;

    GraphicsContext(GraphicsContext&& Other) = delete;
    GraphicsContext& operator=(GraphicsContext&& Other) = delete;

    bool Init(GraphicsContextProperties Props = GraphicsContextProperties{});

    ScopePtr<SwapChain> CreateSwapChain(NativeWindowHandle WindowHandle,
                                        int Width,
                                        int Height,
                                        const SwapChainProperties& Props = SwapChainProperties{});
    ScopePtr<Shader> CreateShader(const ByteSpan& ShaderContents, const ShaderProperties& Props);
    ScopePtr<Image> CreateImage(int Width,
                                int Height,
                                const ImageProperties& Props,
                                ByteSpan InitData);
    ScopePtr<Image> CreateImageArray(int Width,
                                     int Height,
                                     int ArraySize,
                                     const ImageProperties& Props,
                                     Span<ByteSpan> InitData);
    ScopePtr<Image> CreateCubeMap(int Width,
                                  int Height,
                                  const ImageProperties& Props,
                                  Span<ByteSpan> InitData);
    ScopePtr<Image> CreateImageForSwapChain(SwapChain* SwapChain, int BufferIndex);
    ScopePtr<Sampler> CreateSampler(const SamplerProperties& Props = SamplerProperties{});
    ScopePtr<Buffer> CreateBuffer(const BufferProperties& Props, ByteSpan InitialData);
    ScopePtr<FrameBuffer> CreateFrameBuffer(int Width,
                                            int Height,
                                            const FrameBufferProperties& Props);
    ScopePtr<FrameBuffer> CreateFrameBufferForSwapChain(int Width,
                                                        int Height,
                                                        SwapChain* SwapChain);
    ScopePtr<InputLayout> CreateInputLayout(Span<InputLayoutProperties> Props, Shader* Shader);
    ScopePtr<RasterizerState> CreateRasterizerState(const RasterizerProperties& Props);
    ScopePtr<DepthStencilState> CreateDepthStencilState(const DepthStencilProperties& Props);
    ScopePtr<BlendState> CreateBlendState(const BlendProperties& Props);
    ScopePtr<Pipeline> CreatePipeline(const PipelineProperties& Props);
    ScopePtr<CommandList> CreateCommandList();

    // TODO(Marko): Instead of taking raw pointers these API calls should take a reference in order
    // to remove the need for null checks
    void ClearColor(Image* Image, math::Vector4 Color);
    void ClearDepth(Image* Image, real Depth);
    void ClearStencil(Image* Image, uint8_t Stencil);
    void ClearDepthStencil(Image* Image, real Depth, uint8_t Stencil);

    void BindShader(Shader* Shader);
    void BindImageAsShaderResource(Image* Image, int Slot, Shader* Shader);
    void BindSampler(Sampler* Sampler, int Slot, Shader* Shader);
    // Split buffer binding for index/vertex buffers and for constant buffers
    void BindBuffer(Buffer* Buffer, int Slot, Shader* Shader = nullptr);
    void BindFrameBuffer(FrameBuffer* FrameBuffer);
    void BindInputLayout(InputLayout* InputLayout);
    void BindRasterizerState(RasterizerState* State);
    void BindDepthStencilState(DepthStencilState* State);
    void BindBlendState(BlendState* State);
    void BindPipeline(Pipeline* Pipeline);

    void DrawIndexed(PrimitiveTopology Topology, int IndicesCount);
    void DrawIndexedInstanced(PrimitiveTopology Topology,
                              uint32_t IndexCount,
                              uint32_t InstanceCount,
                              uint32_t IndexOffset = 0,
                              uint32_t InstanceOffset = 0);

    void Dispatch(uint32_t ThreadGroupCountX,
                  uint32_t ThreadGroupCountY,
                  uint32_t ThreadGroupCountZ);

    bool SubmitCommandList(CommandList* List);

    void Present(SwapChain* SwapChain, bool BVSync);

    std::string WindowsGetErrorMessage(HRESULT ErrorCode = S_OK);
    bool WindowsHasFailed(HRESULT ErrorCode = S_OK);

private:
    GraphicsContextProperties m_Props;
};

}  // namespace rndr

#endif  // RNDR_DX11
