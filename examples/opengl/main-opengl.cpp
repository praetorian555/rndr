#include <rndr/rndr.h>

#include <glad/glad.h>

int main()
{
    const rndr::RndrContext rndr_context;
    rndr::ScopePtr<rndr::Window> window = rndr_context.CreateWin(800, 600);
    rndr::ScopePtr<rndr::GraphicsContext> graphics_context =
        rndr_context.CreateGraphicsContext({.window_handle = window->GetNativeWindowHandle()});

    GLuint vao;
    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    const char* shader_code_vertex = R"(
        #version 460 core
        layout (location=0) out vec3 color;
        const vec2 pos[3] = vec2[3](
          vec2(-0.6, -0.4),
          vec2(0.6, -0.4),
          vec2(0.0, 0.6)
        );
        const vec3 col[3] = vec3[3](
          vec3(1.0, 0.0, 0.0),
          vec3(0.0, 1.0, 0.0),
          vec3(0.0, 0.0, 1.0)
        );
        void main() {
          gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
          color = col[gl_VertexID];
        }
    )";

    const char* shader_code_fragment = R"(
        #version 460 core
        layout (location=0) in vec3 color;
        layout (location=0) out vec4 out_FragColor;
        void main() {
          out_FragColor = vec4(color, 1.0);
        };
    )";

    const GLuint shader_vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader_vertex, 1, &shader_code_vertex, nullptr);
    glCompileShader(shader_vertex);
    const GLuint shader_fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader_fragment, 1, &shader_code_fragment, nullptr);
    glCompileShader(shader_fragment);
    const GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, shader_vertex);
    glAttachShader(shader_program, shader_fragment);
    glLinkProgram(shader_program);
    glUseProgram(shader_program);

    rndr::ImGuiWrapper::Init(window.GetRef(), graphics_context.GetRef());

    while (!window->IsClosed())
    {
        window->ProcessEvents();

        glViewport(0, 0, window->GetWidth(), window->GetHeight());

        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        rndr::ImGuiWrapper::StartFrame();
        rndr::ImGuiWrapper::EndFrame();

        graphics_context->Present(nullptr, false);
    }

    glDeleteProgram(shader_program);
    glDeleteShader(shader_fragment);
    glDeleteShader(shader_vertex);
    glDeleteVertexArrays(1, &vao);

    return 0;
}