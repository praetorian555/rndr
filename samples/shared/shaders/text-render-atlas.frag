//
#version 460 core

layout (location=0) in vec2 in_uv;
layout (location=1) in vec4 in_color;

layout (location=0) out vec4 out_color;

layout (binding = 0) uniform sampler2D atlas;

void main()
{
    float alpha = texture(atlas, in_uv).r;
    out_color = vec4(alpha) * in_color;
}
