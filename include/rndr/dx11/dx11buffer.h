#pragma once

#include "rndr/core/base.h"

#if defined RNDR_DX11

#include "rndr/core/graphicstypes.h"

struct ID3D11Buffer;

namespace rndr
{

class GraphicsContext;

struct Buffer
{
    GraphicsContext* GraphicsContext;
    BufferProperties Props;
    ID3D11Buffer* DX11Buffer;

    Buffer(rndr::GraphicsContext* Context, const BufferProperties& P = BufferProperties{}, ByteSpan InitialData = ByteSpan{});
    ~Buffer();

    void Update(ByteSpan Data) const;
};

}  // namespace rndr

#endif  // RNDR_DX11
