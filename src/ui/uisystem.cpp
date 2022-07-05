#include "rndr/ui/uisystem.h"

#include <algorithm>
#include <vector>

#include <stb_truetype/stb_truetype.h>

#include "math/bounds2.h"
#include "math/point2.h"
#include "math/vector4.h"

#include "rndr/core/buffer.h"
#include "rndr/core/fileutils.h"
#include "rndr/core/framebuffer.h"
#include "rndr/core/graphicscontext.h"
#include "rndr/core/graphicstypes.h"
#include "rndr/core/input.h"
#include "rndr/core/log.h"
#include "rndr/core/pipeline.h"
#include "rndr/core/shader.h"

namespace rndr
{
namespace ui
{

static constexpr int kMaxInstances = 512;
static constexpr int kDefaultFontSize = 36;
static constexpr int kGlyphImageSize = 36;
static constexpr int kWhiteImageIndex = 0;

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

struct InstanceData
{
    math::Point2 BottomLeft;
    math::Point2 TopRight;
    math::Point2 TexCoordsBottomLeft;
    math::Point2 TexCoordsTopRight;
    math::Vector4 Color;
    float AtlasIndex;
};

RNDR_ALIGN(16) struct ShaderGlobals
{
    math::Vector2 ScreenSize;
};

struct Box
{
    BoxProperties Props;
    Box* Parent;
    std::vector<Box*> Children;
    int Level;
    math::Bounds2 Bounds;
    int AtlasIndex = kWhiteImageIndex;
    math::Point2 TexCoordsBottomLeft = math::Point2{0, 0};
    math::Point2 TexCoordsTopRight = math::Point2{1, 1};
};

struct GlyphInfo
{
    int Width = 0;
    int Height = 0;
    int OffsetX = 0;
    int OffsetY = 0;
    math::Point2 UVStart;
    math::Point2 UVEnd;
    ByteSpan Data;
};

struct AtlasInfo
{
    stbtt_fontinfo FontInfo;
    Span<GlyphInfo> Glyphs;
    // Factor to convert unscaled units to pixels
    float Scale;
    // Distance between baseline and the highest point of a font can have in pixels
    int Ascent;
    // Distance between baseline and the lowest point of a font, negative number in pixels
    int Descent;
    // Distance between font descent of one line and font ascent of the line beneath
    int LineGap;
    // First unicode value of the character range
    int StartCodepoint;
    // Unicode value after the last value in the character range
    int EndCodepoint;
};

static std::vector<Box*> g_Stack;
static std::vector<Box*> g_Boxes;

static math::Point2 g_PrevMousePosition;
static math::Point2 g_MousePosition;
static bool g_PrevButtonState[3];
static bool g_ButtonState[3];
static int g_PrevScrollPosition = 0;
static int g_ScrollPosition = 0;

static AtlasInfo g_Atlas;

static std::vector<InstanceData> BatchBoxes();
static void CleanupBoxes();
static void OnMouseMovement(InputPrimitive Primitive, InputTrigger Trigger, real Value);
static void OnButtonEvent(InputPrimitive Primitive, InputTrigger Trigger, real Value);
static void OnScroll(InputPrimitive Primitive, InputTrigger Trigger, real Value);

static AtlasInfo GetAtlas(const std::string& FontPath, float SizeInPixels, float ImageSize);

}  // namespace ui
}  // namespace rndr

bool rndr::ui::Init(GraphicsContext* Context, const Properties& Props)
{
    g_Context = Context;

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

    InputLayoutProperties ILProps[6];
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
    g_InputLayout = g_Context->CreateInputLayout(Span(ILProps, 6), g_VertexShader);
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

    InstanceData Instances[kMaxInstances];
    BufferProperties InstanceBufferProps;
    InstanceBufferProps.BindFlag = rndr::BufferBindFlag::Vertex;
    InstanceBufferProps.CPUAccess = rndr::CPUAccess::None;
    InstanceBufferProps.Usage = rndr::Usage::GPUReadWrite;
    InstanceBufferProps.Size = kMaxInstances * sizeof(InstanceData);
    InstanceBufferProps.Stride = sizeof(InstanceData);
    g_InstanceBuffer = g_Context->CreateBuffer(InstanceBufferProps, ByteSpan(Instances));
    if (!g_InstanceBuffer)
    {
        RNDR_LOG_ERROR("Failed to create instance Buffer!");
        return false;
    }

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

    g_Atlas = GetAtlas("C:/Windows/Fonts/consola.ttf", kDefaultFontSize, kGlyphImageSize);
    assert(g_Atlas.Glyphs);

    ImageProperties WhiteImageProps;
    WhiteImageProps.ArraySize = g_Atlas.Glyphs.Size + 1;
    WhiteImageProps.bUseMips = false;
    WhiteImageProps.CPUAccess = CPUAccess::None;
    WhiteImageProps.Usage = Usage::GPURead;
    WhiteImageProps.PixelFormat = PixelFormat::R8G8B8A8_UNORM_SRGB;
    WhiteImageProps.ImageBindFlags = ImageBindFlags::ShaderResource;
    Span<ByteSpan> InitData;
    InitData.Size = g_Atlas.Glyphs.Size + 1;
    InitData.Data = new ByteSpan[InitData.Size];
    InitData.Data[0].Size = kGlyphImageSize * kGlyphImageSize * 4;
    InitData.Data[0].Data = new uint8_t[InitData.Data[0].Size];
    memset(InitData.Data[0].Data, 0xFF, InitData.Data[0].Size);
    for (int i = 0; i < g_Atlas.Glyphs.Size; i++)
    {
        InitData[i + 1] = g_Atlas.Glyphs[i].Data;
    }
    g_ImageArray = g_Context->CreateImageArray(kGlyphImageSize, kGlyphImageSize, WhiteImageProps, InitData);
    if (!g_ImageArray)
    {
        RNDR_LOG_ERROR("Failed to create white image!");
        return false;
    }

    SamplerProperties SamplerProps;
    g_Sampler = g_Context->CreateSampler(SamplerProps);
    if (!g_Sampler)
    {
        RNDR_LOG_ERROR("Failed to create sampler!");
        return false;
    }

    Box* ScreenBox = new Box();
    BoxProperties BoxProps;
    BoxProps.BottomLeft = math::Point2{0, 0};
    BoxProps.Size = Globals.ScreenSize;
    BoxProps.Color = math::Vector4{0, 0, 0, 0};
    ScreenBox->Props = BoxProps;
    ScreenBox->Parent = nullptr;
    ScreenBox->Level = 0;
    ScreenBox->Bounds = math::Bounds2();
    g_Stack.push_back(ScreenBox);

    rndr::InputContext* InputContext = rndr::InputSystem::Get()->GetContext();
    InputContext->CreateMapping("MouseMovement", OnMouseMovement);
    InputContext->AddBinding("MouseMovement", rndr::InputPrimitive::Mouse_AxisX, rndr::InputTrigger::AxisChangedAbsolute);
    InputContext->AddBinding("MouseMovement", rndr::InputPrimitive::Mouse_AxisY, rndr::InputTrigger::AxisChangedAbsolute);
    InputContext->CreateMapping("ButtonAction", OnButtonEvent);
    InputContext->AddBinding("ButtonAction", rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::ButtonDown);
    InputContext->AddBinding("ButtonAction", rndr::InputPrimitive::Mouse_LeftButton, rndr::InputTrigger::ButtonUp);
    InputContext->AddBinding("ButtonAction", rndr::InputPrimitive::Mouse_RightButton, rndr::InputTrigger::ButtonDown);
    InputContext->AddBinding("ButtonAction", rndr::InputPrimitive::Mouse_RightButton, rndr::InputTrigger::ButtonUp);
    InputContext->CreateMapping("Scroll", OnScroll);
    InputContext->AddBinding("Scroll", rndr::InputPrimitive::Mouse_AxisWheel, rndr::InputTrigger::AxisChangedAbsolute);

    return true;
}

bool rndr::ui::ShutDown()
{
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

    return true;
}

void rndr::ui::StartFrame()
{
    ShaderGlobals Globals;
    Globals.ScreenSize.X = g_Context->GetWindowFrameBuffer()->Width;
    Globals.ScreenSize.Y = g_Context->GetWindowFrameBuffer()->Height;
    g_GlobalsConstantBuffer->Update(ByteSpan(&Globals));

    g_Stack[0]->Props.Size = Globals.ScreenSize;
    g_Stack[0]->Bounds = math::Bounds2(math::Point2{}, math::Point2{} + Globals.ScreenSize);
}

void rndr::ui::EndFrame()
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

    auto Compare = [](const Box* A, const Box* B) { return A->Level < B->Level; };
    std::sort(g_Boxes.begin(), g_Boxes.end(), Compare);
    std::vector<InstanceData> Data = BatchBoxes();
    int Offset = 0;
    while (Offset < g_Boxes.size())
    {
        const int InstanceOffset = Offset;
        int RefLevel = g_Boxes[Offset++]->Level;
        while (Offset < g_Boxes.size() && RefLevel == g_Boxes[Offset]->Level)
        {
            Offset++;
        }
        const int InstanceCount = Offset - InstanceOffset + 1;

        g_InstanceBuffer->Update(ByteSpan(Data));
        const int IndexCount = 6;
        const int IndexOffset = 0;
        g_Context->DrawIndexedInstanced(PrimitiveTopology::TriangleList, IndexCount, InstanceCount, IndexOffset, InstanceOffset);
    }

    CleanupBoxes();

    for (int i = 0; i < 3; i++)
    {
        g_PrevButtonState[i] = g_ButtonState[i];
    }
}

void rndr::ui::StartBox(const BoxProperties& Props)
{
    assert(g_Boxes.size() < kMaxInstances);
    Box* B = new Box();
    B->Props = Props;
    B->Parent = g_Stack.back();
    B->Parent->Children.push_back(B);
    B->Level = B->Parent->Level + 1;
    B->Bounds = math::Bounds2(Props.BottomLeft, Props.BottomLeft + Props.Size);
    g_Stack.push_back(B);
    g_Boxes.push_back(B);
}

void rndr::ui::EndBox()
{
    assert(g_Stack.size() > 1);
    g_Stack.pop_back();
}

void rndr::ui::DrawTextBox(const std::string& Text, const TextBoxProperties& Props)
{
    math::Point2 StartPos = Props.BaseLineStart;
    for (int i = 0; i < Text.size(); i++)
    {
        int Codepoint = Text[i];

        int Advance, Bearing;
        stbtt_GetCodepointHMetrics(&g_Atlas.FontInfo, Codepoint, &Advance, &Bearing);
        Advance *= g_Atlas.Scale;
        Bearing *= g_Atlas.Scale;

        if (Codepoint == ' ')
        {
            int KernAdvance = stbtt_GetCodepointKernAdvance(&g_Atlas.FontInfo, Codepoint, Text[i + 1]);
            KernAdvance *= g_Atlas.Scale;
            StartPos.X += Props.Scale * (Bearing + Advance + KernAdvance);
            continue;
        }

        if (Codepoint == '\n')
        {
            StartPos.X = Props.BaseLineStart.X;
            StartPos.Y -= Props.Scale * (g_Atlas.Ascent + g_Atlas.LineGap - g_Atlas.Descent);
            continue;
        }

        const GlyphInfo& Info = g_Atlas.Glyphs[Codepoint - g_Atlas.StartCodepoint];

        assert(g_Boxes.size() < kMaxInstances);
        Box* B = new Box();
        BoxProperties BoxProps;
        BoxProps.BottomLeft = StartPos;
        BoxProps.BottomLeft.X += Props.Scale * Info.OffsetX;
        BoxProps.BottomLeft.Y -= Props.Scale * (Info.Height + Info.OffsetY);
        BoxProps.Size = math::Vector2{Props.Scale * Info.Width, Props.Scale * Info.Height};
        BoxProps.Color = Props.Color;
        B->Props = BoxProps;
        B->Parent = g_Stack.back();
        B->Parent->Children.push_back(B);
        B->Level = B->Parent->Level + 1;
        B->Bounds = math::Bounds2(BoxProps.BottomLeft, BoxProps.BottomLeft + BoxProps.Size);
        B->AtlasIndex = Codepoint - g_Atlas.StartCodepoint + 1; // TODO(mkostic): This should be part of the GlyphInfo or Atlas
        B->TexCoordsBottomLeft = Info.UVStart;
        B->TexCoordsTopRight = Info.UVEnd;
        g_Boxes.push_back(B);

        int KernAdvance = stbtt_GetCodepointKernAdvance(&g_Atlas.FontInfo, Codepoint, Text[i + 1]);
        KernAdvance *= g_Atlas.Scale;
        StartPos.X += Props.Scale * (Bearing + Advance + KernAdvance);
    }
}

void rndr::ui::SetColor(const math::Vector4& Color)
{
    Box* B = g_Stack.back();
    B->Props.Color = Color;
}

void rndr::ui::SetDim(const math::Point2& BottomLeft, const math::Vector2& Size)
{
    Box* B = g_Stack.back();
    B->Props.BottomLeft = BottomLeft;
    B->Props.Size = Size;
    B->Bounds = math::Bounds2(BottomLeft, BottomLeft + Size);
}

bool rndr::ui::MouseHovers()
{
    Box* B = g_Stack.back();
    assert(B);

    const math::Bounds2 BoxBounds(B->Props.BottomLeft, B->Props.BottomLeft + B->Props.Size);
    return math::InsideInclusive(g_MousePosition, BoxBounds);
}

bool rndr::ui::LeftMouseButtonClicked()
{
    const int Index = 0;
    return MouseHovers() && g_PrevButtonState[Index] && !g_ButtonState[Index];
}

bool rndr::ui::RightMouseButtonClicked()
{
    const int Index = (int)InputPrimitive::Mouse_RightButton - (int)InputPrimitive::Mouse_LeftButton;
    return g_PrevButtonState[Index] && !g_ButtonState[Index];
}

std::vector<rndr::ui::InstanceData> rndr::ui::BatchBoxes()
{
    std::vector<InstanceData> Instances;
    for (Box* B : g_Boxes)
    {
        InstanceData Data;
        Data.BottomLeft = B->Props.BottomLeft;
        Data.TopRight = B->Props.BottomLeft + B->Props.Size;
        Data.TexCoordsBottomLeft = B->TexCoordsBottomLeft;
        Data.TexCoordsTopRight = B->TexCoordsTopRight;
        Data.Color = B->Props.Color;
        Data.AtlasIndex = B->AtlasIndex;
        Instances.push_back(Data);
    }

    return Instances;
}

void rndr::ui::CleanupBoxes()
{
    for (Box* B : g_Boxes)
    {
        delete B;
    }
    g_Boxes.clear();

    while (g_Stack.size() > 1)
        g_Stack.pop_back();
}

void rndr::ui::OnMouseMovement(InputPrimitive Primitive, InputTrigger Trigger, real Value)
{
    if (Primitive == InputPrimitive::Mouse_AxisX)
    {
        g_PrevMousePosition.X = g_MousePosition.X;
        g_MousePosition.X = Value;
    }
    else if (Primitive == InputPrimitive::Mouse_AxisY)
    {
        g_PrevMousePosition.Y = g_MousePosition.Y;
        g_MousePosition.Y = Value;
    }
    else
    {
        assert(false);
    }
}

void rndr::ui::OnButtonEvent(InputPrimitive Primitive, InputTrigger Trigger, real Value)
{
    // TODO(mkostic): Currently this doesn't handle multiple edges in one frame
    int Index = (int)Primitive - (int)InputPrimitive::Mouse_LeftButton;
    g_ButtonState[Index] = Trigger == InputTrigger::ButtonUp;
}

void rndr::ui::OnScroll(InputPrimitive Primitive, InputTrigger Trigger, real Value)
{
    g_PrevScrollPosition = g_ScrollPosition;
    g_ScrollPosition += Value;
}

rndr::ui::AtlasInfo rndr::ui::GetAtlas(const std::string& FontPath, float SizeInPixels, float ImageSize)
{
    assert(ImageSize >= SizeInPixels);

    // TODO(mkostic): ASCII table hardcode
    constexpr int kStartCodepoint = 33;
    constexpr int kEndCodepoint = 127;

    ByteSpan FileContents = ReadEntireFile(FontPath);
    assert(FileContents);

    AtlasInfo AtlasInfo;
    AtlasInfo.StartCodepoint = kStartCodepoint;
    AtlasInfo.EndCodepoint = kEndCodepoint;
    int FontOffset = stbtt_GetFontOffsetForIndex(FileContents.Data, 0);
    int Result = stbtt_InitFont(&AtlasInfo.FontInfo, FileContents.Data, FontOffset);
    if (!Result)
    {
        RNDR_LOG_ERROR("stbtt_InitFont failed!");
        return AtlasInfo;
    }

    AtlasInfo.Glyphs.Size = kEndCodepoint - kStartCodepoint;
    AtlasInfo.Glyphs.Data = new GlyphInfo[AtlasInfo.Glyphs.Size];

    AtlasInfo.Scale = stbtt_ScaleForPixelHeight(&AtlasInfo.FontInfo, SizeInPixels);

    int Ascent, Descent, LineGap;
    stbtt_GetFontVMetrics(&AtlasInfo.FontInfo, &AtlasInfo.Ascent, &AtlasInfo.Descent, &AtlasInfo.LineGap);
    AtlasInfo.Ascent *= AtlasInfo.Scale;
    AtlasInfo.Descent *= AtlasInfo.Scale;
    AtlasInfo.LineGap *= AtlasInfo.Scale;

    for (int Codepoint = kStartCodepoint; Codepoint < kEndCodepoint; Codepoint++)
    {
        const int GlyphIndex = Codepoint - kStartCodepoint;
        assert(GlyphIndex >= 0);
        GlyphInfo& Info = AtlasInfo.Glyphs[GlyphIndex];
        uint8_t* MonoData = stbtt_GetCodepointBitmap(&AtlasInfo.FontInfo, 0, AtlasInfo.Scale, Codepoint, &Info.Width, &Info.Height,
                                                     &Info.OffsetX, &Info.OffsetY);
        Info.Data.Size = ImageSize * ImageSize * 4;
        Info.Data.Data = new uint8_t[Info.Data.Size];

        constexpr uint32_t kClearValue = 0x00FFFFFF;
        for (int Y = 0; Y < ImageSize; Y++)
        {
            for (int X = 0; X < ImageSize; X++)
            {
                uint32_t* Data = (uint32_t*)Info.Data.Data;
                Data[Y * (int)ImageSize + X] = kClearValue;
            }
        }

        constexpr int kOffset = 0;
        for (int Y = 0; Y < Info.Height; Y++)
        {
            for (int X = 0; X < Info.Width; X++)
            {
                uint32_t* Data = (uint32_t*)Info.Data.Data;
                uint32_t Alpha = MonoData[Y * Info.Width + X];

                Data[(Y + kOffset) * (int)ImageSize + X + kOffset] = kClearValue | (Alpha << 24);
            }
        }

        Info.UVStart = math::Point2{0, 0};
        Info.UVEnd = math::Point2{(Info.Width + 2 * kOffset) / (float)ImageSize, (Info.Height + 2 * kOffset) / (float)ImageSize};

        stbtt_FreeBitmap(MonoData, nullptr);
    }
    return AtlasInfo;
}
