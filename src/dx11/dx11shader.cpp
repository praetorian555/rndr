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

rndr::Shader::Shader(GraphicsContext* Context, const ShaderProperties& Props) : m_GraphicsContext(Context), m_Props(Props)
{
    m_Images.resize(GraphicsConstants::MaxShaderResourceBindSlots);
    m_Samplers.resize(GraphicsConstants::MaxShaderResourceBindSlots);
    m_ConstantBuffers.resize(GraphicsConstants::MaxConstantBuffers);

    ID3DBlob* ErrorMessage;
    const char* Model = GetShaderModel(m_GraphicsContext->GetFeatureLevel(), Props.Type);

    UINT Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
    Flags |= D3DCOMPILE_DEBUG;
#endif

    HRESULT Result = D3DCompileFromFile(Props.FilePath.c_str(), nullptr, nullptr, Props.EntryPoint.c_str(), Model, Flags, 0,
                                        &m_ShaderBuffer, &ErrorMessage);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR("Failed to compile %s shader from file!", Props.FilePath.c_str());
        return;
    }

    ID3D11Device* Device = m_GraphicsContext->GetDevice();
    switch (m_Props.Type)
    {
        case ShaderType::Vertex:
            Result =
                Device->CreateVertexShader(m_ShaderBuffer->GetBufferPointer(), m_ShaderBuffer->GetBufferSize(), nullptr, &m_VertexShader);
            break;
        case ShaderType::Fragment:
            Result =
                Device->CreatePixelShader(m_ShaderBuffer->GetBufferPointer(), m_ShaderBuffer->GetBufferSize(), nullptr, &m_FragmentShader);
        default:
            assert(false);
    }
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR("Failed to create a shader!");
        return;
    }
}

rndr::Shader::~Shader()
{
    DX11SafeRelease(m_ShaderBuffer);
    switch (m_Props.Type)
    {
        case ShaderType::Vertex:
            DX11SafeRelease(m_VertexShader);
            break;
        case ShaderType::Fragment:
            DX11SafeRelease(m_FragmentShader);
            break;
    }
    DX11SafeRelease(m_InputLayout);
}

void rndr::Shader::AddInputLayout(Span<InputLayoutProperties> InputLayout)
{
    assert(m_InputLayout == nullptr);
    assert(InputLayout.Size <= GraphicsConstants::MaxInputLayoutEntries);

    D3D11_INPUT_ELEMENT_DESC InputDescriptors[GraphicsConstants::MaxInputLayoutEntries] = {};
    for (int i = 0; i < InputLayout.Size; i++)
    {
        InputDescriptors[i].SemanticName = InputLayout[i].SemanticName.c_str();
        InputDescriptors[i].SemanticIndex = InputLayout[i].SemanticIndex;
        InputDescriptors[i].Format = FromPixelFormat(InputLayout[i].Format);
        InputDescriptors[i].AlignedByteOffset = InputLayout[i].OffsetInVertex;
        InputDescriptors[i].InputSlot = InputLayout[i].InputSlot;
        InputDescriptors[i].InputSlotClass = FromDataRepetition(InputLayout[i].Repetition);
        InputDescriptors[i].InstanceDataStepRate = InputLayout[i].InstanceStepRate;
    }

    ID3D11Device* Device = m_GraphicsContext->GetDevice();
    HRESULT Result = Device->CreateInputLayout(InputDescriptors, InputLayout.Size, m_ShaderBuffer->GetBufferPointer(),
                                               m_ShaderBuffer->GetBufferSize(), &m_InputLayout);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR("Failed to setup the input layout!");
        return;
    }
}

void rndr::Shader::AddImage(int Slot, Image* I, Sampler* S)
{
    assert(Slot > 0 && Slot < m_Images.size());
    m_Images[Slot] = I;
    m_Samplers[Slot] = S;
}

void rndr::Shader::AddConstantBuffer(int Slot, ConstantBufferProperties& Props)
{
    assert(Slot > 0 && Slot < m_ConstantBuffers.size());
    assert(Props.Size > 0);
    ID3D11Device* Device = m_GraphicsContext->GetDevice();

    D3D11_BUFFER_DESC BufferDesc;
    BufferDesc.ByteWidth = Props.Size;
    BufferDesc.Usage = FromUsage(Props.Usage);
    BufferDesc.CPUAccessFlags = FromCPUAccess(Props.CPUAccess);
    BufferDesc.BindFlags = 0;
    BufferDesc.MiscFlags = 0;
    BufferDesc.StructureByteStride = 0;
    HRESULT Result = Device->CreateBuffer(&BufferDesc, nullptr, &m_ConstantBuffers[Slot]);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR("Failed to create a constant buffer!");
        return;
    }
}

rndr::ShaderType rndr::Shader::GetShaderType() const
{
    return m_Props.Type;
}

void rndr::Shader::UpdateConstantBuffer(int Slot, ByteSpan Data)
{
    assert(Slot > 0 && Slot < m_ConstantBuffers.size());
    ID3D11DeviceContext* DeviceContext = m_GraphicsContext->GetDeviceContext();
 
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    HRESULT Result = DeviceContext->Map(m_ConstantBuffers[Slot], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
    if (FAILED(Result))
    {
        RNDR_LOG_ERROR("Failed to map a constant buffer!");
        return;
    }
    memcpy(MappedResource.pData, Data.Data, Data.Size);
    DeviceContext->Unmap(m_ConstantBuffers[Slot], 0);
}

void rndr::Shader::Render()
{
    ID3D11DeviceContext* DeviceContext = m_GraphicsContext->GetDeviceContext();

    if (m_InputLayout)
    {
        DeviceContext->IASetInputLayout(m_InputLayout);
    }

    switch (m_Props.Type)
    {
        case ShaderType::Vertex:
        {
            DeviceContext->VSSetShader(m_VertexShader, nullptr, 0);
            break;
        }
        case ShaderType::Fragment:
        {
            DeviceContext->PSSetShader(m_FragmentShader, nullptr, 0);
            break;
        }
        default:
        {
            assert(false);
        }
    }

    if (m_Props.Type == ShaderType::Fragment)
    {
        for (int i = 0; i < m_Images.size(); i++)
        {
            if (!m_Images[i])
            {
                continue;
            }

            ID3D11ShaderResourceView* ResourceView = m_Images[i]->GetShaderResourceView();
            ID3D11SamplerState* SamplerState = m_Samplers[i]->GetSamplerState();
            switch (m_Props.Type)
            {
                case ShaderType::Vertex:
                {
                    DeviceContext->VSSetShaderResources(i, 1, &ResourceView);
                    DeviceContext->VSSetSamplers(i, 1, &SamplerState);
                    break;
                }
                case ShaderType::Fragment:
                {
                    DeviceContext->PSSetShaderResources(i, 1, &ResourceView);
                    DeviceContext->PSSetSamplers(i, 1, &SamplerState);
                    break;
                }
                default:
                {
                    assert(false);
                }
            }
        }
    }

    for (int i = 0; i < m_ConstantBuffers.size(); i++)
    {
        if (m_ConstantBuffers[i] == nullptr)
        {
            continue;
        }

        switch (m_Props.Type)
        {
            case ShaderType::Vertex:
            {
                DeviceContext->VSSetConstantBuffers(i, 1, &m_ConstantBuffers[i]);
                break;
            }
            case ShaderType::Fragment:
            {
                DeviceContext->PSSetConstantBuffers(i, 1, &m_ConstantBuffers[i]);
                break;
            }
            default:
            {
                assert(false);
            }
        }
    }
}

#endif  // RNDR_DX11
