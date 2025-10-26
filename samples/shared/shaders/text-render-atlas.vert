//
#version 460 core

layout(binding = 0) uniform PerFrameData
{
    mat4 mvp;
} per_frame_data;

struct Vertex
{
    float p[2];
    float tc[2];
    float color[4];
};

layout(std430, binding = 1) restrict readonly buffer Vertices
{
    Vertex in_vertices[];
};

vec3 getPosition(int i)
{
    return vec3(in_vertices[i].p[0], in_vertices[i].p[1], 0.0f);
}

vec2 getTexCoord(int i)
{
    return vec2(in_vertices[i].tc[0], in_vertices[i].tc[1]);
}

vec4 getColor(int i)
{
    return vec4(in_vertices[i].color[0], in_vertices[i].color[1], in_vertices[i].color[2], in_vertices[i].color[3]);
}

layout (location=0) out vec2 out_uv;
layout (location=1) out vec4 out_color;

void main()
{
    vec3 pos = getPosition(gl_VertexID);
    gl_Position = per_frame_data.mvp * vec4(pos, 1.0);

    out_uv = getTexCoord(gl_VertexID);
    out_color = getColor(gl_VertexID);
}