#include <queue>
#include <utility>
#include <variant>

#include "rndr/core/array.h"
#include "rndr/core/hash-map.h"
#include "rndr/core/input.h"
#include "rndr/core/stack-array.h"

namespace rndr::InputPrivate
{
struct ActionData
{
    rndr::InputAction action;
    rndr::InputCallback callback;
    rndr::OpaquePtr native_window;
    rndr::Array<rndr::InputBinding> bindings;
};

struct ContextData
{
    rndr::Array<ActionData> actions;
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

using EventQueue = std::queue<Event>;

struct SystemData
{
    InputContext default_context = InputContext("Default");
    Array<ContextData*> contexts;
    EventQueue events;
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
    ContextData* context_data = static_cast<ContextData*>(other.m_context_data);
    if (context_data == nullptr)
    {
        m_context_data = nullptr;
        return;
    }
    ContextData* new_context_data = RNDR_DEFAULT_NEW(ContextData, "Context data");
    assert(new_context_data != nullptr);
    new_context_data->actions = context_data->actions;
    new_context_data->context = this;
    m_context_data = new_context_data;
    m_name = other.m_name;
}

rndr::InputContext& rndr::InputContext::operator=(const rndr::InputContext& other)
{
    using namespace InputPrivate;
    if (this == &other)
    {
        return *this;
    }
    ContextData* other_context_data = static_cast<ContextData*>(other.m_context_data);
    ContextData* context_data = static_cast<ContextData*>(m_context_data);
    if (context_data != nullptr)
    {
        RNDR_DELETE(ContextData, context_data);
        context_data = nullptr;
    }
    m_name = other.m_name;
    if (other_context_data == nullptr)
    {
        m_context_data = nullptr;
        return *this;
    }
    ContextData* new_context_data = RNDR_DEFAULT_NEW(ContextData, "Context data");
    assert(new_context_data != nullptr);
    new_context_data->actions = other_context_data->actions;
    new_context_data->context = this;
    m_context_data = new_context_data;
    return *this;
}

rndr::InputContext::InputContext(rndr::InputContext&& other) noexcept
{
    using namespace InputPrivate;
    if (this == &other)
    {
        return;
    }
    m_context_data = other.m_context_data;
    ContextData* context_data = static_cast<InputPrivate::ContextData*>(m_context_data);
    if (context_data != nullptr)
    {
        context_data->context = this;
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
    ContextData* context_data = static_cast<ContextData*>(m_context_data);
    if (context_data != nullptr)
    {
        RNDR_DELETE(ContextData, context_data);
        context_data = nullptr;
    }
    m_context_data = other.m_context_data;
    m_name = other.m_name;
    if (context_data != nullptr)
    {
        context_data->context = this;
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
                                   const rndr::InputActionData& data)
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
    if (data.callback == nullptr)
    {
        RNDR_LOG_ERROR("Invalid callback");
        return false;
    }
    using namespace InputPrivate;
    ContextData* context_data = static_cast<ContextData*>(m_context_data);
    for (const ActionData& action_data : context_data->actions)
    {
        if (action_data.action == action)
        {
            RNDR_LOG_ERROR("Action already exists");
            return false;
        }
    }
    ActionData action_data;
    action_data.action = action;
    action_data.callback = data.callback;
    action_data.native_window = data.native_window;
    action_data.bindings.assign(data.bindings.begin(), data.bindings.end());
    context_data->actions.push_back(action_data);
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

    using namespace InputPrivate;
    ContextData* context_data = static_cast<ContextData*>(m_context_data);
    for (ActionData& action_data : context_data->actions)
    {
        if (action_data.action == action)
        {
            for (InputBinding& existing_binding : action_data.bindings)
            {
                if (existing_binding == binding)
                {
                    existing_binding = binding;
                    return true;
                }
            }
            action_data.bindings.push_back(binding);
            return true;
        }
    }
    RNDR_LOG_ERROR("Action does not exist");
    return false;
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

    using namespace InputPrivate;
    ContextData* context_data = static_cast<ContextData*>(m_context_data);
    for (ActionData& action_data : context_data->actions)
    {
        if (action_data.action == action)
        {
            std::erase_if(action_data.bindings,
                          [&binding](const InputBinding& existing_binding)
                          { return existing_binding == binding; });
            return true;
        }
    }
    RNDR_LOG_ERROR("Action does not exist");
    return false;
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
    using namespace InputPrivate;
    ContextData* context_data = static_cast<ContextData*>(m_context_data);
    return std::ranges::any_of(context_data->actions,
                               [&action](const auto& action_data)
                               { return action_data.action == action; });
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
    using namespace InputPrivate;
    ContextData* context_data = static_cast<ContextData*>(m_context_data);
    const auto& action_iter = std::ranges::find_if(context_data->actions,
                                                   [&action](const auto& action_data)
                                                   { return action_data.action == action; });
    return action_iter != context_data->actions.end() ? action_iter->callback : nullptr;
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
    using namespace InputPrivate;
    ContextData* context_data = static_cast<ContextData*>(m_context_data);
    const auto& action_iter = std::ranges::find_if(context_data->actions,
                                                   [&action](const auto& action_data)
                                                   { return action_data.action == action; });
    return action_iter != context_data->actions.end() ? action_iter->bindings
                                                      : Span<InputBinding>{};
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
    if (InputPrivate::g_system_data->contexts.size() == 1)
    {
        RNDR_LOG_ERROR("Cannot pop default context");
        return false;
    }
    InputPrivate::g_system_data->contexts.pop_back();
    return true;
}

bool rndr::InputSystem::SubmitButtonEvent(OpaquePtr window,
                                          InputPrimitive primitive,
                                          InputTrigger trigger)
{
    assert(InputPrivate::g_system_data != nullptr);
    const InputPrivate::Event event{window, InputPrivate::ButtonData{primitive, trigger}};
    InputPrivate::g_system_data->events.push(event);
    return true;
}

bool rndr::InputSystem::SubmitMousePositionEvent(OpaquePtr window,
                                                 const math::Point2& position,
                                                 const math::Vector2& screen_size)
{
    assert(InputPrivate::g_system_data != nullptr);
    if (screen_size.X == 0 || screen_size.Y == 0)
    {
        return false;
    }
    const InputPrivate::Event event{window, InputPrivate::MousePositionData{position, screen_size}};
    InputPrivate::g_system_data->events.push(event);
    return true;
}

bool rndr::InputSystem::SubmitRelativeMousePositionEvent(OpaquePtr window,
                                                         const math::Vector2& delta_position,
                                                         const math::Vector2& screen_size)
{
    assert(InputPrivate::g_system_data != nullptr);
    if (screen_size.X == 0 || screen_size.Y == 0)
    {
        return false;
    }
    const InputPrivate::Event event{
        window,
        InputPrivate::RelativeMousePositionData{delta_position, screen_size}};
    InputPrivate::g_system_data->events.push(event);
    return true;
}

bool rndr::InputSystem::SubmitMouseWheelEvent(OpaquePtr window, int32_t delta_wheel)
{
    const InputPrivate::Event event{window, InputPrivate::MouseWheelData{delta_wheel}};
    InputPrivate::g_system_data->events.push(event);
    return true;
}

namespace rndr::InputPrivate
{
void ProcessEvent(const ContextData& context, OpaquePtr native_window, const ButtonData& event);
void ProcessEvent(const ContextData& context,
                  OpaquePtr native_window,
                  const MousePositionData& event);
void ProcessEvent(const ContextData& context,
                  OpaquePtr native_window,
                  const RelativeMousePositionData& event);
void ProcessEvent(const ContextData& context, OpaquePtr native_window, const MouseWheelData& event);
}  // namespace rndr::InputPrivate

bool rndr::InputSystem::ProcessEvents(real delta_seconds)
{
    RNDR_UNUSED(delta_seconds);

    assert(InputPrivate::g_system_data != nullptr);
    InputPrivate::EventQueue& events = InputPrivate::g_system_data->events;
    const InputPrivate::ContextData& context = *InputPrivate::g_system_data->contexts.back();
    while (!events.empty())
    {
        InputPrivate::Event event = events.front();
        events.pop();

        switch (event.data.index())
        {
            case 0:
                ProcessEvent(context,
                             event.native_window,
                             std::get<InputPrivate::ButtonData>(event.data));
                continue;
            case 1:
                ProcessEvent(context,
                             event.native_window,
                             std::get<InputPrivate::MousePositionData>(event.data));
                continue;
            case 2:
                ProcessEvent(context,
                             event.native_window,
                             std::get<InputPrivate::RelativeMousePositionData>(event.data));
                continue;
            case 3:
                ProcessEvent(context,
                             event.native_window,
                             std::get<InputPrivate::MouseWheelData>(event.data));
                continue;
        }
    }
    return true;
}

void rndr::InputPrivate::ProcessEvent(const ContextData& context,
                                      OpaquePtr native_window,
                                      const ButtonData& event)
{
    for (const ActionData& action_data : context.actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == event.primitive && binding.trigger == event.trigger)
            {
                action_data.callback(event.primitive, event.trigger, binding.modifier);
                break;
            }
        }
    }
}

void rndr::InputPrivate::ProcessEvent(const ContextData& context,
                                      OpaquePtr native_window,
                                      const MousePositionData& event)
{
    const StackArray<InputPrimitive, 2> axes = {InputPrimitive::Mouse_AxisX,
                                                InputPrimitive::Mouse_AxisY};
    const InputTrigger trigger = InputTrigger::AxisChangedAbsolute;
    for (const ActionData& action_data : context.actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == axes[0] && binding.trigger == trigger)
            {
                const real value = binding.modifier * event.position[0];
                action_data.callback(axes[0], trigger, value);
            }
            if (binding.primitive == axes[1] && binding.trigger == trigger)
            {
                const real value = binding.modifier * event.position[1];
                action_data.callback(axes[1], trigger, value);
            }
        }
    }
}

void rndr::InputPrivate::ProcessEvent(const ContextData& context,
                                      OpaquePtr native_window,
                                      const RelativeMousePositionData& event)
{
    const StackArray<InputPrimitive, 2> axes = {InputPrimitive::Mouse_AxisX,
                                                InputPrimitive::Mouse_AxisY};
    const InputTrigger trigger = InputTrigger::AxisChangedRelative;
    for (const ActionData& action_data : context.actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == axes[0] && binding.trigger == trigger)
            {
                const real value = binding.modifier * event.delta_position[0];
                action_data.callback(axes[0], trigger, value);
            }
            if (binding.primitive == axes[1] && binding.trigger == trigger)
            {
                const real value = binding.modifier * event.delta_position[1];
                action_data.callback(axes[1], trigger, value);
            }
        }
    }
}

void rndr::InputPrivate::ProcessEvent(const ContextData& context,
                                      OpaquePtr native_window,
                                      const MouseWheelData& event)
{
    const InputPrimitive primitive = InputPrimitive::Mouse_AxisWheel;
    const InputTrigger trigger = InputTrigger::AxisChangedRelative;
    for (const ActionData& action_data : context.actions)
    {
        if (action_data.native_window != nullptr && action_data.native_window != native_window)
        {
            continue;
        }
        for (const InputBinding& binding : action_data.bindings)
        {
            if (binding.primitive == primitive && binding.trigger == trigger)
            {
                const real value = binding.modifier * static_cast<real>(event.delta_wheel);
                action_data.callback(primitive, trigger, value);
                break;
            }
        }
    }
}
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
