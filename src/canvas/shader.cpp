#include "rndr/canvas/shader.hpp"

#include "glad/glad.h"

#include "rndr/canvas/gl-result.hpp"
#include "rndr/core/shader-compiler.hpp"
#include "opal/exceptions.h"
#include "opal/file-system.h"

#include "rndr/file.hpp"
#include "rndr/log.hpp"
#include "rndr/trace.hpp"

#include <cstring>

namespace
{

GLenum ToGLShaderType(Rndr::ShaderStage stage)
{
    switch (stage)
    {
        case Rndr::ShaderStage::Vertex:
            return GL_VERTEX_SHADER;
        case Rndr::ShaderStage::Fragment:
            return GL_FRAGMENT_SHADER;
        case Rndr::ShaderStage::Compute:
            return GL_COMPUTE_SHADER;
        default:
            return 0;
    }
}

// ---------------------------------------------------------------------------
// Vertex layout extraction from CompileResult vertex inputs.
// ---------------------------------------------------------------------------

Rndr::Canvas::Attrib AttribFromName(const char* name)
{
    if (strcmp(name, "position") == 0 || strcmp(name, "pos") == 0)
    {
        return Rndr::Canvas::Attrib::Position;
    }
    if (strcmp(name, "normal") == 0 || strcmp(name, "norm") == 0)
    {
        return Rndr::Canvas::Attrib::Normal;
    }
    if (strcmp(name, "uv") == 0 || strcmp(name, "texcoord") == 0 || strcmp(name, "texCoord") == 0 || strcmp(name, "tex_coord") == 0)
    {
        return Rndr::Canvas::Attrib::UV;
    }
    if (strcmp(name, "color") == 0 || strcmp(name, "col") == 0)
    {
        return Rndr::Canvas::Attrib::Color;
    }
    if (strcmp(name, "tangent") == 0 || strcmp(name, "tan") == 0)
    {
        return Rndr::Canvas::Attrib::Tangent;
    }
    return Rndr::Canvas::Attrib::EnumCount;
}

Rndr::Canvas::Format FormatFromVertexInput(const Rndr::VertexInputAttribute& input)
{
    if (input.scalar_type == Rndr::ScalarType::Float32)
    {
        switch (input.component_count)
        {
            case 1:
                return Rndr::Canvas::Format::Float1;
            case 2:
                return Rndr::Canvas::Format::Float2;
            case 3:
                return Rndr::Canvas::Format::Float3;
            case 4:
                return Rndr::Canvas::Format::Float4;
            default:
                return Rndr::Canvas::Format::EnumCount;
        }
    }
    if (input.scalar_type == Rndr::ScalarType::Int32)
    {
        switch (input.component_count)
        {
            case 1:
                return Rndr::Canvas::Format::Int1;
            case 2:
                return Rndr::Canvas::Format::Int2;
            case 3:
                return Rndr::Canvas::Format::Int3;
            case 4:
                return Rndr::Canvas::Format::Int4;
            default:
                return Rndr::Canvas::Format::EnumCount;
        }
    }
    return Rndr::Canvas::Format::EnumCount;
}

Opal::Expected<Rndr::Canvas::VertexLayout, Rndr::ErrorCode> BuildVertexLayout(
    const Opal::DynamicArray<Rndr::VertexInputAttribute>& vertex_inputs)
{
    using ResultType = Opal::Expected<Rndr::Canvas::VertexLayout, Rndr::ErrorCode>;
    Rndr::Canvas::VertexLayout vertex_layout;

    for (Rndr::u64 i = 0; i < vertex_inputs.GetSize(); ++i)
    {
        const Rndr::VertexInputAttribute& input = vertex_inputs[i];

        const Rndr::Canvas::Attrib attrib = AttribFromName(input.name.GetData());
        if (attrib == Rndr::Canvas::Attrib::EnumCount)
        {
            RNDR_LOG_ERROR("Canvas: Cannot map vertex attribute '{}' to a known Attrib semantic", *input.name);
            return ResultType(Rndr::ErrorCode::ShaderCompilationError);
        }

        const Rndr::Canvas::Format format = FormatFromVertexInput(input);
        if (format == Rndr::Canvas::Format::EnumCount)
        {
            RNDR_LOG_ERROR("Canvas: Unsupported vertex attribute format for '{}'", *input.name);
            return ResultType(Rndr::ErrorCode::ShaderCompilationError);
        }

        vertex_layout.Add(attrib, format);
    }

    return ResultType(std::move(vertex_layout));
}

// ---------------------------------------------------------------------------
// OpenGL shader and program creation.
// ---------------------------------------------------------------------------

Opal::Expected<GLuint, Rndr::ErrorCode> CreateShaderFromGlsl(GLenum stage, const void* glsl_data, size_t glsl_size)
{
    using ResultType = Opal::Expected<GLuint, Rndr::ErrorCode>;
    // Slang's blob size may include a trailing null terminator; trim it so we pass GL the exact
    // source length and don't feed an embedded null into the compiler.
    const auto* source = static_cast<const GLchar*>(glsl_data);
    GLint length = static_cast<GLint>(glsl_size);
    while (length > 0 && source[length - 1] == '\0')
    {
        --length;
    }

    const GLuint shader = glCreateShader(stage);
    if (shader == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL shader");
        return ResultType(Rndr::ErrorCode::GraphicsAPIError);
    }

    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE)
    {
        GLint log_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        Opal::StringUtf8 log;
        if (log_len > 0)
        {
            log.Resize(static_cast<Rndr::u64>(log_len));
            glGetShaderInfoLog(shader, log_len, &log_len, log.GetData());
        }
        glDeleteShader(shader);

        RNDR_LOG_ERROR("Canvas: Failed to compile shader:\n{}", *log);
        return ResultType(Rndr::ErrorCode::ShaderCompilationError);
    }

    return ResultType(shader);
}

Opal::Expected<GLuint, Rndr::ErrorCode> LinkProgram(GLuint vertex_shader, GLuint fragment_shader)
{
    using ResultType = Opal::Expected<GLuint, Rndr::ErrorCode>;
    const GLuint program = glCreateProgram();
    if (program == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL program");
        return ResultType(Rndr::ErrorCode::GraphicsAPIError);
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE)
    {
        GLint log_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        Opal::StringUtf8 log;
        if (log_len > 0)
        {
            log.Resize(static_cast<Rndr::u64>(log_len));
            glGetProgramInfoLog(program, log_len, &log_len, log.GetData());
        }
        glDeleteProgram(program);

        RNDR_LOG_ERROR("Canvas: Failed to link shader program:\n{}", *log);
        return ResultType(Rndr::ErrorCode::ShaderLinkingError);
    }

    return ResultType(program);
}

Opal::Expected<GLuint, Rndr::ErrorCode> LinkProgram(GLuint shader)
{
    using ResultType = Opal::Expected<GLuint, Rndr::ErrorCode>;
    const GLuint program = glCreateProgram();
    if (program == 0)
    {
        RNDR_LOG_ERROR("Canvas: Failed to create GL program");
        return ResultType(Rndr::ErrorCode::GraphicsAPIError);
    }

    glAttachShader(program, shader);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE)
    {
        GLint log_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        Opal::StringUtf8 log;
        if (log_len > 0)
        {
            log.Resize(static_cast<Rndr::u64>(log_len));
            glGetProgramInfoLog(program, log_len, &log_len, log.GetData());
        }
        glDeleteProgram(program);

        RNDR_LOG_ERROR("Canvas: Failed to link shader program:\n{}", *log);
        return ResultType(Rndr::ErrorCode::ShaderLinkingError);
    }

    return ResultType(program);
}

// ---------------------------------------------------------------------------
// Core build: compile stages via ShaderCompiler, link GL program.
// ---------------------------------------------------------------------------

struct ShaderBuildResult
{
    GLuint program = 0;
    Opal::StringUtf8 vertex_entry;
    Opal::StringUtf8 fragment_entry;
    Opal::DynamicArray<Rndr::ShaderParameter> parameters;
    Rndr::Canvas::VertexLayout vertex_layout;
    Rndr::NumThreads num_threads;
};

/**
 * Take the value of a compiler step, or leave the enclosing function with the code it reported. Every step of
 * a build is one of these, and a helper per step would be five; what the caller needs to know beyond the code
 * is already in the log line the step wrote. Expects a local ResultType, which both builders below declare.
 */
#define RNDR_CANVAS_TAKE(destination, expression)                 \
    {                                                             \
        auto step_result = (expression);                          \
        if (!step_result.HasValue())                              \
        {                                                         \
            RNDR_LOG_ERROR("Canvas: compiling the shader failed"); \
            return ResultType(step_result.GetError());            \
        }                                                         \
        (destination) = std::move(step_result.GetValue());        \
    }

Opal::Expected<ShaderBuildResult, Rndr::ErrorCode> BuildFromSingleSource(const Opal::StringUtf8& source, Opal::StringUtf8 debug_name)
{
    using ResultType = Opal::Expected<ShaderBuildResult, Rndr::ErrorCode>;

    // ShaderCompiler is shared between the rendering APIs and reports the same way Canvas does, so the codes
    // below travel out as they are. What went wrong is already in the log by the time one arrives.
    bool is_compute = false;
    Rndr::CompileResult cs_result;
    Opal::StringUtf8 vs_entry;
    Opal::StringUtf8 fs_entry;
    Rndr::CompileResult vs_result;
    Rndr::CompileResult fs_result;
    Opal::DynamicArray<Rndr::ShaderParameter> merged;
    {
        Rndr::ShaderCompiler compiler;
        const Rndr::ErrorCode load_status = compiler.LoadModule(source, Rndr::ShaderOutputFormat::Glsl);
        if (load_status != Rndr::ErrorCode::Success)
        {
            RNDR_LOG_ERROR("Canvas: compiling the shader failed");
            return ResultType(load_status);
        }
        auto entries_result = compiler.DiscoverEntryPoints();
        if (!entries_result.HasValue())
        {
            RNDR_LOG_ERROR("Canvas: compiling the shader failed");
            return ResultType(entries_result.GetError());
        }
        const Opal::DynamicArray<Rndr::EntryPointInfo>& entries = entries_result.GetValue();

        // Count entry points by stage.
        int vertex_count = 0;
        int fragment_count = 0;
        int compute_count = 0;
        for (Rndr::u64 i = 0; i < entries.GetSize(); ++i)
        {
            switch (entries[i].stage)
            {
                case Rndr::ShaderStage::Vertex:
                    ++vertex_count;
                    break;
                case Rndr::ShaderStage::Fragment:
                    ++fragment_count;
                    break;
                case Rndr::ShaderStage::Compute:
                    ++compute_count;
                    break;
                default:
                    break;
            }
        }

        if (compute_count > 0 && (vertex_count > 0 || fragment_count > 0))
        {
            RNDR_LOG_ERROR("Canvas: Shader source contains both compute and graphics entry points");
            return ResultType(Rndr::ErrorCode::ShaderCompilationError);
        }

        is_compute = compute_count > 0;
        if (is_compute)
        {
            Opal::StringUtf8 cs_entry;
            RNDR_CANVAS_TAKE(cs_entry, Rndr::ShaderCompiler::FindSingleEntryPoint(entries, Rndr::ShaderStage::Compute, "compute"))
            RNDR_CANVAS_TAKE(cs_result, compiler.CompileEntryPoint(cs_entry))
        }
        else
        {
            RNDR_CANVAS_TAKE(vs_entry, Rndr::ShaderCompiler::FindSingleEntryPoint(entries, Rndr::ShaderStage::Vertex, "vertex"))
            RNDR_CANVAS_TAKE(fs_entry, Rndr::ShaderCompiler::FindSingleEntryPoint(entries, Rndr::ShaderStage::Fragment, "fragment"))
            RNDR_CANVAS_TAKE(vs_result, compiler.CompileEntryPoint(vs_entry))
            RNDR_CANVAS_TAKE(fs_result, compiler.CompileEntryPoint(fs_entry))
            RNDR_CANVAS_TAKE(merged, Rndr::ShaderCompiler::MergeParameters(vs_result.parameters, fs_result.parameters))
        }
    }

    // Compute path.
    if (is_compute)
    {
        if (cs_result.stage != Rndr::ShaderStage::Compute)
        {
            RNDR_LOG_ERROR("Canvas: Compute entry point does not have [shader(\"compute\")] annotation");
            return ResultType(Rndr::ErrorCode::ShaderCompilationError);
        }

        const auto cs_gl_result = CreateShaderFromGlsl(GL_COMPUTE_SHADER, cs_result.code.GetData(), cs_result.code.GetSize());
        if (!cs_gl_result.HasValue())
        {
            return ResultType(cs_gl_result.GetError());
        }
        const GLuint cs = cs_gl_result.GetValue();
        const Opal::StringUtf8 shader_name = debug_name + " - Compute Shader";
        glObjectLabel(GL_SHADER, cs, static_cast<GLsizei>(shader_name.GetSize()), *shader_name);

        const auto program_result = LinkProgram(cs);
        glDeleteShader(cs);
        if (!program_result.HasValue())
        {
            return ResultType(program_result.GetError());
        }
        const GLuint program = program_result.GetValue();
        const Opal::StringUtf8 program_name = debug_name + " - Shader Program";
        glObjectLabel(GL_PROGRAM, program, static_cast<GLsizei>(program_name.GetSize()), *program_name);

        ShaderBuildResult out;
        out.program = program;
        out.parameters = std::move(cs_result.parameters);
        out.num_threads = cs_result.num_threads;
        return ResultType(std::move(out));
    }

    // Graphics path.
    if (vs_result.stage != Rndr::ShaderStage::Vertex)
    {
        RNDR_LOG_ERROR("Canvas: Vertex entry point does not have [shader(\"vertex\")] annotation");
        return ResultType(Rndr::ErrorCode::ShaderCompilationError);
    }
    if (fs_result.stage != Rndr::ShaderStage::Fragment)
    {
        RNDR_LOG_ERROR("Canvas: Fragment entry point does not have [shader(\"fragment\")] annotation");
        return ResultType(Rndr::ErrorCode::ShaderCompilationError);
    }

    const auto vs_gl_result = CreateShaderFromGlsl(GL_VERTEX_SHADER, vs_result.code.GetData(), vs_result.code.GetSize());
    if (!vs_gl_result.HasValue())
    {
        return ResultType(vs_gl_result.GetError());
    }
    const GLuint vs = vs_gl_result.GetValue();
    const Opal::StringUtf8 vertex_shader_name = debug_name + " - Vertex Shader";
    glObjectLabel(GL_SHADER, vs, static_cast<GLsizei>(vertex_shader_name.GetSize()), *vertex_shader_name);

    const auto fs_gl_result = CreateShaderFromGlsl(GL_FRAGMENT_SHADER, fs_result.code.GetData(), fs_result.code.GetSize());
    if (!fs_gl_result.HasValue())
    {
        glDeleteShader(vs);
        return ResultType(fs_gl_result.GetError());
    }
    const GLuint fs = fs_gl_result.GetValue();
    const Opal::StringUtf8 fragment_shader_name = debug_name + " - Fragment Shader";
    glObjectLabel(GL_SHADER, fs, static_cast<GLsizei>(fragment_shader_name.GetSize()), *fragment_shader_name);

    const auto program_result = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!program_result.HasValue())
    {
        return ResultType(program_result.GetError());
    }
    const GLuint program = program_result.GetValue();
    const Opal::StringUtf8 program_name = debug_name + " - Shader Program";
    glObjectLabel(GL_PROGRAM, program, static_cast<GLsizei>(program_name.GetSize()), *program_name);

    auto layout_result = BuildVertexLayout(vs_result.vertex_inputs);
    if (!layout_result.HasValue())
    {
        glDeleteProgram(program);
        return ResultType(layout_result.GetError());
    }

    ShaderBuildResult out;
    out.program = program;
    out.vertex_entry = std::move(vs_entry);
    out.fragment_entry = std::move(fs_entry);
    out.parameters = std::move(merged);
    out.vertex_layout = std::move(layout_result).GetValue();
    return ResultType(std::move(out));
}

Opal::Expected<ShaderBuildResult, Rndr::ErrorCode> BuildFromTwoSources(const Opal::StringUtf8& vertex_source,
                                                                       const Opal::StringUtf8& fragment_source,
                                                                       Opal::StringUtf8 debug_name)
{
    using ResultType = Opal::Expected<ShaderBuildResult, Rndr::ErrorCode>;

    // The compiler boundary, same as BuildFromSingleSource: whatever a step reports comes out of this block
    // as the code it reported.
    Opal::StringUtf8 vs_entry;
    Opal::StringUtf8 fs_entry;
    Rndr::CompileResult vs_result;
    Rndr::CompileResult fs_result;
    Opal::DynamicArray<Rndr::ShaderParameter> merged;
    {
        Rndr::ShaderCompiler vs_compiler;
        const Rndr::ErrorCode vs_load_status = vs_compiler.LoadModule(vertex_source, Rndr::ShaderOutputFormat::Glsl);
        if (vs_load_status != Rndr::ErrorCode::Success)
        {
            RNDR_LOG_ERROR("Canvas: compiling the shader failed");
            return ResultType(vs_load_status);
        }
        Opal::DynamicArray<Rndr::EntryPointInfo> vs_entries;
        RNDR_CANVAS_TAKE(vs_entries, vs_compiler.DiscoverEntryPoints())
        RNDR_CANVAS_TAKE(vs_entry, Rndr::ShaderCompiler::FindSingleEntryPoint(vs_entries, Rndr::ShaderStage::Vertex, "vertex"))

        Rndr::ShaderCompiler fs_compiler;
        const Rndr::ErrorCode fs_load_status = fs_compiler.LoadModule(fragment_source, Rndr::ShaderOutputFormat::Glsl);
        if (fs_load_status != Rndr::ErrorCode::Success)
        {
            RNDR_LOG_ERROR("Canvas: compiling the shader failed");
            return ResultType(fs_load_status);
        }
        Opal::DynamicArray<Rndr::EntryPointInfo> fs_entries;
        RNDR_CANVAS_TAKE(fs_entries, fs_compiler.DiscoverEntryPoints())
        RNDR_CANVAS_TAKE(fs_entry, Rndr::ShaderCompiler::FindSingleEntryPoint(fs_entries, Rndr::ShaderStage::Fragment, "fragment"))

        RNDR_CANVAS_TAKE(vs_result, vs_compiler.CompileEntryPoint(vs_entry))
        RNDR_CANVAS_TAKE(fs_result, fs_compiler.CompileEntryPoint(fs_entry))
        RNDR_CANVAS_TAKE(merged, Rndr::ShaderCompiler::MergeParameters(vs_result.parameters, fs_result.parameters))
    }

    if (vs_result.stage != Rndr::ShaderStage::Vertex)
    {
        RNDR_LOG_ERROR("Canvas: Vertex entry point does not have [shader(\"vertex\")] annotation");
        return ResultType(Rndr::ErrorCode::ShaderCompilationError);
    }
    if (fs_result.stage != Rndr::ShaderStage::Fragment)
    {
        RNDR_LOG_ERROR("Canvas: Fragment entry point does not have [shader(\"fragment\")] annotation");
        return ResultType(Rndr::ErrorCode::ShaderCompilationError);
    }

    const auto vs_gl_result = CreateShaderFromGlsl(GL_VERTEX_SHADER, vs_result.code.GetData(), vs_result.code.GetSize());
    if (!vs_gl_result.HasValue())
    {
        return ResultType(vs_gl_result.GetError());
    }
    const GLuint vs = vs_gl_result.GetValue();
    const Opal::StringUtf8 vertex_shader_name = debug_name + " - Vertex Shader";
    glObjectLabel(GL_SHADER, vs, static_cast<GLsizei>(vertex_shader_name.GetSize()), *vertex_shader_name);

    const auto fs_gl_result = CreateShaderFromGlsl(GL_FRAGMENT_SHADER, fs_result.code.GetData(), fs_result.code.GetSize());
    if (!fs_gl_result.HasValue())
    {
        glDeleteShader(vs);
        return ResultType(fs_gl_result.GetError());
    }
    const GLuint fs = fs_gl_result.GetValue();
    const Opal::StringUtf8 fragment_shader_name = debug_name + " - Fragment Shader";
    glObjectLabel(GL_SHADER, fs, static_cast<GLsizei>(fragment_shader_name.GetSize()), *fragment_shader_name);

    const auto program_result = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!program_result.HasValue())
    {
        return ResultType(program_result.GetError());
    }
    const GLuint program = program_result.GetValue();
    const Opal::StringUtf8 program_name = debug_name + " - Shader Program";
    glObjectLabel(GL_PROGRAM, program, static_cast<GLsizei>(program_name.GetSize()), *program_name);

    auto layout_result = BuildVertexLayout(vs_result.vertex_inputs);
    if (!layout_result.HasValue())
    {
        glDeleteProgram(program);
        return ResultType(layout_result.GetError());
    }

    ShaderBuildResult out;
    out.program = program;
    out.vertex_entry = std::move(vs_entry);
    out.fragment_entry = std::move(fs_entry);
    out.parameters = std::move(merged);
    out.vertex_layout = std::move(layout_result).GetValue();
    return ResultType(std::move(out));
}

}  // namespace

Opal::Expected<Rndr::Canvas::Shader, Rndr::ErrorCode> Rndr::Canvas::Shader::FromSource(const Opal::StringUtf8& path,
                                                                                       Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSource");
    using ResultType = Opal::Expected<Shader, ErrorCode>;

    if (!Opal::Exists(path))
    {
        RNDR_LOG_ERROR("Canvas: Shader file does not exist: {}", *path);
        return ResultType(ErrorCode::FileNotFound);
    }
    const Opal::StringUtf8 source = File::ReadEntireTextFile(path);
    if (source.IsEmpty())
    {
        RNDR_LOG_ERROR("Canvas: Failed to read shader file or file is empty: {}", *path);
        return ResultType(ErrorCode::InvalidArgument);
    }

    return FromSourceInMemory(source, std::move(debug_name));
}

Opal::Expected<Rndr::Canvas::Shader, Rndr::ErrorCode> Rndr::Canvas::Shader::FromSourceInMemory(const Opal::StringUtf8& source,
                                                                                               Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSourceInMemory");
    using ResultType = Opal::Expected<Shader, ErrorCode>;

    if (source.IsEmpty())
    {
        RNDR_LOG_ERROR("Canvas: Shader source is empty");
        return ResultType(ErrorCode::InvalidArgument);
    }

    auto build_result = BuildFromSingleSource(source, debug_name.Clone());
    RNDR_CANVAS_CHECK_EXPECTED(build_result.GetErrorOr(ErrorCode::Success), ResultType);
    ShaderBuildResult build = std::move(build_result).GetValue();

    Shader shader;
    shader.m_program = build.program;
    shader.m_vertex_source = source.Clone();
    shader.m_vertex_entry = std::move(build.vertex_entry);
    shader.m_fragment_entry = std::move(build.fragment_entry);
    shader.m_parameters = std::move(build.parameters);
    shader.m_vertex_layout = std::move(build.vertex_layout);
    shader.m_num_threads = build.num_threads;
    shader.m_debug_name = std::move(debug_name);

    return ResultType(std::move(shader));
}

Opal::Expected<Rndr::Canvas::Shader, Rndr::ErrorCode> Rndr::Canvas::Shader::FromSources(const Opal::StringUtf8& vertex_path,
                                                                                        const Opal::StringUtf8& fragment_path,
                                                                                        Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSources");
    using ResultType = Opal::Expected<Shader, ErrorCode>;

    if (!Opal::Exists(vertex_path) || !Opal::Exists(fragment_path))
    {
        RNDR_LOG_ERROR("Canvas: Shader file does not exist: {} or {}", *vertex_path, *fragment_path);
        return ResultType(ErrorCode::FileNotFound);
    }
    const Opal::StringUtf8 vs_source = File::ReadEntireTextFile(vertex_path);
    if (vs_source.IsEmpty())
    {
        RNDR_LOG_ERROR("Canvas: Failed to read vertex shader file or file is empty: {}", *vertex_path);
        return ResultType(ErrorCode::InvalidArgument);
    }

    const Opal::StringUtf8 fs_source = File::ReadEntireTextFile(fragment_path);
    if (fs_source.IsEmpty())
    {
        RNDR_LOG_ERROR("Canvas: Failed to read fragment shader file or file is empty: {}", *fragment_path);
        return ResultType(ErrorCode::InvalidArgument);
    }

    return FromSourcesInMemory(vs_source, fs_source, std::move(debug_name));
}

Opal::Expected<Rndr::Canvas::Shader, Rndr::ErrorCode> Rndr::Canvas::Shader::FromSourcesInMemory(const Opal::StringUtf8& vertex_source,
                                                                                                const Opal::StringUtf8& fragment_source,
                                                                                                Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSourcesInMemory");
    using ResultType = Opal::Expected<Shader, ErrorCode>;

    if (vertex_source.IsEmpty())
    {
        RNDR_LOG_ERROR("Canvas: Vertex shader source is empty");
        return ResultType(ErrorCode::InvalidArgument);
    }
    if (fragment_source.IsEmpty())
    {
        RNDR_LOG_ERROR("Canvas: Fragment shader source is empty");
        return ResultType(ErrorCode::InvalidArgument);
    }

    auto build_result = BuildFromTwoSources(vertex_source, fragment_source, std::move(debug_name));
    RNDR_CANVAS_CHECK_EXPECTED(build_result.GetErrorOr(ErrorCode::Success), ResultType);
    ShaderBuildResult build = std::move(build_result).GetValue();

    Shader shader;
    shader.m_program = build.program;
    shader.m_vertex_source = vertex_source.Clone();
    shader.m_vertex_entry = std::move(build.vertex_entry);
    shader.m_fragment_source = fragment_source.Clone();
    shader.m_fragment_entry = std::move(build.fragment_entry);
    shader.m_parameters = std::move(build.parameters);
    shader.m_vertex_layout = std::move(build.vertex_layout);
    shader.m_num_threads = build.num_threads;

    return ResultType(std::move(shader));
}

Rndr::Canvas::Shader::~Shader()
{
    Destroy();
}

Rndr::Canvas::Shader::Shader(Shader&& other) noexcept
    : m_program(other.m_program),
      m_vertex_source(std::move(other.m_vertex_source)),
      m_vertex_entry(std::move(other.m_vertex_entry)),
      m_fragment_source(std::move(other.m_fragment_source)),
      m_fragment_entry(std::move(other.m_fragment_entry)),
      m_parameters(std::move(other.m_parameters)),
      m_vertex_layout(std::move(other.m_vertex_layout)),
      m_num_threads(other.m_num_threads)
{
    other.m_program = 0;
    other.m_num_threads = {};
}

Rndr::Canvas::Shader& Rndr::Canvas::Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_program = other.m_program;
        m_vertex_source = std::move(other.m_vertex_source);
        m_vertex_entry = std::move(other.m_vertex_entry);
        m_fragment_source = std::move(other.m_fragment_source);
        m_fragment_entry = std::move(other.m_fragment_entry);
        m_parameters = std::move(other.m_parameters);
        m_vertex_layout = std::move(other.m_vertex_layout);
        m_num_threads = other.m_num_threads;
        other.m_program = 0;
        other.m_num_threads = {};
    }
    return *this;
}

Opal::Expected<Rndr::Canvas::Shader, Rndr::ErrorCode> Rndr::Canvas::Shader::Clone() const
{
    if (!IsValid())
    {
        RNDR_LOG_ERROR("Canvas: Cannot clone an invalid shader");
        return Opal::Expected<Shader, ErrorCode>(ErrorCode::InvalidArgument);
    }

    Opal::StringUtf8 clone_debug_name = m_debug_name.Clone() + " Clone";
    if (m_fragment_source.IsEmpty())
    {
        return FromSourceInMemory(m_vertex_source, std::move(clone_debug_name));
    }
    return FromSourcesInMemory(m_vertex_source, m_fragment_source, std::move(clone_debug_name));
}

void Rndr::Canvas::Shader::Destroy()
{
    if (m_program != 0)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_parameters.Clear();
    m_vertex_layout = VertexLayout();
    m_num_threads = {};
}

bool Rndr::Canvas::Shader::IsValid() const
{
    return m_program != 0;
}

Rndr::u32 Rndr::Canvas::Shader::GetNativeHandle() const
{
    return m_program;
}

const Opal::DynamicArray<Rndr::ShaderParameter>& Rndr::Canvas::Shader::GetParameters() const
{
    return m_parameters;
}

const Rndr::ShaderParameter* Rndr::Canvas::Shader::FindParameter(const Opal::StringUtf8& name) const
{
    for (u64 i = 0; i < m_parameters.GetSize(); ++i)
    {
        if (m_parameters[i].name == name)
        {
            return &m_parameters[i];
        }
    }
    return nullptr;
}

const Rndr::Canvas::VertexLayout& Rndr::Canvas::Shader::GetVertexLayout() const
{
    return m_vertex_layout;
}

const Rndr::NumThreads& Rndr::Canvas::Shader::GetNumThreads() const
{
    return m_num_threads;
}