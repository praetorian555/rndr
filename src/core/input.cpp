#include "rndr/core/input.h"

#include <queue>

#include "rndr/core/memory.h"
#include "rndr/core/window.h"

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

struct ButtonEvent
{
    rndr::Window* OriginWindow = nullptr;
    rndr::InputPrimitive Primitive;
    rndr::InputTrigger Trigger;
};

struct MousePositionEvent
{
    rndr::Window* OriginWindow = nullptr;
    int X, Y;
};

struct MouseWheelEvent
{
    rndr::Window* OriginWindow = nullptr;
    int DeltaWheel;
};

static std::queue<ButtonEvent> g_ButtonEvents;
static std::queue<MousePositionEvent> g_MousePositionEvents;
static std::queue<MouseWheelEvent> g_MouseWheelEvents;

rndr::InputSystem::InputSystem()
{
    assert(!m_Context);
    m_Context = RNDR_NEW(InputContext, "InputSystem: Default InputContext");

    rndr::WindowDelegates::OnButtonDelegate.Add(
        [this](rndr::Window* Win, rndr::InputPrimitive Primitive, rndr::InputTrigger Trigger) {
            g_ButtonEvents.push(ButtonEvent{Win, Primitive, Trigger});
        });

    rndr::WindowDelegates::OnMousePositionDelegate.Add(
        [this](rndr::Window* Win, int X, int Y) {
            g_MousePositionEvents.push(MousePositionEvent{Win, X, Y});
        });

    rndr::WindowDelegates::OnMouseWheelMovedDelegate.Add(
        [this](rndr::Window* Win, int DeltaWheel) {
            g_MouseWheelEvents.push(MouseWheelEvent{Win, DeltaWheel});
        });
}

rndr::InputSystem::~InputSystem()
{
    RNDR_DELETE(InputContext, m_Context);
}

void rndr::InputSystem::Update(real DeltaSeconds)
{
    while (!g_ButtonEvents.empty())
    {
        const ButtonEvent& Event = g_ButtonEvents.front();
        g_ButtonEvents.pop();

        for (const auto& MappingEntry : m_Context->Mappings)
        {
            for (const auto& Binding : MappingEntry.Mapping->Bindings)
            {
                if (Binding.Primitive == Event.Primitive && Binding.Trigger == Event.Trigger)
                {
                    const real Value = Binding.Modifier;
                    MappingEntry.Mapping->Callback(Event.Primitive, Event.Trigger, Value);
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

        for (const auto& MappingEntry : m_Context->Mappings)
        {
            for (const auto& Binding : MappingEntry.Mapping->Bindings)
            {
                if (Binding.Primitive == InputPrimitive::Mouse_AxisX)
                {
                    real Value = 0;
                    if (Binding.Trigger == InputTrigger::AxisChangedRelative)
                    {
                        Value = Binding.Modifier * ((Event.X - m_X) / (real)Event.OriginWindow->GetWidth());
                    }
                    else if (Binding.Trigger == InputTrigger::AxisChangedAbsolute)
                    {
                        Value = Event.X;
                    }
                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
                }
                else if (Binding.Primitive == InputPrimitive::Mouse_AxisY)
                {
                    real Value = 0;
                    if (Binding.Trigger == InputTrigger::AxisChangedRelative)
                    {
                        real Value = Binding.Modifier * ((Event.Y - m_Y) / (real)Event.OriginWindow->GetHeight());
                    }
                    else if (Binding.Trigger == InputTrigger::AxisChangedAbsolute)
                    {
                        Value = Event.Y;
                    }
                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
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

        for (const auto& MappingEntry : m_Context->Mappings)
        {
            for (const auto& Binding : MappingEntry.Mapping->Bindings)
            {
                if (Binding.Primitive == InputPrimitive::Mouse_AxisWheel)
                {
                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Event.DeltaWheel);
                }
            }
        }
    }
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
    if (Primitive == InputPrimitive::Mouse_LeftButton || Primitive == InputPrimitive::Mouse_RightButton ||
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

math::Point2 rndr::InputSystem::GetMousePosition() const
{
    return math::Point2(m_X, m_Y);
}

// InputContext ///////////////////////////////////////////////////////////////////////////////////

rndr::InputMapping* rndr::InputContext::CreateMapping(const InputAction& Action, InputCallback Callback)
{
    std::unique_ptr<InputMapping> Mapping = std::make_unique<InputMapping>(Action, Callback);

    for (Entry& E : Mappings)
    {
        if (E.Action == Action)
        {
            E.Mapping = std::move(Mapping);
            return E.Mapping.get();
        }
    }

    Mappings.push_back(Entry{Action, std::move(Mapping)});
    return Mappings.back().Mapping.get();
}

void rndr::InputContext::AddBinding(const InputAction& Action, InputPrimitive Primitive, InputTrigger Trigger, real Modifier)
{
    for (const Entry& E : Mappings)
    {
        if (E.Action == Action)
        {
            InputMapping* Mapping = E.Mapping.get();
            // TODO(mkostic): Check for duplicates
            Mapping->Bindings.push_back(InputBinding{Primitive, Trigger, Modifier});
        }
    }
}

const rndr::InputMapping* rndr::InputContext::GetMapping(const InputAction& Action)
{
    for (const Entry& E : Mappings)
    {
        if (E.Action == Action)
        {
            return E.Mapping.get();
        }
    }
    return nullptr;
}
