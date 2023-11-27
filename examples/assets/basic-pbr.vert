#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 view_matrix;
    mat4 projection_matrix;
    vec4 camera_position_world;
};

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 tex_coord;
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
    return vertices[i].position;
}

vec3 GetNormal(int i)
{
    return vertices[i].normal;
}

vec2 GetTexCoord(int i)
{
    return vertices[i].tex_coord;
}

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec2 out_tex_coord;
layout (location = 2) out vec3 out_position_world;

void main()
{
    mat4 model_matrix = instances[gl_DrawID].model_matrix;
    mat3 normal_matrix = mat3(instances[gl_DrawID].normal_matrix);

    mat4 mvp = projection_matrix * view_matrix * model_matrix;
    vec3 pos = GetPosition(gl_VertexID);
    gl_Position = mvp * vec4(pos, 1.0);

    out_normal = normal_matrix * GetNormal(gl_VertexID);
    out_tex_coord = GetTexCoord(gl_VertexID);
    out_position_world = (model_matrix * vec4(pos, 1.0)).xyz;
}