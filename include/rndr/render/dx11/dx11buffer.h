#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct ID3D11Buffer;

namespace rndr
{

struct GraphicsContext;

struct Buffer
{
    BufferProperties Props;

    ID3D11Buffer* DX11Buffer = nullptr;

    Buffer() = default;
    ~Buffer();

    Buffer(const Buffer& Other) = delete;
    Buffer& operator=(const Buffer& Other) = delete;

    Buffer(Buffer&& Other) = delete;
    Buffer& operator=(Buffer&& Other) = delete;

    bool Init(rndr::GraphicsContext* Context,
              const BufferProperties& InProps = BufferProperties{},
              ByteSpan InitialData = ByteSpan{});

    bool Update(rndr::GraphicsContext* Context, ByteSpan Data, uint32_t StartOffset = 0) const;
    bool Read(rndr::GraphicsContext* Context, ByteSpan OutData, uint32_t ReadOffset = 0) const;
};

}  // namespace rndr

#endif  // RNDR_DX11
