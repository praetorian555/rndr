#include "rndr/core/input.h"

#include "rndr/core/window.h"

// InputSystem ////////////////////////////////////////////////////////////////////////////////////

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
    rndr::WindowDelegates::OnButtonDelegate.Add(
        [this](rndr::Window*, rndr::InputTrigger Trigger, rndr::InputPrimitive Primitive)
        {
            for (const auto& Mapping : m_Context->Mappings)
            {
                for (const auto& Binding : Mapping.second->Bindings)
                {
                    if (Binding.Primitive == Primitive && Binding.Trigger == Trigger)
                    {
                        Mapping.second->Callback(Binding, 0, 0);
                    }
                }
            }
        });

    rndr::WindowDelegates::OnMousePositionDelegate.Add(
        [this](rndr::Window*, int X, int Y)
        {
            if (m_FirstTime)
            {
                m_FirstTime = false;
                m_X = X;
                m_Y = Y;
                return;
            }

            for (const auto& Mapping : m_Context->Mappings)
            {
                for (const auto& Binding : Mapping.second->Bindings)
                {
                    if (Binding.Primitive == InputPrimitive::Mouse_AxisX)
                    {
                        Mapping.second->Callback(Binding, X - m_X, 0);
                    }
                    else if (Binding.Primitive == InputPrimitive::Mouse_AxisY)
                    {
                        Mapping.second->Callback(Binding, 0, Y - m_Y);
                    }
                    else if (Binding.Primitive == InputPrimitive::Mouse_Position)
                    {
                        Mapping.second->Callback(Binding, X, Y);
                    }
                }
            }

            m_X = X;
            m_Y = Y;
        });
}

void rndr::InputSystem::ShutDown() {}

void rndr::InputSystem::Update(real DeltaSeconds) {}

void rndr::InputSystem::SetContext(const InputContext* Context)
{
    assert(Context);
    m_Context = Context;
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
                                    InputTrigger Trigger)
{
    const auto& It = Mappings.find(Action);
    assert(It != Mappings.end());

    InputMapping* Mapping = It->second.get();

    // TODO(mkostic): Check for duplicates
    Mapping->Bindings.push_back(InputBinding{Primitive, Trigger});
}

const rndr::InputMapping* rndr::InputContext::GetMapping(const InputAction& Action)
{
    const auto& It = Mappings.find(Action);
    return It != Mappings.end() ? It->second.get() : nullptr;
}
