#pragma once

#include "rndr/core/base.h"

#if RNDR_OPENGL

#include "rndr/core/graphics-types.h"

namespace rndr
{

struct SwapChain;
struct Image;
struct Sampler;
struct Shader;
struct Buffer;
struct FrameBuffer;
struct InputLayout;
struct RasterizerState;
struct BlendState;
struct DepthStencilState;
struct Pipeline;
struct CommandList;

struct GraphicsContext
{
    GraphicsContextProperties properties;

    void* gl_device_context = nullptr;
    void* gl_graphics_context = nullptr;

    RNDR_DEFAULT_BODY(GraphicsContext);

    bool Init(GraphicsContextProperties props = GraphicsContextProperties{});
    ~GraphicsContext();

    bool OpenGL_HasError();

    ScopePtr<SwapChain> CreateSwapChain(NativeWindowHandle window_handle,
                                        int width,
                                        int height,
                                        const SwapChainProperties& props = SwapChainProperties{});
    ScopePtr<Shader> CreateShader(const ByteSpan& shader_contents, const ShaderProperties& props);
    ScopePtr<Image> CreateImage(int width,
                                int height,
                                const ImageProperties& props,
                                ByteSpan init_data);
    ScopePtr<Image> CreateImageArray(int width,
                                     int height,
                                     int array_size,
                                     const ImageProperties& props,
                                     Span<ByteSpan> init_data);
    ScopePtr<Image> CreateCubeMap(int width,
                                  int height,
                                  const ImageProperties& props,
                                  Span<ByteSpan> init_data);
    ScopePtr<Image> CreateImageForSwapChain(SwapChain* swap_chain, int buffer_index);
    ScopePtr<Sampler> CreateSampler(const SamplerProperties& props = SamplerProperties{});
    ScopePtr<Buffer> CreateBuffer(const BufferProperties& props, ByteSpan InitialData);
    ScopePtr<FrameBuffer> CreateFrameBuffer(int width,
                                            int height,
                                            const FrameBufferProperties& props);
    ScopePtr<FrameBuffer> CreateFrameBufferForSwapChain(int width,
                                                        int height,
                                                        SwapChain* swap_chain);
    ScopePtr<InputLayout> CreateInputLayout(Span<InputLayoutProperties> props, Shader* shader);
    ScopePtr<RasterizerState> CreateRasterizerState(const RasterizerProperties& props);
    ScopePtr<DepthStencilState> CreateDepthStencilState(const DepthStencilProperties& props);
    ScopePtr<BlendState> CreateBlendState(const BlendProperties& props);
    ScopePtr<Pipeline> CreatePipeline(const PipelineProperties& props);
    ScopePtr<CommandList> CreateCommandList();

    // TODO(Marko): Instead of taking raw pointers these API calls should take a reference in order
    // to remove the need for null checks
    void ClearColor(Image* Image, math::Vector4 Color) const;
    void ClearDepth(Image* Image, real Depth) const;
    void ClearStencil(Image* Image, uint8_t Stencil) const;
    void ClearDepthStencil(Image* Image, real Depth, uint8_t Stencil) const;

    void BindShader(Shader* Shader) const;
    void BindImageAsShaderResource(Image* Image, int Slot, Shader* Shader) const;
    void BindSampler(Sampler* Sampler, int Slot, Shader* Shader) const;
    // Split buffer binding for index/vertex buffers and for constant buffers
    void BindBuffer(Buffer* Buffer, int Slot, Shader* Shader = nullptr) const;
    void BindFrameBuffer(FrameBuffer* FrameBuffer) const;
    void BindInputLayout(InputLayout* InputLayout) const;
    void BindRasterizerState(RasterizerState* State) const;
    void BindDepthStencilState(DepthStencilState* State) const;
    void BindBlendState(BlendState* State) const;
    void BindPipeline(Pipeline* Pipeline) const;

    void DrawIndexed(PrimitiveTopology Topology, int IndicesCount) const;
    void DrawIndexedInstanced(PrimitiveTopology Topology,
                              uint32_t IndexCount,
                              uint32_t InstanceCount,
                              uint32_t IndexOffset = 0,
                              uint32_t InstanceOffset = 0) const;

    void Dispatch(uint32_t ThreadGroupCountX,
                  uint32_t ThreadGroupCountY,
                  uint32_t ThreadGroupCountZ) const;

    bool SubmitCommandList(CommandList* List) const;

    void Present(SwapChain* SwapChain, bool ActivateVSync) const;
};

}  // namespace rndr

#endif  // RNDR_OPENGL
