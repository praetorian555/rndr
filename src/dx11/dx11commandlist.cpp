#include "rndr/render/dx11/dx11commandlist.h"

#if defined RNDR_DX11

#include <d3d.h>

#include "rndr/core/log.h"

#include "rndr/render/dx11/dx11buffer.h"
#include "rndr/render/dx11/dx11framebuffer.h"
#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11image.h"
#include "rndr/render/dx11/dx11pipeline.h"
#include "rndr/render/dx11/dx11sampler.h"
#include "rndr/render/dx11/dx11shader.h"
#include "rndr/render/dx11/dx11swapchain.h"

bool rndr::CommandList::Init(GraphicsContext* Context)
{
    ID3D11Device* DX11Device = Context->GetDevice();
    assert(DX11Device);

    const HRESULT Result = DX11Device->CreateDeferredContext(0, &DX11DeferredContext);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("CommandList::Init: %s", ErrorMessage.c_str());
        return false;
    }

    return true;
}

rndr::CommandList::~CommandList()
{
    DX11SafeRelease(DX11DeferredContext);
    DX11SafeRelease(DX11CommandList);
}

bool rndr::CommandList::Finish(GraphicsContext* Context)
{
    const HRESULT Result = DX11DeferredContext->FinishCommandList(false, &DX11CommandList);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string ErrorMessage = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("CommandList::Finish: %s", ErrorMessage.c_str());
        return false;
    }
    return true;
}

bool rndr::CommandList::IsFinished() const
{
    return DX11CommandList != nullptr;
}

#endif  // RNDR_DX11
