#include "rndr/canvas/shader.hpp"

#include "glad/glad.h"

#include "canvas/spirv-patch.hpp"
#include "rndr/core/shader-compiler.hpp"
#include "rndr/exception.hpp"
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

Rndr::Canvas::VertexLayout BuildVertexLayout(const Opal::DynamicArray<Rndr::VertexInputAttribute>& vertex_inputs)
{
    Rndr::Canvas::VertexLayout vertex_layout;

    for (Rndr::u64 i = 0; i < vertex_inputs.GetSize(); ++i)
    {
        const Rndr::VertexInputAttribute& input = vertex_inputs[i];

        const Rndr::Canvas::Attrib attrib = AttribFromName(input.name.GetData());
        if (attrib == Rndr::Canvas::Attrib::EnumCount)
        {
            Opal::StringUtf8 msg = Opal::StringUtf8("Cannot map vertex attribute '") + input.name + "' to a known Attrib semantic!";
            throw Rndr::GraphicsAPIException(0, msg.GetData());
        }

        const Rndr::Canvas::Format format = FormatFromVertexInput(input);
        if (format == Rndr::Canvas::Format::EnumCount)
        {
            Opal::StringUtf8 msg = Opal::StringUtf8("Unsupported vertex attribute format for '") + input.name + "'!";
            throw Rndr::GraphicsAPIException(0, msg.GetData());
        }

        vertex_layout.Add(attrib, format);
    }

    return vertex_layout;
}

// ---------------------------------------------------------------------------
// OpenGL shader and program creation.
// ---------------------------------------------------------------------------

GLuint CreateShaderFromSpirv(GLenum stage, const void* spirv_data, size_t spirv_size)
{
    const size_t word_count = spirv_size / sizeof(Rndr::u32);
    Opal::DynamicArray<Rndr::u32> patched(word_count);
    memcpy(patched.GetData(), spirv_data, spirv_size);
    Rndr::Impl::PatchSpirv(patched);

    const GLuint shader = glCreateShader(stage);
    if (shader == 0)
    {
        throw Rndr::GraphicsAPIException(0, "Failed to create GL shader!");
    }

    glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, patched.GetData(), static_cast<GLsizei>(spirv_size));

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        glDeleteShader(shader);
        throw Rndr::GraphicsAPIException(err, "Failed to upload SPIR-V binary!");
    }

    glSpecializeShader(shader, "main", 0, nullptr, nullptr);

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

        Opal::StringUtf8 msg = Opal::StringUtf8("Failed to specialize shader:\n") + log;
        throw Rndr::GraphicsAPIException(0, msg.GetData());
    }

    return shader;
}

GLuint LinkProgram(GLuint vertex_shader, GLuint fragment_shader)
{
    const GLuint program = glCreateProgram();
    if (program == 0)
    {
        throw Rndr::GraphicsAPIException(0, "Failed to create GL program!");
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

        Opal::StringUtf8 msg = Opal::StringUtf8("Failed to link shader program:\n") + log;
        RNDR_LOG_ERROR("{}", *msg);
        throw Rndr::GraphicsAPIException(0, msg.GetData());
    }

    return program;
}

GLuint LinkProgram(GLuint shader)
{
    const GLuint program = glCreateProgram();
    if (program == 0)
    {
        throw Rndr::GraphicsAPIException(0, "Failed to create GL program!");
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

        Opal::StringUtf8 msg = Opal::StringUtf8("Failed to link shader program:\n") + log;
        throw Rndr::GraphicsAPIException(0, msg.GetData());
    }

    return program;
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

ShaderBuildResult BuildFromSingleSource(const Opal::StringUtf8& source, Opal::StringUtf8 debug_name)
{
    Rndr::ShaderCompiler compiler;
    compiler.LoadModule(source);
    Opal::DynamicArray<Rndr::EntryPointInfo> entries = compiler.DiscoverEntryPoints();

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
        throw Rndr::GraphicsAPIException(0, "Shader source contains both compute and graphics entry points!");
    }

    // Compute path.
    if (compute_count > 0)
    {
        const Opal::StringUtf8 cs_entry = Rndr::ShaderCompiler::FindSingleEntryPoint(entries, Rndr::ShaderStage::Compute, "compute");
        Rndr::CompileResult cs_result = compiler.CompileEntryPoint(cs_entry);
        if (cs_result.stage != Rndr::ShaderStage::Compute)
        {
            throw Rndr::GraphicsAPIException(0, "Compute entry point does not have [shader(\"compute\")] annotation!");
        }

        const GLuint cs = CreateShaderFromSpirv(GL_COMPUTE_SHADER, cs_result.spirv.GetData(), cs_result.spirv.GetSize());
        const Opal::StringUtf8 shader_name = debug_name + " - Compute Shader";
        glObjectLabel(GL_SHADER, cs, static_cast<GLsizei>(shader_name.GetSize()), *shader_name);
        GLuint program = 0;
        try
        {
            program = LinkProgram(cs);
            const Opal::StringUtf8 program_name = debug_name + " - Shader Program";
            glObjectLabel(GL_PROGRAM, program, static_cast<GLsizei>(program_name.GetSize()), *program_name);
        }
        catch (...)
        {
            glDeleteShader(cs);
            throw;
        }
        glDeleteShader(cs);

        ShaderBuildResult out;
        out.program = program;
        out.parameters = std::move(cs_result.parameters);
        out.num_threads = cs_result.num_threads;
        return out;
    }

    // Graphics path.
    Opal::StringUtf8 vs_entry = Rndr::ShaderCompiler::FindSingleEntryPoint(entries, Rndr::ShaderStage::Vertex, "vertex");
    Opal::StringUtf8 fs_entry = Rndr::ShaderCompiler::FindSingleEntryPoint(entries, Rndr::ShaderStage::Fragment, "fragment");

    Rndr::CompileResult vs_result = compiler.CompileEntryPoint(vs_entry);
    if (vs_result.stage != Rndr::ShaderStage::Vertex)
    {
        throw Rndr::GraphicsAPIException(0, "Vertex entry point does not have [shader(\"vertex\")] annotation!");
    }

    const Rndr::CompileResult fs_result = compiler.CompileEntryPoint(fs_entry);
    if (fs_result.stage != Rndr::ShaderStage::Fragment)
    {
        throw Rndr::GraphicsAPIException(0, "Fragment entry point does not have [shader(\"fragment\")] annotation!");
    }

    Opal::DynamicArray<Rndr::ShaderParameter> merged = Rndr::ShaderCompiler::MergeParameters(vs_result.parameters, fs_result.parameters);

    const GLuint vs = CreateShaderFromSpirv(GL_VERTEX_SHADER, vs_result.spirv.GetData(), vs_result.spirv.GetSize());
    const Opal::StringUtf8 vertex_shader_name = debug_name + " - Vertex Shader";
    glObjectLabel(GL_SHADER, vs, static_cast<GLsizei>(vertex_shader_name.GetSize()), *vertex_shader_name);
    GLuint fs = 0;
    try
    {
        fs = CreateShaderFromSpirv(GL_FRAGMENT_SHADER, fs_result.spirv.GetData(), fs_result.spirv.GetSize());
        const Opal::StringUtf8 fragment_shader_name = debug_name + " - Fragment Shader";
        glObjectLabel(GL_SHADER, fs, static_cast<GLsizei>(fragment_shader_name.GetSize()), *fragment_shader_name);
    }
    catch (...)
    {
        glDeleteShader(vs);
        throw;
    }

    GLuint program = 0;
    try
    {
        program = LinkProgram(vs, fs);
        const Opal::StringUtf8 program_name = debug_name + " - Shader Program";
        glObjectLabel(GL_PROGRAM, program, static_cast<GLsizei>(program_name.GetSize()), *program_name);
    }
    catch (...)
    {
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    ShaderBuildResult out;
    out.program = program;
    out.vertex_entry = std::move(vs_entry);
    out.fragment_entry = std::move(fs_entry);
    out.parameters = std::move(merged);
    out.vertex_layout = BuildVertexLayout(vs_result.vertex_inputs);
    return out;
}

ShaderBuildResult BuildFromTwoSources(const Opal::StringUtf8& vertex_source, const Opal::StringUtf8& fragment_source,
                                      Opal::StringUtf8 debug_name)
{
    Rndr::ShaderCompiler vs_compiler;
    vs_compiler.LoadModule(vertex_source);
    const Opal::DynamicArray<Rndr::EntryPointInfo> vs_entries = vs_compiler.DiscoverEntryPoints();
    Opal::StringUtf8 vs_entry = Rndr::ShaderCompiler::FindSingleEntryPoint(vs_entries, Rndr::ShaderStage::Vertex, "vertex");

    Rndr::ShaderCompiler fs_compiler;
    fs_compiler.LoadModule(fragment_source);
    const Opal::DynamicArray<Rndr::EntryPointInfo> fs_entries = fs_compiler.DiscoverEntryPoints();
    Opal::StringUtf8 fs_entry = Rndr::ShaderCompiler::FindSingleEntryPoint(fs_entries, Rndr::ShaderStage::Fragment, "fragment");

    Rndr::CompileResult vs_result = vs_compiler.CompileEntryPoint(vs_entry);
    if (vs_result.stage != Rndr::ShaderStage::Vertex)
    {
        throw Rndr::GraphicsAPIException(0, "Vertex entry point does not have [shader(\"vertex\")] annotation!");
    }

    const Rndr::CompileResult fs_result = fs_compiler.CompileEntryPoint(fs_entry);
    if (fs_result.stage != Rndr::ShaderStage::Fragment)
    {
        throw Rndr::GraphicsAPIException(0, "Fragment entry point does not have [shader(\"fragment\")] annotation!");
    }

    Opal::DynamicArray<Rndr::ShaderParameter> merged = Rndr::ShaderCompiler::MergeParameters(vs_result.parameters, fs_result.parameters);

    const GLuint vs = CreateShaderFromSpirv(GL_VERTEX_SHADER, vs_result.spirv.GetData(), vs_result.spirv.GetSize());
    const Opal::StringUtf8 vertex_shader_name = debug_name + " - Vertex Shader";
    glObjectLabel(GL_SHADER, vs, static_cast<GLsizei>(vertex_shader_name.GetSize()), *vertex_shader_name);
    GLuint fs = 0;
    try
    {
        fs = CreateShaderFromSpirv(GL_FRAGMENT_SHADER, fs_result.spirv.GetData(), fs_result.spirv.GetSize());
        const Opal::StringUtf8 fragment_shader_name = debug_name + " - Fragment Shader";
        glObjectLabel(GL_SHADER, fs, static_cast<GLsizei>(fragment_shader_name.GetSize()), *fragment_shader_name);
    }
    catch (...)
    {
        glDeleteShader(vs);
        throw;
    }

    GLuint program = 0;
    try
    {
        program = LinkProgram(vs, fs);
        const Opal::StringUtf8 program_name = debug_name + " - Shader Program";
        glObjectLabel(GL_PROGRAM, program, static_cast<GLsizei>(program_name.GetSize()), *program_name);
    }
    catch (...)
    {
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    ShaderBuildResult out;
    out.program = program;
    out.vertex_entry = std::move(vs_entry);
    out.fragment_entry = std::move(fs_entry);
    out.parameters = std::move(merged);
    out.vertex_layout = BuildVertexLayout(vs_result.vertex_inputs);
    return out;
}

}  // namespace

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSource(const Opal::StringUtf8& path, Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSource");

    const Opal::StringUtf8 source = File::ReadEntireTextFile(path);
    if (source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Failed to read shader file or file is empty!");
    }

    return FromSourceInMemory(source, std::move(debug_name));
}

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSourceInMemory(const Opal::StringUtf8& source, Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSourceInMemory");

    if (source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Shader source is empty!");
    }

    ShaderBuildResult build = BuildFromSingleSource(source, debug_name.Clone());

    Shader shader;
    shader.m_program = build.program;
    shader.m_vertex_source = source.Clone();
    shader.m_vertex_entry = std::move(build.vertex_entry);
    shader.m_fragment_entry = std::move(build.fragment_entry);
    shader.m_parameters = std::move(build.parameters);
    shader.m_vertex_layout = std::move(build.vertex_layout);
    shader.m_num_threads = build.num_threads;
    shader.m_debug_name = std::move(debug_name);

    return shader;
}

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSources(const Opal::StringUtf8& vertex_path, const Opal::StringUtf8& fragment_path,
                                                       Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSources");

    const Opal::StringUtf8 vs_source = File::ReadEntireTextFile(vertex_path);
    if (vs_source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Failed to read vertex shader file or file is empty!");
    }

    const Opal::StringUtf8 fs_source = File::ReadEntireTextFile(fragment_path);
    if (fs_source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Failed to read fragment shader file or file is empty!");
    }

    return FromSourcesInMemory(vs_source, fs_source, std::move(debug_name));
}

Rndr::Canvas::Shader Rndr::Canvas::Shader::FromSourcesInMemory(const Opal::StringUtf8& vertex_source,
                                                               const Opal::StringUtf8& fragment_source, Opal::StringUtf8 debug_name)
{
    RNDR_CPU_EVENT_SCOPED("Canvas::Shader::FromSourcesInMemory");

    if (vertex_source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Vertex shader source is empty!");
    }
    if (fragment_source.IsEmpty())
    {
        throw Opal::InvalidArgumentException(__FUNCTION__, "Fragment shader source is empty!");
    }

    ShaderBuildResult build = BuildFromTwoSources(vertex_source, fragment_source, std::move(debug_name));

    Shader shader;
    shader.m_program = build.program;
    shader.m_vertex_source = vertex_source.Clone();
    shader.m_vertex_entry = std::move(build.vertex_entry);
    shader.m_fragment_source = fragment_source.Clone();
    shader.m_fragment_entry = std::move(build.fragment_entry);
    shader.m_parameters = std::move(build.parameters);
    shader.m_vertex_layout = std::move(build.vertex_layout);
    shader.m_num_threads = build.num_threads;

    return shader;
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

Rndr::Canvas::Shader Rndr::Canvas::Shader::Clone() const
{
    if (!IsValid())
    {
        return {};
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