#include "rndr/core/input.h"

#include <queue>

#include "rndr/core/window.h"

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

struct ButtonEvent
{
    rndr::InputPrimitive Primitive;
    rndr::InputTrigger Trigger;
};

struct MousePositionEvent
{
    int X, Y;
};

struct MouseWheelEvent
{
    int DeltaWheel;
};

static std::queue<ButtonEvent> g_ButtonEvents;
static std::queue<MousePositionEvent> g_MousePositionEvents;
static std::queue<MouseWheelEvent> g_MouseWheelEvents;

std::unique_ptr<rndr::InputSystem> rndr::InputSystem::s_Input;

rndr::InputSystem* rndr::InputSystem::Get()
{
    if (!s_Input)
    {
        s_Input.reset(new InputSystem());
    }
    return s_Input.get();
}

void rndr::InputSystem::Init()
{
    m_Context = new InputContext{};

    rndr::WindowDelegates::OnButtonDelegate.Add(
        [this](rndr::Window*, rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger) {
            g_ButtonEvents.push(ButtonEvent{Primitive, Trigger});
        });

    rndr::WindowDelegates::OnMousePositionDelegate.Add(
        [this](rndr::Window*, int X, int Y) {
            g_MousePositionEvents.push(MousePositionEvent{X, Y});
        });

    rndr::WindowDelegates::OnMouseWheelMovedDelegate.Add(
        [this](rndr::Window*, int DeltaWheel)
        { g_MouseWheelEvents.push(MouseWheelEvent{DeltaWheel}); });
}

void rndr::InputSystem::ShutDown() {}

void rndr::InputSystem::Update(real DeltaSeconds)
{
    while (!g_ButtonEvents.empty())
    {
        const ButtonEvent& Event = g_ButtonEvents.front();
        g_ButtonEvents.pop();

        for (const auto& Mapping : m_Context->Mappings)
        {
            for (const auto& Binding : Mapping.second->Bindings)
            {
                if (Binding.Primitive == Event.Primitive && Binding.Trigger == Event.Trigger)
                {
                    const real Value = Binding.Modifier;
                    Mapping.second->Callback(Event.Primitive, Event.Trigger, Value);
                }
            }
        }
    }

    while (!g_MousePositionEvents.empty())
    {
        const MousePositionEvent& Event = g_MousePositionEvents.front();
        g_MousePositionEvents.pop();

        if (m_FirstTime)
        {
            m_FirstTime = false;
            m_X = Event.X;
            m_Y = Event.Y;
            continue;
        }

        for (const auto& Mapping : m_Context->Mappings)
        {
            for (const auto& Binding : Mapping.second->Bindings)
            {
                if (Binding.Primitive == InputPrimitive::Mouse_AxisX)
                {
                    const real Value =
                        Binding.Modifier * ((Event.X - m_X) / (real)m_Window->GetWidth());
                    Mapping.second->Callback(Binding.Primitive, Binding.Trigger, Value);
                }
                else if (Binding.Primitive == InputPrimitive::Mouse_AxisY)
                {
                    const real Value =
                        Binding.Modifier * ((Event.Y - m_Y) / (real)m_Window->GetHeight());
                    Mapping.second->Callback(Binding.Primitive, Binding.Trigger, Value);
                }
            }
        }

        m_X = Event.X;
        m_Y = Event.Y;
    }

    while (!g_MouseWheelEvents.empty())
    {
        const MouseWheelEvent& Event = g_MouseWheelEvents.front();
        g_MouseWheelEvents.pop();

        for (const auto& Mapping : m_Context->Mappings)
        {
            for (const auto& Binding : Mapping.second->Bindings)
            {
                if (Binding.Primitive == InputPrimitive::Mouse_AxisWheel)
                {
                    Mapping.second->Callback(Binding.Primitive, Binding.Trigger, Event.DeltaWheel);
                }
            }
        }
    }
}

void rndr::InputSystem::SetWindow(const Window* Window)
{
    assert(Window);
    m_Window = Window;
}

rndr::InputContext* rndr::InputSystem::GetContext()
{
    return m_Context;
}

bool rndr::InputSystem::IsButton(InputPrimitive Primitive) const
{
    if (IsKeyboardButton(Primitive))
    {
        return true;
    }

    if (IsMouseButton(Primitive))
    {
        return true;
    }

    return false;
}

bool rndr::InputSystem::IsMouseButton(InputPrimitive Primitive) const
{
    if (Primitive == InputPrimitive::Mouse_LeftButton ||
        Primitive == InputPrimitive::Mouse_RightButton ||
        Primitive == InputPrimitive::Mouse_MiddleButton)
    {
        return true;
    }

    return false;
}

bool rndr::InputSystem::IsKeyboardButton(InputPrimitive Primitive) const
{
    if (Primitive > InputPrimitive::_KeyboardStart && Primitive < InputPrimitive::_KeyboardEnd)
    {
        return true;
    }

    return false;
}

bool rndr::InputSystem::IsAxis(InputPrimitive Primitive) const
{
    if (IsMouseAxis(Primitive))
    {
        return true;
    }

    return false;
}

bool rndr::InputSystem::IsMouseAxis(InputPrimitive Primitive) const
{
    if (Primitive == InputPrimitive::Mouse_AxisX || Primitive == InputPrimitive::Mouse_AxisY ||
        Primitive == InputPrimitive::Mouse_AxisWheel)
    {
        return true;
    }

    return false;
}

rndr::Point2i rndr::InputSystem::GetMousePosition() const
{
    return rndr::Point2i(m_X, m_Y);
}

// InputContext ///////////////////////////////////////////////////////////////////////////////////

rndr::InputMapping* rndr::InputContext::CreateMapping(const InputAction& Action,
                                                      InputCallback Callback)
{
    const auto& It = Mappings.find(Action);
    assert(It == Mappings.end());

    std::unique_ptr<InputMapping> Mapping = std::make_unique<InputMapping>(Action, Callback);
    auto& MappingIt = Mappings.insert(std::make_pair(Action, std::move(Mapping)));

    return nullptr;
}

void rndr::InputContext::AddBinding(const InputAction& Action,
                                    InputPrimitive Primitive,
                                    InputTrigger Trigger,
                                    real Modifier)
{
    const auto& It = Mappings.find(Action);
    assert(It != Mappings.end());

    InputMapping* Mapping = It->second.get();

    // TODO(mkostic): Check for duplicates
    Mapping->Bindings.push_back(InputBinding{Primitive, Trigger, Modifier});
}

const rndr::InputMapping* rndr::InputContext::GetMapping(const InputAction& Action)
{
    const auto& It = Mappings.find(Action);
    return It != Mappings.end() ? It->second.get() : nullptr;
}
