#version 460

layout(location = 0) in vec4 world_position;

layout(location = 0) out vec4 out_color;

void main()
{
    vec2 uv = world_position.xz;
    vec2 deriv_uv = fwidth(uv);
    vec2 line_aa = 1.5 * deriv_uv;
    float line_width = 1 / 50.0f;
    vec2 draw_width = clamp(vec2(line_width), deriv_uv, vec2(0.5));
    // Triangle function that is 0 at whole numbers and grows to 1 an the .5 coordinates
    vec2 grid_uv = 1.0 - abs(fract(uv) * 2.0 - 1.0);
    // I don't quite understand this
    vec2 grid_2 = 1.0f - smoothstep(draw_width - line_aa, draw_width + line_aa, grid_uv);
    grid_2 *= clamp(vec2(line_width, line_width) / draw_width, 0.0f, 1.0f);
    // Since we have two transparent lines overlapping, we are doing alpha blending essentially
    float grid = mix(grid_2.x, 1.0f, grid_2.y);

    out_color = vec4(0, 0, 0, 1.0f);
    if (uv.x > -draw_width.x && uv.x < draw_width.x)
    {
        out_color.x = grid;
    }
    else if (uv.y > -draw_width.y && uv.y < draw_width.y)
    {
        out_color.z = grid;
    }
    else
    {
        out_color.xyz = vec3(grid);
    }
}