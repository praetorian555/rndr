#version 460

layout(location = 0) in vec4 world_position;

layout(location = 0) out vec4 out_color;

void main()
{
    vec2 coord = world_position.xz * 2;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    out_color = vec4(0.2f, 0.2, 0.2, 1.0 - min(line, 1.0));
}