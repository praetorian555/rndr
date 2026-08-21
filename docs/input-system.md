# Input System

The input system handles keyboard, mouse, gamepad, and text input through a stack-based context model. It is defined in `include/rndr/input-system.hpp` and implemented in `src/input-system.cpp`.

## Core Concepts

### InputSystem

The central class that receives raw platform events via the `SystemMessageHandler` interface, queues them, and dispatches them to the context stack. It is owned by the application (no global singletons).

```cpp
Rndr::InputSystem input_system;

// Call once per frame to process hold timers, combo timeouts, and dispatch queued events.
input_system.ProcessSystemEvents(delta_seconds);
```

### InputContext

A named collection of input actions. Contexts are pushed and popped on the `InputSystem` stack. The topmost context gets the first chance to handle each event. If it consumes the event, lower contexts do not see it. If it does not handle the event, the event propagates downward.

```cpp
// A default context is always present.
Rndr::InputContext& default_ctx = input_system.GetCurrentContext();

// Push a new context for a specific game state.
Rndr::InputContext menu_context(Opal::StringUtf8("Menu"));
input_system.PushContext(menu_context);

// Pop when leaving that state.
input_system.PopContext();
```

Contexts can be enabled or disabled. A disabled context is skipped during dispatch.

```cpp
menu_context.SetEnabled(false);  // Temporarily bypass this context.
menu_context.SetEnabled(true);   // Re-enable it.
```

Contexts can be retrieved by name from the `InputSystem`. This throws an `Opal::Exception` if no context with the given name exists.

```cpp
Rndr::InputContext& ctx = input_system.GetContextByName(Opal::StringUtf8("Menu"));
```

### InputAction

A named action that groups one or more input bindings and routes them to a callback. Actions are created through a fluent builder API returned by `InputContext::AddAction()`.

Action names must be unique within a context. Adding a duplicate name throws an `Opal::Exception`.

```cpp
context.AddAction("Jump")
    .OnButton([](Rndr::Trigger trigger, bool is_repeat)
    {
        // Handle jump.
    })
    .Bind(Rndr::Key::Space, Rndr::Trigger::Pressed);
```

Actions can be removed by name:

```cpp
context.RemoveAction("Jump");
```

And queried:

```cpp
context.ContainsAction("Jump");          // Returns bool.
Rndr::InputAction* action = context.GetAction("Jump");  // Returns nullptr if not found.
```

## Binding Types

### Keyboard

Bind keyboard keys with a trigger (Pressed or Released). Multiple keys can be bound to the same action.

```cpp
context.AddAction("MoveForward")
    .OnButton([](Rndr::Trigger trigger, bool is_repeat) { /* ... */ })
    .Bind(Rndr::Key::W, Rndr::Trigger::Pressed)
    .Bind(Rndr::Key::UpArrow, Rndr::Trigger::Pressed);
```

**Callback signature:** `void(Trigger trigger, bool is_repeat)`

### Modifier Keys

Modifiers (Ctrl, Shift, Alt) are specified as flags on the binding. The system tracks modifier state automatically from key press/release events. Both left and right variants (e.g., `LeftCtrl`, `RightCtrl`) set the same modifier flag.

```cpp
// Single modifier.
context.AddAction("SelectAll")
    .OnButton(callback)
    .Bind(Rndr::Key::A, Rndr::Trigger::Pressed, Rndr::Modifiers::Ctrl);

// Combined modifiers.
context.AddAction("SaveAs")
    .OnButton(callback)
    .Bind(Rndr::Key::S, Rndr::Trigger::Pressed, Rndr::Modifiers::Ctrl | Rndr::Modifiers::Shift);
```

A binding with `Modifiers::None` (the default) matches regardless of which modifiers are held.

### Mouse Buttons

Bind mouse button press/release events. The callback receives the button, trigger, and cursor position.

```cpp
context.AddAction("Shoot")
    .OnMouseButton([](Rndr::MouseButton button, Rndr::Trigger trigger, const Rndr::Vector2i& cursor_pos)
    {
        // Handle click.
    })
    .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Pressed);
```

Available buttons: `Left`, `Right`, `Middle`, `X1`, `X2`.

Double-click events are treated as press events.

**Callback signature:** `void(MouseButton button, Trigger trigger, const Vector2i& cursor_position)`

### Mouse Movement

Bind mouse axis movement. Each axis is dispatched individually. Zero-delta axes are not dispatched. Events are not coalesced; each raw mouse move is delivered separately.

```cpp
context.AddAction("Look")
    .OnMousePosition([](Rndr::MouseAxis axis, Rndr::f32 delta)
    {
        if (axis == Rndr::MouseAxis::X) { /* horizontal */ }
        else if (axis == Rndr::MouseAxis::Y) { /* vertical */ }
    })
    .Bind(Rndr::MouseAxis::X)
    .Bind(Rndr::MouseAxis::Y);
```

**Callback signature:** `void(MouseAxis axis, f32 delta)`

### Mouse Wheel

Bind mouse wheel scrolling.

```cpp
context.AddAction("Zoom")
    .OnMouseWheel([](Rndr::f32 delta_x, Rndr::f32 delta_y)
    {
        // delta_y is the vertical scroll amount.
    })
    .Bind(Rndr::MouseAxis::WheelY);
```

**Callback signature:** `void(f32 delta_x, f32 delta_y)`

### Text Input

Bind character/text input events. These are separate from key press events (pressing `A` generates both a key event and a character event; they are dispatched to different binding types).

```cpp
context.AddAction("ChatInput")
    .OnText([](Rndr::uchar32 character)
    {
        // Handle typed character (supports Unicode).
    })
    .BindText();
```

**Callback signature:** `void(uchar32 character)`

### Hold

A hold binding fires a single callback after a key has been held down for a specified duration. It does not auto-repeat. Releasing the key before the duration cancels the hold. Re-pressing restarts the timer.

```cpp
context.AddAction("Interact")
    .OnButton(callback)
    .BindHold(Rndr::Key::E, 0.5f);  // Fires after 0.5 seconds.
```

Hold timers are updated in `ProcessSystemEvents(delta_seconds)`, so this method must be called every frame.

### Sequential Combos

A sequence of keys that must be pressed in order within a timeout window between each step. The timer resets after each successful step.

```cpp
Rndr::Key keys[] = {Rndr::Key::UpArrow, Rndr::Key::UpArrow, Rndr::Key::DownArrow};
context.AddAction("SecretCode")
    .OnButton(callback)
    .BindCombo(keys, 0.3f);  // 0.3s max between each key.
```

- Pressing the wrong key resets the combo.
- Only press events advance the combo (releases are ignored).
- After completion, the combo resets and can be triggered again.
- Combo timeouts are updated in `ProcessSystemEvents(delta_seconds)`.

### Simultaneous Combos

A combo with `timeout_seconds == 0`. Currently uses the same sequential matching logic as sequential combos but without a timeout, meaning keys must be pressed in the declared order. True order-independent simultaneous detection is not yet implemented.

```cpp
Rndr::Key keys[] = {Rndr::Key::A, Rndr::Key::B};
context.AddAction("SpecialMove")
    .OnButton(callback)
    .BindCombo(keys, 0.0f);  // No timeout.
```

### Gamepad

Up to four gamepads (`Rndr::k_max_gamepads`) are polled through XInput. Polling happens inside
`Application::ProcessSystemEvents()`, so nothing extra needs to be called from the frame loop.

```cpp
// Gamepad button.
context.AddAction("Jump")
    .OnGamepadButton([](Rndr::GamepadButton button, Rndr::Trigger trigger, Rndr::u8 gamepad_index)
    {
        // Handle jump.
    })
    .Bind(Rndr::GamepadButton::A, Rndr::Trigger::Pressed);

// Gamepad axis.
context.AddAction("MoveX")
    .OnGamepadAxis([](Rndr::GamepadAxis axis, Rndr::f32 value, Rndr::u8 gamepad_index)
    {
        // value is dead-zone corrected and in [-1, 1].
    })
    .Bind(Rndr::GamepadAxis::LeftStickX, /*dead_zone=*/ 0.15f);
```

**Callback signatures:**
`void(GamepadButton button, Trigger trigger, u8 gamepad_index)` and
`void(GamepadAxis axis, f32 value, u8 gamepad_index)`

Buttons: `A`, `B`, `X`, `Y`, `LeftBumper`, `RightBumper`, `Back`, `Start`, `LeftThumb`, `RightThumb`,
`DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight`, `LeftTrigger`, `RightTrigger`.

Axes: `LeftStickX`, `LeftStickY`, `RightStickX`, `RightStickY`, `LeftTrigger`, `RightTrigger`. Sticks
report `[-1, 1]` with up and right positive. Triggers report `[0, 1]`.

The triggers appear in both lists. They are analog, so they always report an axis value, and they also
report button press/release as the analog value crosses the platform's trigger threshold. Binding one
as a button and as an axis at the same time is fine; both fire.

#### Gamepad Index

Every binding carries a gamepad index. The default is `Rndr::k_any_gamepad`, which matches events from
every pad — a single-player game keeps working no matter which slot the pad connects on. Pass an
explicit index for local multiplayer.

```cpp
context.AddAction("PlayerTwoJump")
    .OnGamepadButton(callback)
    .Bind(Rndr::GamepadButton::A, Rndr::Trigger::Pressed, /*gamepad_index=*/ 1);

context.AddAction("PlayerTwoMove")
    .OnGamepadAxis(callback)
    .Bind(Rndr::GamepadAxis::LeftStickX, /*dead_zone=*/ -1.0f, /*gamepad_index=*/ 1);
```

Gamepad events carry no window, so the `ForWindow()` filter does not apply to them. An action with a
window filter still receives gamepad events.

#### Connection Changes

Connected slots are sampled every `ProcessSystemEvents()`. Empty slots are far more expensive to
sample, so they are only retried once a second — expect up to a second of delay before a pad plugged
in mid-session starts reporting.

```cpp
app->on_gamepad_connection_change.Bind([](Rndr::u8 gamepad_index, bool is_connected) { /* ... */ });

bool connected = app->IsGamepadConnected(0);
```

A pad that disconnects while inputs are active reports a release for every button still held and a
zero for every axis still deflected, so nothing stays stuck on.

## Multiple Callback Types Per Action

An action can have callbacks of different types set simultaneously. The correct callback is invoked based on the event type.

```cpp
context.AddAction("Interact")
    .OnButton([](Rndr::Trigger trigger, bool is_repeat)
    {
        // Fires on keyboard E press.
    })
    .OnMouseButton([](Rndr::MouseButton button, Rndr::Trigger trigger, const Rndr::Vector2i& pos)
    {
        // Fires on left mouse click.
    })
    .Bind(Rndr::Key::E, Rndr::Trigger::Pressed)
    .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Pressed);
```

## Window Filter

Actions can optionally be scoped to a specific window. This is useful for multi-window applications or editor tools where different windows should handle different inputs.

```cpp
context.AddAction("EditorClick")
    .OnMouseButton(callback)
    .Bind(Rndr::MouseButton::Left, Rndr::Trigger::Pressed)
    .ForWindow(editor_window);  // Only fires for events from this window.
```

An action with no window filter (the default) fires for events from any window. Gamepad events carry
no window, so the filter never applies to them.

## Dead Zones

The `InputSystem` has a default dead zone (0.2) for analog inputs. Individual gamepad axis bindings can override it with a per-binding value. A dead zone of `-1.0` means "use the system default".

```cpp
input_system.SetDefaultDeadZone(0.15f);

// Per-binding override.
context.AddAction("MoveX")
    .OnGamepadAxis(callback)
    .Bind(Rndr::GamepadAxis::LeftStickX, /*dead_zone=*/ 0.1f);
```

A raw value inside the dead zone dispatches nothing. Outside it, the remaining range is rescaled back
onto `[0, 1]`, so the value ramps up continuously from zero rather than jumping to the dead zone edge:

    value = sign(raw) * (|raw| - dead_zone) / (1 - dead_zone)

With a dead zone of 0.2, a raw 0.6 reports 0.5 and a raw 1.0 still reports 1.0.

Returning to center is reported once. When an axis moves from outside the dead zone to inside it, the
binding fires a single callback with a value of 0 and then stays quiet until the axis leaves the dead
zone again. Without that, a released stick would leave whatever it was driving stuck at the last
non-zero value.

## Event Flow

0. Gamepads have no event queue of their own, so `Application::ProcessSystemEvents()` samples every
   XInput slot and reports whatever changed since the previous sample.
1. Platform events arrive through `SystemMessageHandler` callbacks (`OnButtonDown`, `OnMouseMove`, etc.).
2. The `InputSystem` translates them from the legacy `InputPrimitive` enum to the new typed enums (`Key`, `MouseButton`, etc.) and queues them.
3. Modifier key state (Ctrl, Shift, Alt) is tracked on press/release.
4. `ProcessSystemEvents(delta_seconds)` dispatches all queued events and updates time-dependent state:
   - Events are dispatched to contexts top-to-bottom. The first context that handles an event consumes it.
   - Within a context, actions are checked in order. The first matching action consumes the event.
   - Hold timers are advanced by `delta_seconds`. If a hold reaches its threshold, its callback fires.
   - Combo timeouts are checked. If a combo step times out, the combo resets.

## Limitations

- **Simultaneous combos are order-dependent.** `BindCombo` with `timeout == 0` requires keys in the declared order.
- **Horizontal mouse wheel (`WheelX`)** is defined in the `MouseAxis` enum but there is no corresponding `SystemMessageHandler` event, so it cannot be bound.
- **Gamepad rumble** is not implemented. Only input is wired up, there is no `XInputSetState` path.
- **A gamepad plugged in mid-session** takes up to a second to be noticed, because empty XInput slots are
  expensive to sample and are therefore only retried once a second.
- **Rebinding/serialization** is not implemented. The API is designed to support it in the future (actions are named, bindings are data-describable).
- **Not thread-safe.** All input system operations must happen on the same thread.