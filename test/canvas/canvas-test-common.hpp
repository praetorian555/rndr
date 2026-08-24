#pragma once

#include <catch2/catch2.hpp>

#include "opal/container/expected.h"
#include "opal/container/ref.h"
#include "opal/container/scope-ptr.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/error-codes.hpp"
#include "rndr/generic-window.hpp"

namespace CanvasTest
{

/**
 * The value a Canvas call reported, failing the case with the code when it reported one instead. Everything
 * that is not itself about a failure goes through this, so an unexpected code names itself where it happened
 * rather than becoming a crash on the line after.
 */
template <typename T>
T Unwrap(Opal::Expected<T, Rndr::ErrorCode>&& result)
{
    const Rndr::u32 code = static_cast<Rndr::u32>(result.GetErrorOr(Rndr::ErrorCode::Success));
    INFO("Canvas reported error code " << code);
    REQUIRE(result.HasValue());
    return std::move(result).GetValue();
}

/** An application, a hidden window and a Canvas context bound to it - what every Canvas test starts from. */
inline Rndr::Canvas::Context CreateTestContext(Opal::ScopePtr<Rndr::Application>& app, Opal::Ref<Rndr::GenericWindow>& window)
{
    app = Unwrap(Rndr::Application::Create());
    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    window = Unwrap(app->CreateGenericWindow(window_desc));
    return Unwrap(Rndr::Canvas::Context::CreateContext(window.Clone()));
}

}  // namespace CanvasTest
