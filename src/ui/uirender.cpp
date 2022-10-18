#include "rndr/ui/uirender.h"

#include <vector>

#include "math/bounds2.h"
#include "math/point2.h"
#include "math/vector4.h"

#include "rndr/core/fileutils.h"
#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/span.h"

#include "rndr/render/buffer.h"
#include "rndr/render/framebuffer.h"
#include "rndr/render/graphicscontext.h"
#include "rndr/render/graphicstypes.h"
#include "rndr/render/pipeline.h"
#include "rndr/render/shader.h"

#include "rndr/ui/uibox.h"
#include "rndr/ui/uisystem.h"

namespace rndr
{
namespace ui
{

// Private data types

struct InstanceData
{
    math::Point2 BottomLeft;
    math::Point2 TopRight;
    math::Point2 TexCoordsBottomLeft;
    math::Point2 TexCoordsTopRight;
    math::Vector4 Color;
    float RenderId;
    float CornerRadius = 0.0f;
    float EdgeSoftness = 0.0f;
    float BorderThickness = 0.0f;
};

RNDR_ALIGN(16) struct ShaderGlobals
{
    math::Vector2 ScreenSize;
};

// Module private data

extern UIProperties g_UIProps;

// Private data

static GraphicsContext* g_Context = nullptr;
static Shader* g_VertexShader = nullptr;
static Shader* g_FragmentShader = nullptr;
static InputLayout* g_InputLayout = nullptr;
static RasterizerState* g_RasterizerState = nullptr;
static DepthStencilState* g_DepthStencilState = nullptr;
static BlendState* g_BlendState = nullptr;

static Buffer* g_InstanceBuffer = nullptr;
static Buffer* g_IndexBuffer = nullptr;
static Buffer* g_GlobalsConstantBuffer = nullptr;

static Sampler* g_Sampler = nullptr;
static Image* g_ImageArray = nullptr;

static Span<bool> g_RenderResources;

// Module private API
bool InitRender(GraphicsContext* Context);
void ShutDownRender();
RenderId AllocateRenderId();
void FreeRenderId(RenderId Id);
void UpdateRenderResource(RenderId Id, ByteSpan Contents, int Width, int Height);
void StartRenderFrame();
void EndRenderFrame(const Span<Box*> SortedBoxes);

// Private functions
static std::vector<InstanceData> ConvertBoxesIntoInstanceData(const Span<Box*> SortedBoxes);

}  // namespace ui
}  // namespace rndr

bool rndr::ui::InitRender(GraphicsContext* Context)
{
    g_Context = Context;

    assert(g_UIProps.MaxImageArraySize != 0);
    g_RenderResources.Size = g_UIProps.MaxImageArraySize;
    g_RenderResources.Data = new bool[g_RenderResources.Size];
    for (int i = 0; i < g_RenderResources.Size; i++)
    {
        g_RenderResources[i] = false;
    }

    ShaderProperties VertexShaderProps;
    VertexShaderProps.bCompilationNeeded = true;
    VertexShaderProps.Type = ShaderType::Vertex;
    VertexShaderProps.FilePath = RNDR_ASSET_DIR L"/shaders/UIVertexShader.hlsl";
    VertexShaderProps.EntryPoint = "Main";
    g_VertexShader = g_Context->CreateShader(VertexShaderProps);
    if (!g_VertexShader)
    {
        RNDR_LOG_ERROR("Failed to create vertex shader!");
        return false;
    }

    ShaderProperties FragmentShaderProps;
    FragmentShaderProps.bCompilationNeeded = true;
    FragmentShaderProps.Type = ShaderType::Fragment;
    FragmentShaderProps.FilePath = RNDR_ASSET_DIR L"/shaders/UIFragmentShader.hlsl";
    FragmentShaderProps.EntryPoint = "Main";
    g_FragmentShader = g_Context->CreateShader(FragmentShaderProps);
    if (!g_FragmentShader)
    {
        RNDR_LOG_ERROR("Failed to create fragment shader!");
        return false;
    }

    InputLayoutProperties ILProps[9];
    ILProps[0].SemanticName = "POSITION";
    ILProps[0].SemanticIndex = 0;
    ILProps[0].InputSlot = 0;
    ILProps[0].Format = rndr::PixelFormat::R32G32_FLOAT;
    ILProps[0].OffsetInVertex = 0;
    ILProps[0].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[0].InstanceStepRate = 1;
    ILProps[1].SemanticName = "POSITION";
    ILProps[1].SemanticIndex = 1;
    ILProps[1].InputSlot = 0;
    ILProps[1].Format = rndr::PixelFormat::R32G32_FLOAT;
    ILProps[1].OffsetInVertex = AppendAlignedElement;
    ILProps[1].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[1].InstanceStepRate = 1;
    ILProps[2].SemanticName = "TEXCOORD";
    ILProps[2].SemanticIndex = 0;
    ILProps[2].InputSlot = 0;
    ILProps[2].Format = rndr::PixelFormat::R32G32_FLOAT;
    ILProps[2].OffsetInVertex = AppendAlignedElement;
    ILProps[2].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[2].InstanceStepRate = 1;
    ILProps[3].SemanticName = "TEXCOORD";
    ILProps[3].SemanticIndex = 1;
    ILProps[3].InputSlot = 0;
    ILProps[3].Format = rndr::PixelFormat::R32G32_FLOAT;
    ILProps[3].OffsetInVertex = AppendAlignedElement;
    ILProps[3].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[3].InstanceStepRate = 1;
    ILProps[4].SemanticName = "COLOR";
    ILProps[4].SemanticIndex = 0;
    ILProps[4].InputSlot = 0;
    ILProps[4].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
    ILProps[4].OffsetInVertex = AppendAlignedElement;
    ILProps[4].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[4].InstanceStepRate = 1;
    ILProps[5].SemanticName = "BLENDINDICES";
    ILProps[5].SemanticIndex = 0;
    ILProps[5].InputSlot = 0;
    ILProps[5].Format = rndr::PixelFormat::R32_FLOAT;
    ILProps[5].OffsetInVertex = AppendAlignedElement;
    ILProps[5].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[5].InstanceStepRate = 1;
    ILProps[6].SemanticName = "BLENDINDICES";
    ILProps[6].SemanticIndex = 1;
    ILProps[6].InputSlot = 0;
    ILProps[6].Format = rndr::PixelFormat::R32_FLOAT;
    ILProps[6].OffsetInVertex = AppendAlignedElement;
    ILProps[6].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[6].InstanceStepRate = 1;
    ILProps[7].SemanticName = "BLENDINDICES";
    ILProps[7].SemanticIndex = 2;
    ILProps[7].InputSlot = 0;
    ILProps[7].Format = rndr::PixelFormat::R32_FLOAT;
    ILProps[7].OffsetInVertex = AppendAlignedElement;
    ILProps[7].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[7].InstanceStepRate = 1;
    ILProps[8].SemanticName = "BLENDINDICES";
    ILProps[8].SemanticIndex = 3;
    ILProps[8].InputSlot = 0;
    ILProps[8].Format = rndr::PixelFormat::R32_FLOAT;
    ILProps[8].OffsetInVertex = AppendAlignedElement;
    ILProps[8].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[8].InstanceStepRate = 1;
    g_InputLayout = g_Context->CreateInputLayout(Span(ILProps, 9), g_VertexShader);
    if (!g_InputLayout)
    {
        RNDR_LOG_ERROR("Failed to create InputLayout!");
        return false;
    }

    RasterizerProperties RasterProps;
    RasterProps.bAntialiasedLineEnable = false;
    RasterProps.bDepthClipEnable = false;
    RasterProps.bMultisampleEnable = false;
    RasterProps.bScissorEnable = false;
    RasterProps.CullFace = Face::Back;
    RasterProps.FrontFaceWindingOrder = WindingOrder::CCW;
    RasterProps.FillMode = FillMode::Solid;
    g_RasterizerState = g_Context->CreateRasterizerState(RasterProps);
    if (!g_RasterizerState)
    {
        RNDR_LOG_ERROR("Failed to create RasterizerState!");
        return false;
    }

    DepthStencilProperties DSProps;
    DSProps.bDepthEnable = false;
    DSProps.bStencilEnable = false;
    DSProps.DepthComparator = rndr::Comparator::Always;
    DSProps.DepthMask = DepthMask::All;
    g_DepthStencilState = g_Context->CreateDepthStencilState(DSProps);
    if (!g_DepthStencilState)
    {
        RNDR_LOG_ERROR("Failed to create DepthStencilState!");
        return false;
    }

    rndr::BlendProperties BlendProps;
    BlendProps.bBlendEnable = true;
    BlendProps.SrcColorFactor = rndr::BlendFactor::SrcAlpha;
    BlendProps.DstColorFactor = rndr::BlendFactor::OneMinusSrcAlpha;
    BlendProps.ColorOperator = rndr::BlendOperator::Add;
    BlendProps.SrcAlphaFactor = rndr::BlendFactor::One;
    BlendProps.DstAlphaFactor = rndr::BlendFactor::OneMinusSrcAlpha;
    BlendProps.AlphaOperator = rndr::BlendOperator::Add;
    g_BlendState = g_Context->CreateBlendState(BlendProps);
    if (!g_BlendState)
    {
        RNDR_LOG_ERROR("Failed to create BlendState!");
        return false;
    }

    assert(g_UIProps.MaxInstanceCount != 0);
    InstanceData* Instances = new InstanceData[g_UIProps.MaxInstanceCount];
    BufferProperties InstanceBufferProps;
    InstanceBufferProps.BindFlag = rndr::BufferBindFlag::Vertex;
    InstanceBufferProps.CPUAccess = rndr::CPUAccess::None;
    InstanceBufferProps.Usage = rndr::Usage::GPUReadWrite;
    InstanceBufferProps.Size = g_UIProps.MaxInstanceCount * sizeof(InstanceData);
    InstanceBufferProps.Stride = sizeof(InstanceData);
    g_InstanceBuffer = g_Context->CreateBuffer(InstanceBufferProps, ByteSpan(Instances));
    if (!g_InstanceBuffer)
    {
        RNDR_LOG_ERROR("Failed to create instance Buffer!");
        return false;
    }
    delete[] Instances;

    int IndexData[] = {0, 1, 2, 1, 3, 2};
    BufferProperties IndexBufferProps;
    IndexBufferProps.BindFlag = BufferBindFlag::Index;
    IndexBufferProps.CPUAccess = CPUAccess::None;
    IndexBufferProps.Usage = Usage::GPUReadWrite;
    IndexBufferProps.Size = sizeof(IndexData);
    IndexBufferProps.Stride = sizeof(int);
    g_IndexBuffer = g_Context->CreateBuffer(IndexBufferProps, ByteSpan(IndexData));
    if (!g_IndexBuffer)
    {
        RNDR_LOG_ERROR("Failed to create index Buffer!");
        return false;
    }

    ShaderGlobals Globals;
    BufferProperties ConstBufferProps;
    ConstBufferProps.BindFlag = BufferBindFlag::Constant;
    ConstBufferProps.CPUAccess = CPUAccess::None;
    ConstBufferProps.Usage = Usage::GPUReadWrite;
    ConstBufferProps.Size = sizeof(ShaderGlobals);
    ConstBufferProps.Stride = sizeof(ShaderGlobals);
    g_GlobalsConstantBuffer = g_Context->CreateBuffer(ConstBufferProps, ByteSpan(&Globals));
    if (!g_GlobalsConstantBuffer)
    {
        RNDR_LOG_ERROR("Failed to create globals constant Buffer!");
        return false;
    }

    assert(g_UIProps.MaxImageArraySize != 0);
    ImageProperties ImageArrayProps;
    ImageArrayProps.bUseMips = false;
    ImageArrayProps.CPUAccess = CPUAccess::None;
    ImageArrayProps.Usage = Usage::GPUReadWrite;
    ImageArrayProps.PixelFormat = PixelFormat::R8G8B8A8_UNORM_SRGB;
    ImageArrayProps.ImageBindFlags = ImageBindFlags::ShaderResource;
    Span<ByteSpan> InitData;
    InitData.Size = 1;
    InitData.Data = new ByteSpan[InitData.Size];
    InitData.Data[0].Size = g_UIProps.MaxImageSideSize * g_UIProps.MaxImageSideSize * 4;
    InitData.Data[0].Data = new uint8_t[InitData.Data[0].Size];
    memset(InitData.Data[0].Data, 0xFF, InitData.Data[0].Size);
    g_ImageArray = g_Context->CreateImageArray(g_UIProps.MaxImageSideSize, g_UIProps.MaxImageSideSize, g_UIProps.MaxImageArraySize, ImageArrayProps, InitData);
    if (!g_ImageArray)
    {
        RNDR_LOG_ERROR("Failed to create white image!");
        return false;
    }
    delete[] InitData.Data[0].Data;
    delete[] InitData.Data;
    g_RenderResources[0] = true;

    SamplerProperties SamplerProps;
    SamplerProps.Filter = ImageFiltering::MinMagPoint_MipLinear;
    g_Sampler = g_Context->CreateSampler(SamplerProps);
    if (!g_Sampler)
    {
        RNDR_LOG_ERROR("Failed to create sampler!");
        return false;
    }

    return true;
}

void rndr::ui::ShutDownRender()
{
    delete[] g_RenderResources.Data;

    g_Context->DestroyImage(g_ImageArray);
    g_Context->DestroySampler(g_Sampler);
    g_Context->DestroyBuffer(g_GlobalsConstantBuffer);
    g_Context->DestroyBuffer(g_IndexBuffer);
    g_Context->DestroyBuffer(g_InstanceBuffer);
    g_Context->DestroyBlendState(g_BlendState);
    g_Context->DestroyDepthStencilState(g_DepthStencilState);
    g_Context->DestroyRasterizerState(g_RasterizerState);
    g_Context->DestroyInputLayout(g_InputLayout);
    g_Context->DestroyShader(g_FragmentShader);
    g_Context->DestroyShader(g_VertexShader);
}

rndr::ui::RenderId rndr::ui::AllocateRenderId()
{
    for (int i = 0; i < g_UIProps.MaxImageArraySize; i++)
    {
        if (!g_RenderResources[i])
        {
            g_RenderResources[i] = true;
            return i;
        }
    }
    return -1;
}

void rndr::ui::FreeRenderId(RenderId Id)
{
    if (Id < 0 || Id > g_RenderResources.Size)
    {
        return;
    }
    g_RenderResources[Id] = false;
}

void rndr::ui::UpdateRenderResource(RenderId Id, ByteSpan Contents, int Width, int Height)
{
    assert(g_RenderResources[Id]);
    const math::Point2 Start;
    const math::Vector2 Size{(float)Width, (float)Height};
    g_ImageArray->Update(g_Context, Id, Start, Size, Contents);
}

math::Vector2 rndr::ui::GetRenderScreenSize()
{
    return math::Vector2{/* (float)g_Context->GetWindowFrameBuffer()->Width, (float)g_Context->GetWindowFrameBuffer()->Height*/};
}

void rndr::ui::StartRenderFrame()
{
    ShaderGlobals Globals;
    Globals.ScreenSize = GetRenderScreenSize();
    g_GlobalsConstantBuffer->Update(ByteSpan(&Globals));
}

void rndr::ui::EndRenderFrame(const Span<Box*> SortedBoxes)
{
    g_Context->BindShader(g_VertexShader);
    g_Context->BindShader(g_FragmentShader);
    g_Context->BindInputLayout(g_InputLayout);
    g_Context->BindRasterizerState(g_RasterizerState);
    g_Context->BindDepthStencilState(g_DepthStencilState);
    g_Context->BindBlendState(g_BlendState);
    g_Context->BindBuffer(g_InstanceBuffer, 0);
    g_Context->BindBuffer(g_IndexBuffer, 0);
    g_Context->BindBuffer(g_GlobalsConstantBuffer, 0, g_VertexShader);
    g_Context->BindImageAsShaderResource(g_ImageArray, 0, g_FragmentShader);
    g_Context->BindSampler(g_Sampler, 0, g_FragmentShader);
    g_Context->BindFrameBuffer(nullptr);

    std::vector<InstanceData> Data = ConvertBoxesIntoInstanceData(SortedBoxes);
    g_InstanceBuffer->Update(ByteSpan(Data));

    int Offset = 0;
    while (Offset < SortedBoxes.Size)
    {
        const int InstanceOffset = Offset;
        int RefLevel = SortedBoxes[Offset++]->Level;
        while (Offset < SortedBoxes.Size && RefLevel == SortedBoxes[Offset]->Level)
        {
            Offset++;
        }

        const int InstanceCount = Offset - InstanceOffset + 1;
        const int IndexCount = 6;
        const int IndexOffset = 0;
        g_Context->DrawIndexedInstanced(PrimitiveTopology::TriangleList, IndexCount, InstanceCount, IndexOffset, InstanceOffset);
    }
}

std::vector<rndr::ui::InstanceData> rndr::ui::ConvertBoxesIntoInstanceData(const Span<Box*> Boxes)
{
    std::vector<InstanceData> Instances;
    for (int i = 0; i < Boxes.Size; i++)
    {
        Box* B = Boxes[i];
        InstanceData Data;
        Data.BottomLeft = B->Props.BottomLeft;
        Data.TopRight = B->Props.BottomLeft + B->Props.Size;
        Data.TexCoordsBottomLeft = B->TexCoordsBottomLeft;
        Data.TexCoordsTopRight = B->TexCoordsTopRight;
        Data.Color = B->Props.Color;
        Data.RenderId = B->RenderId;
        Data.CornerRadius = B->Props.CornerRadius;
        Data.EdgeSoftness = B->Props.EdgeSoftness;
        Data.BorderThickness = B->Props.BorderThickness;
        Instances.push_back(Data);
    }
    return Instances;
}
