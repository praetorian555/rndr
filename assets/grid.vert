#version 460

layout(set = 0, binding = 0) uniform ViewUniforms {
    mat4 view;
    mat4 proj;
    vec3 pos;
} view;

vec3 grid_plane[6] = vec3[](
    vec3(100, -1, 100), vec3(-100, -1, -100), vec3(-100, -1, 100),
    vec3(-100, -1, -100), vec3(100, -1, 100), vec3(100, -1, -100)
);

layout(location=0) out vec4 world_position;

void main()
{
    gl_Position = view.proj * view.view * vec4(grid_plane[gl_VertexID].xyz, 1);
    world_position = vec4(grid_plane[gl_VertexID].xyz, 1);
}