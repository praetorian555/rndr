#include "rndr/core/input.h"

#include "rndr/core/log.h"
#include "rndr/core/memory.h"

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

rndr::InputSystem::InputSystem()
{
    assert(!m_Context);
    m_Context = New<InputContext>("InputSystem: Default InputContext");
}

rndr::InputSystem::~InputSystem()
{
    Delete(m_Context);
}

void rndr::InputSystem::SubmitButtonEvent(NativeWindowHandle Window,
                                          InputPrimitive Primitive,
                                          InputTrigger Trigger)
{
    const Event Evt{Window, ButtonEvent{Primitive, Trigger}};
    m_Events.push(Evt);
}

void rndr::InputSystem::SubmitMousePositionEvent(NativeWindowHandle Window,
                                                 const math::Point2& Position,
                                                 const math::Vector2& ScreenSize)
{
    assert(ScreenSize.X != 0 && ScreenSize.Y != 0);
    const Event Evt{Window, MousePositionEvent{Position, ScreenSize}};
    m_Events.push(Evt);
}

void rndr::InputSystem::SubmitRelativeMousePositionEvent(rndr::NativeWindowHandle Window,
                                                         const math::Vector2& DeltaPosition,
                                                         const math::Vector2& ScreenSize)
{
    assert(ScreenSize.X != 0 && ScreenSize.Y != 0);
    const Event Evt{Window, RelativeMousePositionEvent{DeltaPosition, ScreenSize}};
    m_Events.push(Evt);
}

void rndr::InputSystem::SubmitMouseWheelEvent(NativeWindowHandle Window, int DeltaWheel)
{
    const Event Evt{Window, MouseWheelEvent{DeltaWheel}};
    m_Events.push(Evt);
}

void rndr::InputSystem::Update(real DeltaSeconds)
{
    RNDR_UNUSED(DeltaSeconds);

    while (!m_Events.empty())
    {
        Event Evt = m_Events.front();
        m_Events.pop();

        switch (Evt.Data.index())
        {
            case 0:
                ProcessEvent(std::get<ButtonEvent>(Evt.Data));
                continue;
            case 1:
                ProcessEvent(std::get<MousePositionEvent>(Evt.Data));
                continue;
            case 2:
                ProcessEvent(std::get<RelativeMousePositionEvent>(Evt.Data));
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
                if (Binding.Trigger == InputTrigger::AxisChangedAbsolute)
                {
                    const real Value = Binding.Modifier * Event.Position.X;
                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
                }
            }
            else if (Binding.Primitive == InputPrimitive::Mouse_AxisY)
            {
                if (Binding.Trigger == InputTrigger::AxisChangedAbsolute)
                {
                    const real Value = Binding.Modifier * Event.Position.Y;
                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
                }
            }
        }
    }
}

void rndr::InputSystem::ProcessEvent(const rndr::InputSystem::RelativeMousePositionEvent& Event)
{
    for (const auto& MappingEntry : m_Context->Mappings)
    {
        for (const auto& Binding : MappingEntry.Mapping->Bindings)
        {
            if (Binding.Primitive == InputPrimitive::Mouse_AxisX)
            {
                if (Binding.Trigger == InputTrigger::AxisChangedRelative)
                {
                    const real Value = Binding.Modifier * (Event.Position.X / Event.ScreenSize.X);
                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
                }
            }
            else if (Binding.Primitive == InputPrimitive::Mouse_AxisY)
            {
                if (Binding.Trigger == InputTrigger::AxisChangedRelative)
                {
                    const real Value = Binding.Modifier * (Event.Position.Y / Event.ScreenSize.Y);
                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
                }
            }
        }
    }
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
                                               static_cast<float>(Event.DeltaWheel));
            }
        }
    }
}

rndr::InputContext* rndr::InputSystem::GetContext()
{
    return m_Context;
}

bool rndr::InputSystem::IsButton(InputPrimitive Primitive)
{
    return IsKeyboardButton(Primitive) || IsMouseButton(Primitive);
}

bool rndr::InputSystem::IsMouseButton(InputPrimitive Primitive)
{
    return Primitive == InputPrimitive::Mouse_LeftButton ||
           Primitive == InputPrimitive::Mouse_RightButton ||
           Primitive == InputPrimitive::Mouse_MiddleButton;
}

bool rndr::InputSystem::IsKeyboardButton(InputPrimitive Primitive)
{
    return Primitive > InputPrimitive::_KeyboardStart && Primitive < InputPrimitive::_KeyboardEnd;
}

bool rndr::InputSystem::IsAxis(InputPrimitive Primitive)
{
    return IsMouseAxis(Primitive);
}

bool rndr::InputSystem::IsMouseAxis(InputPrimitive Primitive)
{
    return Primitive == InputPrimitive::Mouse_AxisX || Primitive == InputPrimitive::Mouse_AxisY ||
           Primitive == InputPrimitive::Mouse_AxisWheel;
}

math::Point2 rndr::InputSystem::GetMousePosition() const
{
    assert(m_AbsolutePosition.has_value());
    return m_AbsolutePosition.value();
}

// InputContext ///////////////////////////////////////////////////////////////////////////////////

rndr::InputMapping* rndr::InputContext::CreateMapping(const InputAction& Action,
                                                      const InputCallback& Callback)
{
    std::unique_ptr<InputMapping> NewMapping = std::make_unique<InputMapping>(Action, Callback);

    const auto FindMappingByAction = [&](const InputContext::Entry& Entry)
    { return Entry.Action == Action; };

    auto EntryIt = std::find_if(Mappings.begin(), Mappings.end(), FindMappingByAction);
    if (EntryIt == Mappings.end())
    {
        Mappings.push_back(Entry{Action, std::move(NewMapping)});
        return Mappings.back().Mapping.get();
    }

    RNDR_LOG_WARNING("InputContext::CreateMapping: Overriding existing mapping entry!");
    EntryIt->Mapping = std::move(NewMapping);
    return EntryIt->Mapping.get();
}

void rndr::InputContext::AddBinding(const InputAction& Action,
                                    InputPrimitive Primitive,
                                    InputTrigger Trigger,
                                    real Modifier)
{
    const auto FindMappingByAction = [&](const InputContext::Entry& Entry)
    { return Entry.Action == Action; };

    auto EntryIt = std::find_if(Mappings.begin(), Mappings.end(), FindMappingByAction);
    if (EntryIt == Mappings.end())
    {
        RNDR_LOG_ERROR("InputContext::AddBinding: Failed to find mapping based on action %s",
                       Action.c_str());
        return;
    }

    InputMapping* Mapping = EntryIt->Mapping.get();

    const auto FindBindingMatch = [&](const InputBinding& Binding)
    { return Binding.Primitive == Primitive && Binding.Trigger == Trigger; };

    auto BindingIt =
        std::find_if(Mapping->Bindings.begin(), Mapping->Bindings.end(), FindBindingMatch);
    if (BindingIt == Mapping->Bindings.end())
    {
        Mapping->Bindings.push_back(InputBinding{Primitive, Trigger, Modifier});
    }
    else
    {
        *BindingIt = {Primitive, Trigger, Modifier};
    }
}

const rndr::InputMapping* rndr::InputContext::GetMapping(const InputAction& Action)
{
    const auto FindMappingByAction = [&](const InputContext::Entry& Entry)
    { return Entry.Action == Action; };

    auto EntryIt = std::find_if(Mappings.begin(), Mappings.end(), FindMappingByAction);
    return EntryIt != Mappings.end() ? EntryIt->Mapping.get() : nullptr;
}
