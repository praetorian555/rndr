#pragma once

#include "rndr/platform/windows-forward-def.hpp"
#include "rndr/types.hpp"

namespace Rndr
{
namespace Canvas
{

/**
 * Simplified data format enum covering both pixel formats and vertex attribute formats.
 * Canvas uses its own format vocabulary instead of exposing raw API-level formats.
 */
enum class Format : u8
{
    // Pixel formats.
    R8,
    RG8,
    RGB8,
    RGBA8,
    R16F,
    RG16F,
    RGBA16F,
    R32F,
    RG32F,
    RGBA32F,
    D24S8,
    D32F,

    // Vertex data formats.
    Float1,
    Float2,
    Float3,
    Float4,
    Int1,
    Int2,
    Int3,
    Int4,

    EnumCount
};

/**
 * Represents the graphics backend being alive. Thin keycard object, not a god object.
 * Created exclusively through the Init() factory. RAII: destructor tears down the GL backend.
 * Debug mode asserts that all resources are destroyed before Context.
 */
class Context
{
public:
    /**
     * Initialize the Canvas graphics backend.
     * @param window_handle Native window handle to bind the GL context to.
     * @return A valid Context object.
     * @throw Opal::InvalidArgumentException if called while a Context already exists.
     * @throw Rndr::GraphicsAPIException if the OpenGL backend fails to initialize.
     */
    [[nodiscard]] static Context Init(NativeWindowHandle window_handle);

    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&& other) noexcept;
    Context& operator=(Context&& other) noexcept;

    [[nodiscard]] bool IsValid() const;

private:
    Context();

    static bool g_context_exists;
    NativeDeviceContextHandle m_device_context = k_invalid_device_context_handle;
    NativeGraphicsContextHandle m_graphics_context = k_invalid_graphics_context_handle;
};

}  // namespace Canvas
}  // namespace Rndr
