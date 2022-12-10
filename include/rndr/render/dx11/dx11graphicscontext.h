#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <string>

#include <d3d11.h>
#include <d3dcompiler.h>

#include "rndr/render/graphicstypes.h"

// Forward declarations
struct math::Vector4;

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

class GraphicsContext
{
public:
    GraphicsContext() = default;
    ~GraphicsContext();

    bool Init(GraphicsContextProperties Props = GraphicsContextProperties{});

    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetDeviceContext();
    D3D_FEATURE_LEVEL GetFeatureLevel();

    SwapChain* CreateSwapChain(NativeWindowHandle WindowHandle,
                               int Width,
                               int Height,
                               const SwapChainProperties& Props = SwapChainProperties{});
    Shader* CreateShader(const ByteSpan& ShaderContents, const ShaderProperties& Props);
    Image* CreateImage(int Width, int Height, const ImageProperties& Props, ByteSpan InitData);
    Image* CreateImageArray(int Width, int Height, int ArraySize, const ImageProperties& Props, Span<ByteSpan> InitData);
    Image* CreateCubeMap(int Width, int Height, const ImageProperties& Props, Span<ByteSpan> InitData);
    Image* CreateImageForSwapChain(SwapChain* SwapChain, int BufferIndex);
    Sampler* CreateSampler(const SamplerProperties& Props);
    Buffer* CreateBuffer(const BufferProperties& Props, ByteSpan InitialData);
    FrameBuffer* CreateFrameBuffer(int Width, int Height, const FrameBufferProperties& Props);
    FrameBuffer* CreateFrameBufferForSwapChain(int Width, int Height, SwapChain* SwapChain);
    InputLayout* CreateInputLayout(Span<InputLayoutProperties> Pros, Shader* Shader);
    RasterizerState* CreateRasterizerState(const RasterizerProperties& Props);
    DepthStencilState* CreateDepthStencilState(const DepthStencilProperties& Props);
    BlendState* CreateBlendState(const BlendProperties& Props);
    CommandList* CreateCommandList();

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
    void DrawIndexedInstanced(PrimitiveTopology Topology, int IndexCount, int InstanceCount, int IndexOffset = 0, int InstanceOffset = 0);

    void Dispatch(const uint32_t ThreadGroupCountX, const uint32_t ThreadGroupCountY, const uint32_t ThreadGroupCountZ);

    bool SubmitCommandList(CommandList* List);

    void Present(SwapChain* SwapChain, bool bVSync);

    std::string WindowsGetErrorMessage(HRESULT ErrorCode = S_OK);
    bool WindowsHasFailed(HRESULT ErrorCode = S_OK);

private:
    GraphicsContextProperties m_Props;

    ID3D11Device* m_Device = nullptr;
    ID3D11DeviceContext* m_DeviceContext = nullptr;
    D3D_FEATURE_LEVEL m_FeatureLevel;

#if RNDR_DEBUG
    ID3D11InfoQueue* m_DebugInfoQueue = nullptr;
    int m_DebugLastMessageId = 0;
#endif  // RNDR_DEBUG
};

}  // namespace rndr

#endif  // RNDR_DX11
