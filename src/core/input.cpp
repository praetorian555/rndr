#include <unordered_map>
#include <utility>
#include <variant>

#include "rndr/core/array.h"
#include "rndr/core/hash-map.h"
#include "rndr/core/input.h"

namespace rndr::InputPrivate
{
struct ActionAndCallback
{
    rndr::InputAction action;
    rndr::InputCallback callback;
};

struct BindingHash
{
    std::size_t operator()(const InputBinding binding) const noexcept
    {
        const std::size_t h1 = static_cast<std::size_t>(binding.primitive);
        const std::size_t h2 = static_cast<std::size_t>(binding.trigger);
        return h1 + (h2 + 1) * 1000u;
    }
};

struct ActionHash
{
    std::size_t operator()(const InputAction& action) const noexcept
    {
        return std::hash<String>{}(action.GetName());
    }
};

using BindingMap = rndr::HashMap<InputBinding, Array<ActionAndCallback>, BindingHash>;

struct ContextData
{
    rndr::HashMap<InputAction, InputCallback, ActionHash> callbacks;
    rndr::HashMap<InputAction, Array<InputBinding>, ActionHash> bindings;
    BindingMap binding_to_actions;
    InputContext* context = nullptr;
};

struct ButtonData
{
    InputPrimitive primitive;
    InputTrigger trigger;
};

struct MousePositionData
{
    math::Point2 position;
    math::Vector2 screen_size;
};

struct RelativeMousePositionData
{
    math::Vector2 delta_position;
    math::Vector2 screen_size;
};

struct MouseWheelData
{
    int32_t delta_wheel;
};

using EventData =
    std::variant<ButtonData, MousePositionData, RelativeMousePositionData, MouseWheelData>;

struct Event
{
    OpaquePtr native_window;
    EventData data;
};

struct SystemData
{
    InputContext default_context = InputContext("Default");
    Array<ContextData*> contexts;
    Array<Event> events;
};

SystemData* g_system_data;
}  // namespace rndr::InputPrivate

// InputAction /////////////////////////////////////////////////////////////////////////////////////

rndr::InputAction::InputAction(String name) : m_name(std::move(name)) {}

const rndr::String& rndr::InputAction::GetName() const
{
    return m_name;
}

bool rndr::InputAction::IsValid() const
{
    return !m_name.empty();
}

bool rndr::InputAction::operator==(const rndr::InputAction& other) const
{
    return m_name == other.m_name;
}

bool rndr::InputAction::operator!=(const rndr::InputAction& other) const
{
    return m_name != other.m_name;
}

// InputBinding ////////////////////////////////////////////////////////////////////////////////////

bool rndr::InputBinding::operator==(const rndr::InputBinding& other) const
{
    return primitive == other.primitive && trigger == other.trigger;
}

bool rndr::InputBinding::operator!=(const rndr::InputBinding& other) const
{
    return !(*this == other);
}

// InputContext ////////////////////////////////////////////////////////////////////////////////////

rndr::InputContext::InputContext(String name) : m_name(std::move(name))
{
    InputPrivate::ContextData* data = RNDR_DEFAULT_NEW(InputPrivate::ContextData, "Context data");
    assert(data != nullptr);
    m_context_data = data;
    data->context = this;
}

rndr::InputContext::~InputContext()
{
    using namespace InputPrivate;
    if (m_context_data != nullptr)
    {
        ContextData* data = static_cast<ContextData*>(m_context_data);
        RNDR_DELETE(ContextData, data);
        m_context_data = nullptr;
    }
}

rndr::InputContext::InputContext(const rndr::InputContext& other)
{
    using namespace InputPrivate;
    if (this == &other)
    {
        return;
    }
    ContextData* data = static_cast<ContextData*>(other.m_context_data);
    if (data == nullptr)
    {
        m_context_data = nullptr;
        return;
    }
    ContextData* new_data = RNDR_DEFAULT_NEW(ContextData, "Context data");
    assert(new_data != nullptr);
    new_data->callbacks = data->callbacks;
    new_data->bindings = data->bindings;
    new_data->binding_to_actions = data->binding_to_actions;
    new_data->context = this;
    m_context_data = new_data;
    m_name = other.m_name;
}

rndr::InputContext& rndr::InputContext::operator=(const rndr::InputContext& other)
{
    using namespace InputPrivate;
    if (this == &other)
    {
        return *this;
    }
    ContextData* other_data = static_cast<ContextData*>(other.m_context_data);
    ContextData* data = static_cast<ContextData*>(m_context_data);
    if (data != nullptr)
    {
        RNDR_DELETE(ContextData, data);
        data = nullptr;
    }
    m_name = other.m_name;
    if (other_data == nullptr)
    {
        m_context_data = nullptr;
        return *this;
    }
    ContextData* new_data = RNDR_DEFAULT_NEW(ContextData, "Context data");
    assert(new_data != nullptr);
    new_data->callbacks = other_data->callbacks;
    new_data->bindings = other_data->bindings;
    new_data->binding_to_actions = other_data->binding_to_actions;
    new_data->context = this;
    m_context_data = new_data;
    return *this;
}

rndr::InputContext::InputContext(rndr::InputContext&& other) noexcept
{
    if (this == &other)
    {
        return;
    }
    m_context_data = other.m_context_data;
    InputPrivate::ContextData* data = static_cast<InputPrivate::ContextData*>(m_context_data);
    if (data != nullptr)
    {
        data->context = this;
    }
    m_name = other.m_name;
    other.m_context_data = nullptr;
    other.m_name = "";
}

rndr::InputContext& rndr::InputContext::operator=(rndr::InputContext&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    using namespace InputPrivate;
    ContextData* data = static_cast<ContextData*>(m_context_data);
    if (data != nullptr)
    {
        RNDR_DELETE(ContextData, data);
        data = nullptr;
    }
    m_context_data = other.m_context_data;
    m_name = other.m_name;
    if (data != nullptr)
    {
        data->context = this;
    }
    other.m_context_data = nullptr;
    other.m_name = "";
    return *this;
}

const rndr::String& rndr::InputContext::GetName() const
{
    return m_name;
}

bool rndr::InputContext::AddAction(const rndr::InputAction& action,
                                   const rndr::InputCallback& callback,
                                   const rndr::Span<rndr::InputBinding>& bindings)
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return false;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return false;
    }
    if (callback == nullptr)
    {
        RNDR_LOG_ERROR("Invalid callback");
        return false;
    }
    InputPrivate::ContextData* data = static_cast<InputPrivate::ContextData*>(m_context_data);
    if (data->callbacks.find(action) != data->callbacks.end())
    {
        RNDR_LOG_ERROR("Action already exists");
        return false;
    }
    data->callbacks[action] = callback;
    data->bindings[action] = Array<InputBinding>(bindings.begin(), bindings.end());
    for (const InputBinding& binding : bindings)
    {
        data->binding_to_actions[binding].push_back({action, callback});
    }
    return true;
}

bool rndr::InputContext::AddBindingToAction(const rndr::InputAction& action,
                                            const rndr::InputBinding& binding)
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return false;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return false;
    }

    InputPrivate::ContextData* data = static_cast<InputPrivate::ContextData*>(m_context_data);
    if (data->callbacks.find(action) == data->callbacks.end())
    {
        RNDR_LOG_ERROR("Action does not exist");
        return false;
    }
    const InputCallback& callback = data->callbacks[action];
    Array<InputBinding>& bindings = data->bindings[action];
    auto match_binding = std::find_if(bindings.begin(),
                                      bindings.end(),
                                      [&binding](const InputBinding& b) { return b == binding; });
    if (match_binding != bindings.cend())
    {
        *match_binding = binding;
        return true;
    }
    else
    {
        bindings.push_back(binding);
    }

    data->binding_to_actions[binding].push_back({action, callback});
    return true;
}

bool rndr::InputContext::RemoveBindingFromAction(const rndr::InputAction& action,
                                                 const rndr::InputBinding& binding)
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return false;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return false;
    }

    InputPrivate::ContextData* data = static_cast<InputPrivate::ContextData*>(m_context_data);
    if (data->callbacks.find(action) == data->callbacks.end())
    {
        RNDR_LOG_ERROR("Action does not exist");
        return false;
    }
    Array<InputBinding>& bindings = data->bindings[action];
    auto match_binding = std::find_if(bindings.begin(),
                                      bindings.end(),
                                      [&binding](const InputBinding& b) { return b == binding; });
    if (match_binding == bindings.cend())
    {
        return false;
    }
    bindings.erase(match_binding);

    auto& actions = data->binding_to_actions[binding];
    std::erase_if(actions,
                  [&action](const InputPrivate::ActionAndCallback& ac)
                  { return ac.action == action; });

    return true;
}

bool rndr::InputContext::ContainsAction(const rndr::InputAction& action) const
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return false;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return false;
    }
    InputPrivate::ContextData* data = static_cast<InputPrivate::ContextData*>(m_context_data);
    return data->callbacks.find(action) != data->callbacks.end();
}

rndr::InputCallback rndr::InputContext::GetActionCallback(const rndr::InputAction& action) const
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return nullptr;
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return nullptr;
    }
    InputPrivate::ContextData* data = static_cast<InputPrivate::ContextData*>(m_context_data);
    const auto& callback_iter = data->callbacks.find(action);
    return callback_iter != data->callbacks.end() ? callback_iter->second : nullptr;
}

rndr::Span<rndr::InputBinding> rndr::InputContext::GetActionBindings(
    const rndr::InputAction& action) const
{
    if (m_context_data == nullptr)
    {
        RNDR_LOG_ERROR("Invalid context data");
        return {};
    }
    if (!action.IsValid())
    {
        RNDR_LOG_ERROR("Invalid action");
        return {};
    }
    InputPrivate::ContextData* data = static_cast<InputPrivate::ContextData*>(m_context_data);
    const auto& bindings_iter = data->bindings.find(action);
    return bindings_iter != data->bindings.end() ? bindings_iter->second : Span<InputBinding>{};
}

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

bool rndr::InputSystem::Init()
{
    using namespace InputPrivate;
    if (g_system_data != nullptr)
    {
        return true;
    }
    g_system_data = RNDR_NEW(SystemData, "Input system");
    ContextData* data = static_cast<ContextData*>(g_system_data->default_context.m_context_data);
    g_system_data->contexts.push_back(data);
    return true;
}

bool rndr::InputSystem::Destroy()
{
    using namespace InputPrivate;
    if (g_system_data == nullptr)
    {
        return true;
    }
    g_system_data->contexts.clear();
    RNDR_DELETE(SystemData, g_system_data);
    g_system_data = nullptr;
    return true;
}

rndr::InputContext& rndr::InputSystem::GetCurrentContext()
{
    assert(InputPrivate::g_system_data != nullptr);
    return *InputPrivate::g_system_data->contexts.back()->context;
}

bool rndr::InputSystem::PushContext(const rndr::InputContext& context)
{
    assert(InputPrivate::g_system_data != nullptr);
    InputPrivate::ContextData* data =
        static_cast<InputPrivate::ContextData*>(context.m_context_data);
    InputPrivate::g_system_data->contexts.push_back(data);
    return true;
}

bool rndr::InputSystem::PopContext()
{
    assert(InputPrivate::g_system_data != nullptr);
    InputPrivate::g_system_data->contexts.pop_back();
    return true;
}

bool rndr::InputSystem::SubmitButtonEvent(OpaquePtr window,
                                          InputPrimitive primitive,
                                          InputTrigger trigger)
{
    assert(InputPrivate::g_system_data != nullptr);
    const InputPrivate::Event event{window, InputPrivate::ButtonData{primitive, trigger}};
    InputPrivate::g_system_data->events.push_back(event);
    return true;
}

bool rndr::InputSystem::SubmitMousePositionEvent(OpaquePtr window,
                                                 const math::Point2& position,
                                                 const math::Vector2& screen_size)
{
    assert(InputPrivate::g_system_data != nullptr);
    if (screen_size.X != 0 && screen_size.Y != 0)
    {
        return false;
    }
    const InputPrivate::Event event{window, InputPrivate::MousePositionData{position, screen_size}};
    InputPrivate::g_system_data->events.push_back(event);
    return true;
}

bool rndr::InputSystem::SubmitRelativeMousePositionEvent(OpaquePtr window,
                                                         const math::Vector2& delta_position,
                                                         const math::Vector2& screen_size)
{
    assert(InputPrivate::g_system_data != nullptr);
    if (screen_size.X != 0 && screen_size.Y != 0)
    {
        return false;
    }
    const InputPrivate::Event event{
        window,
        InputPrivate::RelativeMousePositionData{delta_position, screen_size}};
    InputPrivate::g_system_data->events.push_back(event);
    return true;
}

bool rndr::InputSystem::SubmitMouseWheelEvent(OpaquePtr window, int32_t delta_wheel)
{
    const InputPrivate::Event event{window, InputPrivate::MouseWheelData{delta_wheel}};
    InputPrivate::g_system_data->events.push_back(event);
    return true;
}

// void rndr::InputSystem::Update(real DeltaSeconds)
//{
//    RNDR_UNUSED(DeltaSeconds);
//
//    while (!m_Events.empty())
//    {
//        Event Evt = m_Events.front();
//        m_Events.pop();
//
//        switch (Evt.Data.index())
//        {
//            case 0:
//                ProcessEvent(std::get<ButtonEvent>(Evt.Data));
//                continue;
//            case 1:
//                ProcessEvent(std::get<MousePositionEvent>(Evt.Data));
//                continue;
//            case 2:
//                ProcessEvent(std::get<RelativeMousePositionEvent>(Evt.Data));
//                continue;
//            case 3:
//                ProcessEvent(std::get<MouseWheelEvent>(Evt.Data));
//                continue;
//        }
//    }
//}
//
// void rndr::InputSystem::ProcessEvent(const ButtonEvent& Event)
//{
//    for (const auto& MappingEntry : m_Context->Mappings)
//    {
//        for (const auto& Binding : MappingEntry.Mapping->Bindings)
//        {
//            if (Binding.Primitive == Event.Primitive && Binding.Trigger == Event.Trigger)
//            {
//                const real Value = Binding.Modifier;
//                MappingEntry.Mapping->Callback(Event.Primitive, Event.Trigger, Value);
//            }
//        }
//    }
//}
//
// void rndr::InputSystem::ProcessEvent(const MousePositionEvent& Event)
//{
//    for (const auto& MappingEntry : m_Context->Mappings)
//    {
//        for (const auto& Binding : MappingEntry.Mapping->Bindings)
//        {
//            if (Binding.Primitive == InputPrimitive::Mouse_AxisX)
//            {
//                if (Binding.Trigger == InputTrigger::AxisChangedAbsolute)
//                {
//                    const real Value = Binding.Modifier * Event.Position.X;
//                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
//                }
//            }
//            else if (Binding.Primitive == InputPrimitive::Mouse_AxisY)
//            {
//                if (Binding.Trigger == InputTrigger::AxisChangedAbsolute)
//                {
//                    const real Value = Binding.Modifier * Event.Position.Y;
//                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
//                }
//            }
//        }
//    }
//}
//
// void rndr::InputSystem::ProcessEvent(const rndr::InputSystem::RelativeMousePositionEvent& Event)
//{
//    for (const auto& MappingEntry : m_Context->Mappings)
//    {
//        for (const auto& Binding : MappingEntry.Mapping->Bindings)
//        {
//            if (Binding.Primitive == InputPrimitive::Mouse_AxisX)
//            {
//                if (Binding.Trigger == InputTrigger::AxisChangedRelative)
//                {
//                    const real Value = Binding.Modifier * (Event.Position.X / Event.ScreenSize.X);
//                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
//                }
//            }
//            else if (Binding.Primitive == InputPrimitive::Mouse_AxisY)
//            {
//                if (Binding.Trigger == InputTrigger::AxisChangedRelative)
//                {
//                    const real Value = Binding.Modifier * (Event.Position.Y / Event.ScreenSize.Y);
//                    MappingEntry.Mapping->Callback(Binding.Primitive, Binding.Trigger, Value);
//                }
//            }
//        }
//    }
//}
//
// void rndr::InputSystem::ProcessEvent(const MouseWheelEvent& Event)
//{
//    for (const auto& MappingEntry : m_Context->Mappings)
//    {
//        for (const auto& Binding : MappingEntry.Mapping->Bindings)
//        {
//            if (Binding.Primitive == InputPrimitive::Mouse_AxisWheel)
//            {
//                MappingEntry.Mapping->Callback(Binding.Primitive,
//                                               Binding.Trigger,
//                                               static_cast<float>(Event.DeltaWheel));
//            }
//        }
//    }
//}
//
// rndr::InputContext* rndr::InputSystem::GetContext()
//{
//    return m_Context;
//}
//
// bool rndr::InputSystem::IsButton(InputPrimitive Primitive)
//{
//    return IsKeyboardButton(Primitive) || IsMouseButton(Primitive);
//}
//
// bool rndr::InputSystem::IsMouseButton(InputPrimitive Primitive)
//{
//    return Primitive == InputPrimitive::Mouse_LeftButton
//           || Primitive == InputPrimitive::Mouse_RightButton
//           || Primitive == InputPrimitive::Mouse_MiddleButton;
//}
//
// bool rndr::InputSystem::IsKeyboardButton(InputPrimitive Primitive)
//{
//    return Primitive > InputPrimitive::_KeyboardStart && Primitive < InputPrimitive::_KeyboardEnd;
//}
//
// bool rndr::InputSystem::IsAxis(InputPrimitive Primitive)
//{
//    return IsMouseAxis(Primitive);
//}
//
// bool rndr::InputSystem::IsMouseAxis(InputPrimitive Primitive)
//{
//    return Primitive == InputPrimitive::Mouse_AxisX || Primitive == InputPrimitive::Mouse_AxisY
//           || Primitive == InputPrimitive::Mouse_AxisWheel;
//}
//
// math::Point2 rndr::InputSystem::GetMousePosition() const
//{
//    assert(m_AbsolutePosition.has_value());
//    return m_AbsolutePosition.value();
//}
//
//// InputContext
//////////////////////////////////////////////////////////////////////////////////////
//
// rndr::InputMapping* rndr::InputContext::CreateMapping(const InputAction& Action,
//                                                      const InputCallback& Callback)
//{
//    std::unique_ptr<InputMapping> NewMapping = std::make_unique<InputMapping>(Action, Callback);
//
//    const auto FindMappingByAction = [&](const InputContext::Entry& Entry)
//    { return Entry.Action == Action; };
//
//    auto EntryIt = std::find_if(Mappings.begin(), Mappings.end(), FindMappingByAction);
//    if (EntryIt == Mappings.end())
//    {
//        Mappings.push_back(Entry{Action, std::move(NewMapping)});
//        return Mappings.back().Mapping.get();
//    }
//
//    RNDR_LOG_WARNING("InputContext::CreateMapping: Overriding existing mapping entry!");
//    EntryIt->Mapping = std::move(NewMapping);
//    return EntryIt->Mapping.get();
//}
//
// void rndr::InputContext::AddBinding(const InputAction& Action,
//                                    InputPrimitive Primitive,
//                                    InputTrigger Trigger,
//                                    real Modifier)
//{
//    const auto FindMappingByAction = [&](const InputContext::Entry& Entry)
//    { return Entry.Action == Action; };
//
//    auto EntryIt = std::find_if(Mappings.begin(), Mappings.end(), FindMappingByAction);
//    if (EntryIt == Mappings.end())
//    {
//        RNDR_LOG_ERROR("InputContext::AddBinding: Failed to find mapping based on action %s",
//                       Action.c_str());
//        return;
//    }
//
//    InputMapping* Mapping = EntryIt->Mapping.get();
//
//    const auto FindBindingMatch = [&](const InputBinding& Binding)
//    { return Binding.Primitive == Primitive && Binding.Trigger == Trigger; };
//
//    auto BindingIt =
//        std::find_if(Mapping->Bindings.begin(), Mapping->Bindings.end(), FindBindingMatch);
//    if (BindingIt == Mapping->Bindings.end())
//    {
//        Mapping->Bindings.push_back(InputBinding{Primitive, Trigger, Modifier});
//    }
//    else
//    {
//        *BindingIt = {Primitive, Trigger, Modifier};
//    }
//}
//
// const rndr::InputMapping* rndr::InputContext::GetMapping(const InputAction& Action)
//{
//    const auto FindMappingByAction = [&](const InputContext::Entry& Entry)
//    { return Entry.Action == Action; };
//
//    auto EntryIt = std::find_if(Mappings.begin(), Mappings.end(), FindMappingByAction);
//    return EntryIt != Mappings.end() ? EntryIt->Mapping.get() : nullptr;
//}
// rndr::InputSystem* rndr::InputSystem::Get()
//{
//    static InputSystem s_input_system;
//    return &s_input_system;
//}
