#include "rndr/render/dx11/dx11shader.h"

#if defined RNDR_DX11

#include <d3d11.h>

#include "rndr/core/log.h"

#include "rndr/utility/array.h"

#include "rndr/render/dx11/dx11graphicscontext.h"
#include "rndr/render/dx11/dx11helpers.h"
#include "rndr/render/dx11/dx11image.h"
#include "rndr/render/dx11/dx11sampler.h"
#include "rndr/render/graphicstypes.h"

// TODO(Marko): Add support for different shader models and move this to dx11helpers
static const char* GetShaderModel(D3D_FEATURE_LEVEL FeatureLevel, rndr::ShaderType Type)
{
    switch (Type)
    {
        case rndr::ShaderType::Vertex:
        {
            switch (FeatureLevel)
            {
                case D3D_FEATURE_LEVEL_11_1:
                case D3D_FEATURE_LEVEL_11_0:
                {
                    return "vs_5_0";
                }
                break;
                case D3D_FEATURE_LEVEL_10_1:
                {
                    return "vs_4_1";
                }
                break;
                case D3D_FEATURE_LEVEL_10_0:
                {
                    return "vs_4_0";
                }
                break;
                case D3D_FEATURE_LEVEL_9_3:
                {
                    return "vs_4_0_level_9_3";
                }
                break;
                case D3D_FEATURE_LEVEL_9_2:
                case D3D_FEATURE_LEVEL_9_1:
                {
                    return "vs_4_0_level_9_1";
                }
                break;
            }
        }
        case rndr::ShaderType::Fragment:
        {
            switch (FeatureLevel)
            {
                case D3D_FEATURE_LEVEL_11_1:
                case D3D_FEATURE_LEVEL_11_0:
                {
                    return "ps_5_0";
                }
                break;
                case D3D_FEATURE_LEVEL_10_1:
                {
                    return "ps_4_1";
                }
                break;
                case D3D_FEATURE_LEVEL_10_0:
                {
                    return "ps_4_0";
                }
                break;
                case D3D_FEATURE_LEVEL_9_3:
                {
                    return "ps_4_0_level_9_3";
                }
                break;
                case D3D_FEATURE_LEVEL_9_2:
                case D3D_FEATURE_LEVEL_9_1:
                {
                    return "ps_4_0_level_9_1";
                }
                break;
            }
        }
        default:
            assert(false);
    }

    return "";
}

bool rndr::Shader::Init(GraphicsContext* Context,
                        const ByteSpan& ShaderContents,
                        const ShaderProperties& InProps)
{
    if (!ShaderContents)
    {
        RNDR_LOG_ERROR("rndr::Shader::Init: Shader contents are empty!");
        return false;
    }

    Props = InProps;

    ID3DBlob* ErrorMessage = nullptr;
    const char* Model = GetShaderModel(Context->GetFeatureLevel(), Props.Type);

    UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if RNDR_DEBUG
    Flags |= D3DCOMPILE_DEBUG;
#endif

    Array<D3D_SHADER_MACRO> Macros;
    if (!Props.Macros.empty())
    {
        Macros.resize(Props.Macros.size() + 1);
        for (int Index = 0; Index < Macros.size() - 1; Index++)
        {
            Macros[Index].Name = Props.Macros[Index].Name.c_str();
            Macros[Index].Definition = Props.Macros[Index].Definition.c_str();
        }
        // Leave the last one empty to signify that the list is done
        Macros.back().Name = "";
        Macros.back().Definition = "";
    }

    HRESULT Result =
        D3DCompile(ShaderContents.Data, ShaderContents.Size, nullptr, Macros.data(), nullptr,
                   Props.EntryPoint.c_str(), Model, Flags, 0, &DX11ShaderBuffer, &ErrorMessage);
    if (Context->WindowsHasFailed(Result))
    {
        const std::string Message = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("rndr::Shader::Init: %s", Message.c_str());
        RNDR_LOG_ERROR("rndr::Shader::Init: %s", ErrorMessage->GetBufferPointer());
        return false;
    }

    ID3D11Device* Device = Context->GetDevice();
    switch (Props.Type)
    {
        case ShaderType::Vertex:
            Result = Device->CreateVertexShader(DX11ShaderBuffer->GetBufferPointer(),
                                                DX11ShaderBuffer->GetBufferSize(), nullptr,
                                                &DX11VertexShader);
            break;
        case ShaderType::Fragment:
            Result = Device->CreatePixelShader(DX11ShaderBuffer->GetBufferPointer(),
                                               DX11ShaderBuffer->GetBufferSize(), nullptr,
                                               &DX11FragmentShader);
            break;
        case ShaderType::Compute:
            Result = Device->CreateComputeShader(DX11ShaderBuffer->GetBufferPointer(),
                                                 DX11ShaderBuffer->GetBufferSize(), nullptr,
                                                 &DX11ComputeShader);
            break;
        default:
            assert(false);
    }
    if (Context->WindowsHasFailed(Result))
    {
        const std::string Message = Context->WindowsGetErrorMessage(Result);
        RNDR_LOG_ERROR("rndr::Shader::Init: %s", Message.c_str());
        return false;
    }

    return true;
}

rndr::Shader::~Shader()
{
    DX11SafeRelease(DX11ShaderBuffer);
    switch (Props.Type)
    {
        case ShaderType::Vertex:
            DX11SafeRelease(DX11VertexShader);
            break;
        case ShaderType::Fragment:
            DX11SafeRelease(DX11FragmentShader);
            break;
        case ShaderType::Compute:
            DX11SafeRelease(DX11ComputeShader);
            break;
    }
}

#endif  // RNDR_DX11
