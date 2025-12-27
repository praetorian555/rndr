#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 view_projection_transform;
    vec3 camera_position_world;
    vec3 light_direction_world;
};

struct Vertex
{
    float position[3];
    float normal[3];
    float tex_coord[2];
};

layout(std430, binding = 1) restrict readonly buffer Vertices
{
    Vertex vertices[];
};

struct Instance
{
    mat4 model_matrix;
    mat4 normal_matrix;

    vec4 albedo_color;
    vec4 emissive_color;
    vec4 roughness;

    float transparency_factor;
    float alpha_test;
    float metalic_factor;
    uint flags;
};

layout(std430, binding = 2) restrict readonly buffer Instances
{
    Instance instances[];
};

vec3 GetPosition(int i)
{
    return vec3(vertices[i].position[0], vertices[i].position[1], vertices[i].position[2]);
}

vec3 GetNormal(int i)
{
    return vec3(vertices[i].normal[0], vertices[i].normal[1], vertices[i].normal[2]);
}

vec2 GetTexCoord(int i)
{
    return vec2(vertices[i].tex_coord[0], vertices[i].tex_coord[1]);
}

layout (location = 0) out vec3 out_normal_world;
layout (location = 1) out vec2 out_tex_coords;
layout (location = 2) out vec3 out_position_world;
layout (location = 3) out vec4 out_albedo_color;
layout (location = 4) out vec4 out_emissive_color;
layout (location = 5) out vec4 out_roughness;
layout (location = 6) out float out_transparency_factor;
layout (location = 7) out float out_alpha_test;
layout (location = 8) out float out_metalic_factor;
layout (location = 9) out flat uint out_flags;

void main()
{
    mat4 model_matrix = instances[gl_DrawID].model_matrix;
    mat3 normal_matrix = mat3(instances[gl_DrawID].normal_matrix);

    mat4 mvp = view_projection_transform * model_matrix;
    vec3 pos = GetPosition(gl_VertexID);
    gl_Position = mvp * vec4(pos, 1.0);

    out_normal_world = normalize(normal_matrix * GetNormal(gl_VertexID));
    out_tex_coords = GetTexCoord(gl_VertexID);
    out_position_world = (model_matrix * vec4(pos, 1.0)).xyz;
    out_albedo_color = instances[gl_DrawID].albedo_color;
    out_emissive_color = instances[gl_DrawID].emissive_color;
    out_roughness = instances[gl_DrawID].roughness;
    out_transparency_factor = instances[gl_DrawID].transparency_factor;
    out_alpha_test = instances[gl_DrawID].alpha_test;
    out_metalic_factor = instances[gl_DrawID].metalic_factor;
    out_flags = instances[gl_DrawID].flags;
}
