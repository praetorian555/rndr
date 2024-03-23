#pragma once

#include "rndr/core/definitions.h"
#include "rndr/core/graphics-types.h"
#include "rndr/core/platform/opengl-forward-def.h"

namespace Rndr
{

class GraphicsContext;

/**
 * Represents a state of the graphics pipeline. This includes the shaders, input layout, rasterizer
 * state, blend state, depth stencil state, and frame buffer.
 */
class Pipeline
{
public:
    /**
     * Default constructor. Creates an invalid pipeline.
     */
    Pipeline() = default;

    /**
     * Creates a new pipeline.
     * @param graphics_context The graphics context to create the pipeline with.
     * @param desc The description of the pipeline to create.
     */
    Pipeline(const GraphicsContext& graphics_context, const PipelineDesc& desc);

    ~Pipeline();
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&& other) noexcept;
    Pipeline& operator=(Pipeline&& other) noexcept;

    void Destroy();

    [[nodiscard]] bool IsValid() const;

    [[nodiscard]] const PipelineDesc& GetDesc() const;
    [[nodiscard]] GLuint GetNativeShaderProgram() const;
    [[nodiscard]] GLuint GetNativeVertexArray() const;
    [[nodiscard]] bool IsIndexBufferBound() const;
    [[nodiscard]] uint32_t GetIndexBufferElementSize() const;

private:
    PipelineDesc m_desc;
    GLuint m_native_shader_program = k_invalid_opengl_object;
    GLuint m_native_vertex_array = k_invalid_opengl_object;
};

}  // namespace Rndr
