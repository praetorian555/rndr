#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 view_projection_transform;
    vec3 camera_position_world;
    vec3 light_direction_world;
};

layout (location = 0) in vec3 in_normal_world;
layout (location = 1) in vec2 in_tex_coords;
layout (location = 2) in vec3 in_position_world;
layout (location = 3) in vec4 in_color;

layout (location = 4) in vec3 in_normal_matrix;

layout (location = 0) out vec4 out_frag_color;

vec4 NormalToColor(vec3 normal)
{
    return vec4((normal + 1) * 0.5f, 1.0f);
}

bool CompareVec3(vec3 expected, vec3 actual, float eps)
{
    return abs(expected.x - actual.x) <= eps && abs(expected.y - actual.y) <= eps && abs(expected.z - actual.z) <= eps;
}

void main()
{
    vec3 normal = normalize(in_normal_world);
    vec3 light_direction_world_norm = normalize(light_direction_world);
    float n_dot_l = clamp(dot(normal, light_direction_world_norm), 0.02f, 1.0f);
    vec3 color = in_color.rgb * n_dot_l;
    out_frag_color = vec4(color, in_color.a);
}
