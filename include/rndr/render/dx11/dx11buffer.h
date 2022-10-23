#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/render/graphicstypes.h"

struct ID3D11Buffer;

namespace rndr
{

class GraphicsContext;

struct Buffer
{
    BufferProperties Props;

    ID3D11Buffer* DX11Buffer;

    Buffer() = default;
    ~Buffer();

    bool Init(rndr::GraphicsContext* Context, const BufferProperties& Props = BufferProperties{}, ByteSpan InitialData = ByteSpan{});

    bool Update(rndr::GraphicsContext* Context, ByteSpan Data, int StartOffset = 0) const;
    bool Read(rndr::GraphicsContext* Context, ByteSpan OutData, int ReadOffset = 0) const;
};

}  // namespace rndr

#endif  // RNDR_DX11
