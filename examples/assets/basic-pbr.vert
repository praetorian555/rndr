#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 view_projection_matrix;
    vec4 camera_position_world;
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

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec2 out_tex_coord;
layout (location = 2) out vec3 out_position_world;

void main()
{
    mat4 model_matrix = instances[gl_DrawID].model_matrix;
    mat3 normal_matrix = mat3(instances[gl_DrawID].normal_matrix);

    mat4 mvp = view_projection_matrix * model_matrix;
    vec3 pos = GetPosition(gl_VertexID);
    gl_Position = mvp * vec4(pos, 1.0);

    out_normal = normal_matrix * GetNormal(gl_VertexID);
    out_tex_coord = GetTexCoord(gl_VertexID);
    out_position_world = (model_matrix * vec4(pos, 1.0)).xyz;
}