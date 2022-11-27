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
};

}  // namespace rndr

#endif  // RNDR_DX11
