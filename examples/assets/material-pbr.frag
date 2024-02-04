#version 460

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : enable

#include "material-data.glsl"

layout(std140, location = 0) uniform PerFrameData
{
    mat4 view_transform;
    mat4 projection_transform;
    vec3 camera_position_world;
};

layout(std430, binding = 2) restrict readonly buffer Materials
{
    MaterialData in_materials[];
};

layout (location = 0) in vec2 in_tex_coords;
layout (location = 1) in vec3 in_normal_world;
layout (location = 2) in vec3 in_position_world;
layout (location = 3) in flat uint in_material_index;

// Textures that are same for all materials
layout (binding = 5) uniform samplerCube tex_env_map;
layout (binding = 6) uniform samplerCube tex_env_map_irradiance;
layout (binding = 7) uniform sampler2D tex_brdf_lut;

layout (location = 0) out vec4 out_frag_color;

#include "pbr-shared.glsl"
#include "alpha-test.glsl"

void main()
{
    MaterialData mtl = in_materials[in_material_index];

    // Figure out albedo color
    vec4 albedo = mtl.albedo_color;
    if (mtl.albedo_map > 0)
    {
        albedo = texture(sampler2D(unpackUint2x32(mtl.albedo_map)), in_tex_coords);
    }

    // Figute out the normal
    vec3 normal_sample = vec3(0.0, 0.0, 0.0);
    if (mtl.normal_map > 0)
    {
        normal_sample = texture(sampler2D(unpackUint2x32(mtl.normal_map)), in_tex_coords).xyz;
    }

    // If alpha test fails, discard the fragment
    RunAlphaTest(albedo.a, mtl.alpha_test);

    vec3 normal_world = normalize(in_normal_world);
    if (length(normal_sample) > 0.5)
    {
        normal_world = PerturbNormal(normal_world, normalize(camera_position_world - in_position_world), normal_sample, in_tex_coords);
    }

    // Hardcoded directional light
    vec3 light_direction = normalize(vec3(-1.0, 1.0, 0.1));

    float n_dot_l = clamp(dot(normal_world, light_direction), 0.3, 1.0);

    out_frag_color = vec4(albedo.rgb * n_dot_l, albedo.a);
}
