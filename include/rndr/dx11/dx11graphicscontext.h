#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include <d3d11.h>
#include <d3dcompiler.h>

#include "rndr/core/graphicstypes.h"
#include "rndr/core/math.h"

namespace rndr
{

class Window;
class FrameBuffer;
class Image;
class Shader;
class Sampler;
class Buffer;

struct InputLayout;
struct RasterizerState;
struct DepthStencilState;
struct BlendState;

class GraphicsContext
{
public:
    GraphicsContext(Window* Window, GraphicsContextProperties Props = GraphicsContextProperties{});
    ~GraphicsContext();

    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetDeviceContext();
    IDXGISwapChain* GetSwapchain();
    D3D_FEATURE_LEVEL GetFeatureLevel();

    FrameBuffer* GetWindowFrameBuffer();

    Shader* CreateShader(const ShaderProperties& Props);
    Image* CreateImage(int Width, int Height, const ImageProperties& Props);
    Image* CreateImage(const std::string& FilePath, const ImageProperties& Props);
    Image* CreateImage();
    Sampler* CreateSampler(const SamplerProperties& Props);
    Buffer* CreateBuffer(const BufferProperties& Props, ByteSpan InitialData);
    FrameBuffer* CreateFrameBuffer(int Width, int Height, const FrameBufferProperties& Props);
    InputLayout* CreateInputLayout(Span<InputLayoutProperties> Pros, Shader* Shader);
    RasterizerState* CreateRasterizerState(const RasterizerProperties& Props);
    DepthStencilState* CreateDepthStencilState(const DepthStencilProperties& Props);
    BlendState* CreateBlendState(const BlendProperties& Props);

    void ClearColor(Image* Image, Vector4r Color);
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
    void DrawIndexedInstanced(PrimitiveTopology Topology, int IndicesCount, int InstanceCount);

    void Present(bool bVSync);

    void DestroyShader(Shader* Shader);
    void DestroyImage(Image* Image);
    void DestroySampler(Sampler* Sampler);
    void DestroyBuffer(Buffer* Buffer);
    void DestroyFrameBuffer(FrameBuffer* FrameBuffer);
    void DestroyInputLayout(InputLayout* InputLayout);
    void DestroyRasterizerState(RasterizerState* State);
    void DestroyDepthStencilState(DepthStencilState* State);
    void DestroyBlendState(BlendState* State);

private:
    void WindowResize(Window* Window, int Width, int Height);

private:
    Window* m_Window;
    GraphicsContextProperties m_Props;

    ID3D11Device* m_Device;
    ID3D11DeviceContext* m_DeviceContext;
    IDXGISwapChain* m_Swapchain;
    D3D_FEATURE_LEVEL m_FeatureLevel;

    std::unique_ptr<FrameBuffer> m_WindowFrameBuffer;
};

}  // namespace rndr

#endif  // RNDR_DX11
