#include "rndr/core/input.h"

#include "rndr/core/memory.h"

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

rndr::InputSystem::InputSystem()
{
    assert(!m_Context);
    m_Context = RNDR_NEW(InputContext, "InputSystem: Default InputContext");
}

rndr::InputSystem::~InputSystem()
{
    RNDR_DELETE(InputContext, m_Context);
}

void rndr::InputSystem::SubmitButtonEvent(NativeWindowHandle Window,
                                          InputPrimitive Primitive,
                                          InputTrigger Trigger)
{
    Event Evt{Window, ButtonEvent{Primitive, Trigger}};
    m_Events.push(Evt);
}

void rndr::InputSystem::SubmitMousePositionEvent(NativeWindowHandle Window,
                                                 const math::Point2& Position,
                                                 const math::Vector2& ScreenSize)
{
    assert(ScreenSize.X != 0 && ScreenSize.Y != 0);
    Event Evt{Window, MousePositionEvent{Position, ScreenSize}};
    m_Events.push(Evt);
}

void rndr::InputSystem::SubmitMouseWheelEvent(NativeWindowHandle Window, int DeltaWheel)
{
    Event Evt{Window, MouseWheelEvent{DeltaWheel}};
    m_Events.push(Evt);
}

void rndr::InputSystem::Update(real DeltaSeconds)
{
    while (!m_Events.empty())
    {
        Event Evt = m_Events.front();
        m_Events.pop();

        switch (Evt.Data.index())
        {
            case 0:
                continue;
            case 1:
                ProcessEvent(std::get<ButtonEvent>(Evt.Data));
                continue;
            case 2:
                ProcessEvent(std::get<MousePositionEvent>(Evt.Data));
                continue;
            case 3:
                ProcessEvent(std::get<MouseWheelEvent>(Evt.Data));
                continue;
        }
    }
}

void rndr::InputSystem::ProcessEvent(const ButtonEvent& Event)
{
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

void rndr::InputSystem::ProcessEvent(const MousePositionEvent& Event)
{
    for (const auto& MappingEntry : m_Context->Mappings)
    {
        for (const auto& Binding : MappingEntry.Mapping->Bindings)
        {
            if (Binding.Primitive == InputPrimitive::Mouse_AxisX)
            {
                real Value = 0;
                if (Binding.Trigger == InputTrigger::AxisChangedRelative &&
                    m_AbsolutePosition.has_value())
                {
                    Value =
                        ((Event.Position.X - m_AbsolutePosition.value().X) / Event.ScreenSize.X);
                    Value *= Binding.Modifier;
                }
                else if (Binding.Trigger == InputTrigger::AxisChangedAbsolute)
                {
                    Value = Binding.Modifier * Event.Position.X;
                }
                MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
            }
            else if (Binding.Primitive == InputPrimitive::Mouse_AxisY)
            {
                real Value = 0;
                if (Binding.Trigger == InputTrigger::AxisChangedRelative &&
                    m_AbsolutePosition.has_value())
                {
                    Value =
                        ((Event.Position.Y - m_AbsolutePosition.value().Y) / Event.ScreenSize.Y);
                    Value *= Binding.Modifier;
                }
                else if (Binding.Trigger == InputTrigger::AxisChangedAbsolute)
                {
                    Value = Binding.Modifier * Event.Position.Y;
                }
                MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
            }
        }
    }

    m_AbsolutePosition = Event.Position;
}

void rndr::InputSystem::ProcessEvent(const MouseWheelEvent& Event)
{
    for (const auto& MappingEntry : m_Context->Mappings)
    {
        for (const auto& Binding : MappingEntry.Mapping->Bindings)
        {
            if (Binding.Primitive == InputPrimitive::Mouse_AxisWheel)
            {
                MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger,
                                               Event.DeltaWheel);
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

math::Point2 rndr::InputSystem::GetMousePosition() const
{
    assert(m_AbsolutePosition.has_value());
    return m_AbsolutePosition.value();
}

// InputContext ///////////////////////////////////////////////////////////////////////////////////

rndr::InputMapping* rndr::InputContext::CreateMapping(const InputAction& Action,
                                                      InputCallback Callback)
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

void rndr::InputContext::AddBinding(const InputAction& Action,
                                    InputPrimitive Primitive,
                                    InputTrigger Trigger,
                                    real Modifier)
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
