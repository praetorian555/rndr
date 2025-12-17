#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 view_projection_transform;
    vec3 camera_position_world;
};

layout (location = 0) in vec3 in_normal_world;
layout (location = 1) in vec2 in_tex_coords;
layout (location = 2) in vec3 in_position_world;
layout (location = 3) in vec4 in_color;

layout (location = 0) out vec4 out_frag_color;

void main()
{
    out_frag_color = in_color;
}
