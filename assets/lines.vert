//
#version 460 core

layout(std140, binding = 0) uniform PerFrameData
{
    mat4 MVP;
};

struct Vertex
{
    float p[3];
    float c[4];
};

layout(std430, binding = 1) restrict readonly buffer Vertices
{
    Vertex in_Vertices[];
};

vec3 getPosition(int i)
{
    return vec3(in_Vertices[i].p[0], in_Vertices[i].p[1], in_Vertices[i].p[2]);
}

vec4 getColor(int i)
{
    return vec4(in_Vertices[i].c[0], in_Vertices[i].c[1], in_Vertices[i].c[2], in_Vertices[i].c[3]);
}

layout (location=0) out vec4 lineColor;

void main()
{
    vec3 pos = getPosition(gl_VertexID);
    gl_Position = MVP * vec4(pos, 1.0);

    lineColor = getColor(gl_VertexID);
}