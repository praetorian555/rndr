#include "rndr/ui/uisystem.h"

#include <vector>

#include "math/bounds2.h"
#include "math/point2.h"
#include "math/vector4.h"

#include "rndr/core/buffer.h"
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

static GraphicsContext* g_Context = nullptr;
static Shader* g_VertexShader = nullptr;
static Shader* g_FragmentShader = nullptr;
static InputLayout* g_InputLayout = nullptr;
static RasterizerState* g_RasterizerState = nullptr;
static DepthStencilState* g_DepthStencilState = nullptr;
static BlendState* g_BlendState = nullptr;

static Buffer* g_VertexBuffer = nullptr;
static Buffer* g_InstanceBuffer = nullptr;
static Buffer* g_IndexBuffer = nullptr;
static Buffer* g_GlobalsConstantBuffer = nullptr;

static constexpr int kMaxInstances = 512;

struct InstanceData
{
    math::Point2 BottomLeft;
    math::Point2 TopRight;
    math::Vector4 Color;
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
};

static std::vector<Box*> g_Stack;
static std::vector<Box*> g_Boxes;

static math::Point2 g_PrevMousePosition;
static math::Point2 g_MousePosition;
static bool g_PrevButtonState[3];
static bool g_ButtonState[3];
static int g_PrevScrollPosition = 0;
static int g_ScrollPosition = 0;

static std::vector<InstanceData> BatchBoxes(int Level = 1);
static void CleanupBoxes();
static void OnMouseMovement(InputPrimitive Primitive, InputTrigger Trigger, real Value);
static void OnButtonEvent(InputPrimitive Primitive, InputTrigger Trigger, real Value);
static void OnScroll(InputPrimitive Primitive, InputTrigger Trigger, real Value);

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

    InputLayoutProperties ILProps[4];
    ILProps[0].SemanticName = "POSITION";
    ILProps[0].SemanticIndex = 0;
    ILProps[0].InputSlot = 0;
    ILProps[0].Format = rndr::PixelFormat::R32G32_FLOAT;
    ILProps[0].OffsetInVertex = 0;
    ILProps[0].Repetition = rndr::DataRepetition::PerVertex;
    ILProps[0].InstanceStepRate = 0;
    ILProps[1].SemanticName = "POSITION";
    ILProps[1].SemanticIndex = 1;
    ILProps[1].InputSlot = 1;
    ILProps[1].Format = rndr::PixelFormat::R32G32_FLOAT;
    ILProps[1].OffsetInVertex = 0;
    ILProps[1].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[1].InstanceStepRate = 1;
    ILProps[2].SemanticName = "POSITION";
    ILProps[2].SemanticIndex = 2;
    ILProps[2].InputSlot = 1;
    ILProps[2].Format = rndr::PixelFormat::R32G32_FLOAT;
    ILProps[2].OffsetInVertex = AppendAlignedElement;
    ILProps[2].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[2].InstanceStepRate = 1;
    ILProps[3].SemanticName = "COLOR";
    ILProps[3].SemanticIndex = 0;
    ILProps[3].InputSlot = 1;
    ILProps[3].Format = rndr::PixelFormat::R32G32B32A32_FLOAT;
    ILProps[3].OffsetInVertex = AppendAlignedElement;
    ILProps[3].Repetition = rndr::DataRepetition::PerInstance;
    ILProps[3].InstanceStepRate = 1;
    g_InputLayout = g_Context->CreateInputLayout(Span(ILProps, 4), g_VertexShader);
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

    math::Vector2 VertexData[] = {math::Vector2{-1, -1}, math::Vector2{1, -1}, math::Vector2{-1, 1}, math::Vector2{1, 1}};

    BufferProperties VertexBufferProps;
    VertexBufferProps.BindFlag = BufferBindFlag::Vertex;
    VertexBufferProps.CPUAccess = CPUAccess::None;
    VertexBufferProps.Usage = Usage::GPUReadWrite;
    VertexBufferProps.Size = sizeof(VertexData);
    VertexBufferProps.Stride = sizeof(math::Vector2);
    g_VertexBuffer = g_Context->CreateBuffer(VertexBufferProps, ByteSpan(VertexData));
    if (!g_VertexBuffer)
    {
        RNDR_LOG_ERROR("Failed to create vertex Buffer!");
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
    g_Context->DestroyBuffer(g_GlobalsConstantBuffer);
    g_Context->DestroyBuffer(g_IndexBuffer);
    g_Context->DestroyBuffer(g_InstanceBuffer);
    g_Context->DestroyBuffer(g_VertexBuffer);
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
    g_Context->BindBuffer(g_VertexBuffer, 0);
    g_Context->BindBuffer(g_InstanceBuffer, 1);
    g_Context->BindBuffer(g_IndexBuffer, 0);
    g_Context->BindBuffer(g_GlobalsConstantBuffer, 0, g_VertexShader);
    g_Context->BindFrameBuffer(nullptr);

    int TargetLevel = 1;
    while (true)
    {
        std::vector<InstanceData> Data = BatchBoxes(TargetLevel++);
        if (!Data.empty())
        {
            g_InstanceBuffer->Update(ByteSpan(Data));
            g_Context->DrawIndexedInstanced(PrimitiveTopology::TriangleList, 6, Data.size());
        }
        else
        {
            break;
        }
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

std::vector<rndr::ui::InstanceData> rndr::ui::BatchBoxes(int Level)
{
    std::vector<InstanceData> Instances;
    for (Box* B : g_Boxes)
    {
        if (B->Level != Level)
        {
            continue;
        }
        InstanceData Data;
        Data.BottomLeft = B->Props.BottomLeft;
        Data.TopRight = B->Props.BottomLeft + B->Props.Size;
        Data.Color = B->Props.Color;
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
