#include "rndr/dx11/dx11shader.h"

#if defined RNDR_DX11

#include "rndr/core/graphicstypes.h"
#include "rndr/core/log.h"

#include "rndr/dx11/dx11graphicscontext.h"
#include "rndr/dx11/dx11helpers.h"
#include "rndr/dx11/dx11image.h"
#include "rndr/dx11/dx11sampler.h"

// TODO(mkostic): Add support for different shader models and move this to dx11helpers
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

rndr::Shader::Shader(GraphicsContext* Context, const ShaderProperties& P) : Props(P)
{
    ID3DBlob* ErrorMessage;
    const char* Model = GetShaderModel(Context->GetFeatureLevel(), Props.Type);

    UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
    Flags |= D3DCOMPILE_DEBUG;
#endif

    HRESULT Result = D3DCompileFromFile(Props.FilePath.c_str(), nullptr, nullptr, Props.EntryPoint.c_str(), Model, Flags, 0,
                                        &DX11ShaderBuffer, &ErrorMessage);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to compile shader from file:\n%s", (const char*)ErrorMessage->GetBufferPointer());
        return;
    }

    ID3D11Device* Device = Context->GetDevice();
    switch (Props.Type)
    {
        case ShaderType::Vertex:
            Result = Device->CreateVertexShader(DX11ShaderBuffer->GetBufferPointer(), DX11ShaderBuffer->GetBufferSize(), nullptr,
                                                &DX11VertexShader);
            break;
        case ShaderType::Fragment:
            Result = Device->CreatePixelShader(DX11ShaderBuffer->GetBufferPointer(), DX11ShaderBuffer->GetBufferSize(), nullptr,
                                               &DX11FragmentShader);
            break;
        default:
            assert(false);
    }
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR_OR_ASSERT("Failed to create a shader!");
        return;
    }
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
    }
}

#endif  // RNDR_DX11
